#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <vector>
#include <memory>

struct Sample
{
    juce::AudioBuffer<float> buffer;
    int midiNote = 0;
    int velocity = 0;
    int roundRobin = 0;
    double sampleRate = 44100.0;
};

struct VelocityLayer
{
    int velocityValue;      // The actual velocity value from the file name
    int velocityRangeStart; // Computed: lowest velocity that triggers this layer
    int velocityRangeEnd;   // Computed: highest velocity that triggers this layer
    std::array<std::shared_ptr<Sample>, 4> roundRobinSamples; // Index 1-3 used
};

struct NoteMapping
{
    int midiNote;
    std::vector<VelocityLayer> velocityLayers; // Sorted by velocity ascending
    int fallbackNote = -1; // If this note has no samples, use this note instead
};

class SamplerEngine
{
public:
    SamplerEngine();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void loadSamplesFromFolder(const juce::File& folder);
    void noteOn(int midiNote, int velocity, int roundRobin);
    void noteOff(int midiNote);
    void processBlock(juce::AudioBuffer<float>& buffer);

    bool isLoaded() const { return !noteMappings.empty(); }
    juce::String getLoadedFolderPath() const { return loadedFolderPath; }

private:
    // Parse note name to MIDI note number (e.g., "C4" -> 60, "G#6" -> 104)
    int parseNoteName(const juce::String& noteName) const;

    // Parse file name: returns true if valid, fills out note, velocity, roundRobin
    bool parseFileName(const juce::String& fileName, int& note, int& velocity, int& roundRobin) const;

    // Build velocity ranges after all samples are loaded
    void buildVelocityRanges();

    // Build note fallbacks for missing notes
    void buildNoteFallbacks();

    // Find the sample to play for a given note/velocity/roundRobin
    std::shared_ptr<Sample> findSample(int midiNote, int velocity, int roundRobin) const;

    std::map<int, NoteMapping> noteMappings; // Key: MIDI note number
    juce::AudioFormatManager formatManager;

    // Active voices
    struct Voice
    {
        std::shared_ptr<Sample> sample;
        int position = 0;
        int midiNote = 0;
        bool active = false;
    };
    static constexpr int maxVoices = 32;
    std::array<Voice, maxVoices> voices;

    double currentSampleRate = 44100.0;
    juce::String loadedFolderPath;
};
