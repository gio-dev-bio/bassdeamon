#include "ConsoleSkin.h"
#include <stdexcept>
#include <array>
#include <utility>

// Guard the approved uniform switch mechanism by comparing the actual pixels
// rendered for different labels and control widths, including global Power.
void guiVisualContracts() {
    console::Skin skin;skin.setRaw(true);
    juce::Image cap(juce::Image::ARGB,100,100,true);
    { juce::Graphics g(cap);skin.rawKnob(g,50,50,32,2.5f,true); }
    if(cap.getPixelAt(24,24).getAlpha()!=0)
        throw std::runtime_error("Steel cap reflection escapes its circular clip");
    juce::Image reference;
    for(const auto& entry:std::array<std::pair<const char*,int>,4>{{{"POWER",83},{"ON",58},{"DELTA",65},{"AUTO GAIN",87}}}) {
        juce::ToggleButton button(entry.first);button.setSize(entry.second,27);
        button.setToggleState(true,juce::dontSendNotification);
        juce::Image image(juce::Image::ARGB,entry.second,27,true);
        juce::Graphics g(image);skin.drawToggleButton(g,button,false,false);
        // The mechanism is anchored to the right, independent of text width.
        const auto mechanism=image.getClippedImage({entry.second-22,2,21,23}).createCopy();
        if(!reference.isValid())reference=mechanism;
        else for(int y=0;y<23;++y)for(int x=0;x<21;++x)
            if(mechanism.getPixelAt(x,y)!=reference.getPixelAt(x,y))
                throw std::runtime_error("Power and module switch mechanisms differ");
    }
}
