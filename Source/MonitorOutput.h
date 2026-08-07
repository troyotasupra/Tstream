#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

// A second, independent audio output so the user can hear what is being sent
// to the streaming endpoint.
//
// Why this needs its own AudioDeviceManager: a JUCE AudioDeviceManager owns
// exactly one device, and in the standalone that one is already committed to
// the endpoint Discord captures. Hearing the same audio on a different piece
// of hardware means opening a second device, which means a second clock.
//
// Why that matters: the streaming device and the monitor device free-run on
// unrelated crystals. Whatever the nominal rates say, one of them delivers
// slightly more frames per real second than the other. Feeding a fixed-size
// buffer from one and draining it with the other therefore walks it to an end
// - starving (a silence gap) or overflowing (a discarded chunk) - and no
// buffer size prevents it, because the cause is a rate mismatch and not
// jitter. The same fix the receive path uses applies here: absorb the
// difference as a sub-0.5% resampling ratio, spread over every sample, which
// is inaudible.
//
// Threading:
//   - pushAudio() is called from the plugin's audio thread (producer).
//   - the device callback drains it (consumer).
//   - start()/stop()/device enumeration happen on the message thread.
// The rings are allocated once in prepare() and never resized afterwards, so
// the producer can never race a reallocation while the device is being
// swapped underneath it.
class MonitorOutput : private juce::AudioIODeviceCallback
{
public:
    MonitorOutput();
    ~MonitorOutput() override;

    MonitorOutput (const MonitorOutput&) = delete;
    MonitorOutput& operator= (const MonitorOutput&) = delete;

    // Message thread. Names are those of the shared-mode Windows Audio type,
    // matching the picker used for the streaming output.
    juce::StringArray getAvailableDevices();

    // Message thread. Returns an empty string on success, or a human-readable
    // reason on failure - notably "device is in use", which is the expected
    // result for an interface a DAW already holds open via ASIO.
    juce::String start (const juce::String& deviceName);
    void stop();

    bool isRunning() const noexcept { return running.load (std::memory_order_relaxed); }
    juce::String getCurrentDeviceName() const;
    double getCurrentSampleRate() const noexcept { return monitorRate.load (std::memory_order_relaxed); }

    // Audio thread (plugin side). Must be called with the post-gain buffer so
    // the monitor hears exactly what listeners hear.
    void prepare (double sourceSampleRate);
    void pushAudio (const juce::AudioBuffer<float>& buffer);

    uint32_t getUnderruns() const noexcept { return underruns.load (std::memory_order_relaxed); }
    uint32_t getRingFillFrames() const noexcept { return (uint32_t) ringL.availableToRead(); }

    // Same numbers the receive path is tuned around, kept identical here so
    // the two drift loops behave the same way rather than diverging.
    static constexpr double kTargetFillMs = 80.0;
    static constexpr double kMaxRatioDeviation = 0.005; // +/-0.5%

private:
    // Single-producer / single-consumer float ring. Indices only ever
    // increase; the storage index is taken modulo capacity.
    class Ring
    {
    public:
        void init (size_t capacityFrames)
        {
            capacity = capacityFrames;
            storage.assign (capacity, 0.0f);
            writeIndex.store (0, std::memory_order_relaxed);
            readIndex.store (0, std::memory_order_relaxed);
        }

        size_t availableToRead() const noexcept
        {
            return writeIndex.load (std::memory_order_acquire) - readIndex.load (std::memory_order_relaxed);
        }

        // Producer. Drops oldest on overflow: latency matters more than
        // completeness on a monitoring path, and an unbounded backlog would
        // just mean hearing yourself further and further behind.
        void write (const float* src, size_t count) noexcept
        {
            if (capacity == 0)
                return;

            const size_t w = writeIndex.load (std::memory_order_relaxed);
            const size_t r = readIndex.load (std::memory_order_acquire);

            if (count > capacity)
            {
                src += (count - capacity);
                count = capacity;
            }

            const size_t freeSpace = capacity - (w - r);

            if (count > freeSpace)
                readIndex.store (r + (count - freeSpace), std::memory_order_release);

            for (size_t i = 0; i < count; ++i)
                storage[(w + i) % capacity] = src[i];

            writeIndex.store (w + count, std::memory_order_release);
        }

        // Consumer. Returns false once the ring is empty, so the caller can
        // hold the last sample rather than reading garbage.
        bool pop (float& dest) noexcept
        {
            const size_t w = writeIndex.load (std::memory_order_acquire);
            const size_t r = readIndex.load (std::memory_order_relaxed);

            if (w == r || capacity == 0)
                return false;

            dest = storage[r % capacity];
            readIndex.store (r + 1, std::memory_order_release);
            return true;
        }

        void reset() noexcept
        {
            writeIndex.store (0, std::memory_order_relaxed);
            readIndex.store (0, std::memory_order_relaxed);
        }

    private:
        std::vector<float> storage;
        size_t capacity = 0;
        std::atomic<size_t> writeIndex { 0 };
        std::atomic<size_t> readIndex { 0 };
    };

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager;

    Ring ringL, ringR;
    std::atomic<bool> running { false };
    std::atomic<bool> accepting { false };

    std::atomic<double> sourceRate { 48000.0 };
    std::atomic<double> monitorRate { 48000.0 };
    std::atomic<uint32_t> underruns { 0 };

    // Resampler state, consumer thread only. srcPhase persists across
    // callbacks so there is no discontinuity at block boundaries.
    double srcPhase = 0.0;
    float prevL = 0.0f, prevR = 0.0f;
    float curL = 0.0f, curR = 0.0f;
    bool primed = false;

    // 2 seconds at 96kHz. Allocated once so the producer never races a resize.
    static constexpr size_t kRingCapacityFrames = 192000;
};
