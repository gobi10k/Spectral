#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SpectralVocoderAudioProcessorEditor::SpectralVocoderAudioProcessorEditor (SpectralVocoderAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Get the AudioProcessorValueTreeState from the processor
    auto& vts = audioProcessor.getValueTreeState();

    // Number of Bands Slider
    numBandsSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    numBandsSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(numBandsSlider);
    numBandsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, "numBands", numBandsSlider);
    
    numBandsLabel.setText("Bands", juce::dontSendNotification);
    numBandsLabel.attachToComponent(&numBandsSlider, false); // Attach below
    numBandsLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(numBandsLabel);

    // Attack Time Slider
    attackTimeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    attackTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    // attackTimeSlider.setRange(0.001, 1.0, 0.001); // Parameter range is defined in APVTS
    addAndMakeVisible(attackTimeSlider);
    attackTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, "attackTime", attackTimeSlider);

    attackTimeLabel.setText("Attack (ms)", juce::dontSendNotification);
    attackTimeLabel.attachToComponent(&attackTimeSlider, false);
    attackTimeLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(attackTimeLabel);

    // Release Time Slider
    releaseTimeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    releaseTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    // releaseTimeSlider.setRange(0.01, 5.0, 0.01); // Parameter range is defined in APVTS
    addAndMakeVisible(releaseTimeSlider);
    releaseTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, "releaseTime", releaseTimeSlider);
    
    releaseTimeLabel.setText("Release (ms)", juce::dontSendNotification);
    releaseTimeLabel.attachToComponent(&releaseTimeSlider, false);
    releaseTimeLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(releaseTimeLabel);

    // Spectral Display Placeholder
    addAndMakeVisible(spectralDisplayPlaceholder);

    // Set the editor size
    setSize (600, 400);
}

SpectralVocoderAudioProcessorEditor::~SpectralVocoderAudioProcessorEditor()
{
    // Attachments are cleaned up by std::unique_ptr
    // No specific cleanup needed here unless other raw pointers were used.
}

//==============================================================================
void SpectralVocoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker(0.2f));

    // Optional: Draw a title or placeholder text for the spectral display area
    // g.setColour (juce::Colours::white);
    // g.setFont (15.0f);
    // g.drawFittedText ("Spectral Vocoder", getLocalBounds().removeFromTop(20), juce::Justification::centred, 1);
}

void SpectralVocoderAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    int margin = 10;

    // Top area for title or global controls (optional)
    // auto titleArea = bounds.removeFromTop(30);

    // Area for controls (sliders) - let's put them on the left
    auto controlsWidth = 120;
    auto controlsArea = bounds.removeFromLeft(controlsWidth * 3 + margin * 2); // enough for 3 rotaries + margins
    
    // Area for spectral display
    auto spectralArea = bounds.reduced(margin); // Remaining area for spectral display

    spectralDisplayPlaceholder.setBounds(spectralArea);

    // Layout for controls using FlexBox for simplicity
    juce::FlexBox controlsBox;
    controlsBox.flexDirection = juce::FlexBox::Direction::row; // or column
    controlsBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    controlsBox.alignItems = juce::FlexBox::AlignItems::center;

    int sliderWidth = 100;
    int sliderHeight = 120; // Increased height to accommodate label below

    // Add items to FlexBox - each slider with its label (conceptually)
    // For direct bounds setting:
    auto currentControlsArea = controlsArea.reduced(margin);
    int singleControlAreaWidth = currentControlsArea.getWidth() / 3;

    auto bandsArea = currentControlsArea.removeFromLeft(singleControlAreaWidth).reduced(margin/2);
    numBandsSlider.setBounds(bandsArea.removeFromTop(sliderHeight - 20)); // Slider takes up most space
    // numBandsLabel is attached, so its position is relative to the slider.

    auto attackArea = currentControlsArea.removeFromLeft(singleControlAreaWidth).reduced(margin/2);
    attackTimeSlider.setBounds(attackArea.removeFromTop(sliderHeight - 20));
    
    auto releaseArea = currentControlsArea.removeFromLeft(singleControlAreaWidth).reduced(margin/2);
    releaseTimeSlider.setBounds(releaseArea.removeFromTop(sliderHeight - 20));

    // If using FlexBox (alternative layout approach):
    // juce::Array<juce::FlexItem> items;
    // items.add(juce::FlexItem(numBandsSlider).withWidth(sliderWidth).withHeight(sliderHeight));
    // items.add(juce::FlexItem(attackTimeSlider).withWidth(sliderWidth).withHeight(sliderHeight));
    // items.add(juce::FlexItem(releaseTimeSlider).withWidth(sliderWidth).withHeight(sliderHeight));
    // controlsBox.performLayout(controlsArea.toFloat());
}
