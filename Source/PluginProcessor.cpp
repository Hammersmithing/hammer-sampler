#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiKeyboardProcessor::MidiKeyboardProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    noteVelocities.fill(0);
    noteVelocityLayerIdx.fill(-1);
    noteRoundRobin.fill(0);
    noteSustained.fill(false);
    for (auto& arr : noteLayersActivated) arr.fill(false);
    for (auto& arr : noteRRActivated) arr.fill(false);
}

void MidiKeyboardProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    noteVelocities.fill(0);
    noteVelocityLayerIdx.fill(-1);
    noteRoundRobin.fill(0);
    noteSustained.fill(false);
    for (auto& arr : noteLayersActivated) arr.fill(false);
    for (auto& arr : noteRRActivated) arr.fill(false);
    currentRoundRobin = 1;
    sustainPedalDown = false;

    samplerEngine.prepareToPlay(sampleRate, samplesPerBlock);
}

void MidiKeyboardProcessor::loadSamplesFromFolder(const juce::File& folder)
{
    samplerEngine.loadSamplesFromFolder(folder);
}

void MidiKeyboardProcessor::setADSR(float attack, float decay, float sustain, float release)
{
    samplerEngine.setADSR(attack, decay, sustain, release);
}

void MidiKeyboardProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        // Handle sustain pedal (CC64)
        if (message.isController() && message.getControllerNumber() == 64)
        {
            bool pedalNowDown = message.getControllerValue() >= 64;

            if (!pedalNowDown && sustainPedalDown)
            {
                // Pedal released - clear all sustained notes and per-note activations
                for (size_t i = 0; i < 128; ++i)
                {
                    if (noteSustained[i])
                    {
                        noteVelocities[i] = 0;
                        noteVelocityLayerIdx[i] = -1;
                        noteRoundRobin[i] = 0;
                        noteSustained[i] = false;
                        samplerEngine.noteOff(static_cast<int>(i));
                    }
                    noteLayersActivated[i].fill(false);
                    noteRRActivated[i].fill(false);
                }
            }
            sustainPedalDown = pedalNowDown;
            continue;
        }

        auto noteIndex = static_cast<size_t>(message.getNoteNumber());
        int midiNote = message.getNoteNumber();

        if (message.isNoteOn())
        {
            noteSustained[noteIndex] = false;  // Clear sustained state on new note
            int velocity = message.getVelocity();
            noteVelocities[noteIndex] = velocity;
            noteRoundRobin[noteIndex] = currentRoundRobin;

            // Get the velocity layer index for this note/velocity combo
            int layerIdx = samplerEngine.getVelocityLayerIndex(midiNote, velocity);
            noteVelocityLayerIdx[noteIndex] = layerIdx;

            // Mark per-note velocity layer and RR as activated if pedal is down
            if (sustainPedalDown && layerIdx >= 0 && layerIdx < maxVelocityLayers)
            {
                noteLayersActivated[noteIndex][static_cast<size_t>(layerIdx)] = true;
                noteRRActivated[noteIndex][static_cast<size_t>(currentRoundRobin)] = true;
            }

            // Trigger sample playback
            samplerEngine.noteOn(midiNote, velocity, currentRoundRobin);

            // Advance round-robin: 1 -> 2 -> 3 -> 1
            currentRoundRobin = (currentRoundRobin % 3) + 1;
        }
        else if (message.isNoteOff())
        {
            if (sustainPedalDown)
            {
                // Pedal is down - sustain the note visually
                noteSustained[noteIndex] = true;
            }
            else
            {
                // No pedal - release immediately
                noteVelocities[noteIndex] = 0;
                noteVelocityLayerIdx[noteIndex] = -1;
                noteRoundRobin[noteIndex] = 0;
                samplerEngine.noteOff(midiNote);
            }
        }
    }

    // Generate audio from sampler
    samplerEngine.processBlock(buffer);
}

juce::AudioProcessorEditor* MidiKeyboardProcessor::createEditor()
{
    return new MidiKeyboardEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiKeyboardProcessor();
}
