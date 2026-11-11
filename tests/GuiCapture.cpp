#include "PluginEditor.h"
#include <juce_graphics/juce_graphics.h>
#include <iostream>
#include <cstring>
#include <stdexcept>

int captureGui(const juce::File& output){
    BassDeamonProcessor p;
    auto layout=p.getBusesLayout();layout.inputBuses.set(1,juce::AudioChannelSet::mono());
    if(!p.setBusesLayout(layout))return 1;
    p.prepareToPlay(48000,256);juce::AudioBuffer<float> buffer(3,256);juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());editor->setSize(1200,1376);
    auto* timedGui=static_cast<BassDeamonEditor*>(editor.get());timedGui->stopTimer();double visualClock=0;
    const auto visualTick=[&]{visualClock+=1000./30;timedGui->refreshVisuals(visualClock);};
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
        if(block%16==0)visualTick();
        #if JUCE_MODAL_LOOPS_PERMITTED
        if(block%16==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(35);
        #endif
    }
    #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    #endif
    visualTick();
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
    auto* eq=p.parameters.getParameter("kickEq.frequency");
    eq->setValueNotifyingHost(eq->convertTo0to1(60));
    p.parameters.getParameter("kickEq.on")->setValueNotifyingHost(1);
    if(!save(output))return 1;
    p.parameters.getParameter("kickEq.on")->setValueNotifyingHost(0);
    auto* gui=static_cast<BassDeamonEditor*>(editor.get());
    // Freeze one real audio frame for paired pictures. Rendering a full PNG
    // may take >500 ms in Debug: do not run the no-audio expiry between shots.
    // Real button callbacks are exercised separately by interactionTests.
    const auto selectViewForCapture=[&](bool post){
        p.setSpectrumView(post?1:0);
        gui->spectrumPre.setToggleState(!post,juce::dontSendNotification);
        gui->spectrumPost.setToggleState(post,juce::dontSendNotification);
    };
    selectViewForCapture(false);
    if(!save(output.getSiblingFile("GUI-Pre.png")))return 1;
    selectViewForCapture(true);
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
    visualTick();
    if(!save(output.getSiblingFile("GUI-Sync.png")))return 1;
    // A single shared 75 Hz partial; the bass-only 150 Hz partial must not
    // acquire a red area. Capture both actual PRE and actual POST displays.
    for(int block=0;block<160;++block){for(int n=0;n<256;++n){const double t=(801*256+block*256+n)/48000.0;
        const float bass=float(.3*std::sin(2*bassdeamon::pi*75*t)+.12*std::sin(2*bassdeamon::pi*150*t));
        buffer.setSample(0,n,bass);buffer.setSample(1,n,bass);
        buffer.setSample(2,n,float(.4*std::sin(2*bassdeamon::pi*75*t)));}
        p.processBlock(buffer,midi);if(block%8==0)visualTick();}
    visualTick();
    if(!save(output.getSiblingFile("GUI-Overlap.png")))return 1;
    selectViewForCapture(false);
    if(!save(output.getSiblingFile("GUI-Overlap-Pre.png")))return 1;
    selectViewForCapture(true);
    // Freeze a genuine 20 ms kick hit at -28 dB relative to the bass, with
    // red derived exclusively from processor samples and the real analyzer.
    for(const char* id:{"tube.on","tube.audition","glue.on","phase.on","unmask.on"})
        p.parameters.getParameter(id)->setValueNotifyingHost(0);
    gui->analyzer.prepare(48000);bassdeamon::AnalysisFrame discarded;
    while(p.analysisQueue.pop(discarded)){}
    bool captured=false;
    for(int block=0;block<240&&!captured;++block){for(int n=0;n<256;++n){const double t=(block*256+n)/48000.,since=std::fmod(t,.5);
        const float bass=float(.3*std::sin(2*bassdeamon::pi*75*t)+.12*std::sin(2*bassdeamon::pi*150*t));
        buffer.setSample(0,n,bass);buffer.setSample(1,n,bass);
        const double envelope=since<.020?std::pow(std::sin(bassdeamon::pi*since/.020),2):0;
        buffer.setSample(2,n,float(.012*envelope*std::sin(2*bassdeamon::pi*75*t)));}
        p.processBlock(buffer,midi);
        if(block%6==0){visualTick();if(*std::max_element(gui->visualMaskPre.begin(),gui->visualMaskPre.end())>=.12f&&*std::max_element(gui->visualMaskPost.begin(),gui->visualMaskPost.end())>=.12f){
            selectViewForCapture(true);if(!save(output.getSiblingFile("GUI-ShortKick-Post.png")))return 1;
            selectViewForCapture(false);if(!save(output.getSiblingFile("GUI-ShortKick-Pre.png")))return 1;captured=true;
        }}
    }
    if(!captured){std::cerr<<"No real short-kick collision for capture\n";return 1;}
    // Actual processed PRE/POST comparison at the slowest selected rate.
    const auto setValue=[&](const char* id,float value){auto* parameter=p.parameters.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
    p.setAnalyzerRate(3);setValue("unmask.controlDb",1);setValue("unmask.attenuation",20);setValue("unmask.on",1);
    setValue("unmask.track",0);setValue("unmask.frequency",75);setValue("unmask.agc",0);setValue("output.trim",0);
    for(int block=0;block<360;++block){for(int n=0;n<256;++n){const float v=float(.4*std::sin(2*bassdeamon::pi*75*(block*256+n)/48000.));
        buffer.setSample(0,n,v);buffer.setSample(1,n,v);buffer.setSample(2,n,v);}
        p.processBlock(buffer,midi);if(block%6==0)visualTick();}
    if(*std::max_element(gui->visualMaskPost.begin(),gui->visualMaskPost.end())>=.12f){std::cerr<<"POST retains red after 20 dB attenuation\n";return 1;}
    selectViewForCapture(true);if(!save(output.getSiblingFile("GUI-Post-20dB.png")))return 1;
    selectViewForCapture(false);if(!save(output.getSiblingFile("GUI-Pre-20dB.png")))return 1;
    std::cout<<output.getFullPathName()<<'\n';return 0;
}

// Local QA: two stereo 48 kHz interleaved float exports, never bundled with CI.
int captureLoopGui(const juce::File& bassFile, const juce::File& kickFile, const juce::File& output) {
    const auto load=[](const juce::File& file){
        juce::MemoryBlock bytes;
        if(!file.loadFileAsData(bytes) || bytes.getSize()<8 || bytes.getSize()%8!=0)
            throw std::runtime_error("Expected a stereo interleaved float export");
        std::vector<std::array<float,2>> samples(bytes.getSize()/8);
        std::memcpy(samples.data(),bytes.getData(),bytes.getSize());
        return samples;
    };
    const auto bass=load(bassFile),kick=load(kickFile);
    BassDeamonProcessor processor;
    auto layout=processor.getBusesLayout();
    layout.inputBuses.set(1,juce::AudioChannelSet::stereo());
    if(!processor.setBusesLayout(layout))return 1;
    processor.prepareToPlay(48000,256);
    std::unique_ptr<BassDeamonEditor> editor(static_cast<BassDeamonEditor*>(processor.createEditor()));
    editor->setSize(1200,1376);
    editor->stopTimer();
    juce::AudioBuffer<float> buffer(4,256);
    juce::MidiBuffer midi;
    int pos=0;
    const auto renderBlock=[&](bool knownOffset){
        for(int n=0;n<256;++n){
            const auto k=kick[size_t(pos+n)%kick.size()];
            const auto b=knownOffset?kick[(size_t(pos+n+96))%kick.size()]:bass[size_t(pos+n)%bass.size()];
            for(int c=0;c<2;++c){
                buffer.setSample(c,n,knownOffset?-b[c]:b[c]);
                buffer.setSample(c+2,n,k[c]);
            }
        }
        processor.processBlock(buffer,midi);
        pos+=256;
    };
    processor.analysisQueue.enabled.store(false);
    editor->kickEqLearn.onClick();
    editor->learn.onClick();
    for(int block=0;block<750;++block)renderBlock(false);
    processor.timerCallback();
    const auto hz=processor.parameters.getRawParameterValue("kickEq.frequency")->load();
    std::cout<<"Actual loop KICK EQ "<<hz<<" Hz / phase status "<<processor.learnStatus.load()
             <<" / r "<<processor.learnCorrelation.load()<<'\n';
    processor.parameters.getParameter("kickEq.on")->setValueNotifyingHost(0);
    processor.parameters.getParameter("glue.on")->setValueNotifyingHost(0);
    editor->learn.onClick();
    for(int block=0;block<750;++block)renderBlock(true);
    processor.timerCallback();
    const auto delay=processor.parameters.getRawParameterValue("phase.delay")->load();
    const auto inverted=processor.parameters.getRawParameterValue("phase.invert")->load();
    std::cout<<"Actual kick with known offset: phase status "<<processor.learnStatus.load()
             <<" / delay "<<delay<<" ms / inverted "<<inverted<<'\n';
    processor.parameters.getParameter("phase.on")->setValueNotifyingHost(0);
    processor.parameters.getParameter("kickEq.on")->setValueNotifyingHost(1);
    processor.parameters.getParameter("glue.on")->setValueNotifyingHost(1);
    processor.analysisQueue.enabled.store(true);
    auto* maxParam=processor.parameters.getParameter("view.maxHz");
    maxParam->setValueNotifyingHost(maxParam->convertTo0to1(250));
    processor.setAnalyzerRate(5);
    double clock=0;
    const auto initialRevision=editor->spectrumDisplay.revision;
    for(int block=0;block<1100;++block){
        renderBlock(false);
        if(block%6==0){
            clock+=32;
            editor->refreshVisuals(clock);
        }
    }
    const auto slowFrames=editor->spectrumDisplay.revision-initialRevision;
    std::cout<<"1 fps: "<<slowFrames<<" spectrum frames in "<<clock/1000<<" s\n";
    if(hz<=0||std::abs(delay-2)>.26||inverted<.5f||slowFrames<4||slowFrames>7)return 1;
    const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
    auto stream=output.createOutputStream();
    juce::PNGImageFormat png;
    return stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&png.writeImageToStream(snapshot,*stream)?0:1;
}
