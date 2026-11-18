#include "Dsp.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

using namespace bassdeamon;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
using Signal = std::vector<float>;
Signal tone(int count, double frequency, double amplitude, double sr = 48000) {
    Signal x(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) x[static_cast<size_t>(i)] = static_cast<float>(amplitude*std::sin(2*pi*frequency*i/sr));
    return x;
}
void render(Engine& e, Signal& x, const Signal* k, Settings s, int block = 257, double bpm = 120) {
    for (size_t i = 0; i < x.size(); i += static_cast<size_t>(block)) {
        float* b[]{x.data()+i}; const float* side[]{k ? k->data()+i : nullptr};
        e.process(b, 1, k ? side : nullptr, k ? 1 : 0, std::min(block, static_cast<int>(x.size()-i)), s, bpm);
    }
}
double rms(const Signal& x, size_t first = 0) {
    double p = 0; for (size_t i = first; i < x.size(); ++i) p += static_cast<double>(x[i])*x[i];
    return std::sqrt(p/static_cast<double>(x.size()-first));
}
double projection(const Signal& x, double frequency, size_t first, double sr = 48000) {
    double real = 0, imag = 0;
    for (size_t i = first; i < x.size(); ++i) {
        real += x[i]*std::cos(2*pi*frequency*static_cast<double>(i)/sr);
        imag += x[i]*std::sin(2*pi*frequency*static_cast<double>(i)/sr);
    }
    return 2*std::hypot(real, imag)/static_cast<double>(x.size()-first);
}
Settings bypass() { Settings s; s.glue = s.phase = s.unmask = false; return s; }

int main() {
    try {
        // Identity and kick isolation at multiple sample rates and irregular buffers.
        for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
            auto x = tone(24000, 63, 0.3, sr), original = x, kick = tone(24000, 80, 0.8, sr);
            Engine e; e.prepare(sr); render(e, x, &kick, bypass());
            require(x == original, "Bypass must be bit-exact");
            Signal zero(24000); e.prepare(sr); render(e, zero, &kick, Settings{});
            require(rms(zero) == 0, "Kick leaked into bass output");
        }
        std::cout << "PASS identity / sidechain isolation / sample rates\n";

        for (double sr : {44100.0, 48000.0}) {
            const auto kick = tone(24000, 55, 0.6, sr);
            for (bool duck : {false, true}) {
                Settings settings = bypass();
                settings.duck = duck;
                settings.unmask = !duck;
                auto instant = tone(24000, 55, 0.5, sr), normal = instant;
                Engine engine;
                engine.prepare(sr);
                render(engine, normal, &kick, settings);
                settings.duckAttackMs = settings.unmaskAttackMs = 0;
                engine.prepare(sr);
                render(engine, instant, &kick, settings);
                require(instant != normal, "Zero attack must change the applied envelope");
                require(std::isfinite(rms(instant)), "Zero attack must produce finite output");
            }
        }
        std::cout << "PASS zero attack for Duck and Unmask\n";

        // Same stream partitioned differently must produce identical DSP output.
        auto source = tone(96000, 75, 0.5), kick = tone(96000, 60, 0.4);
        for (int i = 0; i < 96000; ++i) kick[static_cast<size_t>(i)] *= (i % 24000 < 4000 ? 1.0f : 0.0f);
        auto a = source, b = source;
        Engine one, two; one.prepare(48000); two.prepare(48000);
        Settings s; s.phase = true; s.delayMs = 1.375; s.rotate = true;
        s.agc[0].enabled = s.agc[2].enabled = true;
        render(one, a, &kick, s, 1); render(two, b, &kick, s, 511);
        require(a == b, "Audio depends on host block size");
        std::cout << "PASS buffer partition invariance\n";

        {
            constexpr double sr=44100;
            constexpr int samples=44100;
            const auto dry=tone(samples,75,.3,sr);
            const auto reference=tone(samples,60,.4,sr);
            Settings chain;
            chain.kickEqEnabled=chain.phase=chain.tube.enabled=chain.duck=true;
            chain.kickEqFrequency=67;chain.kickEqCut=2;
            chain.phaseAngle=45;chain.delayMs=.85;
            chain.duckMultiband=true;chain.duckLowHz=110;chain.duckAmountDb=9;
            chain.unmaskDirect=true;chain.unmaskAttenuationDb=12;
            chain.outputDb=-9;chain.balanceTrimDb=-6;
            chain.matchEnabled=true;
            auto shortBlocks=dry,longBlocks=dry,dryAudition=dry;
            Engine shortEngine,longEngine,dryEngine;
            shortEngine.prepare(sr);longEngine.prepare(sr);dryEngine.prepare(sr);
            render(shortEngine,shortBlocks,&reference,chain,17);
            render(longEngine,longBlocks,&reference,chain,4096);
            require(shortBlocks==longBlocks,"Level Match depends on buffer partition");
            chain.auditionDry=true;
            render(dryEngine,dryAudition,&reference,chain,256);
            const auto settled=dry.begin()+int(.025*sr);
            require(std::equal(settled,dry.end(),dryAudition.begin()+int(.025*sr)),
                    "Dry audition inherited a wet chain gain");

            Settings lowBand=bypass();
            lowBand.duck=lowBand.duckMultiband=true;
            lowBand.duckLowHz=110;lowBand.duckAmountDb=18;lowBand.duckAttackMs=0;
            const auto high=tone(int(sr*.003),2200,.25,sr);
            auto initial=high, restarted=high;
            Engine low;
            low.prepare(sr);
            render(low,initial,&reference,lowBand);
            low.clearHistory();
            render(low,restarted,&reference,lowBand);
            require(db(rms(initial)/rms(high))>-.2 && initial==restarted,
                    "Low Band starts by ducking the entire spectrum");
            std::cout<<"PASS Level Match partition null, true Dry and Low Band on first hit and restart\n";
        }

        // Stereo gain control must preserve the channel ratio.
        auto left = source, right = source;
        for (auto& v : right) v *= 0.25f;
        Engine linked; linked.prepare(48000); s = {}; s.phase = false;
        for (int i = 0; i < 96000; i += 128) {
            float* bass[]{left.data()+i, right.data()+i}; const float* side[]{kick.data()+i};
            linked.process(bass, 2, side, 1, std::min(128, 96000-i), s);
        }
        for (size_t i = 0; i < left.size(); ++i)
            require(std::abs(right[i]-left[i]*0.25f) < 1e-6f, "Stereo image changed");
        std::cout << "PASS stereo linking\n";

        // Feed-forward GLUE must compress without a kick reference.
        auto compressed = tone(144000, 70, 0.5);
        const double inputRms = rms(compressed, 96000);
        Engine compressor; compressor.prepare(48000); s = bypass(); s.glue = true; s.glueAmount = 1;
        render(compressor, compressed, nullptr, s, 256, 0);
        require(rms(compressed, 96000) < inputRms * 0.95, "GLUE did not compress");
        std::cout << "PASS GLUE without sidechain/BPM\n";
        const auto held = compressor.runtimeState();
        auto changed = tone(96000, 150, 0.01); s.glueLocked = true;
        render(compressor, changed, nullptr, s);
        const auto stillHeld = compressor.runtimeState();
        for (size_t i = 0; i < 4; ++i) require(held[i] == stillHeld[i], "GLUE lock adapted its parameters");
        Engine recalled; recalled.prepare(48000); recalled.restoreRuntime(held);
        require(recalled.runtimeState() == held, "Frozen runtime recall failed");
        std::cout << "PASS GLUE adaptation lock / runtime recall\n";

        // Independent AGC lock, quiet-signal suspension and 3 dB cap.
        AutoGain agc; agc.prepare(48000, 100);
        for (int i = 0; i < 100000; ++i) agc.tick(0.1, 0.01, {true, false}, 6);
        const double frozen = agc.appliedDb();
        require(frozen > 2.9 && frozen <= 3.0, "AGC boost limit failed");
        for (int i = 0; i < 100000; ++i) agc.tick(0.1, 1, {true, true}, 6);
        require(agc.appliedDb() == frozen, "AGC lock changed gain");
        for (int i = 0; i < 100000; ++i) agc.tick(0, 0, {true, false}, 6);
        require(agc.appliedDb() == frozen, "AGC learned silence");
        for (int i = 0; i < 48000; ++i) agc.tick(0.1, 0.01, {false, false}, 6);
        require(agc.tick(0.1, 0.01, {false, false}, 6) == 1, "AGC bypass not unity after transition");
        std::cout << "PASS AGC lock / silence / bounds\n";

        // Spectral attenuation should affect overlapping low end, leaving 1 kHz intact.
        auto multi = tone(144000, 75, 0.3), high = tone(144000, 1000, 0.1);
        auto side = tone(144000, 75, 0.5);
        for (size_t i = 0; i < multi.size(); ++i) multi[i] += high[i];
        const auto dry = multi;
        Engine unmask; unmask.prepare(48000); s = bypass(); s.unmask = true; s.unmaskAmount = 1;
        render(unmask, multi, &side, s);
        const double lowRatio = projection(multi,75,96000)/projection(dry,75,96000);
        const double highRatio = projection(multi,1000,96000)/projection(dry,1000,96000);
        require(lowRatio < 0.8, "UNMASK failed to attenuate conflict");
        require(highRatio > 0.98, "UNMASK attenuated unrelated high band");
        auto noKick = dry; unmask.prepare(48000); render(unmask, noKick, nullptr, s);
        require(noKick == dry, "UNMASK without kick must be unity");
        std::cout << "PASS UNMASK selectivity / missing sidechain\n";

        // Focus follows overlapping resonance, HOLD stops frequency movement while cuts stay dynamic.
        auto trackingBass = tone(192000, 186, 0.35), trackingKick = tone(192000, 186, 0.5);
        Engine tracking; tracking.prepare(48000); Settings trackingSettings = bypass();
        trackingSettings.unmask = true; trackingSettings.unmaskAmount = 1;
        render(tracking, trackingBass, &trackingKick, trackingSettings);
        const double heldFocus = tracking.getMeters().focusHz;
        require(heldFocus > 140, "Automatic focus did not follow the overlap");
        trackingSettings.unmaskHold = true;
        trackingBass = tone(96000, 38, 0.35); trackingKick = tone(96000, 38, 0.5);
        render(tracking, trackingBass, &trackingKick, trackingSettings);
        require(tracking.getMeters().focusHz == heldFocus, "HOLD focus kept moving");
        Engine recalledFocus; recalledFocus.prepare(48000); recalledFocus.restoreRuntime(tracking.runtimeState());
        require(recalledFocus.runtimeState()[7] == heldFocus, "HOLD focus was not persisted");
        std::cout << "PASS automatic focus / HOLD / recall\n";

        // A narrow manual focus must leave an unrelated upper-bass note relatively intact.
        auto manual = tone(144000, 48, 0.3), upper = tone(144000, 186, 0.3), manualKick = manual;
        for (size_t i = 0; i < manual.size(); ++i) { manual[i] += upper[i]; manualKick[i] += upper[i]; }
        const auto manualDry = manual;
        Engine manualEngine; manualEngine.prepare(48000); Settings manualSettings = bypass();
        manualSettings.unmask = true; manualSettings.unmaskAmount = 1; manualSettings.unmaskTrack = false;
        manualSettings.unmaskFrequency = 48; manualSettings.unmaskWidth = 0;
        render(manualEngine, manual, &manualKick, manualSettings);
        require(projection(manual,48,96000)/projection(manualDry,48,96000) < 0.7, "Manual focus did not cut the selected region");
        require(projection(manual,186,96000)/projection(manualDry,186,96000) > 0.9, "Manual focus cut an unrelated upper-bass note");
        std::cout << "PASS manual focus selectivity\n";

        // Delta is removed bass, before module AGC and trim.
        auto delta = dry; Engine diff; diff.prepare(48000); s.delta = true; s.outputDb = 12;
        render(diff, delta, &side, s);
        for (size_t i = 0; i < dry.size(); ++i)
            require(std::abs(delta[i]+multi[i]-dry[i]) < 1e-6f, "Delta reconstruction failed");
        std::cout << "PASS delta reconstruction\n";

        // Causal delay and all-pass gain at steady state.
        Engine phase; phase.prepare(48000); auto impulse = Signal(1000, 0); impulse[0] = 1;
        s = bypass(); s.phase = true; s.delayMs = 2; s.invert = true;
        render(phase, impulse, nullptr, s);
        require(impulse[96] == -1 && std::abs(rms(impulse)*std::sqrt(1000)-1) < 1e-12, "PHASE delay/polarity failed");
        auto rotated = tone(96000, 80, 0.3); phase.prepare(48000); s.delayMs = 0; s.rotate = true;
        render(phase, rotated, nullptr, s);
        require(std::abs(rms(rotated,48000)-0.3/std::sqrt(2)) < 1e-5, "All-pass magnitude not unity");
        for (double angle : {-180.,-90.,0.,45.,90.,180.}) {
            Settings dial;
            dial.glue=dial.unmask=false;
            dial.phase=true;dial.phaseAngle=angle;dial.phaseReference=60;
            Signal shifted(96000);
            for (size_t n=0;n<shifted.size();++n) shifted[n]=float(.3*std::sin(2*pi*60*n/48000.));
            Engine phaseDial;phaseDial.prepare(48000);render(phaseDial,shifted,nullptr,dial);
            double i=0,q=0;
            for (size_t n=48000;n<shifted.size();++n) {
                const auto rad=2*pi*60*n/48000.;
                i+=shifted[n]*std::sin(rad);q+=shifted[n]*std::cos(rad);
            }
            require(std::abs(std::remainder(std::atan2(q,i)*180/pi-angle,360.))<.1,
                    "Phase dial angle differs from its reference");
            require(std::abs(std::hypot(i,q)*2/48000-.3)<1e-5,"Phase dial did not preserve sine amplitude");
        }
        std::cout << "PASS PHASE delay / polarity / all-pass response\n";

        // Learn searches a phrase and then stops: no dynamic tracking.
        PhaseLearner learner; learner.prepare(48000); learner.start();
        Signal noise(192000); std::mt19937 random(42); std::uniform_real_distribution<float> distribution(-0.5f,0.5f);
        for (auto& v : noise) v = distribution(random);
        for (size_t i = 0; i < noise.size(); ++i)
            learner.tick(noise[i], i >= 96 ? -noise[i-96] : 0);
        const auto learned = learner.getResult();
        require(!learner.learning() && learned.valid && learned.invert, "Learn did not lock polarity");
        require(std::abs(learned.delayMs-2) <= 0.25, "Learn did not recover known delay");
        for (int i = 0; i < 48000; ++i) learner.tick(0, 0);
        require(learner.getResult().delayMs == learned.delayMs, "Locked phase kept tracking");
        learner.start(); for (int i = 0; i < 192000; ++i) learner.tick(0, 0);
        require(!learner.getResult().valid, "Learn accepted silence");
        std::cout << "PASS PHASE Learn + Lock / silence rejection\n";

        // Alignment is accepted only when distinct and stable; DC and periodic ambiguities reject.
        const auto reject=[&](int kind){
            learner.start();std::mt19937 unrelated(937);std::uniform_real_distribution<float> rnd(-.5f,.5f);
            for(size_t i=0;i<noise.size();++i){
                double b=noise[i],k=rnd(unrelated);
                if(kind==1)b=k=.25;
                if(kind==2)b=k=.25*std::sin(2*pi*160*i/48000);
                if(kind==3){const size_t d=i<96000?24:192;k=i>=d?noise[i-d]:0;}
                learner.tick(b,k);
            }
            require(!learner.getResult().valid,"Unreliable phase match was accepted");
        };
        for(int kind=0;kind<4;++kind)reject(kind);
        std::cout<<"PASS Learn rejects unrelated / DC / ambiguous / changing lag\n";

        // Global bypass preserves bits after a finite ramp, including -0 and subnormal floats.
        Engine master;master.prepare(48000);s={};s.phase=s.rotate=s.invert=true;s.delayMs=4;s.outputDb=6;
        for(auto& cfg:s.agc)cfg.enabled=true;
        auto warm=tone(4096,80,.25);render(master,warm,nullptr,s);
        s.active=false;Signal fade(960,0.125f);render(master,fade,nullptr,s,17);
        Signal exact{0.0f,-0.0f,1e-39f,-1e-39f,.25f,-.5f};auto original=exact;
        render(master,exact,nullptr,s,1);
        require(std::memcmp(exact.data(),original.data(),exact.size()*sizeof(float))==0,"Global bypass endpoint is not bit exact");
        s.active=true;Signal silentResume(2048,0);render(master,silentResume,nullptr,s,17);
        for(float v:silentResume)require(v==0,"Stale wet history leaked after global bypass");
        auto resume=tone(4096,80,.25);render(master,resume,nullptr,s,17);
        for(float v:resume)require(std::isfinite(v),"Nonfinite global resume");
        std::cout<<"PASS finite global bypass / exact IEEE endpoint / resume\n";

        // Delay changes blend fixed taps, rather than sweeping the pitch of a moving tap.
        Engine taps;taps.prepare(48000);s=bypass();s.phase=true;
        Signal before(noise.begin(),noise.begin()+4096);render(taps,before,nullptr,s,64);
        Signal after(noise.begin()+4096,noise.begin()+8192);s.delayMs=2;render(taps,after,nullptr,s,63);
        for(size_t n=0;n<after.size();++n){
            const double blend=std::min(1.0,static_cast<double>(n+1)/960);
            const double expected=noise[4096+n]+blend*(static_cast<double>(noise[4096+n-96])-noise[4096+n]);
            require(std::abs(after[n]-expected)<1e-7,"Fixed-tap delay crossfade mismatch");
        }
        std::cout<<"PASS fixed-tap PHASE delay crossfade\n";

        // A short high-frequency transient followed by a low kick body,
        // over a bassline with 125 ms notes at 120 BPM.
        {
            constexpr double sr=48000.;
            Signal bass(48000), kick(bass.size());
            for (size_t n=0;n<bass.size();++n) {
                const double t=n/sr;
                const double hit=std::fmod(t,.5);
                const double pitch=(n/6000)%2==0?55.:65.4;
                bass[n]=float(.3*std::sin(2*pi*pitch*t));
                if (hit<.055)
                    kick[n]=float(.75*std::exp(-hit*65)*std::sin(2*pi*55*hit)
                        +.35*std::exp(-hit*850)*std::sin(2*pi*2100*hit));
            }
            Settings duck;
            duck.glue=duck.unmask=false;
            Engine baseline, idle, active, otherBlocks, silentSidechain;
            for (auto* e : {&baseline,&idle,&active,&otherBlocks,&silentSidechain}) e->prepare(sr);
            auto dry=bass, noSide=bass, wet=bass, repartitioned=bass, noKick=bass;
            const Signal silence(kick.size());
            render(baseline,dry,&kick,duck);
            duck.duck=true;
            render(idle,noSide,nullptr,duck);
            render(active,wet,&kick,duck);
            render(otherBlocks,repartitioned,&kick,duck,63);
            render(silentSidechain,noKick,&silence,duck);
            require(wet==repartitioned,"Duck depends on host block size");
            require(noKick==bass,"Silent sidechain changed the bass");
            require(std::memcmp(bass.data(),dry.data(),bass.size()*sizeof(float))==0,"Duck OFF changed the audio");
            require(std::memcmp(bass.data(),noSide.data(),bass.size()*sizeof(float))==0,"Duck attenuated with no kick bus");
            const auto reduction=[&](int first,int end) {
                double a=0,b=0;
                for(int n=first;n<end;++n){a+=dry[size_t(n)]*dry[size_t(n)];b+=wet[size_t(n)]*wet[size_t(n)];}
                return 10*std::log10(a/b);
            };
            const double transient=reduction(48,288), body=reduction(480,1680), next=reduction(6000,7440);
            require(transient>1.5 && body>2.4 && body<=3.001 && next<.8,"Duck failed the transient/body/recovery checks");
            duck.duckMultiband=true;
            auto banded=bass, bandedOther=bass, bandedSilent=bass;
            Engine bands,otherBands,bandsSilent;
            for (auto* engine : {&bands,&otherBands,&bandsSilent}) engine->prepare(sr);
            render(bands,banded,&kick,duck);
            render(otherBands,bandedOther,&kick,duck,63);
            render(bandsSilent,bandedSilent,&silence,duck);
            require(banded==bandedOther && bandedSilent==bass,"Multiband Duck bypass or block invariance failed");
            require(banded!=bass && banded!=wet,"Multiband Duck did not produce its own response");
            std::cout<<"PASS Duck transient "<<transient<<" dB, kick body "<<body
                     <<" dB, next note "<<next<<" dB / exact OFF and no-sidechain bypass\n";
        }

        {
            Settings settings;
            settings.glue=settings.unmask=false;
            settings.tube.enabled=true;
            settings.tube.driveDb=14;
            settings.tube.amount=.8;
            Signal dry(8192);
            for (size_t n=0;n<dry.size();++n) dry[n]=float(.65*std::sin(2*pi*110*n/48000.));
            auto two=dry,four=dry;
            Engine a,b;
            a.prepare(48000);b.prepare(48000);
            render(a,four,nullptr,settings);
            settings.tube.oversampling=2;
            render(b,two,nullptr,settings);
            require(two!=four && two!=dry && four!=dry,"Oversampling modes did not affect the saturated signal");
            render(a,four,nullptr,settings,63);
            require(std::all_of(four.begin(),four.end(),[](float v){return std::isfinite(v);}),
                    "Changing oversampling generated invalid samples");
        }


        // Extreme legal settings stay finite at all supported rates.
        for (double sr : {8000.0, 44100.0, 96000.0, 192000.0}) {
            Engine e; e.prepare(sr); auto x = tone(20000, 40, 0.8, sr), k = tone(20000, 60, 0.8, sr);
            s = {}; s.phase = s.rotate = s.invert = true; s.delayMs = 5; s.rotationHz = 500;
            s.glueAmount = s.unmaskAmount = 1;
            for (auto& cfg : s.agc) cfg.enabled = true;
            render(e, x, &k, s, 63);
            for (float v : x) require(std::isfinite(v) && std::abs(v) < 10, "Unstable DSP");
        }
        std::cout << "PASS extreme settings / stability\nAll DSP contracts passed.\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return EXIT_FAILURE; }
}
