#include <JuceHeader.h>
#include "../../Source/DSP.h"
#include "../../Source/Analysis.h"
#include "../../Source/RemoteServer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
volatile std::sig_atomic_t exitRequested = 0;
void requestExit(int) { exitRequested = 1; }
void log(const juce::String& msg)
{
    std::cout << "[Simple DSP Pi] " << msg.toStdString() << std::endl;
}
juce::String configLocation()
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".config").getChildFile("simple-dsp")
        .getChildFile("config.json").getFullPathName();
}
bool privateAddress(const juce::String& ip)
{
    if (ip == "127.0.0.1") return true; // explicit test/localhost access
    const auto parts = juce::StringArray::fromTokens(ip, ".", "");
    if (parts.size() != 4) return false;
    std::array<int,4> a {};
    for (int i = 0; i < 4; ++i)
    {
        if (parts[i].isEmpty() || !parts[i].containsOnly("0123456789")) return false;
        a[static_cast<std::size_t>(i)] = parts[i].getIntValue();
        if (a[static_cast<std::size_t>(i)] < 0 || a[static_cast<std::size_t>(i)] > 255) return false;
    }
    return (a[0] == 10) || (a[0] == 192 && a[1] == 168)
        || (a[0] == 172 && a[1] >= 16 && a[1] <= 31);
}
juce::String safePresetName(const juce::String& s)
{
    return s.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-()")
            .trim().substring(0,60);
}
juce::var defaultConfig()
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty("audio_device", "");
    o->setProperty("sample_rate", 48000);
    o->setProperty("buffer_size", 256);
    o->setProperty("http_port", 8765);
    o->setProperty("bind_address", "");
    o->setProperty("remote_pin_required", true);
    o->setProperty("remote_pin", "");
    o->setProperty("start_muted", true);
    o->setProperty("load_at_launch", false);
    o->setProperty("startup_preset", "");
    return juce::var(o.release());
}
}

class PiEngine final : public juce::AudioIODeviceCallback
{
public:
    std::array<dualdsp::ChannelSettings, 2> settings;
    std::array<dualdsp::AudioTelemetry, 2> telemetry;
    std::atomic<double> sampleRate { 48000.0 };

    PiEngine()
    {
        settings[0].inputRoute.store(0); settings[0].outputRoute.store(0);
        settings[1].inputRoute.store(1); settings[1].outputRoute.store(1);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* d) override
    {
        const double sr = d ? d->getCurrentSampleRate() : 48000.0;
        sampleRate.store(sr, std::memory_order_release);
        for (auto& p : processors) p.prepare(sr);
        for (auto& t : telemetry) t.prepare(sr);
        refreshRoutes(d);
    }

    void audioDeviceStopped() override
    {
        for (auto& t : telemetry) t.prepare(48000.0);
        for (auto& p : callbackInputs) p.store(-1);
        for (auto& p : callbackOutputs) p.store(-1);
    }

    void refreshRoutes(juce::AudioIODevice* device)
    {
        const auto inputBits = device ? device->getActiveInputChannels() : juce::BigInteger{};
        const auto outputBits = device ? device->getActiveOutputChannels() : juce::BigInteger{};
        for (int i = 0; i < 2; ++i)
        {
            const auto idx = static_cast<std::size_t>(i);
            callbackInputs[idx].store(packIndex(inputBits, settings[idx].inputRoute.load()), std::memory_order_release);
            callbackOutputs[idx].store(packIndex(outputBits, settings[idx].outputRoute.load()), std::memory_order_release);
        }
    }

    void audioDeviceIOCallbackWithContext(const float* const* inputData, int inputCount,
                                          float* const* outputData, int outputCount,
                                          int sampleCount, const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ScopedNoDenormals noDenormals;
        for (int i = 0; i < outputCount; ++i)
            if (outputData[i]) juce::FloatVectorOperations::clear(outputData[i], sampleCount);
        // Strictly separate mono paths, never summed. Unassigned output stays silent.
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto idx = static_cast<std::size_t>(ch);
            const int in = callbackInputs[idx].load(std::memory_order_acquire);
            const int out = callbackOutputs[idx].load(std::memory_order_acquire);
            const float* src = (in >= 0 && in < inputCount) ? inputData[in] : nullptr;
            float* dst = (out >= 0 && out < outputCount) ? outputData[out] : nullptr;
            if (!src || !dst)
            {
                telemetry[idx].recordBlock(src, nullptr, sampleCount);
                continue;
            }
            auto& p = processors[idx];
            p.beginBlock(settings[idx]);
            for (int n = 0; n < sampleCount; ++n) dst[n] = p.process(src[n]);
            telemetry[idx].recordBlock(src, dst, sampleCount);
        }
    }
private:
    static int packIndex(const juce::BigInteger& bits, int physical)
    {
        if (physical < 0 || !bits[physical]) return -1;
        int packed = 0;
        for (int i = 0; i < physical; ++i) if (bits[i]) ++packed;
        return packed;
    }
    std::array<dualdsp::MonoProcessor,2> processors;
    std::array<std::atomic<int>,2> callbackInputs { -1,-1 }, callbackOutputs { -1,-1 };
};

class PiHost
{
public:
    PiHost(juce::File file, bool skipAudio, juce::String bindOverride, juce::String selectedDevice)
        : configFile(std::move(file)), noAudio(skipAudio), overrideAddress(std::move(bindOverride)),
          overrideDevice(std::move(selectedDevice)),
          presetDirectory(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile(".local").getChildFile("share")
                          .getChildFile("simple-dsp").getChildFile("Presets"))
    {
        for (auto& a : inputRta) a.fill(-96.0f);
        for (auto& a : outputRta) a.fill(-96.0f);
    }

    bool loadConfig(bool createIfAbsent)
    {
        if (!configFile.existsAsFile())
        {
            if (!createIfAbsent) { log("Missing config: " + configFile.getFullPathName()); return false; }
            configFile.getParentDirectory().createDirectory();
            if (!configFile.replaceWithText(juce::JSON::toString(defaultConfig())))
                { log("Could not create config"); return false; }
        }
        auto parsed = juce::JSON::parse(configFile.loadFileAsString());
        if (!parsed.isObject())
        {
            log("Invalid JSON configuration: " + configFile.getFullPathName()); return false;
        }
        config = parsed;
        auto pin = config.getProperty("remote_pin", "").toString();
        if (pin.length() != 6 || !pin.containsOnly("0123456789"))
        {
            pin = juce::String(100000 + juce::Random::getSystemRandom().nextInt(900000));
            config.getDynamicObject()->setProperty("remote_pin", pin);
            if (!writeConfig()) { log("Cannot persist remote PIN"); return false; }
        }
        return true;
    }

    void listDevices()
    {
        juce::AudioDeviceManager manager;
        const auto err = manager.initialise(0, 0, nullptr, false);
        if (err.isNotEmpty()) log("Audio discovery: " + err);
        for (auto* t : manager.getAvailableDeviceTypes())
        {
            t->scanForDevices();
            log("Backend: " + t->getTypeName());
            log("  Inputs: " + t->getDeviceNames(true).joinIntoString(" | "));
            log("  Outputs: " + t->getDeviceNames(false).joinIntoString(" | "));
        }
    }

    bool run()
    {
        if (!loadConfig(true)) return false;
        presetDirectory.createDirectory();
        auto initError = deviceManager.initialise(0, 0, nullptr, false);
        if (initError.isNotEmpty()) log("Audio manager: " + initError);
        deviceManager.addAudioCallback(&engine);
        if (static_cast<bool>(config.getProperty("load_at_launch",false)))
        {
            const auto n = safePresetName(config.getProperty("startup_preset","").toString());
            if (n.isNotEmpty())
            {
                const auto f = presetDirectory.getChildFile(n + ".dsp.json");
                if (f.existsAsFile() && f.getSize() <= 100000)
                {
                    auto v = juce::JSON::parse(f.loadFileAsString());
                    if (applySnapshot(v)) log("Loaded startup preset: " + n);
                    else log("Startup preset invalid: " + n);
                }
                else log("Startup preset missing: " + n);
            }
        }
        if (static_cast<bool>(config.getProperty("start_muted",true)))
        {
            for (auto& s : engine.settings) s.outputMuted.store(true);
            log("Safety: outputs start MUTED (unmute from phone after checking levels)");
        }
        if (!noAudio) tryOpenAudio();
        else log("Audio disabled for testing (--no-audio)");
        live = true;
        monitoring = std::thread([this]{ monitor(); });
        tryStartRemote();

        // Remain operational without a screen, even if interface/Wi-Fi
        // starts after the service. Periodically retry device/network discovery.
        int ticks = 0;
        while (!exitRequested)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            if (++ticks < 20) continue;
            ticks = 0;
            if (!noAudio && deviceManager.getCurrentAudioDevice() == nullptr) tryOpenAudio();
            if (!remoteActive) tryStartRemote();
            else engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
        }
        live = false;
        remote.stop();
        if (monitoring.joinable()) monitoring.join();
        deviceManager.removeAudioCallback(&engine);
        deviceManager.closeAudioDevice();
        log("Stopped cleanly");
        return true;
    }

private:
    juce::File configFile, presetDirectory;
    bool noAudio = false;
    juce::String overrideAddress, overrideDevice;
    juce::var config;
    std::mutex configMutex, stateMutex, liveMutex;
    juce::AudioDeviceManager deviceManager;
    PiEngine engine;
    RemoteServer remote;
    bool remoteActive = false;
    std::atomic<bool> live { false };
    std::thread monitoring;
    std::array<juce::String,2> names {"Channel 1", "Channel 2"};
    juce::String activeDevice = "No audio interface", lastInputName, lastOutputName;
    juce::StringArray availableInputs, availableOutputs;
    std::array<dualdsp::SpectrumAnalyzer,2> fftInput, fftOutput;
    std::array<dualdsp::LiveLevels,2> liveLevels {};
    std::array<std::array<float,dualdsp::kRtaBins>,2> inputRta, outputRta;

    bool writeConfig()
    {
        if (!configFile.replaceWithText(juce::JSON::toString(config, true))) return false;
        return juce::JSON::parse(configFile.loadFileAsString()).isObject();
    }

    bool selectStartup(const juce::File& f)
    {
        if (!f.isAChildOf(presetDirectory) || !f.existsAsFile()) return false;
        std::lock_guard<std::mutex> lock(configMutex);
        config.getDynamicObject()->setProperty("startup_preset", f.getFileNameWithoutExtension().dropLastCharacters(4));
        config.getDynamicObject()->setProperty("load_at_launch", true);
        if (!writeConfig()) return false;
        log("Load at launch: " + f.getFileName());
        return true;
    }

    static juce::var cutoffJson(const dualdsp::CutoffSettings& c)
    {
        auto x = std::make_unique<juce::DynamicObject>();
        x->setProperty("enabled", c.enabled.load());
        x->setProperty("frequency", c.frequencyHz.load());
        x->setProperty("slope", c.slopeDbPerOct.load());
        return juce::var(x.release());
    }

    juce::String snapshot()
    {
        std::array<juce::String,2> currentNames;
        juce::String currentDevice;
        juce::StringArray ins, outs;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            currentNames = names; currentDevice = activeDevice;
            ins = availableInputs; outs = availableOutputs;
        }
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("version",2);
        root->setProperty("device",currentDevice);
        root->setProperty("inputs",juce::var(juce::Array<juce::var>{}));
        root->setProperty("outputs",juce::var(juce::Array<juce::var>{}));
        juce::Array<juce::var> inputs, outputs, channels;
        for (auto str : ins) inputs.add(str);
        for (auto str : outs) outputs.add(str);
        root->setProperty("inputs",juce::var(inputs));
        root->setProperty("outputs",juce::var(outputs));
        {
            std::lock_guard<std::mutex> lock(configMutex);
            const auto startup = config.getProperty("startup_preset","").toString();
            root->setProperty("startupPresetName", startup);
            root->setProperty("loadAtLaunch", static_cast<bool>(config.getProperty("load_at_launch",false)));
            root->setProperty("remotePinRequired", static_cast<bool>(config.getProperty("remote_pin_required",true)));
        }
        const auto sampleRate = engine.sampleRate.load();
        for (int ch=0;ch<2;++ch)
        {
            const auto i=static_cast<std::size_t>(ch);
            const auto& p=engine.settings[i];
            auto c=std::make_unique<juce::DynamicObject>();
            c->setProperty("name",currentNames[i]);
            c->setProperty("inputRoute",p.inputRoute.load());
            c->setProperty("outputRoute",p.outputRoute.load());
            c->setProperty("levelDb",p.levelDb.load());
            c->setProperty("delayMs",p.delayMs.load());
            c->setProperty("invertPolarity",p.invertPolarity.load());
            c->setProperty("outputMuted",p.outputMuted.load());
            juce::Array<juce::var> eq, curve;
            for (const auto& b:p.eq)
            {
                auto e=std::make_unique<juce::DynamicObject>();
                e->setProperty("frequencyHz",b.frequencyHz.load());
                e->setProperty("gainDb",b.gainDb.load());
                e->setProperty("q",b.q.load());
                eq.add(juce::var(e.release()));
            }
            c->setProperty("eq",juce::var(eq));
            c->setProperty("highPass",cutoffJson(p.highPass));
            c->setProperty("lowPass",cutoffJson(p.lowPass));
            for (int k=0;k<128;++k)
                curve.add(dualdsp::responseDb(p,sampleRate,20.0*std::pow(1000.0,k/127.0)));
            c->setProperty("curve",juce::var(curve));
            channels.add(juce::var(c.release()));
        }
        root->setProperty("channels",juce::var(channels));
        return juce::JSON::toString(juce::var(root.release()));
    }

    juce::String telemetryJson()
    {
        std::lock_guard<std::mutex> lock(liveMutex);
        juce::Array<juce::var> channels;
        for (int ch=0;ch<2;++ch)
        {
            const auto i=static_cast<std::size_t>(ch);
            const auto& v=liveLevels[i];
            auto o=std::make_unique<juce::DynamicObject>();
            o->setProperty("inputPeak",v.inputPeak);
            o->setProperty("inputRms",v.inputRms);
            o->setProperty("outputPeak",v.outputPeak);
            o->setProperty("outputRms",v.outputRms);
            o->setProperty("inputClip",v.inputClip);
            o->setProperty("outputClip",v.outputClip);
            juce::Array<juce::var> in, out;
            for (auto x:inputRta[i]) in.add(x);
            for (auto x:outputRta[i]) out.add(x);
            o->setProperty("inputRta",juce::var(in));
            o->setProperty("rta",juce::var(out));
            channels.add(juce::var(o.release()));
        }
        auto root=std::make_unique<juce::DynamicObject>();
        root->setProperty("channels",juce::var(channels));
        return juce::JSON::toString(juce::var(root.release()));
    }

    void monitor()
    {
        int tick=0;
        while(live.load())
        {
            const auto sr=engine.sampleRate.load();
            std::array<dualdsp::LiveLevels,2> levels;
            for(int ch=0;ch<2;++ch)
            {
                const auto i=static_cast<std::size_t>(ch);
                levels[i]=engine.telemetry[i].takeLevels();
                if ((tick%2)==0)
                {
                    fftInput[i].update(engine.telemetry[i],sr,true);
                    fftOutput[i].update(engine.telemetry[i],sr,false);
                }
            }
            {
                std::lock_guard<std::mutex> lock(liveMutex);
                liveLevels=levels;
                if ((tick%2)==0)
                    for(int ch=0;ch<2;++ch)
                    {
                        const auto i=static_cast<std::size_t>(ch);
                        inputRta[i]=fftInput[i].getBins();
                        outputRta[i]=fftOutput[i].getBins();
                    }
            }
            ++tick;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool applySnapshot(const juce::var& v)
    {
        const auto chans=v.getProperty("channels",{});
        if (!chans.isArray() || chans.getArray()->size()!=2) return false;
        std::array<juce::String,2> newNames;
        std::array<int,2> in {},out {};
        juce::String actualDevice;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            actualDevice=activeDevice;
            for(int i=0;i<2;++i)
            {
                newNames[static_cast<std::size_t>(i)]=names[static_cast<std::size_t>(i)];
                in[static_cast<std::size_t>(i)]=engine.settings[static_cast<std::size_t>(i)].inputRoute.load();
                out[static_cast<std::size_t>(i)]=engine.settings[static_cast<std::size_t>(i)].outputRoute.load();
            }
        }
        const bool routeMatches = v.getProperty("device","").toString() == actualDevice;
        for(int ch=0;ch<2;++ch)
        {
            const auto c=chans[ch];
            if(!c.isObject()) return false;
            const auto i=static_cast<std::size_t>(ch);
            auto& p=engine.settings[i];
            newNames[i]=c.getProperty("name",newNames[i]).toString().substring(0,40);
            p.levelDb.store(std::clamp(static_cast<double>(c.getProperty("levelDb",0.0)),-90.0,18.0));
            p.delayMs.store(std::clamp(static_cast<double>(c.getProperty("delayMs",0.0)),0.0,dualdsp::kMaxDelayMs));
            p.invertPolarity.store(static_cast<bool>(c.getProperty("invertPolarity",false)));
            if(c.hasProperty("outputMuted"))
                p.outputMuted.store(static_cast<bool>(c.getProperty("outputMuted",false)));
            const auto eq=c.getProperty("eq",{});
            if(eq.isArray())
                for(int b=0;b<dualdsp::kBands && b<eq.getArray()->size();++b)
                {
                    const auto e=eq[b];
                    auto& dest=p.eq[static_cast<std::size_t>(b)];
                    dest.frequencyHz.store(std::clamp(static_cast<double>(e.getProperty("frequencyHz",1000.0)),20.0,20000.0));
                    dest.gainDb.store(std::clamp(static_cast<double>(e.getProperty("gainDb",0.0)),-18.0,18.0));
                    dest.q.store(std::clamp(static_cast<double>(e.getProperty("q",1.0)),0.1,15.0));
                }
            const auto filter=[&](dualdsp::CutoffSettings& f,const juce::var& x)
            {
                if(!x.isObject())return;
                f.enabled.store(static_cast<bool>(x.getProperty("enabled",false)));
                f.frequencyHz.store(std::clamp(static_cast<double>(x.getProperty("frequency",80.0)),20.0,20000.0));
                f.slopeDbPerOct.store(static_cast<int>(x.getProperty("slope",12))==24?24:12);
            };
            filter(p.highPass,c.getProperty("highPass",{}));
            filter(p.lowPass,c.getProperty("lowPass",{}));
            // A Windows preset carries Windows device labels; don't overwrite
            // safe Pi routing unless its device exactly matches this Pi.
            if(routeMatches)
            {
                in[i]=static_cast<int>(c.getProperty("inputRoute",in[i]));
                out[i]=static_cast<int>(c.getProperty("outputRoute",out[i]));
            }
        }
        if(out[0]>=0 && out[0]==out[1])out[1]=-1;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            names=newNames;
        }
        for(int i=0;i<2;++i)
        {
            engine.settings[static_cast<std::size_t>(i)].inputRoute.store(in[static_cast<std::size_t>(i)]);
            engine.settings[static_cast<std::size_t>(i)].outputRoute.store(out[static_cast<std::size_t>(i)]);
        }
        engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
        return true;
    }

    void applyCommand(const juce::var& command)
    {
        if(command.hasProperty("channels")) { applySnapshot(command); return; }
        const int ch=static_cast<int>(command.getProperty("channel",-1));
        if(ch<0 || ch>1) return;
        const auto i=static_cast<std::size_t>(ch);
        auto& p=engine.settings[i];
        const auto key=command.getProperty("key","").toString();
        const auto value=command.getProperty("value",{});
        const double n=static_cast<double>(value);
        if(key=="name")
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            names[i]=value.toString().substring(0,40);
        }
        else if(key=="outputMuted") p.outputMuted.store(static_cast<bool>(value));
        else if(key=="clearInputClip") engine.telemetry[i].clearClip(true);
        else if(key=="clearOutputClip") engine.telemetry[i].clearClip(false);
        else if(key=="levelDb") p.levelDb.store(std::clamp(n,-90.0,18.0));
        else if(key=="delayMs") p.delayMs.store(std::clamp(n,0.0,dualdsp::kMaxDelayMs));
        else if(key=="invertPolarity") p.invertPolarity.store(static_cast<bool>(value));
        else if(key=="inputRoute" || key=="outputRoute")
        {
            const int route=static_cast<int>(value);
            const int count=key=="inputRoute" ? availableInputs.size() : availableOutputs.size();
            if(route < -1 || route>=count)return;
            if(key=="outputRoute")
            {
                const auto other=engine.settings[static_cast<std::size_t>(1-ch)].outputRoute.load();
                if(route>=0 && route==other)return; // no summing
                p.outputRoute.store(route);
            }
            else p.inputRoute.store(route);
            engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
        }
        else if(key.startsWith("highPass.") || key.startsWith("lowPass."))
        {
            auto& f=key.startsWith("highPass.")?p.highPass:p.lowPass;
            if(key.endsWith(".enabled"))f.enabled.store(static_cast<bool>(value));
            else if(key.endsWith(".frequency"))f.frequencyHz.store(std::clamp(n,20.0,20000.0));
            else if(key.endsWith(".slope"))f.slopeDbPerOct.store(n==24.0?24:12);
        }
        else
        {
            const int band=static_cast<int>(command.getProperty("band",-1));
            if(band<0 || band>=dualdsp::kBands)return;
            auto& b=p.eq[static_cast<std::size_t>(band)];
            if(key=="frequencyHz")b.frequencyHz.store(std::clamp(n,20.0,20000.0));
            else if(key=="gainDb")b.gainDb.store(std::clamp(n,-18.0,18.0));
            else if(key=="q")b.q.store(std::clamp(n,0.1,15.0));
        }
    }

    void tryStartRemote()
    {
        const auto addr=overrideAddress.isNotEmpty()?overrideAddress:
                        config.getProperty("bind_address","").toString().isNotEmpty()?
                        config.getProperty("bind_address","").toString():RemoteServer::getPrivateIPv4();
        if(!privateAddress(addr))
        {
            log("Remote waiting for a private LAN address"); return;
        }
        const int port=std::clamp(static_cast<int>(config.getProperty("http_port",8765)),1024,65535);
        remoteActive=remote.start(addr,port,
            config.getProperty("remote_pin","").toString(),
            static_cast<bool>(config.getProperty("remote_pin_required",true)),
            presetDirectory,
            [this]{return snapshot();},
            [this]{return telemetryJson();},
            [this](const juce::var& v){applyCommand(v);},
            [this](const juce::File& f){return selectStartup(f);});
        if(remoteActive)
        {
            log("Phone controller: http://" + addr + ":" + juce::String(port));
            if(static_cast<bool>(config.getProperty("remote_pin_required",true)))
                log("Remote PIN: " + config.getProperty("remote_pin","").toString());
        }
        else log("Could not bind remote TCP port " + juce::String(port));
    }

    void tryOpenAudio()
    {
        const auto& types=deviceManager.getAvailableDeviceTypes();
        juce::AudioIODeviceType* alsa=nullptr;
        for(auto* t:types)
            if(t->getTypeName().containsIgnoreCase("ALSA")) {alsa=t;break;}
        if(!alsa) {log("ALSA backend unavailable");return;}
        alsa->scanForDevices();
        const auto inputs=alsa->getDeviceNames(true);
        const auto outputs=alsa->getDeviceNames(false);
        juce::String want=overrideDevice.isNotEmpty()?overrideDevice:
                          config.getProperty("audio_device","").toString();
        juce::String selectedIn,selectedOut;
        for(auto out:outputs)
        {
            const bool choice=want.isNotEmpty()?out.containsIgnoreCase(want):
                           (out.containsIgnoreCase("Scarlett")||out.containsIgnoreCase("Focusrite"));
            if(!choice)continue;
            for(auto in:inputs)
                if(in==out || (want.isNotEmpty()?in.containsIgnoreCase(want):
                            (in.containsIgnoreCase("Scarlett")||in.containsIgnoreCase("Focusrite"))))
                {selectedIn=in;selectedOut=out;break;}
            if(selectedIn.isNotEmpty())break;
        }
        if(selectedOut.isEmpty())
        {
            log("Scarlett not found in ALSA. Try --list-devices and set audio_device in config.json.");
            return; // Never fall back silently to a random output interface.
        }

        deviceManager.setCurrentAudioDeviceType(alsa->getTypeName(),true);
        auto setup=deviceManager.getAudioDeviceSetup();
        setup.inputDeviceName=selectedIn;
        setup.outputDeviceName=selectedOut;
        setup.sampleRate=std::clamp(static_cast<double>(config.getProperty("sample_rate",48000)),8000.0,96000.0);
        setup.bufferSize=std::clamp(static_cast<int>(config.getProperty("buffer_size",256)),64,2048);
        setup.useDefaultInputChannels=false;
        setup.useDefaultOutputChannels=false;
        setup.inputChannels.clear();
        setup.outputChannels.clear();
        setup.inputChannels.setBit(0);setup.inputChannels.setBit(1);
        setup.outputChannels.setBit(0);setup.outputChannels.setBit(1);
        auto err=deviceManager.setAudioDeviceSetup(setup,true);
        if(err.isNotEmpty() || !deviceManager.getCurrentAudioDevice())
        {
            log("Could not open Scarlett: " + err);
            return;
        }
        auto* d=deviceManager.getCurrentAudioDevice();
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            activeDevice=d->getName();
            availableInputs=d->getInputChannelNames();
            availableOutputs=d->getOutputChannelNames();
        }
        engine.refreshRoutes(d);
        log("ALSA " + d->getName() + ", " + juce::String(d->getCurrentSampleRate())+
            " Hz, "+juce::String(d->getCurrentBufferSizeSamples())+" samples");
    }
};

int main(int argc, char** argv)
{
    juce::MessageManager::getInstance(); // JUCE events only; no GUI or X server.
    juce::File file(configLocation());
    bool list=false,check=false,noAudio=false;
    juce::String bind,device;
    for(int i=1;i<argc;++i)
    {
        const juce::String arg(argv[i]);
        if(arg=="--help" || arg=="-h")
        {
            std::cout << "Simple DSP Pi (Linux ARM64)\n"
                      << "  --config PATH    Config JSON (default ~/.config/simple-dsp/config.json)\n"
                      << "  --list-devices   List ALSA input/output device names\n"
                      << "  --check-config   Validate config JSON without opening audio\n"
                      << "  --device NAME    Override audio_device substring\n"
                      << "  --bind IP        Bind only RFC1918 LAN address or 127.0.0.1\n"
                      << "  --no-audio       Serve remote without opening an interface (test mode)\n";
            return 0;
        }
        if(arg=="--config" && i+1<argc) file=juce::File(argv[++i]);
        else if(arg=="--list-devices")list=true;
        else if(arg=="--check-config")check=true;
        else if(arg=="--no-audio")noAudio=true;
        else if(arg=="--device" && i+1<argc)device=juce::String(argv[++i]);
        else if(arg=="--bind" && i+1<argc)bind=juce::String(argv[++i]);
        else {std::cerr << "Unknown argument: " << arg.toStdString() << "\n";return 2;}
    }
    if(bind.isNotEmpty() && !privateAddress(bind))
    {
        std::cerr << "Refusing to bind non-private address\n";
        return 2;
    }
    auto host=std::make_unique<PiHost>(file,noAudio,bind,device);
    if(list){host->listDevices();return 0;}
    if(check)
    {
        if(!host->loadConfig(false))return 1;
        log("Config JSON valid");
        return 0;
    }
    std::signal(SIGINT,requestExit);
    std::signal(SIGTERM,requestExit);
    return host->run()?0:1;
}
