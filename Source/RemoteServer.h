#pragma once
#include <JuceHeader.h>
#include "RemotePage.h"
#include "ProjectSettings.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <string>
#include <future>
#include <chrono>

// One small built-in HTTP listener for a trusted private Wi-Fi network.
// This never opens a tunnel or binds a public/Internet-facing address.
class RemoteServer final
{
public:
    using Snapshot = std::function<juce::String()>;
    using Telemetry = std::function<juce::String(int)>;
    using Change = std::function<void(const juce::var&)>;
    // Called by the server worker, returns true only AFTER the GUI persisted
    // and acknowledged the selected startup preset.
    using SelectStartup = std::function<bool(const juce::File&)>;

    bool start(const juce::String& address, int port, const juce::String& sixDigitPin,
               bool pinRequired, juce::File presetDirectory, Snapshot getState, Snapshot getPreset,
               Telemetry getTelemetry, Change applyState, SelectStartup selectStartupPreset)
    {
        stop();
        state = std::move(getState);
        preset = std::move(getPreset);
        telemetry = std::move(getTelemetry);
        change = std::move(applyState);
        selectStartup = std::move(selectStartupPreset);
        requiresPin.store(pinRequired);
        presets = std::move(presetDirectory);
        presets.createDirectory();
        pin = sixDigitPin;
        listener = std::make_unique<juce::StreamingSocket>();
        if (! listener->createListener(port, address))
        {
            listener.reset();
            return false;
        }
        active = true;
        worker = std::thread([this]{ serve(); });
        return true;
    }

    void stop()
    {
        active = false;
        if (listener) listener->close();
        if (worker.joinable()) worker.join();
        listener.reset();
    }
    ~RemoteServer() { stop(); }
    void setPinRequired(bool enabled) noexcept { requiresPin.store(enabled); }

    static juce::String getPrivateIPv4()
    {
        juce::String fallback;
        for (auto ip : juce::IPAddress::getAllAddresses(false))
        {
            const auto s = ip.toString();
            const auto octets = juce::StringArray::fromTokens(s, ".", "");
            if (octets.size() != 4) continue;
            const int a = octets[0].getIntValue(), b = octets[1].getIntValue();
            if (a == 192 && b == 168) return s;  // Typical Wi-Fi/home LAN.
            if (a == 10 || (a == 172 && b >= 16 && b <= 31))
                if (fallback.isEmpty()) fallback = s;
        }
        return fallback;
    }

private:
    void serve()
    {
        while (active && listener)
        {
            if (listener->waitUntilReady(true, 200) <= 0) continue;
            std::unique_ptr<juce::StreamingSocket> client(listener->waitForNextConnection());
            if (client) handle(*client);
        }
    }

    static void send(juce::StreamingSocket& client, int status, const std::string& content, const char* mime)
    {
        const char* msg = status == 200 ? "OK" : status == 202 ? "Accepted" :
                          status == 400 ? "Bad Request" : status == 401 ? "Unauthorized" :
                          status == 404 ? "Not Found" : "Error";
        std::string header = "HTTP/1.1 " + std::to_string(status) + " " + msg
            + "\r\nContent-Type: " + mime
            + "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff"
            + "\r\nContent-Security-Policy: default-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self';"
            + "\r\nConnection: close\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        const auto data = header + content;
        std::size_t written = 0;
        while (written < data.size())
        {
            const int n = client.write(data.data() + written, static_cast<int>(data.size() - written));
            if (n <= 0) break;
            written += static_cast<std::size_t>(n);
        }
    }
    static std::string utf8(juce::String s) { return s.toStdString(); }
    static std::string jsonError(const juce::String& err)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("error", err);
        return utf8(juce::JSON::toString(juce::var(obj.release())));
    }
    static juce::String safeName(const juce::String& n)
    {
        return n.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-()").trim().substring(0, 60);
    }
    juce::File presetFile(const juce::String& name) const
    {
        const auto safe = safeName(name);
        return safe.isNotEmpty() ? presets.getChildFile(safe + ".dsp.json") : juce::File();
    }
    void handle(juce::StreamingSocket& client)
    {
        // Read only small HTTP/1.1 requests. Avoid unbounded reads or hangs.
        std::string request;
        int headerEnd = -1;
        int contentLength = 0;
        while (active && request.size() < 32000)
        {
            if (client.waitUntilReady(true, 1200) <= 0) return;
            char temp[4096];
            const int bytes = client.read(temp, static_cast<int>(sizeof(temp)), false);
            if (bytes <= 0) return;
            request.append(temp, static_cast<std::size_t>(bytes));
            if (headerEnd < 0)
            {
                const auto at = request.find("\r\n\r\n");
                if (at != std::string::npos)
                {
                    headerEnd = static_cast<int>(at + 4);
                    const auto headers = juce::String::fromUTF8(request.data(), headerEnd);
                    for (auto line : juce::StringArray::fromLines(headers))
                        if (line.trimStart().startsWithIgnoreCase("Content-Length:"))
                            contentLength = line.fromFirstOccurrenceOf(":", false, false).trim().getIntValue();
                    if (contentLength < 0 || contentLength > 16384) { send(client, 400, jsonError("Request too large"), "application/json"); return; }
                }
            }
            if (headerEnd >= 0 && static_cast<int>(request.size()) - headerEnd >= contentLength) break;
        }
        if (headerEnd < 0) return;
        const auto headers = juce::String::fromUTF8(request.data(), headerEnd);
        const auto firstLine = juce::StringArray::fromLines(headers)[0];
        const auto method = firstLine.upToFirstOccurrenceOf(" ", false, false);
        const auto path = firstLine.fromFirstOccurrenceOf(" ", false, false).upToFirstOccurrenceOf(" ", false, false);
        if (method == "GET" && path == "/")
        {
            send(client, 200, kRemotePage, "text/html; charset=utf-8"); return;
        }
        if (method == "GET" && path == "/manifest.webmanifest")
        {
            send(client, 200,
                R"JSON({"name":"SimpleDSP","short_name":"SimpleDSP","start_url":"/","scope":"/","display":"standalone","background_color":"#111821","theme_color":"#111821"})JSON",
                "application/manifest+json"); return;
        }
        bool authorised = !requiresPin.load();
        if (!authorised)
            for (auto line : juce::StringArray::fromLines(headers))
                if (line.trimStart().startsWithIgnoreCase("X-DSP-PIN:"))
                    authorised = line.fromFirstOccurrenceOf(":", false, false).trim() == pin;
        if (!authorised) { send(client, 401, jsonError("PIN required"), "application/json"); return; }
        if (method == "GET" && path == "/api/state")
        {
            send(client, 200, utf8(state()), "application/json"); return;
        }
        if (method == "GET" && path.startsWith("/api/telemetry/"))
        {
            const auto suffix = path.substring(15);
            const int channel = suffix.getIntValue();
            if (suffix.isEmpty() || !suffix.containsOnly("0123456789") || suffix.length() > 2
                || channel < 0 || channel >= htdsp::channelCount)
                send(client, 400, jsonError("Invalid channel"), "application/json");
            else
                send(client, 200, utf8(telemetry(channel)), "application/json");
            return;
        }
        if (method == "GET" && path == "/api/presets")
        {
            auto o = std::make_unique<juce::DynamicObject>();
            juce::Array<juce::var> names;
            for (auto f : presets.findChildFiles(juce::File::findFiles, false, "*.dsp.json"))
                names.add(f.getFileName().dropLastCharacters(9)); // remove .dsp.json
            o->setProperty("names", juce::var(names));
            send(client, 200, utf8(juce::JSON::toString(juce::var(o.release()))), "application/json");
            return;
        }
        if (method != "POST" || (path != "/api/set" && path != "/api/save"
                                  && path != "/api/load" && path != "/api/startup"))
        { send(client, 404, jsonError("Not found"), "application/json"); return; }
        const auto payload = juce::JSON::parse(juce::String::fromUTF8(request.data() + headerEnd, contentLength));
        if (!payload.isObject()) { send(client, 400, jsonError("Invalid JSON"), "application/json"); return; }
        if (path == "/api/set")
        {
            change(payload);
            send(client, 202, "{\"ok\":true}", "application/json"); return;
        }
        const auto name = safeName(payload.getProperty("name", juce::var()).toString());
        if (name.isEmpty()) { send(client, 400, jsonError("Invalid preset name"), "application/json"); return; }
        const auto file = presetFile(name);
        if (path == "/api/save")
        {
            if (file.replaceWithText(preset())) send(client, 200, "{\"ok\":true}", "application/json");
            else send(client, 500, jsonError("Failed to save preset"), "application/json");
            return;
        }
        if (!file.existsAsFile()) { send(client, 404, jsonError("Preset not found"), "application/json"); return; }
        if (file.getSize() > htdsp::maxPresetBytes) { send(client, 400, jsonError("Preset too large"), "application/json"); return; }
        const auto loaded = juce::JSON::parse(file.loadFileAsString());
        if (!htdsp::hasValidPreset(loaded))
        { send(client, 400, jsonError("Corrupt preset"), "application/json"); return; }
        if (path == "/api/startup")
        {
            if (!selectStartup || !selectStartup(file))
            { send(client, 500, jsonError("Windows app could not confirm startup preset"), "application/json"); return; }
            auto result = std::make_unique<juce::DynamicObject>();
            result->setProperty("ok", true);
            result->setProperty("name", name);
            result->setProperty("loadAtLaunch", true);
            send(client, 200, utf8(juce::JSON::toString(juce::var(result.release()))), "application/json");
            return;
        }
        change(loaded);
        send(client, 202, "{\"ok\":true}", "application/json");
    }
    std::unique_ptr<juce::StreamingSocket> listener;
    std::thread worker;
    std::atomic<bool> active { false };
    Snapshot state, preset;
    Telemetry telemetry;
    Change change;
    SelectStartup selectStartup;
    juce::File presets;
    juce::String pin;
    std::atomic<bool> requiresPin { true };
};
