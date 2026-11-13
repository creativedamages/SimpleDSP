#include <JuceHeader.h>
#include "DSP.h"
#include <array>
#include <functional>
#include <memory>
#include <utility>

namespace
{
constexpr auto backgroundColour = juce::Colour(0xff131820);
constexpr auto panelColour = juce::Colour(0xff222b38);
constexpr auto mutedColour = juce::Colour(0xffa7b4c6);
constexpr auto accentColour = juce::Colour(0xff65cad8);

void setupSlider(juce::Slider& slider, double low, double high, double step,
                 double initial, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 24);
    slider.setRange(low, high, step);
    slider.setValue(initial, juce::dontSendNotification);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::thumbColourId, accentColour);
    slider.setColour(juce::Slider::trackColourId, accentColour);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff435066));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    std::array<dualdsp::ChannelSettings, 2> parameters;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
        for (auto& processor : processors) processor.prepare(sampleRate);

        // JUCE's callback arrays are packed in active-channel order, even when
        // a user selects physical channels such as input 5 and output 8.
        // Translate the selected *physical* routes to callback indices.
        const auto activeInputs = device != nullptr ? device->getActiveInputChannels() : juce::BigInteger{};
        const auto activeOutputs = device != nullptr ? device->getActiveOutputChannels() : juce::BigInteger{};
        for (int path = 0; path < 2; ++path)
        {
            const auto slot = static_cast<std::size_t>(path);
            callbackInputs[slot].store(callbackIndex(activeInputs, parameters[slot].inputRoute.load()),
                                       std::memory_order_release);
            callbackOutputs[slot].store(callbackIndex(activeOutputs, parameters[slot].outputRoute.load()),
                                        std::memory_order_release);
        }
    }

    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext(const float* const* inputData,
                                          int inputCount,
                                          float* const* outputData,
                                          int outputCount,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ScopedNoDenormals avoidDenormals;

        // Silence every open physical output first. This callback is the only writer.
        // Nothing from the two paths is ever summed or mixed.
        for (int output = 0; output < outputCount; ++output)
            if (outputData[output] != nullptr)
                juce::FloatVectorOperations::clear(outputData[output], numSamples);

        for (int path = 0; path < 2; ++path)
        {
            const auto& params = parameters[static_cast<std::size_t>(path)];
            const int inputRoute = callbackInputs[static_cast<std::size_t>(path)].load(std::memory_order_acquire);
            const int outputRoute = callbackOutputs[static_cast<std::size_t>(path)].load(std::memory_order_acquire);

            // Packed active callback channels, not the user-visible physical IDs.
            if (inputRoute < 0 || inputRoute >= inputCount || outputRoute < 0
                || outputRoute >= outputCount || inputData[inputRoute] == nullptr
                || outputData[outputRoute] == nullptr)
                continue;

            auto& processor = processors[static_cast<std::size_t>(path)];
            processor.beginBlock(params);
            for (int sample = 0; sample < numSamples; ++sample)
                outputData[outputRoute][sample] = processor.process(inputData[inputRoute][sample]);
        }
    }

private:
    static int callbackIndex(const juce::BigInteger& activeChannels, int physical) noexcept
    {
        if (physical < 0 || !activeChannels[physical]) return -1;
        int index = 0;
        for (int ch = 0; ch < physical; ++ch)
            if (activeChannels[ch]) ++index;
        return index;
    }

    std::array<dualdsp::MonoProcessor, 2> processors;
    std::array<std::atomic<int>, 2> callbackInputs { -1, -1 }, callbackOutputs { -1, -1 };
};

class EqRow final : public juce::Component
{
public:
    EqRow(int bandNumber, dualdsp::EqSettings& settings) : params(settings)
    {
        number.setText("EQ " + juce::String(bandNumber + 1), juce::dontSendNotification);
        number.setColour(juce::Label::textColourId, mutedColour);
        number.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(number);

        setupSlider(frequency, 20.0, 20000.0, 1.0, params.frequencyHz.load(), " Hz");
        frequency.setSkewFactorFromMidPoint(1100.0);
        setupSlider(gain, -18.0, 18.0, 0.1, params.gainDb.load(), " dB");
        setupSlider(q, 0.1, 15.0, 0.05, params.q.load(), " Q");
        addAndMakeVisible(frequency);
        addAndMakeVisible(gain);
        addAndMakeVisible(q);
        frequency.onValueChange = [this] { params.frequencyHz.store(frequency.getValue()); };
        gain.onValueChange = [this] { params.gainDb.store(gain.getValue()); };
        q.onValueChange = [this] { params.q.store(q.getValue()); };
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff2c3644));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 7.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8, 6);
        const int total = area.getWidth();
        number.setBounds(area.removeFromLeft(49));
        const int content = total - 49;
        frequency.setBounds(area.removeFromLeft(static_cast<int>(content * 0.40)));
        gain.setBounds(area.removeFromLeft(static_cast<int>(content * 0.31)));
        q.setBounds(area);
    }

private:
    dualdsp::EqSettings& params;
    juce::Label number;
    juce::Slider frequency, gain, q;
};

class ChannelStrip final : public juce::Component
{
public:
    ChannelStrip(int number, dualdsp::ChannelSettings& channelParams)
        : channelNumber(number), params(channelParams)
    {
        heading.setText("CHANNEL " + juce::String(number), juce::dontSendNotification);
        heading.setFont(juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));
        heading.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(heading);

        inputCaption.setText("INPUT ROUTE", juce::dontSendNotification);
        outputCaption.setText("OUTPUT ROUTE", juce::dontSendNotification);
        for (auto* label : { &inputCaption, &outputCaption })
        {
            label->setColour(juce::Label::textColourId, mutedColour);
            addAndMakeVisible(*label);
        }
        addAndMakeVisible(input);
        addAndMakeVisible(output);
        input.onChange = [this] { if (onRouteChange) onRouteChange(channelNumber - 1, false); };
        output.onChange = [this] { if (onRouteChange) onRouteChange(channelNumber - 1, true); };

        levelCaption.setText("LEVEL", juce::dontSendNotification);
        delayCaption.setText("DELAY", juce::dontSendNotification);
        for (auto* label : { &levelCaption, &delayCaption })
        {
            label->setColour(juce::Label::textColourId, mutedColour);
            addAndMakeVisible(*label);
        }
        setupSlider(level, -90.0, 18.0, 0.1, params.levelDb.load(), " dB");
        setupSlider(delay, 0.0, dualdsp::kMaxDelayMs, 0.01, params.delayMs.load(), " ms");
        addAndMakeVisible(level);
        addAndMakeVisible(delay);
        level.onValueChange = [this] { params.levelDb.store(level.getValue()); };
        delay.onValueChange = [this] { params.delayMs.store(delay.getValue()); };

        polarity.setButtonText("Invert polarity (180\u00b0)");
        polarity.setToggleState(params.invertPolarity.load(), juce::dontSendNotification);
        polarity.setColour(juce::ToggleButton::tickColourId, accentColour);
        polarity.onClick = [this] { params.invertPolarity.store(polarity.getToggleState()); };
        addAndMakeVisible(polarity);

        eqCaption.setText("PARAMETRIC EQ  /  6 BANDS", juce::dontSendNotification);
        eqCaption.setColour(juce::Label::textColourId, accentColour);
        addAndMakeVisible(eqCaption);
        headingLabels.setText("FREQUENCY                         GAIN                         Q", juce::dontSendNotification);
        headingLabels.setColour(juce::Label::textColourId, mutedColour);
        addAndMakeVisible(headingLabels);

        for (int i = 0; i < dualdsp::kBands; ++i)
        {
            eqRows[static_cast<std::size_t>(i)] = std::make_unique<EqRow>(i, params.eq[static_cast<std::size_t>(i)]);
            addAndMakeVisible(*eqRows[static_cast<std::size_t>(i)]);
        }
    }

    std::function<void(int, bool)> onRouteChange;

    void setRouteOptions(const juce::StringArray& availableInputs,
                         const juce::StringArray& availableOutputs,
                         int selectedInput, int selectedOutput)
    {
        input.clear(juce::dontSendNotification);
        output.clear(juce::dontSendNotification);
        input.addItem("Unassigned", 1);
        output.addItem("Unassigned", 1);
        for (int i = 0; i < availableInputs.size(); ++i)
            input.addItem(juce::String(i + 1) + "  \u2022  " + availableInputs[i], i + 2);
        for (int i = 0; i < availableOutputs.size(); ++i)
            output.addItem(juce::String(i + 1) + "  \u2022  " + availableOutputs[i], i + 2);
        setInputRoute(selectedInput);
        setOutputRoute(selectedOutput);
    }

    int getInputRoute() const noexcept { return input.getSelectedId() - 2; }
    int getOutputRoute() const noexcept { return output.getSelectedId() - 2; }
    void setInputRoute(int route) { input.setSelectedId(route + 2, juce::dontSendNotification); }
    void setOutputRoute(int route) { output.setSelectedId(route + 2, juce::dontSendNotification); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(panelColour);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 13.0f);
        g.setColour(accentColour.withAlpha(0.65f));
        g.fillRoundedRectangle(20.0f, 53.0f, static_cast<float>(getWidth() - 40), 2.0f, 1.0f);
    }

    void resized() override
    {
        const int w = getWidth();
        heading.setBounds(22, 13, w - 44, 36);
        const int half = (w - 56) / 2;
        inputCaption.setBounds(22, 72, half, 19);
        outputCaption.setBounds(34 + half, 72, half, 19);
        input.setBounds(22, 98, half, 31);
        output.setBounds(34 + half, 98, half, 31);
        levelCaption.setBounds(22, 150, 110, 22);
        delayCaption.setBounds(22, 203, 110, 22);
        level.setBounds(131, 145, w - 153, 36);
        delay.setBounds(131, 198, w - 153, 36);
        polarity.setBounds(24, 256, w - 48, 30);
        eqCaption.setBounds(22, 312, w - 44, 28);
        headingLabels.setBounds(70, 344, w - 85, 22);
        for (int i = 0; i < dualdsp::kBands; ++i)
            eqRows[static_cast<std::size_t>(i)]->setBounds(18, 374 + i * 65, w - 36, 59);
    }

private:
    int channelNumber;
    dualdsp::ChannelSettings& params;
    juce::Label heading, inputCaption, outputCaption, levelCaption, delayCaption, eqCaption, headingLabels;
    juce::ComboBox input, output;
    juce::Slider level, delay;
    juce::ToggleButton polarity;
    std::array<std::unique_ptr<EqRow>, dualdsp::kBands> eqRows;
};

class StripArea final : public juce::Component
{
public:
    StripArea(dualdsp::ChannelSettings& a, dualdsp::ChannelSettings& b)
        : left(1, a), right(2, b)
    {
        addAndMakeVisible(left);
        addAndMakeVisible(right);
        setSize(1150, 790);
    }

    void resized() override
    {
        left.setBounds(0, 0, 565, 780);
        right.setBounds(585, 0, 565, 780);
    }

    ChannelStrip left, right;
};

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent()
        : selector(deviceManager, 0, 64, 0, 64, false, false, false, true),
          strips(engine.parameters[0], engine.parameters[1])
    {
        title.setText("Dual Channel DSP", juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(title);

        subtitle.setText("Two independent mono signal paths  \u2022  no summing", juce::dontSendNotification);
        subtitle.setColour(juce::Label::textColourId, mutedColour);
        addAndMakeVisible(subtitle);

        deviceLabel.setText("AUDIO INTERFACE", juce::dontSendNotification);
        deviceLabel.setColour(juce::Label::textColourId, accentColour);
        addAndMakeVisible(deviceLabel);

        selector.setSize(295, 790);
        deviceViewport.setViewedComponent(&selector, false);
        deviceViewport.setScrollBarsShown(true, false);
        addAndMakeVisible(deviceViewport);
        contentViewport.setViewedComponent(&strips, false);
        contentViewport.setScrollBarsShown(true, true);
        addAndMakeVisible(contentViewport);

        info.setColour(juce::Label::textColourId, mutedColour);
        info.setText("Choose an audio driver and interface. ASIO is recommended for low latency.", juce::dontSendNotification);
        addAndMakeVisible(info);

        strips.left.onRouteChange = [this](int changed, bool outputChanged)
        { routeChanged(changed, outputChanged); };
        strips.right.onRouteChange = [this](int changed, bool outputChanged)
        { routeChanged(changed, outputChanged); };

        const auto error = deviceManager.initialise(2, 2, nullptr, true);
        if (error.isNotEmpty())
            info.setText("Audio device initialization: " + error, juce::dontSendNotification);

        updateDeviceRouting(true);
        deviceManager.addAudioCallback(&engine);
        startTimer(700);
        setSize(1560, 905);
    }

    ~MainComponent() override
    {
        stopTimer();
        deviceManager.removeAudioCallback(&engine);
        deviceViewport.setViewedComponent(nullptr, false);
        contentViewport.setViewedComponent(nullptr, false);
    }

    void paint(juce::Graphics& g) override { g.fillAll(backgroundColour); }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(22);
        auto header = bounds.removeFromTop(68);
        title.setBounds(header.removeFromLeft(370));
        subtitle.setBounds(header);

        auto footer = bounds.removeFromBottom(37);
        info.setBounds(footer);
        auto sidebar = bounds.removeFromLeft(319);
        deviceLabel.setBounds(sidebar.removeFromTop(28));
        deviceViewport.setBounds(sidebar.reduced(2));
        bounds.removeFromLeft(16);
        contentViewport.setBounds(bounds);
    }

private:
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;
    juce::AudioDeviceSelectorComponent selector;
    juce::Viewport deviceViewport, contentViewport;
    StripArea strips;
    juce::Label title, subtitle, info, deviceLabel;
    juce::String lastDeviceIdentity;
    juce::StringArray lastInputs, lastOutputs;
    std::array<int, 2> lastValidInput { -1, -1 }, lastValidOutput { -1, -1 };
    bool suppressUpdates = false;

    void timerCallback() override
    {
        updateDeviceRouting(false);
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            info.setText(device->getTypeName() + " / " + device->getName()
                         + "  \u2022  " + juce::String(device->getCurrentSampleRate(), 0) + " Hz"
                         + "  \u2022  " + juce::String(device->getCurrentBufferSizeSamples()) + " samples"
                         + "  \u2022  no summing", juce::dontSendNotification);
        }
    }

    void updateDeviceRouting(bool force)
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr)
        {
            if (force || lastDeviceIdentity.isNotEmpty())
            {
                lastDeviceIdentity.clear();
                lastInputs.clear();
                lastOutputs.clear();
                strips.left.setRouteOptions({}, {}, -1, -1);
                strips.right.setRouteOptions({}, {}, -1, -1);
                publishRoutes();
            }
            return;
        }

        const auto in = device->getInputChannelNames();
        const auto out = device->getOutputChannelNames();
        const auto identity = device->getTypeName() + " :: " + device->getName();
        const bool changedInterface = force || identity != lastDeviceIdentity;
        if (changedInterface || in != lastInputs || out != lastOutputs)
        {
            suppressUpdates = true;
            lastDeviceIdentity = identity;
            lastInputs = in;
            lastOutputs = out;

            // Preserve user patching when the same device restarts. Only default
            // to 1->1 and 2->2 when a different interface is selected.
            const auto valid = [](int route, int count) { return route >= 0 && route < count ? route : -1; };
            const int input1 = changedInterface ? (in.size() > 0 ? 0 : -1) : valid(lastValidInput[0], in.size());
            const int input2 = changedInterface ? (in.size() > 1 ? 1 : -1) : valid(lastValidInput[1], in.size());
            const int output1 = changedInterface ? (out.size() > 0 ? 0 : -1) : valid(lastValidOutput[0], out.size());
            const int output2 = changedInterface ? (out.size() > 1 ? 1 : -1) : valid(lastValidOutput[1], out.size());
            strips.left.setRouteOptions(in, out, input1, output1);
            strips.right.setRouteOptions(in, out, input2, output2);
            suppressUpdates = false;
            publishRoutes();
            applyActivePorts();
        }
        else
        {
            // Re-enable requested ports if the driver selector changed channel masks.
            applyActivePorts();
        }
    }

    void routeChanged(int changedIndex, bool changedOutput)
    {
        if (suppressUpdates) return;
        auto newOutputs = std::array<int, 2> { strips.left.getOutputRoute(), strips.right.getOutputRoute() };
        if (newOutputs[0] >= 0 && newOutputs[0] == newOutputs[1])
        {
            auto& changedStrip = changedIndex == 0 ? strips.left : strips.right;
            changedStrip.setOutputRoute(lastValidOutput[static_cast<std::size_t>(changedIndex)]);
            info.setText("Output conflict: each channel requires a different physical output.",
                         juce::dontSendNotification);
            return;
        }
        juce::ignoreUnused(changedOutput);
        publishRoutes();
        applyActivePorts();
    }

    void publishRoutes()
    {
        lastValidInput = { strips.left.getInputRoute(), strips.right.getInputRoute() };
        lastValidOutput = { strips.left.getOutputRoute(), strips.right.getOutputRoute() };
        for (int i = 0; i < 2; ++i)
        {
            engine.parameters[static_cast<std::size_t>(i)].inputRoute.store(lastValidInput[static_cast<std::size_t>(i)]);
            engine.parameters[static_cast<std::size_t>(i)].outputRoute.store(lastValidOutput[static_cast<std::size_t>(i)]);
        }
    }

    void applyActivePorts()
    {
        if (deviceManager.getCurrentAudioDevice() == nullptr) return;
        juce::BigInteger inputs, outputs;
        for (int route : lastValidInput) if (route >= 0) inputs.setBit(route);
        for (int route : lastValidOutput) if (route >= 0) outputs.setBit(route);

        auto setup = deviceManager.getAudioDeviceSetup();
        if (!setup.useDefaultInputChannels && !setup.useDefaultOutputChannels
            && setup.inputChannels == inputs && setup.outputChannels == outputs)
            return;
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = false;
        setup.inputChannels = inputs;
        setup.outputChannels = outputs;
        const auto error = deviceManager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty()) info.setText("Audio routing error: " + error, juce::dontSendNotification);
    }
};

class DualChannelApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Dual Channel DSP"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
        lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, backgroundColour);
        lookAndFeel.setColour(juce::ComboBox::backgroundColourId, panelColour);
        lookAndFeel.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        lookAndFeel.setColour(juce::PopupMenu::backgroundColourId, panelColour);
        lookAndFeel.setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        lookAndFeel.setColour(juce::Label::textColourId, juce::Colours::white);
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        window = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        window.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    juce::LookAndFeel_V4 lookAndFeel;

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : juce::DocumentWindow("Dual Channel DSP", backgroundColour,
                                   juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(980, 650, 2400, 1400);
            centreWithSize(1560, 905);
            setVisible(true);
        }
        void closeButtonPressed() override
        {
            if (auto* app = juce::JUCEApplication::getInstance()) app->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> window;
};
} // namespace

START_JUCE_APPLICATION(DualChannelApp)
