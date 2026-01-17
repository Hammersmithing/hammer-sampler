#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiKeyboardProcessor::MidiKeyboardProcessor()
    : AudioProcessor(BusesProperties())
{
    noteVelocities.fill(0);
    noteRoundRobin.fill(0);
    noteSustained.fill(false);
}

void MidiKeyboardProcessor::prepareToPlay(double, int)
{
    noteVelocities.fill(0);
    noteRoundRobin.fill(0);
    noteSustained.fill(false);
    currentRoundRobin = 1;
    sustainPedalDown = false;
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
                // Pedal released - clear all sustained notes
                for (size_t i = 0; i < 128; ++i)
                {
                    if (noteSustained[i])
                    {
                        noteVelocities[i] = 0;
                        noteRoundRobin[i] = 0;
                        noteSustained[i] = false;
                    }
                }
            }
            sustainPedalDown = pedalNowDown;
            continue;
        }

        auto noteIndex = static_cast<size_t>(message.getNoteNumber());

        if (message.isNoteOn())
        {
            noteSustained[noteIndex] = false;  // Clear sustained state on new note
            noteVelocities[noteIndex] = message.getVelocity();
            noteRoundRobin[noteIndex] = currentRoundRobin;

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
                noteRoundRobin[noteIndex] = 0;
            }
        }
    }
}

juce::AudioProcessorEditor* MidiKeyboardProcessor::createEditor()
{
    return new MidiKeyboardEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiKeyboardProcessor();
}
