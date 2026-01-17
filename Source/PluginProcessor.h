#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class MidiKeyboardProcessor : public juce::AudioProcessor
{
public:
    MidiKeyboardProcessor();
    ~MidiKeyboardProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // Check if a note is currently pressed
    bool isNoteOn(int midiNote) const { return noteVelocities[midiNote] > 0; }

    // Get velocity of a note (0 if not pressed)
    int getNoteVelocity(int midiNote) const { return noteVelocities[midiNote]; }

    // Check if a velocity tier is active (any held note in that range, or activated while pedal held)
    // Tier 1 = 1-42, Tier 2 = 43-84, Tier 3 = 85-127
    bool isVelocityTierActive(int tier) const
    {
        // Check if this tier was activated during pedal hold
        if (tier >= 1 && tier <= 3 && velocityTiersActivated[static_cast<size_t>(tier)])
            return true;

        // Check if any currently held note is in this tier
        for (int v : noteVelocities)
        {
            if (v == 0) continue;
            int noteTier = (v <= 42) ? 1 : (v <= 84) ? 2 : 3;
            if (noteTier == tier) return true;
        }
        return false;
    }

    // Check if a round-robin position (1, 2, or 3) is currently active
    bool isRoundRobinActive(int rrPosition) const
    {
        for (int rr : noteRoundRobin)
            if (rr == rrPosition) return true;
        return false;
    }

private:
    std::array<int, 128> noteVelocities{};
    std::array<int, 128> noteRoundRobin{};  // Which RR position each note triggered (0=none, 1-3)
    std::array<bool, 128> noteSustained{};  // Notes held by sustain pedal
    std::array<bool, 4> velocityTiersActivated{};  // Tiers activated while pedal held (index 1-3)
    int currentRoundRobin = 1;  // Next RR position to assign (cycles 1->2->3->1)
    bool sustainPedalDown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiKeyboardProcessor)
};
