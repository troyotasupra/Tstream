#include "MonitorOutput.h"

MonitorOutput::MonitorOutput()
{
    ringL.init (kRingCapacityFrames);
    ringR.init (kRingCapacityFrames);
}

MonitorOutput::~MonitorOutput()
{
    stop();
}

juce::StringArray MonitorOutput::getAvailableDevices()
{
    // Deliberately the shared-mode "Windows Audio" type only. Exclusive mode
    // would lock the device away from everything else on the machine, which is
    // the opposite of what a monitoring path should do.
    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName().equalsIgnoreCase ("Windows Audio"))
        {
            type->scanForDevices();
            return type->getDeviceNames (false);
        }
    }

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (false);
    }

    return {};
}

juce::String MonitorOutput::getCurrentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();

    return {};
}

juce::String MonitorOutput::start (const juce::String& deviceName)
{
    stop();

    if (deviceName.isEmpty())
        return "No device selected";

    deviceManager.setCurrentAudioDeviceType ("Windows Audio", true);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.outputDeviceName = deviceName;
    setup.inputDeviceName.clear();
    setup.useDefaultOutputChannels = true;
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();

    // 0 inputs: this is a playback-only path, and asking for capture channels
    // would make the open fail on output-only endpoints like an HDMI monitor.
    auto error = deviceManager.initialise (0, 2, nullptr, true, deviceName, &setup);

    if (error.isNotEmpty())
        return error;

    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return "Device did not open";

    // initialise() falls back to another device rather than failing when the
    // requested one is unavailable, so the name has to be checked explicitly.
    // On this rig the common case is an interface a DAW already holds via
    // ASIO, which WASAPI simply cannot open alongside it.
    if (device->getName() != deviceName)
    {
        const auto fallback = device->getName();
        deviceManager.closeAudioDevice();
        return deviceName + " is unavailable (opened " + fallback + " instead) - another app may hold it exclusively, which is what a DAW using ASIO does";
    }

    monitorRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);

    ringL.reset();
    ringR.reset();
    srcPhase = 0.0;
    prevL = prevR = curL = curR = 0.0f;
    primed = false;
    underruns.store (0, std::memory_order_relaxed);

    deviceManager.addAudioCallback (this);

    running.store (true, std::memory_order_relaxed);
    accepting.store (true, std::memory_order_relaxed);

    return {};
}

void MonitorOutput::stop()
{
    // Stop the producer first, then the consumer, so no one is still filling a
    // ring that is about to stop being drained.
    accepting.store (false, std::memory_order_relaxed);
    running.store (false, std::memory_order_relaxed);

    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

void MonitorOutput::prepare (double sourceSampleRate)
{
    if (sourceSampleRate > 0.0)
        sourceRate.store (sourceSampleRate, std::memory_order_relaxed);
}

void MonitorOutput::pushAudio (const juce::AudioBuffer<float>& buffer)
{
    if (! accepting.load (std::memory_order_relaxed))
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float* left = buffer.getReadPointer (0);
    const float* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

    ringL.write (left, (size_t) numSamples);
    ringR.write (right, (size_t) numSamples);
}

void MonitorOutput::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
        monitorRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);

    srcPhase = 0.0;
    prevL = prevR = curL = curR = 0.0f;
    primed = false;
}

void MonitorOutput::audioDeviceStopped()
{
    primed = false;
}

void MonitorOutput::audioDeviceIOCallbackWithContext (const float* const*,
                                                       int,
                                                       float* const* outputChannelData,
                                                       int numOutputChannels,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (numOutputChannels <= 0 || numSamples <= 0)
        return;

    const double outRate = monitorRate.load (std::memory_order_relaxed);
    const double inRate = sourceRate.load (std::memory_order_relaxed);

    if (outRate <= 0.0 || inRate <= 0.0)
        return;

    // Wait for a cushion before starting, and re-buffer rather than limping
    // along starved - draining the instant any data exists means a partial
    // underrun on nearly every callback, which is audible as a constant tick.
    const size_t targetFill = (size_t) (inRate * (kTargetFillMs / 1000.0));

    if (! primed)
    {
        if (ringL.availableToRead() < targetFill)
            return;

        primed = true;
        srcPhase = 0.0;
        ringL.pop (curL);
        ringR.pop (curR);
        prevL = curL;
        prevR = curR;
    }

    // Base ratio converts between the two nominal rates; the correction term
    // absorbs the residual difference between the two hardware clocks. The
    // error is normalised and clamped so a large transient can't demand more
    // correction than is inaudible.
    const double base = inRate / outRate;
    const double fill = (double) ringL.availableToRead();
    double normalisedError = (fill - (double) targetFill) / (double) juce::jmax<size_t> (targetFill, 1);
    normalisedError = juce::jlimit (-1.0, 1.0, normalisedError);

    const double ratio = base * (1.0 + kMaxRatioDeviation * normalisedError);

    float* out0 = outputChannelData[0];
    float* out1 = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        while (srcPhase >= 1.0)
        {
            prevL = curL;
            prevR = curR;

            if (! ringL.pop (curL))
            {
                // Starved. Hold the last value rather than jumping to zero,
                // re-arm priming, and let the cushion refill.
                underruns.fetch_add (1, std::memory_order_relaxed);
                primed = false;
                return;
            }

            ringR.pop (curR);
            srcPhase -= 1.0;
        }

        const float t = (float) srcPhase;

        if (out0 != nullptr) out0[i] = prevL + (curL - prevL) * t;
        if (out1 != nullptr) out1[i] = prevR + (curR - prevR) * t;

        srcPhase += ratio;
    }
}
