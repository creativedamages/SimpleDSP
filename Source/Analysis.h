#pragma once

// Monitoring only: audio callback publishes simple atomic samples and block statistics;
// FFT work takes place on the Windows GUI timer thread, never on the audio thread.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>

namespace dualdsp
{
constexpr int kRtaFftSize = 4096;
constexpr int kRtaRingSize = 8192;
constexpr int kRtaBins = 128;
constexpr double kMeterFloorDb = -96.0;
static_assert(std::atomic<float>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free,
              "The audio monitoring path requires lock-free atomic writes");

inline float amplitudeToDb(float amplitude) noexcept
{
    return static_cast<float>(std::clamp(20.0 * std::log10(std::max(1.0e-9f, amplitude)),
                                          kMeterFloorDb, 12.0));
}

struct LiveLevels
{
    float inputPeak = static_cast<float>(kMeterFloorDb);
    float inputRms = static_cast<float>(kMeterFloorDb);
    float outputPeak = static_cast<float>(kMeterFloorDb);
    float outputRms = static_cast<float>(kMeterFloorDb);
};

class AudioTelemetry
{
public:
    AudioTelemetry()
    {
        for (auto& v : inputRing) v.store(0.0f, std::memory_order_relaxed);
        for (auto& v : outputRing) v.store(0.0f, std::memory_order_relaxed);
    }
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(8000.0, newSampleRate); // audio callback thread only
        writePosition = 0;
        publishedPosition.store(0, std::memory_order_release);
        for (auto& v : inputRing) v.store(0.0f, std::memory_order_relaxed);
        for (auto& v : outputRing) v.store(0.0f, std::memory_order_relaxed);
        for (auto& v : peakWindow) v.store(0.0f, std::memory_order_relaxed);
        for (auto& v : rmsPower) v.store(0.0f, std::memory_order_relaxed);
        for (auto& v : rmsState) v = 0.0;
    }
    // Executed after both routing and DSP; null pointers represent an unassigned path.
    // Single producer = audio callback. No memory allocations, locks, or FFT here.
    void recordBlock(const float* input, const float* output, int count) noexcept
    {
        if (count <= 0) return;
        float peaks[2] { 0.0f, 0.0f };
        double square[2] { 0.0, 0.0 };
        for (int i = 0; i < count; ++i)
        {
            const float a = input != nullptr ? input[i] : 0.0f;
            const float b = output != nullptr ? output[i] : 0.0f;
            peaks[0] = std::max(peaks[0], std::abs(a));
            peaks[1] = std::max(peaks[1], std::abs(b));
            square[0] += static_cast<double>(a) * a;
            square[1] += static_cast<double>(b) * b;
            inputRing[static_cast<std::size_t>(writePosition % kRtaRingSize)]
                .store(a, std::memory_order_relaxed);
            outputRing[static_cast<std::size_t>(writePosition % kRtaRingSize)]
                .store(b, std::memory_order_relaxed);
            ++writePosition;
        }
        publishedPosition.store(writePosition, std::memory_order_release);
        const double alpha = std::exp(-static_cast<double>(count) / (sampleRate * 0.14));
        for (int i = 0; i < 2; ++i)
        {
            auto& window = peakWindow[static_cast<std::size_t>(i)];
            auto existing = window.load(std::memory_order_relaxed);
            while (existing < peaks[i] && !window.compare_exchange_weak(
                existing, peaks[i], std::memory_order_release, std::memory_order_relaxed)) {}
            const double meanSquare = square[i] / count;
            rmsState[i] = alpha * rmsState[i] + (1.0 - alpha) * meanSquare;
            rmsPower[static_cast<std::size_t>(i)].store(static_cast<float>(rmsState[i]), std::memory_order_release);
        }
    }
    LiveLevels takeLevels() noexcept
    {
        LiveLevels v;
        v.inputPeak = amplitudeToDb(peakWindow[0].exchange(0.0f, std::memory_order_acq_rel));
        v.outputPeak = amplitudeToDb(peakWindow[1].exchange(0.0f, std::memory_order_acq_rel));
        v.inputRms = amplitudeToDb(std::sqrt(std::max(0.0f, rmsPower[0].load(std::memory_order_acquire))));
        v.outputRms = amplitudeToDb(std::sqrt(std::max(0.0f, rmsPower[1].load(std::memory_order_acquire))));
        return v;
    }
    bool copyLatest(std::array<float, kRtaFftSize>& dest, bool inputSignal = false) const noexcept
    {
        const auto end = publishedPosition.load(std::memory_order_acquire);
        if (end < kRtaFftSize) return false;
        for (int i = 0; i < kRtaFftSize; ++i)
            dest[static_cast<std::size_t>(i)] = (inputSignal ? inputRing : outputRing)[
                static_cast<std::size_t>((end - kRtaFftSize + static_cast<std::uint64_t>(i)) % kRtaRingSize)]
                .load(std::memory_order_relaxed);
        return true;
    }
private:
    double sampleRate = 48000.0;
    // Only one writer; stored atomically so the reader never touches non-atomic samples.
    std::array<std::atomic<float>, kRtaRingSize> inputRing {};
    std::array<std::atomic<float>, kRtaRingSize> outputRing {};
    std::atomic<std::uint64_t> publishedPosition { 0 };
    std::uint64_t writePosition = 0;
    std::array<std::atomic<float>, 2> peakWindow {};
    std::array<std::atomic<float>, 2> rmsPower {};
    std::array<double, 2> rmsState { 0.0, 0.0 };
};

class SpectrumAnalyzer
{
public:
    SpectrumAnalyzer() { bins.fill(static_cast<float>(kMeterFloorDb)); }
    void update(const AudioTelemetry& telemetry, double sampleRate, bool inputSignal = false)
    {
        std::array<float, kRtaFftSize> samples {};
        if (!telemetry.copyLatest(samples, inputSignal))
        {
            for (auto& d : bins) d = std::max(static_cast<float>(kMeterFloorDb), d - 5.0f);
            return;
        }
        for (int i = 0; i < kRtaFftSize; ++i)
        {
            const auto window = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * i / (kRtaFftSize - 1));
            spectrum[static_cast<std::size_t>(i)] = std::complex<double>(samples[static_cast<std::size_t>(i)] * window, 0.0);
        }
        // Fixed-size radix-2 FFT. Runs on the message thread, not the audio callback.
        for (int i = 1, j = 0; i < kRtaFftSize; ++i)
        {
            int bit = kRtaFftSize >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(spectrum[static_cast<std::size_t>(i)], spectrum[static_cast<std::size_t>(j)]);
        }
        for (int length = 2; length <= kRtaFftSize; length <<= 1)
        {
            const auto angle = -2.0 * 3.14159265358979323846 / length;
            const std::complex<double> step(std::cos(angle), std::sin(angle));
            for (int start = 0; start < kRtaFftSize; start += length)
            {
                std::complex<double> w(1.0, 0.0);
                for (int j = 0; j < length / 2; ++j)
                {
                    const auto u = spectrum[static_cast<std::size_t>(start+j)];
                    const auto v = spectrum[static_cast<std::size_t>(start+j+length/2)] * w;
                    spectrum[static_cast<std::size_t>(start+j)] = u + v;
                    spectrum[static_cast<std::size_t>(start+j+length/2)] = u - v;
                    w *= step;
                }
            }
        }
        const auto frequencyPerBin = std::max(8000.0, sampleRate) / kRtaFftSize;
        const double lastFrequency = std::min(20000.0, sampleRate * 0.49);
        for (int index = 0; index < kRtaBins; ++index)
        {
            const double hz = 20.0 * std::pow(lastFrequency / 20.0, static_cast<double>(index) / (kRtaBins - 1));
            // Narrow log-frequency bucket, with at least 1 FFT bin at low frequencies.
            const double halfBand = std::pow(2.0, 1.0 / 24.0);
            const int start = std::clamp(static_cast<int>(std::round(hz / halfBand / frequencyPerBin)), 1, kRtaFftSize/2-1);
            const int stop = std::clamp(static_cast<int>(std::round(hz * halfBand / frequencyPerBin)), start, kRtaFftSize/2-1);
            double maxAmp = 0.0;
            for (int k = start; k <= stop; ++k)
                maxAmp = std::max(maxAmp, std::abs(spectrum[static_cast<std::size_t>(k)]));
            const float level = amplitudeToDb(static_cast<float>(maxAmp * (4.0 / (kRtaFftSize - 1))));
            auto& smoothed = bins[static_cast<std::size_t>(index)];
            smoothed += (level > smoothed ? 0.70f : 0.20f) * (level - smoothed);
        }
    }
    const std::array<float, kRtaBins>& getBins() const noexcept { return bins; }
private:
    std::array<std::complex<double>, kRtaFftSize> spectrum {};
    std::array<float, kRtaBins> bins {};
};
}