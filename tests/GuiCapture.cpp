#include "PluginProcessor.h"
#include <juce_graphics/juce_graphics.h>
#include <iostream>

int captureGui(const juce::File& output){
    BassDeamonProcessor p;
    auto layout=p.getBusesLayout();layout.inputBuses.set(1,juce::AudioChannelSet::mono());
    if(!p.setBusesLayout(layout))return 1;
    p.prepareToPlay(48000,256);juce::AudioBuffer<float> buffer(3,256);juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());editor->setSize(1200,1180);
    auto* tube=p.parameters.getParameter("tube.on");tube->setValueNotifyingHost(1);
    // Known synthetic test signals exercise the REAL graph and metering, not decorative traces.
    for(int block=0;block<800;++block){
        for(int n=0;n<256;++n){const double t=(block*256+n)/48000.0,beat=std::fmod(t,0.5);
            const float bass=static_cast<float>(0.32*std::sin(2*bassdeamon::pi*75*t)+0.07*std::sin(2*bassdeamon::pi*150*t));
            buffer.setSample(0,n,bass);buffer.setSample(1,n,bass*0.95f);
            buffer.setSample(2,n,static_cast<float>(0.7*std::exp(-beat*22)*std::sin(2*bassdeamon::pi*60*t)));
        }
        p.processBlock(buffer,midi);
        #if JUCE_MODAL_LOOPS_PERMITTED
        if(block%16==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
        #endif
    }
    #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    #endif
    const auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);
    auto stream=output.createOutputStream();juce::PNGImageFormat format;
    if(!stream||!format.writeImageToStream(image,*stream))return 1;
    std::cout<<output.getFullPathName()<<'\n';return 0;
}
