#include "SamplerEngine.h"
#include <algorithm>

SamplerEngine::SamplerEngine()
{
    formatManager.registerBasicFormats();
}

void SamplerEngine::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Clear all voices
    for (auto& voice : voices)
    {
        voice.active = false;
        voice.sample = nullptr;
        voice.position = 0;
    }
}

int SamplerEngine::parseNoteName(const juce::String& noteName) const
{
    if (noteName.isEmpty())
        return -1;

    juce::String upper = noteName.toUpperCase();
    int index = 0;

    // Parse note letter (C, D, E, F, G, A, B)
    char noteLetter = upper[0];
    int noteBase = -1;
    switch (noteLetter)
    {
        case 'C': noteBase = 0; break;
        case 'D': noteBase = 2; break;
        case 'E': noteBase = 4; break;
        case 'F': noteBase = 5; break;
        case 'G': noteBase = 7; break;
        case 'A': noteBase = 9; break;
        case 'B': noteBase = 11; break;
        default: return -1;
    }
    index++;

    // Parse accidental (# or b)
    if (index < upper.length())
    {
        if (upper[index] == '#')
        {
            noteBase++;
            index++;
        }
        else if (upper[index] == 'B' && index + 1 < upper.length() && juce::CharacterFunctions::isDigit(upper[index + 1]))
        {
            // This 'B' is actually a flat (lowercase 'b' in original)
            noteBase--;
            index++;
        }
        else if (noteName[index] == 'b' && index + 1 < noteName.length() && juce::CharacterFunctions::isDigit(noteName[index + 1]))
        {
            noteBase--;
            index++;
        }
    }

    // Parse octave number
    juce::String octaveStr = noteName.substring(index);
    if (octaveStr.isEmpty() || !octaveStr.containsOnly("0123456789-"))
        return -1;

    int octave = octaveStr.getIntValue();

    // MIDI note: C4 = 60, so C0 = 12
    int midiNote = (octave + 1) * 12 + noteBase;

    if (midiNote < 0 || midiNote > 127)
        return -1;

    return midiNote;
}

bool SamplerEngine::parseFileName(const juce::String& fileName, int& note, int& velocity, int& roundRobin) const
{
    // Expected format: NoteName_Velocity_RoundRobin[_OptionalSuffix].ext
    // Examples: C4_001_02.wav, G#6_033_01.wav, Db3_127_03_soft.wav

    juce::String baseName = fileName.upToLastOccurrenceOf(".", false, false);

    juce::StringArray parts;
    parts.addTokens(baseName, "_", "");

    if (parts.size() < 3)
        return false;

    // Parse note name (first part)
    note = parseNoteName(parts[0]);
    if (note < 0)
        return false;

    // Parse velocity (second part, 3 digits)
    juce::String velStr = parts[1];
    if (velStr.length() < 1 || !velStr.containsOnly("0123456789"))
        return false;
    velocity = velStr.getIntValue();
    if (velocity < 1 || velocity > 127)
        return false;

    // Parse round-robin (third part, 2 digits)
    juce::String rrStr = parts[2];
    if (rrStr.length() < 1 || !rrStr.containsOnly("0123456789"))
        return false;
    roundRobin = rrStr.getIntValue();
    if (roundRobin < 1 || roundRobin > 3)
        return false;

    return true;
}

void SamplerEngine::loadSamplesFromFolder(const juce::File& folder)
{
    noteMappings.clear();
    loadedFolderPath = folder.getFullPathName();

    if (!folder.isDirectory())
        return;

    // Find all audio files
    juce::Array<juce::File> audioFiles;
    folder.findChildFiles(audioFiles, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    for (const auto& file : audioFiles)
    {
        int note, velocity, roundRobin;
        if (!parseFileName(file.getFileName(), note, velocity, roundRobin))
            continue;

        // Load the audio file
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (!reader)
            continue;

        auto sample = std::make_shared<Sample>();
        sample->midiNote = note;
        sample->velocity = velocity;
        sample->roundRobin = roundRobin;
        sample->sampleRate = reader->sampleRate;

        // Read audio data
        sample->buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&sample->buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        // Add to note mappings
        auto& noteMapping = noteMappings[note];
        noteMapping.midiNote = note;

        // Find or create velocity layer
        auto it = std::find_if(noteMapping.velocityLayers.begin(), noteMapping.velocityLayers.end(),
            [velocity](const VelocityLayer& layer) { return layer.velocityValue == velocity; });

        if (it == noteMapping.velocityLayers.end())
        {
            VelocityLayer newLayer;
            newLayer.velocityValue = velocity;
            newLayer.roundRobinSamples.fill(nullptr);
            noteMapping.velocityLayers.push_back(newLayer);
            it = noteMapping.velocityLayers.end() - 1;
        }

        // Add sample to round-robin slot
        if (roundRobin >= 1 && roundRobin <= 3)
        {
            it->roundRobinSamples[static_cast<size_t>(roundRobin)] = sample;
        }
    }

    // Sort velocity layers and compute ranges
    buildVelocityRanges();

    // Build fallbacks for missing notes
    buildNoteFallbacks();
}

void SamplerEngine::buildVelocityRanges()
{
    for (auto& [note, mapping] : noteMappings)
    {
        // Sort velocity layers by velocity value ascending
        std::sort(mapping.velocityLayers.begin(), mapping.velocityLayers.end(),
            [](const VelocityLayer& a, const VelocityLayer& b) {
                return a.velocityValue < b.velocityValue;
            });

        // Compute velocity ranges
        // Each velocity covers from (previous velocity + 1) to (this velocity)
        for (size_t i = 0; i < mapping.velocityLayers.size(); ++i)
        {
            auto& layer = mapping.velocityLayers[i];

            if (i == 0)
            {
                layer.velocityRangeStart = 1;
            }
            else
            {
                layer.velocityRangeStart = mapping.velocityLayers[i - 1].velocityValue + 1;
            }

            layer.velocityRangeEnd = layer.velocityValue;
        }
    }
}

void SamplerEngine::buildNoteFallbacks()
{
    // For each possible MIDI note 0-127, find the fallback if it has no samples
    for (int note = 0; note < 128; ++note)
    {
        if (noteMappings.find(note) != noteMappings.end())
        {
            // This note has samples, no fallback needed
            noteMappings[note].fallbackNote = -1;
        }
        else
        {
            // Find the next higher note that has samples
            int fallback = -1;
            for (int higher = note + 1; higher < 128; ++higher)
            {
                if (noteMappings.find(higher) != noteMappings.end())
                {
                    fallback = higher;
                    break;
                }
            }

            // Create a placeholder mapping with the fallback
            if (fallback >= 0)
            {
                noteMappings[note].midiNote = note;
                noteMappings[note].fallbackNote = fallback;
            }
        }
    }
}

std::shared_ptr<Sample> SamplerEngine::findSample(int midiNote, int velocity, int roundRobin) const
{
    auto it = noteMappings.find(midiNote);
    if (it == noteMappings.end())
        return nullptr;

    const auto& mapping = it->second;

    // If this note has a fallback, use the fallback note's samples
    int actualNote = (mapping.fallbackNote >= 0) ? mapping.fallbackNote : midiNote;

    auto actualIt = noteMappings.find(actualNote);
    if (actualIt == noteMappings.end())
        return nullptr;

    const auto& actualMapping = actualIt->second;

    // Find the velocity layer that matches
    for (const auto& layer : actualMapping.velocityLayers)
    {
        if (velocity >= layer.velocityRangeStart && velocity <= layer.velocityRangeEnd)
        {
            // Try to get the requested round-robin
            if (roundRobin >= 1 && roundRobin <= 3)
            {
                auto sample = layer.roundRobinSamples[static_cast<size_t>(roundRobin)];
                if (sample)
                    return sample;

                // Fallback: try other round-robin positions
                for (int rr = 1; rr <= 3; ++rr)
                {
                    sample = layer.roundRobinSamples[static_cast<size_t>(rr)];
                    if (sample)
                        return sample;
                }
            }
            break;
        }
    }

    return nullptr;
}

void SamplerEngine::noteOn(int midiNote, int velocity, int roundRobin)
{
    auto sample = findSample(midiNote, velocity, roundRobin);
    if (!sample)
        return;

    // Find a free voice or steal the oldest one
    Voice* voiceToUse = nullptr;

    // First, look for an inactive voice
    for (auto& voice : voices)
    {
        if (!voice.active)
        {
            voiceToUse = &voice;
            break;
        }
    }

    // If no inactive voice, steal the one with the most progress
    if (!voiceToUse)
    {
        int maxPosition = -1;
        for (auto& voice : voices)
        {
            if (voice.position > maxPosition)
            {
                maxPosition = voice.position;
                voiceToUse = &voice;
            }
        }
    }

    if (voiceToUse)
    {
        voiceToUse->sample = sample;
        voiceToUse->position = 0;
        voiceToUse->midiNote = midiNote;
        voiceToUse->active = true;
    }
}

void SamplerEngine::noteOff(int midiNote)
{
    // For now, we let samples play to completion (no envelope)
    // Could add release envelope here later
    (void)midiNote;
}

void SamplerEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (auto& voice : voices)
    {
        if (!voice.active || !voice.sample)
            continue;

        const auto& sampleBuffer = voice.sample->buffer;
        const int sampleLength = sampleBuffer.getNumSamples();
        const int sampleChannels = sampleBuffer.getNumChannels();

        int samplesToProcess = juce::jmin(numSamples, sampleLength - voice.position);
        if (samplesToProcess <= 0)
        {
            voice.active = false;
            voice.sample = nullptr;
            continue;
        }

        // Add sample audio to output buffer
        for (int ch = 0; ch < numChannels; ++ch)
        {
            int srcCh = juce::jmin(ch, sampleChannels - 1);
            buffer.addFrom(ch, 0, sampleBuffer, srcCh, voice.position, samplesToProcess);
        }

        voice.position += samplesToProcess;

        if (voice.position >= sampleLength)
        {
            voice.active = false;
            voice.sample = nullptr;
        }
    }
}
