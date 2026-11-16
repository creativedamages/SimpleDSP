#include <JuceHeader.h>
#include "DSP.h"
#include "Analysis.h"
#include "RemoteServer.h"
#include <array>
#include <functional>
#include <future>
#include <chrono>
#include <memory>
#include <utility>
#include <mutex>

namespace
{
const auto backgroundColour = juce::Colour(0xff131820);
const auto panelColour = juce::Colour(0xff222b38);
const auto mutedColour = juce::Colour(0xffa7b4c6);
const auto accentColour = juce::Colour(0xff65cad8);

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
    std::array<dualdsp::AudioTelemetry, 2> telemetry;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
        for (auto& processor : processors) processor.prepare(sampleRate);
        for (auto& meter : telemetry) meter.prepare(sampleRate);

        // JUCE's callback arrays are packed in active-channel order, even when
        // a user selects physical channels such as input 5 and output 8.
        // Translate selected physical routes to packed callback indices.
        refreshRoutes(device);
    }

    void refreshRoutes(juce::AudioIODevice* device)
    {
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

    void audioDeviceStopped() override
    {
        // Clear stale meters/spectrum when the audio interface disconnects.
        for (auto& meter : telemetry) meter.prepare(48000.0);
    }

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

            // Input metering must remain active even if the output route is unassigned.
            const float* source = inputRoute >= 0 && inputRoute < inputCount
                ? inputData[inputRoute] : nullptr;
            float* destination = outputRoute >= 0 && outputRoute < outputCount
                ? outputData[outputRoute] : nullptr;
            if (source == nullptr || destination == nullptr)
            {
                telemetry[static_cast<std::size_t>(path)].recordBlock(source, nullptr, numSamples);
                continue;
            }

            auto& processor = processors[static_cast<std::size_t>(path)];
            processor.beginBlock(params);
            for (int sample = 0; sample < numSamples; ++sample)
                destination[sample] = processor.process(source[sample]);
            telemetry[static_cast<std::size_t>(path)].recordBlock(source, destination, numSamples);
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

    void sync()
    {
        frequency.setValue(params.frequencyHz.load(), juce::dontSendNotification);
        gain.setValue(params.gainDb.load(), juce::dontSendNotification);
        q.setValue(params.q.load(), juce::dontSendNotification);
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

class CutoffRow final : public juce::Component
{
public:
    CutoffRow(const juce::String& title, dualdsp::CutoffSettings& settings) : params(settings)
    {
        enabled.setButtonText(title);
        enabled.setToggleState(params.enabled.load(), juce::dontSendNotification);
        enabled.setColour(juce::ToggleButton::tickColourId, accentColour);
        enabled.onClick = [this]{ params.enabled.store(enabled.getToggleState()); };
        addAndMakeVisible(enabled);
        setupSlider(frequency, 20, 20000, 1, params.frequencyHz.load(), " Hz");
        frequency.setSkewFactorFromMidPoint(1000.0);
        frequency.onValueChange = [this]{ params.frequencyHz.store(frequency.getValue()); };
        addAndMakeVisible(frequency);
        slope.addItem("12 dB/oct", 12);
        slope.addItem("24 dB/oct", 24);
        slope.setSelectedId(params.slopeDbPerOct.load(), juce::dontSendNotification);
        slope.onChange = [this]{ params.slopeDbPerOct.store(slope.getSelectedId()); };
        addAndMakeVisible(slope);
    }
    void sync()
    {
        enabled.setToggleState(params.enabled.load(), juce::dontSendNotification);
        frequency.setValue(params.frequencyHz.load(), juce::dontSendNotification);
        slope.setSelectedId(params.slopeDbPerOct.load(), juce::dontSendNotification);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced(8, 4);
        enabled.setBounds(a.removeFromTop(27).removeFromLeft(190));
        slope.setBounds(a.removeFromRight(115).reduced(0, 2));
        frequency.setBounds(a);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff2c3644));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 7);
    }
private:
    dualdsp::CutoffSettings& params;
    juce::ToggleButton enabled;
    juce::Slider frequency;
    juce::ComboBox slope;
};


class MeterPair final : public juce::Component
{
public:
    void setLevels(const dualdsp::LiveLevels& levels)
    {
        values = levels;
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff101923));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 7);
        g.setFont(11.0f);
        g.setColour(mutedColour);
        g.drawText("SIGNAL (dBFS)", 10, 2, 130, 14, juce::Justification::centredLeft);
        drawMeter(g, 19, "IN", values.inputPeak, values.inputRms, juce::Colour(0xff65cad8));
        drawMeter(g, 38, "OUT", values.outputPeak, values.outputRms, juce::Colour(0xff71daa6));
    }
private:
    void drawMeter(juce::Graphics& g, int y, const juce::String& label,
                   float peak, float rms, juce::Colour colour)
    {
        const float left = 48.0f, width = static_cast<float>(getWidth()) - 123.0f;
        const auto position = [width](float db)
        {
            return width * juce::jlimit(0.0f, 1.0f, (db + 72.0f) / 72.0f);
        };
        g.setColour(mutedColour);
        g.drawText(label, 10, y - 2, 36, 17, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff293745));
        g.fillRoundedRectangle(left, static_cast<float>(y), width, 12.0f, 3.0f);
        g.setColour(colour.withAlpha(0.65f));
        g.fillRoundedRectangle(left, static_cast<float>(y), position(rms), 12.0f, 2.0f);
        g.setColour(peak >= -0.1f ? juce::Colours::red : colour.brighter(0.45f));
        g.fillRect(left + position(peak), static_cast<float>(y) - 1.0f, 3.0f, 14.0f);
        g.setColour(peak >= -0.1f ? juce::Colours::red : juce::Colours::white);
        g.drawText(juce::String(peak, 1) + " dB", getWidth() - 74, y - 2, 69, 17,
                   juce::Justification::centredRight);
    }
    dualdsp::LiveLevels values;
};

class ResponseGraph final : public juce::Component
{
public:
    explicit ResponseGraph(const dualdsp::ChannelSettings& p) : params(p) {}
    void setSampleRate(double sr) { sampleRate = sr; }
    void setSpectrum(const std::array<float, dualdsp::kRtaBins>& input,
                     const std::array<float, dualdsp::kRtaBins>& output)
    {
        inputRta = input;
        outputRta = output;
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        auto rect = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff111c28));
        g.fillRoundedRectangle(rect, 8);
        auto chart = rect.reduced(42.0f, 16.0f);
        chart.removeFromBottom(12.0f);
        const auto fx = [&](double hz) { return chart.getX() + chart.getWidth() *
            static_cast<float>(std::log10(hz / 20.0) / 3.0); };
        const auto fy = [&](double db) { return chart.getY() + chart.getHeight() *
            static_cast<float>((24.0 - std::clamp(db, -24.0, 24.0)) / 48.0); };
        g.setFont(10.0f);
        for (int db : { -24, -12, 0, 12, 24 })
        {
            g.setColour(db == 0 ? juce::Colour(0xff647c90) : juce::Colour(0xff2c3b4c));
            const auto y = fy(db);
            g.drawHorizontalLine(static_cast<int>(y), chart.getX(), chart.getRight());
            g.setColour(mutedColour);
            g.drawText(juce::String(db), juce::Rectangle<int>(2, static_cast<int>(y) - 6, 32, 14),
                       juce::Justification::centredRight);
        }
        for (auto hz : {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000})
        {
            g.setColour(juce::Colour(0xff263b4e));
            const auto x = fx(static_cast<double>(hz));
            g.drawVerticalLine(static_cast<int>(x), chart.getY(), chart.getBottom());
            if (hz == 20 || hz == 100 || hz == 1000 || hz == 10000)
            {
                g.setColour(mutedColour);
                g.drawFittedText(hz >= 1000 ? juce::String(hz / 1000) + "k" : juce::String(hz),
                    static_cast<int>(x) - 17, static_cast<int>(chart.getBottom()), 40, 14,
                    juce::Justification::centred, 1);
            }
        }
        // Measured post-DSP output spectrum: transparent underlay using the
        // right-hand -96 to 0 dBFS scale (EQ uses +/-24 dB on the left).
        const auto rtaY = [&](float level)
        {
            return chart.getY() + chart.getHeight() * (0.0f -
                juce::jlimit(-96.0f, 0.0f, level)) / 96.0f;
        };
        // Draw the measured input first, behind the post-DSP output trace.
        // Both use the same right-side dBFS scale, independent of EQ gain.
        const auto drawRta = [&](const std::array<float, dualdsp::kRtaBins>& bins,
                                 juce::Colour colour, float fillAlpha, float lineAlpha, float width)
        {
            juce::Path trace;
            for (int index = 0; index < dualdsp::kRtaBins; ++index)
            {
                const auto hz = 20.0 * std::pow(1000.0,
                    static_cast<double>(index) / (dualdsp::kRtaBins - 1));
                const auto x = fx(hz);
                if (index == 0) trace.startNewSubPath(x, rtaY(bins[static_cast<std::size_t>(index)]));
                else trace.lineTo(x, rtaY(bins[static_cast<std::size_t>(index)]));
            }
            auto area = trace;
            area.lineTo(chart.getRight(), chart.getBottom());
            area.lineTo(chart.getX(), chart.getBottom());
            area.closeSubPath();
            g.setColour(colour.withAlpha(fillAlpha));
            g.fillPath(area);
            g.setColour(colour.withAlpha(lineAlpha));
            g.strokePath(trace, juce::PathStrokeType(width));
        };
        drawRta(inputRta, juce::Colour(0xff8fa5d9), 0.07f, 0.70f, 1.2f);
        drawRta(outputRta, juce::Colour(0xff4ecf9d), 0.13f, 0.90f, 1.5f);
        g.setColour(juce::Colour(0xffc0d2e4));
        g.setFont(10.0f);
        g.drawText("IN: blue   OUT: green", static_cast<int>(chart.getX()) + 6,
                   static_cast<int>(chart.getY()) + 2, 190, 15, juce::Justification::left);

        g.setColour(juce::Colour(0xff70d6aa));
        g.setFont(10.0f);
        g.drawText("0", static_cast<int>(chart.getRight()) + 3,
                   static_cast<int>(chart.getY()) - 6, 30, 13, juce::Justification::left);
        g.drawText("-48", static_cast<int>(chart.getRight()) + 3,
                   static_cast<int>(chart.getCentreY()) - 6, 35, 13, juce::Justification::left);
        g.drawText("-96", static_cast<int>(chart.getRight()) + 3,
                   static_cast<int>(chart.getBottom()) - 6, 35, 13, juce::Justification::left);
        g.drawText("RTA dBFS", static_cast<int>(chart.getRight()) - 60,
                   2, 92, 13, juce::Justification::centredRight);
        juce::Path response;
        for (int i = 0; i <= 220; ++i)
        {
            const double hz = 20.0 * std::pow(1000.0, i / 220.0);
            const auto x = fx(hz);
            const auto y = fy(dualdsp::responseDb(params, sampleRate, hz));
            if (i == 0) response.startNewSubPath(x, y);
            else response.lineTo(x, y);
        }
        g.setColour(accentColour);
        g.strokePath(response, juce::PathStrokeType(2.1f));
        g.setColour(juce::Colour(0xfff6cd70));
        for (const auto& band : params.eq)
            g.fillEllipse(fx(band.frequencyHz.load()) - 3.5f, fy(band.gainDb.load()) - 3.5f, 7, 7);
    }
private:
    const dualdsp::ChannelSettings& params;
    double sampleRate = 48000;
    static std::array<float, dualdsp::kRtaBins> emptySpectrum()
    {
        std::array<float, dualdsp::kRtaBins> initial {};
        initial.fill(-96.0f);
        return initial;
    }
    std::array<float, dualdsp::kRtaBins> inputRta = emptySpectrum();
    std::array<float, dualdsp::kRtaBins> outputRta = emptySpectrum();
};

class ChannelStrip final : public juce::Component
{
public:
    ChannelStrip(int number, dualdsp::ChannelSettings& channelParams)
        : channelNumber(number), params(channelParams)
    {
        heading.setText("CH " + juce::String(number), juce::dontSendNotification);
        heading.setFont(juce::Font(juce::FontOptions(23.0f, juce::Font::bold)));
        heading.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(heading);
        name.setText("Channel " + juce::String(number), false);
        name.setInputRestrictions(40);
        name.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111c28));
        name.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        name.onTextChange = [this]{ if (onNameChange) onNameChange(channelNumber - 1, name.getText()); };
        addAndMakeVisible(name);
        addAndMakeVisible(graph);
        addAndMakeVisible(meters);
        addAndMakeVisible(hp);
        addAndMakeVisible(lp);
        filtersToggle.onClick = [this]
        {
            filtersExpanded = !filtersExpanded;
            updateExpandedSections();
        };
        addAndMakeVisible(filtersToggle);

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

        eqToggle.onClick = [this]
        {
            eqExpanded = !eqExpanded;
            updateExpandedSections();
        };
        addAndMakeVisible(eqToggle);
        headingLabels.setText("FREQUENCY                         GAIN                         Q", juce::dontSendNotification);
        headingLabels.setColour(juce::Label::textColourId, mutedColour);
        addAndMakeVisible(headingLabels);

        for (int i = 0; i < dualdsp::kBands; ++i)
        {
            eqRows[static_cast<std::size_t>(i)] = std::make_unique<EqRow>(i, params.eq[static_cast<std::size_t>(i)]);
            addAndMakeVisible(*eqRows[static_cast<std::size_t>(i)]);
        }
        updateExpandedSections(); // Default: filters and EQ settings hidden.
    }

    std::function<void(int, bool)> onRouteChange;
    std::function<void(int, juce::String)> onNameChange;
    std::function<void()> onLayoutChanged;

    int preferredHeight() const noexcept
    {
        const int eqY = filtersExpanded ? 783 : 634;
        return eqY + (eqExpanded ? 451 : 49);
    }

    void sync(const juce::String& newName)
    {
        if (! name.hasKeyboardFocus(true)) name.setText(newName, false);
        level.setValue(params.levelDb.load(), juce::dontSendNotification);
        delay.setValue(params.delayMs.load(), juce::dontSendNotification);
        polarity.setToggleState(params.invertPolarity.load(), juce::dontSendNotification);
        hp.sync(); lp.sync();
        for (auto& e : eqRows) e->sync();
        graph.repaint();
    }
    void setGraphSampleRate(double sr) { graph.setSampleRate(sr); }
    void setTelemetry(const dualdsp::LiveLevels& levels,
                      const std::array<float, dualdsp::kRtaBins>& inputSpectrum,
                      const std::array<float, dualdsp::kRtaBins>& outputSpectrum)
    {
        meters.setLevels(levels);
        graph.setSpectrum(inputSpectrum, outputSpectrum);
    }

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
        g.fillRoundedRectangle(20.0f, 87.0f, static_cast<float>(getWidth() - 40), 2.0f, 1.0f);
    }

    void resized() override
    {
        const int w = getWidth();
        heading.setBounds(22, 18, 108, 37);
        name.setBounds(132, 18, w - 154, 34);
        const int half = (w - 56) / 2;
        inputCaption.setBounds(22, 95, half, 19);
        outputCaption.setBounds(34 + half, 95, half, 19);
        input.setBounds(22, 119, half, 31);
        output.setBounds(34 + half, 119, half, 31);
        meters.setBounds(18, 158, w - 36, 57);
        levelCaption.setBounds(22, 228, 110, 22);
        delayCaption.setBounds(22, 273, 110, 22);
        level.setBounds(131, 222, w - 153, 36);
        delay.setBounds(131, 268, w - 153, 36);
        polarity.setBounds(24, 314, w - 48, 30);
        graph.setBounds(20, 355, w - 40, 225);
        filtersToggle.setBounds(22, 591, w - 44, 31);
        hp.setBounds(18, 627, w - 36, 70);
        lp.setBounds(18, 703, w - 36, 70);
        const int eqY = filtersExpanded ? 783 : 634;
        eqToggle.setBounds(22, eqY, w - 44, 31);
        headingLabels.setBounds(70, eqY + 29, w - 85, 22);
        for (int i = 0; i < dualdsp::kBands; ++i)
            eqRows[static_cast<std::size_t>(i)]->setBounds(18, eqY + 54 + i * 65, w - 36, 59);
    }

private:
    void updateExpandedSections()
    {
        hp.setVisible(filtersExpanded);
        lp.setVisible(filtersExpanded);
        headingLabels.setVisible(eqExpanded);
        for (auto& row : eqRows) if (row) row->setVisible(eqExpanded);
        filtersToggle.setButtonText(filtersExpanded ? "▼ Filters · High-pass / Low-pass" :
                                                       "▶ Filters · High-pass / Low-pass");
        eqToggle.setButtonText(eqExpanded ? "▼ Parametric EQ · 6 bands" :
                                           "▶ Parametric EQ · 6 bands");
        resized();
        if (onLayoutChanged) onLayoutChanged();
    }
    bool filtersExpanded = false, eqExpanded = false;
    int channelNumber;
    dualdsp::ChannelSettings& params;
    juce::Label heading, inputCaption, outputCaption, levelCaption, delayCaption, headingLabels;
    juce::TextButton filtersToggle, eqToggle;
    juce::TextEditor name;
    ResponseGraph graph { params };
    MeterPair meters;
    CutoffRow hp { "High-pass enabled", params.highPass };
    CutoffRow lp { "Low-pass enabled", params.lowPass };
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
        left.onLayoutChanged = [this] { refreshHeight(); };
        right.onLayoutChanged = [this] { refreshHeight(); };
        refreshHeight();
    }

    void refreshHeight()
    {
        setSize(1150, std::max(left.preferredHeight(), right.preferredHeight()));
    }

    void resized() override
    {
        left.setBounds(0, 0, 565, getHeight());
        right.setBounds(585, 0, 565, getHeight());
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
        for (auto& spectrum : publishedRta) spectrum.fill(-96.0f);
        for (auto& spectrum : publishedInputRta) spectrum.fill(-96.0f);
        appFolder().createDirectory();
        presetFolder().createDirectory();
        readPreferences();

        title.setText("Simple DSP", juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        subtitle.setText("Independent mono processing  -  no summing", juce::dontSendNotification);
        subtitle.setColour(juce::Label::textColourId, mutedColour);
        deviceLabel.setText("AUDIO INTERFACE", juce::dontSendNotification);
        deviceLabel.setColour(juce::Label::textColourId, accentColour);
        for (auto* l : { &title, &subtitle, &deviceLabel, &remoteInfo, &info }) addAndMakeVisible(*l);
        remoteInfo.setColour(juce::Label::textColourId, accentColour);
        info.setColour(juce::Label::textColourId, mutedColour);
        info.setText("ASIO recommended for low latency. Test first with outputs muted.", juce::dontSendNotification);

        savePreset.setButtonText("Save preset…");
        loadPreset.setButtonText("Load preset…");
        chooseStartup.setButtonText("Startup file…");
        loadOnLaunch.setButtonText("Load at launch");
        startWithWindows.setButtonText("Start with Windows");
        pinRequiredToggle.setButtonText("Require remote PIN");
        pinRequiredToggle.setColour(juce::ToggleButton::tickColourId, accentColour);
        pinRequiredToggle.setToggleState(pinRequired, juce::dontSendNotification);
        pinRequiredToggle.onClick = [this]
        {
            pinRequired = pinRequiredToggle.getToggleState();
            if (writePreferences())
            {
                remote.setPinRequired(pinRequired);
                publishStartupSettings();
                updateRemoteLabel();
                info.setText(pinRequired ? "Remote PIN enabled." : "Remote PIN disabled. Use trusted private Wi-Fi only.", juce::dontSendNotification);
            }
            else
            {
                pinRequired = !pinRequired;
                pinRequiredToggle.setToggleState(pinRequired, juce::dontSendNotification);
                info.setText("Could not save remote PIN setting.", juce::dontSendNotification);
            }
        };
        loadOnLaunch.setToggleState(launchPreset, juce::dontSendNotification);
        loadOnLaunch.setColour(juce::ToggleButton::tickColourId, accentColour);
        startWithWindows.setColour(juce::ToggleButton::tickColourId, accentColour);
       #if JUCE_WINDOWS
        // Preserve and migrate any v0.2 autostart entry to the new app name.
        if (juce::WindowsRegistry::valueExists(legacyRegistryRunKey()))
        {
            if (!juce::WindowsRegistry::valueExists(registryRunKey()))
            {
                const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
                if (juce::WindowsRegistry::setValue(registryRunKey(), "\"" + executable + "\""))
                    juce::WindowsRegistry::deleteValue(legacyRegistryRunKey());
            }
            else juce::WindowsRegistry::deleteValue(legacyRegistryRunKey());
        }
        startWithWindows.setToggleState(juce::WindowsRegistry::valueExists(registryRunKey())
                                   || juce::WindowsRegistry::valueExists(legacyRegistryRunKey()),
                                   juce::dontSendNotification);
       #else
        startWithWindows.setEnabled(false);
       #endif
        for (auto* b : { &savePreset, &loadPreset, &chooseStartup }) addAndMakeVisible(*b);
        for (auto* b : { &loadOnLaunch, &startWithWindows, &pinRequiredToggle }) addAndMakeVisible(*b);
        startupStatus.setColour(juce::Label::textColourId, accentColour);
        addAndMakeVisible(startupStatus);
        publishStartupSettings();
        updateStartupStatus();
        savePreset.onClick = [this] { browseSave(); };
        loadPreset.onClick = [this] { browseLoad(false); };
        chooseStartup.onClick = [this] { browseLoad(true); };
        loadOnLaunch.onClick = [this]
        {
            const bool before = launchPreset;
            launchPreset = loadOnLaunch.getToggleState();
            if (!writePreferences())
            {
                launchPreset = before;
                loadOnLaunch.setToggleState(before, juce::dontSendNotification);
                info.setText("Could not save load-at-launch setting.", juce::dontSendNotification);
            }
            else
            {
                publishStartupSettings();
                updateStartupStatus();
                info.setText(launchPreset ? "Load at launch setting confirmed." : "Load at launch disabled.", juce::dontSendNotification);
            }
        };
        startWithWindows.onClick = [this] { toggleWindowsStartup(); };
        chooseStartup.setTooltip(startupFile.isEmpty() ? "Choose the preset to use on launch" : startupFile);

        selector.setSize(295, 790);
        deviceViewport.setViewedComponent(&selector, false);
        deviceViewport.setScrollBarsShown(true, false);
        contentViewport.setViewedComponent(&strips, false);
        contentViewport.setScrollBarsShown(true, true);
        addAndMakeVisible(deviceViewport);
        addAndMakeVisible(contentViewport);
        strips.left.onRouteChange = [this](int changed, bool outputChanged) { routeChanged(changed, outputChanged); };
        strips.right.onRouteChange = [this](int changed, bool outputChanged) { routeChanged(changed, outputChanged); };
        strips.left.onNameChange = [this](int ch, juce::String n) { setName(ch, n); };
        strips.right.onNameChange = [this](int ch, juce::String n) { setName(ch, n); };

        auto savedDevice = juce::XmlDocument::parse(deviceStateFile());
        const auto error = deviceManager.initialise(2, 2, savedDevice.get(), true);
        if (error.isNotEmpty()) info.setText("Audio device: " + error, juce::dontSendNotification);
        updateDeviceRouting(true);
        deviceManager.addAudioCallback(&engine);
        if (launchPreset && startupFile.isNotEmpty()) loadPresetFromFile(juce::File(startupFile), false);
        syncStrips();

        remoteAddress = RemoteServer::getPrivateIPv4();
        if (remoteAddress.isNotEmpty())
        {
            const auto safe = juce::Component::SafePointer<MainComponent>(this);
            const bool started = remote.start(remoteAddress, 8765, remotePin, pinRequired, presetFolder(),
                [this] { return getStateJson(); },
                [this] { return getTelemetryJson(); },
                [safe](const juce::var& command)
                {
                    juce::MessageManager::callAsync([safe, command]
                    {
                        if (safe != nullptr) safe->handleRemote(command);
                    });
                },
                [safe](const juce::File& file) -> bool
                {
                    // The server waits for a *persisted* desktop selection before
                    // responding to the mobile remote with a success status.
                    auto promise = std::make_shared<std::promise<bool>>();
                    auto future = promise->get_future();
                    if (!juce::MessageManager::callAsync([safe, file, promise]
                    {
                        bool confirmed = false;
                        if (safe != nullptr) confirmed = safe->setStartupFromRemote(file);
                        promise->set_value(confirmed);
                    })) return false;
                    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
                        return false;
                    return future.get();
                });
            if (!started)
            {
                remoteAddress.clear();
                remoteInfo.setText("Remote unavailable: TCP port 8765 already in use", juce::dontSendNotification);
            }
            else updateRemoteLabel();
        }
        else remoteInfo.setText("Remote unavailable: no private IPv4 interface found", juce::dontSendNotification);

        startTimerHz(12);
        setSize(1560, 925);
    }

    ~MainComponent() override
    {
        stopTimer();
        remote.stop(); // Must stop requests before the engine/state is destroyed.
        deviceManager.removeAudioCallback(&engine);
        saveAudioDeviceState();
        deviceViewport.setViewedComponent(nullptr, false);
        contentViewport.setViewedComponent(nullptr, false);
    }

    void paint(juce::Graphics& g) override { g.fillAll(backgroundColour); }
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        auto top = bounds.removeFromTop(159);
        auto headingRow = top.removeFromTop(40);
        title.setBounds(headingRow.removeFromLeft(345));
        subtitle.setBounds(headingRow);
        auto controls = top.removeFromTop(39);
        savePreset.setBounds(controls.removeFromLeft(114).reduced(3, 1));
        loadPreset.setBounds(controls.removeFromLeft(114).reduced(3, 1));
        chooseStartup.setBounds(controls.removeFromLeft(118).reduced(3, 1));
        loadOnLaunch.setBounds(controls.removeFromLeft(145));
        startWithWindows.setBounds(controls.removeFromLeft(175));
        pinRequiredToggle.setBounds(controls.removeFromLeft(190));
        startupStatus.setBounds(top.removeFromTop(34));
        remoteInfo.setBounds(top.removeFromTop(34));
        auto footer = bounds.removeFromBottom(36);
        info.setBounds(footer);
        auto sidebar = bounds.removeFromLeft(315);
        deviceLabel.setBounds(sidebar.removeFromTop(28));
        deviceViewport.setBounds(sidebar.reduced(2));
        bounds.removeFromLeft(15);
        contentViewport.setBounds(bounds);
    }

private:
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;
    std::array<dualdsp::SpectrumAnalyzer, 2> inputSpectrumAnalyzers;
    std::array<dualdsp::SpectrumAnalyzer, 2> outputSpectrumAnalyzers;
    std::array<dualdsp::LiveLevels, 2> displayLevels;
    mutable std::mutex liveMutex;
    std::array<dualdsp::LiveLevels, 2> publishedLevels;
    std::array<std::array<float, dualdsp::kRtaBins>, 2> publishedRta {};
    std::array<std::array<float, dualdsp::kRtaBins>, 2> publishedInputRta {};
    juce::AudioDeviceSelectorComponent selector;
    juce::Viewport deviceViewport, contentViewport;
    StripArea strips;
    juce::Label title, subtitle, info, deviceLabel, remoteInfo, startupStatus;
    juce::TextButton savePreset, loadPreset, chooseStartup;
    juce::ToggleButton loadOnLaunch, startWithWindows, pinRequiredToggle;
    std::unique_ptr<juce::FileChooser> chooser;
    RemoteServer remote;
    std::array<juce::String, 2> names { "Channel 1", "Channel 2" };
    mutable std::mutex dataMutex;
    juce::StringArray publishedInputs, publishedOutputs;
    juce::String publishedDevice = "No audio device";
    std::atomic<double> graphSampleRate { 48000.0 };
    juce::String remotePin;
    juce::String remoteAddress;
    juce::String startupFile;
    juce::String publishedStartupPreset;
    bool publishedLoadAtLaunch = false;
    bool publishedPinRequired = true;
    bool launchPreset = false;
    bool pinRequired = true;
    juce::String lastDeviceIdentity;
    juce::StringArray lastInputs, lastOutputs;
    std::array<int, 2> lastValidInput { -1, -1 }, lastValidOutput { -1, -1 };
    bool suppressUpdates = false;

    static juce::File appFolder()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("RobbyApps").getChildFile("DualChannelDSP");
    }
    static juce::File presetFolder() { return appFolder().getChildFile("Presets"); }
    static juce::File preferenceFile() { return appFolder().getChildFile("settings.json"); }
    static juce::File deviceStateFile() { return appFolder().getChildFile("audio-device.xml"); }
    static juce::String registryRunKey()
    {
        return R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run\SimpleDSP)";
    }
    static juce::String legacyRegistryRunKey()
    {
        return R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run\DualChannelDSP)";
    }
    void readPreferences()
    {
        const auto preferences = juce::JSON::parse(preferenceFile().loadFileAsString());
        if (preferences.isObject())
        {
            startupFile = preferences.getProperty("startupFile", {}).toString();
            launchPreset = static_cast<bool>(preferences.getProperty("loadAtLaunch", false));
            remotePin = preferences.getProperty("remotePin", {}).toString();
            pinRequired = static_cast<bool>(preferences.getProperty("remotePinRequired", true));
        }
        if (remotePin.length() != 6 || !remotePin.containsOnly("0123456789"))
            remotePin = juce::String(juce::Random::getSystemRandom().nextInt(900000) + 100000);
        writePreferences();
    }
    bool writePreferences() const
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("startupFile", startupFile);
        obj->setProperty("loadAtLaunch", launchPreset);
        obj->setProperty("remotePin", remotePin);
        obj->setProperty("remotePinRequired", pinRequired);
        if (!preferenceFile().replaceWithText(juce::JSON::toString(juce::var(obj.release()), true)))
            return false;
        const auto saved = juce::JSON::parse(preferenceFile().loadFileAsString());
        return saved.isObject()
            && saved.getProperty("startupFile", {}).toString() == startupFile
            && static_cast<bool>(saved.getProperty("loadAtLaunch", false)) == launchPreset
            && static_cast<bool>(saved.getProperty("remotePinRequired", true)) == pinRequired;
    }
    void publishStartupSettings()
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        publishedStartupPreset = startupFile.isEmpty() ? juce::String() : juce::File(startupFile).getFileName();
        if (publishedStartupPreset.endsWithIgnoreCase(".dsp.json"))
            publishedStartupPreset = publishedStartupPreset.dropLastCharacters(9);
        publishedLoadAtLaunch = launchPreset;
        publishedPinRequired = pinRequired;
    }
    void updateStartupStatus()
    {
        const auto filename = startupFile.isEmpty() ? juce::String() : juce::File(startupFile).getFileName();
        startupStatus.setText(filename.isEmpty() ? "Load at launch: no preset selected"
                : launchPreset ? "Load at launch: " + filename + "  (selected and saved)"
                               : "Load at launch: OFF  |  Selected: " + filename,
                juce::dontSendNotification);
        chooseStartup.setTooltip(filename.isEmpty() ? "Choose the preset to use on launch" : startupFile);
    }
    void updateRemoteLabel()
    {
        remoteInfo.setText("PHONE: http://" + remoteAddress + ":8765  |  "
                           + (pinRequired ? "PIN required: " + remotePin : "PIN off (trusted Wi-Fi only)"),
                           juce::dontSendNotification);
    }
    bool setStartupFromRemote(const juce::File& file)
    {
        if (!file.existsAsFile() || file.getSize() > 100000) return false;
        const auto parsed = juce::JSON::parse(file.loadFileAsString());
        const auto v = parsed.getProperty("channels", {});
        if (!parsed.isObject() || !v.isArray() || v.getArray()->size() != 2) return false;
        const auto previousFile = startupFile;
        const auto previousLoad = launchPreset;
        startupFile = file.getFullPathName();
        launchPreset = true;
        if (!writePreferences())
        {
            startupFile = previousFile;
            launchPreset = previousLoad;
            return false;
        }
        loadOnLaunch.setToggleState(true, juce::dontSendNotification);
        publishStartupSettings();
        updateStartupStatus();
        info.setText("Confirmed startup preset from phone: " + file.getFileName(), juce::dontSendNotification);
        return true;
    }
    void toggleWindowsStartup()
    {
       #if JUCE_WINDOWS
        bool ok = false;
        if (startWithWindows.getToggleState())
        {
            const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
            ok = juce::WindowsRegistry::setValue(registryRunKey(), "\"" + exe + "\"");
            if (ok && juce::WindowsRegistry::valueExists(legacyRegistryRunKey()))
                juce::WindowsRegistry::deleteValue(legacyRegistryRunKey());
        }
        else
        {
            const bool currentOk = !juce::WindowsRegistry::valueExists(registryRunKey())
                || juce::WindowsRegistry::deleteValue(registryRunKey());
            const bool oldOk = !juce::WindowsRegistry::valueExists(legacyRegistryRunKey())
                || juce::WindowsRegistry::deleteValue(legacyRegistryRunKey());
            ok = currentOk && oldOk;
        }
        if (!ok)
        {
            startWithWindows.setToggleState(!startWithWindows.getToggleState(), juce::dontSendNotification);
            info.setText("Could not update Windows startup setting.", juce::dontSendNotification);
        }
       #endif
    }

    void browseSave()
    {
        chooser = std::make_unique<juce::FileChooser>("Save DSP preset", presetFolder(), "*.dsp.json");
        const auto safe = juce::Component::SafePointer<MainComponent>(this);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                auto file = fc.getResult();
                if (! file.getFileName().endsWithIgnoreCase(".dsp.json"))
                    file = file.getSiblingFile(file.getFileName() + ".dsp.json");
                if (file.replaceWithText(safe->getStateJson()))
                    safe->info.setText("Preset saved: " + file.getFileName(), juce::dontSendNotification);
                else safe->info.setText("Could not save preset.", juce::dontSendNotification);
            });
    }
    void browseLoad(bool forStartup)
    {
        chooser = std::make_unique<juce::FileChooser>(forStartup ? "Select startup preset" : "Load DSP preset",
                                                      presetFolder(), "*.dsp.json");
        const auto safe = juce::Component::SafePointer<MainComponent>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, forStartup](const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                const auto file = fc.getResult();
                if (forStartup)
                {
                    if (!safe->setStartupFromRemote(file))
                        safe->info.setText("Could not save startup preset.", juce::dontSendNotification);
                }
                else safe->loadPresetFromFile(file, true);
            });
    }
    bool loadPresetFromFile(const juce::File& file, bool notify)
    {
        if (!file.existsAsFile() || file.getSize() > 100000)
        {
            if (notify) info.setText("Preset not found or too large.", juce::dontSendNotification);
            return false;
        }
        const auto parsed = juce::JSON::parse(file.loadFileAsString());
        if (!applySnapshot(parsed))
        {
            if (notify) info.setText("Could not read preset format.", juce::dontSendNotification);
            return false;
        }
        if (notify) info.setText("Preset loaded: " + file.getFileName(), juce::dontSendNotification);
        return true;
    }

    void timerCallback() override
    {
        updateDeviceRouting(false);
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            graphSampleRate.store(device->getCurrentSampleRate());
            strips.left.setGraphSampleRate(device->getCurrentSampleRate());
            strips.right.setGraphSampleRate(device->getCurrentSampleRate());
            // Keep the footer informative without overwriting reported errors constantly.
        }

        // Only the GUI timer performs FFT analysis, keeping the audio callback
        // free of transforms, allocations, socket operations and locks.
        const auto sr = graphSampleRate.load();
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto i = static_cast<std::size_t>(ch);
            inputSpectrumAnalyzers[i].update(engine.telemetry[i], sr, true);
            outputSpectrumAnalyzers[i].update(engine.telemetry[i], sr, false);
            auto levels = engine.telemetry[i].takeLevels();
            // GUI peak falloff: hold transients briefly, then decay about 18 dB/s.
            levels.inputPeak = std::max(levels.inputPeak, displayLevels[i].inputPeak - 1.5f);
            levels.outputPeak = std::max(levels.outputPeak, displayLevels[i].outputPeak - 1.5f);
            displayLevels[i] = levels;
            const auto& inputRta = inputSpectrumAnalyzers[i].getBins();
            const auto& outputRta = outputSpectrumAnalyzers[i].getBins();
            if (ch == 0) strips.left.setTelemetry(levels, inputRta, outputRta);
            else strips.right.setTelemetry(levels, inputRta, outputRta);
            {
                std::lock_guard<std::mutex> guard(liveMutex);
                publishedLevels[i] = levels;
                publishedInputRta[i] = inputRta;
                publishedRta[i] = outputRta;
            }
        }
        syncStrips();
    }
    void setName(int ch, juce::String n)
    {
        if (ch < 0 || ch >= 2) return;
        std::lock_guard<std::mutex> guard(dataMutex);
        names[static_cast<std::size_t>(ch)] = n.substring(0, 40);
    }
    void syncStrips()
    {
        juce::String n0, n1;
        {
            std::lock_guard<std::mutex> guard(dataMutex);
            n0 = names[0]; n1 = names[1];
        }
        strips.left.sync(n0);
        strips.right.sync(n1);
    }

    void saveAudioDeviceState() const
    {
        if (auto xml = deviceManager.createStateXml())
            deviceStateFile().replaceWithText(xml->toString());
    }
    void publishDeviceInfo()
    {
        std::lock_guard<std::mutex> guard(dataMutex);
        publishedInputs = lastInputs;
        publishedOutputs = lastOutputs;
        publishedDevice = lastDeviceIdentity.isEmpty() ? "No audio device" : lastDeviceIdentity;
    }
    void updateDeviceRouting(bool force)
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr)
        {
            if (force || lastDeviceIdentity.isNotEmpty())
            {
                lastDeviceIdentity.clear(); lastInputs.clear(); lastOutputs.clear();
                strips.left.setRouteOptions({}, {}, -1, -1);
                strips.right.setRouteOptions({}, {}, -1, -1);
                publishRoutes(); publishDeviceInfo();
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
            lastInputs = in; lastOutputs = out;
            const auto valid = [](int route, int count) { return route >= 0 && route < count ? route : -1; };
            const int input1 = changedInterface ? (in.size() > 0 ? 0 : -1) : valid(lastValidInput[0], in.size());
            const int input2 = changedInterface ? (in.size() > 1 ? 1 : -1) : valid(lastValidInput[1], in.size());
            const int output1 = changedInterface ? (out.size() > 0 ? 0 : -1) : valid(lastValidOutput[0], out.size());
            const int output2 = changedInterface ? (out.size() > 1 ? 1 : -1) : valid(lastValidOutput[1], out.size());
            strips.left.setRouteOptions(in, out, input1, output1);
            strips.right.setRouteOptions(in, out, input2, output2);
            suppressUpdates = false;
            publishRoutes(); publishDeviceInfo(); applyActivePorts();
        }
        else applyActivePorts();
    }
    void routeChanged(int changedIndex, bool)
    {
        if (suppressUpdates) return;
        const auto newOutputs = std::array<int, 2>{ strips.left.getOutputRoute(), strips.right.getOutputRoute() };
        if (newOutputs[0] >= 0 && newOutputs[0] == newOutputs[1])
        {
            auto& changedStrip = changedIndex == 0 ? strips.left : strips.right;
            changedStrip.setOutputRoute(lastValidOutput[static_cast<std::size_t>(changedIndex)]);
            info.setText("Output conflict: each channel requires a different physical output.", juce::dontSendNotification);
            return;
        }
        publishRoutes(); applyActivePorts();
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
        engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
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
        {
            engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
            return;
        }
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = false;
        setup.inputChannels = inputs;
        setup.outputChannels = outputs;
        const auto error = deviceManager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty()) info.setText("Audio routing error: " + error, juce::dontSendNotification);
        else saveAudioDeviceState();
        engine.refreshRoutes(deviceManager.getCurrentAudioDevice());
    }

    juce::var snapshot() const
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("version", 2);
        juce::Array<juce::var> channels, ins, outs;
        std::array<juce::String, 2> safeNames;
        juce::String device;
        {
            std::lock_guard<std::mutex> guard(dataMutex);
            safeNames = names;
            device = publishedDevice;
            for (auto s : publishedInputs) ins.add(s);
            for (auto s : publishedOutputs) outs.add(s);
        }
        juce::String selectedStartup;
        bool startupEnabled, remotePinEnabled;
        {
            std::lock_guard<std::mutex> guard(dataMutex);
            selectedStartup = publishedStartupPreset;
            startupEnabled = publishedLoadAtLaunch;
            remotePinEnabled = publishedPinRequired;
        }
        root->setProperty("startupPresetName", selectedStartup);
        root->setProperty("loadAtLaunch", startupEnabled);
        root->setProperty("remotePinRequired", remotePinEnabled);
        root->setProperty("device", device);
        root->setProperty("inputs", juce::var(ins));
        root->setProperty("outputs", juce::var(outs));
        const auto sr = graphSampleRate.load();
        for (int i = 0; i < 2; ++i)
        {
            const auto& p = engine.parameters[static_cast<std::size_t>(i)];
            auto c = std::make_unique<juce::DynamicObject>();
            c->setProperty("name", safeNames[static_cast<std::size_t>(i)]);
            c->setProperty("inputRoute", p.inputRoute.load());
            c->setProperty("outputRoute", p.outputRoute.load());
            c->setProperty("levelDb", p.levelDb.load());
            c->setProperty("delayMs", p.delayMs.load());
            c->setProperty("invertPolarity", p.invertPolarity.load());
            juce::Array<juce::var> bands, curve;
            for (const auto& band : p.eq)
            {
                auto b = std::make_unique<juce::DynamicObject>();
                b->setProperty("frequencyHz", band.frequencyHz.load());
                b->setProperty("gainDb", band.gainDb.load());
                b->setProperty("q", band.q.load());
                bands.add(juce::var(b.release()));
            }
            c->setProperty("eq", juce::var(bands));
            const auto cutoff = [](const dualdsp::CutoffSettings& f) -> juce::var
            {
                auto o = std::make_unique<juce::DynamicObject>();
                o->setProperty("enabled", f.enabled.load());
                o->setProperty("frequency", f.frequencyHz.load());
                o->setProperty("slope", f.slopeDbPerOct.load());
                return juce::var(o.release());
            };
            c->setProperty("highPass", cutoff(p.highPass));
            c->setProperty("lowPass", cutoff(p.lowPass));
            for (int n = 0; n < 128; ++n)
                curve.add(dualdsp::responseDb(p, sr, 20.0 * std::pow(1000.0, n / 127.0)));
            c->setProperty("curve", juce::var(curve));
            channels.add(juce::var(c.release()));
        }
        root->setProperty("channels", juce::var(channels));
        return juce::var(root.release());
    }

    // Live data is intentionally excluded from preset JSON. Served separately
    // so a phone can poll meters and both RTAs at up to 10 Hz without rebuilding controls.
    juce::String getTelemetryJson() const
    {
        std::array<dualdsp::LiveLevels, 2> levels;
        std::array<std::array<float, dualdsp::kRtaBins>, 2> spectra;
        std::array<std::array<float, dualdsp::kRtaBins>, 2> inputSpectra;
        {
            std::lock_guard<std::mutex> guard(liveMutex);
            levels = publishedLevels;
            spectra = publishedRta;
            inputSpectra = publishedInputRta;
        }
        juce::Array<juce::var> channels;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto i = static_cast<std::size_t>(ch);
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty("inputPeak", levels[i].inputPeak);
            obj->setProperty("inputRms", levels[i].inputRms);
            obj->setProperty("outputPeak", levels[i].outputPeak);
            obj->setProperty("outputRms", levels[i].outputRms);
            juce::Array<juce::var> spectrum;
            for (const auto value : spectra[i]) spectrum.add(value);
            obj->setProperty("rta", juce::var(spectrum));
            juce::Array<juce::var> inputSpectrum;
            for (const auto value : inputSpectra[i]) inputSpectrum.add(value);
            obj->setProperty("inputRta", juce::var(inputSpectrum));
            channels.add(juce::var(obj.release()));
        }
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("channels", juce::var(channels));
        return juce::JSON::toString(juce::var(obj.release()));
    }

    juce::String getStateJson() const { return juce::JSON::toString(snapshot()); }

    bool applySnapshot(const juce::var& state)
    {
        const auto v = state.getProperty("channels", {});
        if (!v.isArray() || v.getArray()->size() != 2) return false;
        std::array<int, 2> pendingInput { -1,-1 }, pendingOutput { -1,-1 };
        const bool matchingDevice = state.getProperty("device", {}).toString() == lastDeviceIdentity;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto c = v[ch];
            if (!c.isObject()) return false;
            const auto idx = static_cast<std::size_t>(ch);
            auto& p = engine.parameters[idx];
            setName(ch, c.getProperty("name", juce::String("Channel ") + juce::String(ch+1)).toString());
            p.levelDb.store(std::clamp(static_cast<double>(c.getProperty("levelDb", 0.0)), -90.0, 18.0));
            p.delayMs.store(std::clamp(static_cast<double>(c.getProperty("delayMs", 0.0)), 0.0, dualdsp::kMaxDelayMs));
            p.invertPolarity.store(static_cast<bool>(c.getProperty("invertPolarity", false)));
            const auto bands = c.getProperty("eq", {});
            if (bands.isArray())
                for (int b = 0; b < dualdsp::kBands && b < bands.getArray()->size(); ++b)
                {
                    const auto entry = bands[b];
                    auto& eq = p.eq[static_cast<std::size_t>(b)];
                    eq.frequencyHz.store(std::clamp(static_cast<double>(entry.getProperty("frequencyHz", 1000.0)), 20.0, 20000.0));
                    eq.gainDb.store(std::clamp(static_cast<double>(entry.getProperty("gainDb", 0.0)), -18.0, 18.0));
                    eq.q.store(std::clamp(static_cast<double>(entry.getProperty("q", 1.0)), 0.1, 15.0));
                }
            const auto setCutoff = [](dualdsp::CutoffSettings& f, const juce::var& x)
            {
                if (!x.isObject()) return;
                f.enabled.store(static_cast<bool>(x.getProperty("enabled", false)));
                f.frequencyHz.store(std::clamp(static_cast<double>(x.getProperty("frequency", 80.0)), 20.0, 20000.0));
                f.slopeDbPerOct.store(static_cast<int>(x.getProperty("slope", 12)) == 24 ? 24 : 12);
            };
            setCutoff(p.highPass, c.getProperty("highPass", {}));
            setCutoff(p.lowPass, c.getProperty("lowPass", {}));
            if (matchingDevice)
            {
                const int rIn = static_cast<int>(c.getProperty("inputRoute", -1));
                const int rOut = static_cast<int>(c.getProperty("outputRoute", -1));
                pendingInput[idx] = rIn >= 0 && rIn < lastInputs.size() ? rIn : -1;
                pendingOutput[idx] = rOut >= 0 && rOut < lastOutputs.size() ? rOut : -1;
            }
        }
        if (pendingOutput[0] >= 0 && pendingOutput[0] == pendingOutput[1]) pendingOutput[1] = -1;
        suppressUpdates = true;
        strips.left.setInputRoute(pendingInput[0]); strips.right.setInputRoute(pendingInput[1]);
        strips.left.setOutputRoute(pendingOutput[0]); strips.right.setOutputRoute(pendingOutput[1]);
        suppressUpdates = false;
        publishRoutes(); applyActivePorts(); syncStrips();
        if (!matchingDevice)
            info.setText("Preset DSP loaded. Routes left unassigned (different audio interface).", juce::dontSendNotification);
        return true;
    }
    void handleRemote(const juce::var& command)
    {
        if (command.hasProperty("channels")) { applySnapshot(command); return; }
        const int ch = static_cast<int>(command.getProperty("channel", -1));
        if (ch < 0 || ch > 1) return;
        const auto key = command.getProperty("key", {}).toString();
        const auto value = command.getProperty("value", {});
        const auto index = static_cast<std::size_t>(ch);
        auto& p = engine.parameters[index];
        const auto numerical = static_cast<double>(value);
        if (key == "name") setName(ch, value.toString());
        else if (key == "levelDb") p.levelDb.store(std::clamp(numerical, -90.0, 18.0));
        else if (key == "delayMs") p.delayMs.store(std::clamp(numerical, 0.0, dualdsp::kMaxDelayMs));
        else if (key == "invertPolarity") p.invertPolarity.store(static_cast<bool>(value));
        else if (key == "inputRoute" || key == "outputRoute")
        {
            const int route = static_cast<int>(value);
            const bool isOut = key == "outputRoute";
            if (route < -1 || route >= (isOut ? lastOutputs.size() : lastInputs.size())) return;
            auto& strip = ch == 0 ? strips.left : strips.right;
            if (isOut) strip.setOutputRoute(route); else strip.setInputRoute(route);
            routeChanged(ch, isOut);
        }
        else if (key.startsWith("highPass.") || key.startsWith("lowPass."))
        {
            auto& f = key.startsWith("highPass.") ? p.highPass : p.lowPass;
            if (key.endsWith(".enabled")) f.enabled.store(static_cast<bool>(value));
            else if (key.endsWith(".frequency")) f.frequencyHz.store(std::clamp(numerical, 20.0, 20000.0));
            else if (key.endsWith(".slope")) f.slopeDbPerOct.store(numerical == 24 ? 24 : 12);
        }
        else
        {
            const int band = static_cast<int>(command.getProperty("band", -1));
            if (band < 0 || band >= dualdsp::kBands) return;
            auto& f = p.eq[static_cast<std::size_t>(band)];
            if (key == "frequencyHz") f.frequencyHz.store(std::clamp(numerical, 20.0, 20000.0));
            else if (key == "gainDb") f.gainDb.store(std::clamp(numerical, -18.0, 18.0));
            else if (key == "q") f.q.store(std::clamp(numerical, 0.1, 15.0));
        }
        syncStrips();
    }
};

class SimpleDSPApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Simple DSP"; }
    const juce::String getApplicationVersion() override { return "0.4.0"; }
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
            : juce::DocumentWindow("Simple DSP", backgroundColour,
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

START_JUCE_APPLICATION(SimpleDSPApp)
