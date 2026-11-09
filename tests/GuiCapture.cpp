#include "PluginEditor.h"
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
        // Deterministic capture also works when nested JUCE message loops are
        // disabled. Run the REAL editor analysis, not decorative test traces.
        if(block%16==0)static_cast<BassDeamonEditor*>(editor.get())->timerCallback();
        #if JUCE_MODAL_LOOPS_PERMITTED
        if(block%16==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
        #endif
    }
    #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    #endif
    static_cast<BassDeamonEditor*>(editor.get())->timerCallback();
    const auto& spectrum=static_cast<BassDeamonEditor*>(editor.get())->analyzer.pre;
    if(*std::max_element(spectrum.begin(),spectrum.end()) < -80){
        std::cerr<<"GUI capture has no real analyzer data\n";return 1;
    }
    if(static_cast<BassDeamonEditor*>(editor.get())->analyzer.scope.completed<1){std::cerr<<"Missing full quarter-note scope capture\n";return 1;}
    const auto save=[&](const juce::File& file){
        const auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);
        auto stream=file.createOutputStream();juce::PNGImageFormat format;
        if(!stream||!stream->setPosition(0)||stream->truncate().failed())return false;
        return format.writeImageToStream(image,*stream);
    };
    if(!save(output))return 1;
    auto* gui=static_cast<BassDeamonEditor*>(editor.get());
    gui->spectrumPre.onClick();
    if(!save(output.getSiblingFile("GUI-Pre.png")))return 1;
    gui->spectrumPost.onClick();
    gui->sumButton.onClick();
    if(gui->showSum||gui->sumButton.getToggleState())return 1;
    if(!save(output.getSiblingFile("GUI-SumOff.png")))return 1;
    gui->sumButton.onClick();
    for(const char* id:{"glue.timing","unmask.timing"}){
        auto* parameter=p.parameters.getParameter(id);
        parameter->setValueNotifyingHost(1);
    }
    // Metered milliseconds update in the audio callback, like the real host.
    // Feed a fresh block after changing timing so this preview shows SYNC's
    // effective values, rather than the last values from AUTO/MS mode.
    for(int n=0;n<256;++n){
        const double t=(800*256+n)/48000.0;
        const float bass=float(.32*std::sin(2*bassdeamon::pi*75*t));
        buffer.setSample(0,n,bass);buffer.setSample(1,n,bass*.95f);
        buffer.setSample(2,n,float(.7*std::exp(-std::fmod(t,.5)*22)*std::sin(2*bassdeamon::pi*60*t)));
    }
    p.processBlock(buffer,midi);
    gui->timerCallback();
    if(!save(output.getSiblingFile("GUI-Sync.png")))return 1;
    // A single shared 75 Hz partial; the bass-only 150 Hz partial must not
    // acquire a red area. Capture both actual PRE and actual POST displays.
    for(int block=0;block<160;++block){for(int n=0;n<256;++n){const double t=(801*256+block*256+n)/48000.0;
        const float bass=float(.3*std::sin(2*bassdeamon::pi*75*t)+.12*std::sin(2*bassdeamon::pi*150*t));
        buffer.setSample(0,n,bass);buffer.setSample(1,n,bass);
        buffer.setSample(2,n,float(.4*std::sin(2*bassdeamon::pi*75*t)));}
        p.processBlock(buffer,midi);if(block%8==0)gui->timerCallback();}
    gui->timerCallback();
    if(!save(output.getSiblingFile("GUI-Overlap.png")))return 1;
    gui->spectrumPre.onClick();
    if(!save(output.getSiblingFile("GUI-Overlap-Pre.png")))return 1;
    gui->spectrumPost.onClick();
    std::cout<<output.getFullPathName()<<'\n';return 0;
}
