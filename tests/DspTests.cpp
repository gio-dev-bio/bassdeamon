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

        for (double rate : {44100.,48000.,96000.}) {
            auto settings=bypass();
            settings.duck=true;
            settings.duckAmountDb=18;
            settings.duckAttackMs=0;
            const auto expectedGain=gain(-settings.duckAmountDb);
            auto dry=tone(int(rate*.1),55,.42,rate);
            auto quietKick=tone(int(dry.size()),55,.02,rate);
            const auto upper=tone(int(dry.size()),2200,.15,rate);
            for (size_t n=0;n<dry.size();++n) {
                dry[n]+=upper[n];
                quietKick[n]*=float(std::exp(-double(n)/(rate*.035)));
            }
            Engine duck;
            duck.prepare(rate);
            auto wet=dry;
            render(duck,wet,&quietKick,settings,63);
            for (size_t n=size_t(rate*.002);n<size_t(rate*.012);++n)
                require(std::abs(wet[n]-dry[n]*expectedGain)<1e-6,
                        "A quiet kick did not reach Full Band Duck depth at the onset");
            duck.prepare(rate);
            const auto belowGate=tone(int(dry.size()),55,.003,rate);
            wet=dry;
            render(duck,wet,&belowGate,settings);
            require(wet==dry,"Duck attenuated a sidechain below its noise gate");

            duck.prepare(rate);
            float bass[]{.5f,.5f,.5f};
            const float left[]{0,0,0},right[]{0,.02f,0};
            float* output[]{bass};
            const float* sidechain[]{left,right};
            duck.process(output,1,sidechain,2,3,settings);
            require(bass[0]==.5f&&std::abs(bass[1]-.5*expectedGain)<1e-6,
                    "Zero attack did not follow the first detected sample of a quiet stereo key");
        }
        std::cout << "PASS Full Band Duck reaches its set depth on quiet keys and ignores sub-gate signals\n";

        {
            constexpr double rate=44100;
            auto settings=bypass();
            settings.duck=settings.duckMultiband=true;
            settings.duckLowHz=110;
            settings.duckAmountDb=18;
            settings.duckAttackMs=0;
            const auto response=[&](double frequency,double kickLevel) {
                auto bass=tone(int(rate),frequency,.42,rate);
                const auto kick=tone(int(rate),55,kickLevel,rate);
                const auto input=rms(bass,size_t(rate/2));
                Engine duck;
                duck.prepare(rate);
                render(duck,bass,&kick,settings,63);
                require(std::abs(duck.getMeters().duckReductionDb-18)<.01,
                        "Low Band Duck did not reach its set depth with this sidechain");
                return db(rms(bass,size_t(rate/2))/input);
            };
            const auto weakKeySub=response(55,.018);
            require(weakKeySub<-15.5 && std::abs(weakKeySub-response(55,.4))<.02,
                    "Low Band Duck response changed with sidechain level");
            require(std::abs(response(110,.018)+9)<.1,
                    "Low Band Duck did not follow its selected crossover");
            require(std::abs(response(2200,.018))<.02,
                    "A quiet key made Low Band Duck attenuate the upper bass");
            auto bass=tone(int(rate/4),55,.42,rate);
            const auto kick=tone(int(bass.size()),55,.018,rate);
            auto wet=bass,repartitioned=bass,silentKey=bass;
            const Signal silence(bass.size());
            Engine duck;
            duck.prepare(rate);
            render(duck,wet,&kick,settings,63);
            duck.prepare(rate);
            render(duck,repartitioned,&kick,settings,1024);
            duck.prepare(rate);
            render(duck,silentKey,&silence,settings,63);
            require(wet==repartitioned && silentKey==bass,
                    "Low Band Duck bypass or buffer partition changed the output");
            std::cout << "PASS Low Band Duck is equally selective with quiet and strong keys\n";
        }

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

            chain.matchEnabled=chain.auditionDry=false;
            chain.delayMs=5;
            constexpr int offStart=22050, onAgain=23400;
            const auto withPause=[&](int block) {
                Engine engine;
                engine.prepare(sr);
                auto output=dry;
                for (int n=0;n<samples;) {
                    chain.active=n<offStart||n>=onAgain;
                    const int edge=n<offStart?offStart:n<onAgain?onAgain:samples;
                    const int count=std::min(block,edge-n);
                    float* bass[]{output.data()+n};
                    const float* side[]{reference.data()+n};
                    engine.process(bass,1,side,1,count,chain,120);
                    n+=count;
                }
                return output;
            };
            require(withPause(64)==withPause(4096),"Master bypass keeps different history with large buffers");
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

        // A strong bass must not be over-compressed while the automatic threshold
        // starts up, or after silence or a transport reset.
        for (double sr : {44100., 48000., 96000.}) {
            const auto phrase = tone(static_cast<int>(sr), 55, 0.8, sr);
            auto swell = phrase;
            for (size_t i = 0; i < swell.size(); ++i) {
                const double progress = std::min(1., double(i) / (0.5 * sr));
                swell[i] *= static_cast<float>(0.5 - 0.5 * std::cos(pi * progress));
            }
            for (int timing : {0, 1, 2}) {
                Settings startup = bypass();
                startup.glue = true;
                startup.glueAmount = 1;
                startup.glueTiming = timing;
                startup.glueAttackMs = 0.5;
                startup.glueReleaseMs = 70;
                Engine glue;
                glue.prepare(sr);
                const auto checkPhrase = [&](Engine& engine, int block, const Signal& input, double peakLimit) {
                    auto wet = input;
                    render(engine, wet, nullptr, startup, block);
                    double peakReduction = 0;
                    for (size_t i = 0; i < wet.size(); ++i) {
                        if (std::abs(input[i]) > 1e-6)
                            peakReduction = std::max(peakReduction, -db(std::abs(double(wet[i]) / input[i])));
                    }
                    require(peakReduction < peakLimit, "GLUE crushed the bass onset");
                    const auto steadyStart = wet.size() * 3 / 4;
                    require(-db(rms(wet, steadyStart) / rms(input, steadyStart)) > 1,
                            "GLUE lost sustained compression");
                    return wet;
                };
                const auto start = checkPhrase(glue, 257, phrase, 4);
                Engine alternate;
                alternate.prepare(sr);
                require(start == checkPhrase(alternate, 1024, phrase, 4), "GLUE acquisition depends on host buffer size");
                const double learned = glue.runtimeState()[0];
                auto tail = tone(static_cast<int>(sr * 0.1), 67, 1e-5, sr);
                render(glue, tail, nullptr, startup, 63);
                const double heldThreshold = glue.runtimeState()[0];
                require(std::abs(heldThreshold - learned) < 0.75, "GLUE lost its level reference when the bass stopped");
                auto nearSilence = tone(static_cast<int>(sr * 2.9), 67, 1e-5, sr);
                render(glue, nearSilence, nullptr, startup, 63);
                require(glue.runtimeState()[0] == heldThreshold, "GLUE threshold followed silence");
                checkPhrase(glue, 512, phrase, 4);

                auto previous = glue.runtimeState();
                previous[0] = -45;
                glue.restoreRuntime(previous);
                glue.clearHistory();
                checkPhrase(glue, 121, phrase, 4);

                glue.prepare(sr);
                checkPhrase(glue, 257, swell, 6);
            }
        }
        {
            Engine glue;
            glue.prepare(44100);
            auto frozen = glue.runtimeState();
            frozen[0] = -36;
            frozen[1] = 3.5;
            glue.restoreRuntime(frozen);
            glue.clearHistory();
            Settings locked = bypass();
            locked.glue = locked.glueLocked = true;
            locked.glueAmount = 1;
            auto bass = tone(44100, 55, 0.8, 44100);
            render(glue, bass, nullptr, locked);
            const auto current = glue.runtimeState();
            require(std::equal(frozen.begin(), frozen.begin() + 4, current.begin()),
                    "GLUE adapted its locked parameters after a reset");
            require(glue.getMeters().glueReductionDb > 12, "GLUE limited compression from a locked threshold");

            locked.glue = false;
            Signal transition(612, 0.5f);
            render(glue, transition, nullptr, locked);
            require(std::abs(glue.getMeters().glueReductionDb + db(transition.back() / 0.5)) < 1e-5,
                    "GLUE meter differs from actual gain during its wet/dry transition");
        }
        std::cout << "PASS GLUE onset / fade-in / silence / transport reset / locked threshold / meter\n";

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

        // A learned fundamental lies between the detector bands. A hotter
        // harmonic on both inputs must not pull Auto Focus off that frequency.
        for (double sr : {44100., 48000., 96000.}) {
            constexpr double fundamental = 53;
            const int length = static_cast<int>(sr * 2);
            const auto first = tone(length, fundamental, .15, sr);
            const auto second = tone(length, fundamental * 2, .46, sr);
            const auto third = tone(length, fundamental * 3, .26, sr);
            auto bass = first;
            auto kick = tone(length, fundamental, .10, sr);
            const auto loudHarmonic = tone(length, fundamental * 2, .38, sr);
            for (int n = 0; n < length; ++n) {
                bass[n] += second[n] + third[n];
                kick[n] += loudHarmonic[n];
            }
            Settings focused = bypass();
            focused.unmask = focused.unmaskDirect = true;
            focused.kickReferenceHz = fundamental;
            focused.unmaskWidth = 1;
            focused.unmaskAttenuationDb = 24;
            Engine detector;
            detector.prepare(sr);
            auto wet = bass;
            render(detector, wet, &kick, focused, 127);
            const auto change = [&](double frequency) {
                const auto steady = static_cast<size_t>(sr);
                return db(projection(wet, frequency, steady, sr) / projection(bass, frequency, steady, sr));
            };
            require(std::abs(detector.getMeters().focusHz - fundamental) < .1, "Unmask rounded the learned fundamental or chased a harmonic");
            require(std::abs(change(fundamental) + 24) < .25, "Unmask did not deliver the requested fundamental cut");
            require(std::abs(change(fundamental * 2) + 4) < .12 && change(fundamental * 3) > -.6,
                    "Unmask did not isolate the severe upper overlap");
            const auto response = detector.unmaskResponse(focused, 1);
            require(std::abs(response.gainDb(fundamental, sr) - change(fundamental)) < .25 &&
                    std::abs(response.gainDb(fundamental * 2, sr) - change(fundamental * 2)) < .08,
                    "Unmask spectrum curve differs from the actual focused cut");

            auto mildBass = tone(length, fundamental, .42, sr);
            auto mildKick = tone(length, fundamental, .45, sr);
            const auto mildBassHarmonic = tone(length, fundamental * 2, .14, sr);
            const auto mildKickHarmonic = tone(length, fundamental * 2, .13, sr);
            for (int n = 0; n < length; ++n) {
                mildBass[n] += mildBassHarmonic[n];
                mildKick[n] += mildKickHarmonic[n];
            }
            detector.prepare(sr);
            auto mildWet = mildBass;
            render(detector, mildWet, &mildKick, focused, 1024);
            const auto measureSecond = [&](const Signal& signal) {
                return projection(signal, fundamental * 2, static_cast<size_t>(sr), sr);
            };
            require(std::abs(db(measureSecond(mildWet) / measureSecond(mildBass)) + 1.25) < .12,
                    "Ordinary shared harmonic exceeded the light cut");

            detector.prepare(sr);
            auto harmonicOnly = second;
            render(detector, harmonicOnly, &kick, focused, 257);
            require(detector.getMeters().focusHz > fundamental * 1.45, "Harmonic leakage counted as fundamental overlap");
            require(detector.getMeters().unmaskReductionDb > .5 && detector.getMeters().unmaskReductionDb <= 4.001,
                    "Auto Focus did not limit the residual harmonic cut");

            detector.prepare(sr);
            const auto kickWithoutHarmonic = tone(length, fundamental, .45, sr);
            harmonicOnly = second;
            render(detector, harmonicOnly, &kickWithoutHarmonic, focused, 257);
            require(std::abs(db(measureSecond(harmonicOnly) / measureSecond(second))) < .05,
                    "Unmask mistook a bass-only harmonic for overlap");

            detector.prepare(sr);
            focused.kickReferenceHz = 0;
            wet = bass;
            render(detector, wet, &kick, focused);
            require(detector.getMeters().unmaskReductionDb <= 6.001, "Unverified focus received the full automatic cut");
        }
        {
            constexpr int sr = 44100;
            const auto bass = tone(sr * 2, 53, .10, sr);
            const auto kick = tone(sr * 2, 53, .40, sr);
            Settings focused = bypass();
            focused.duck = focused.unmask = focused.unmaskDirect = true;
            focused.duckAmountDb = 18;
            focused.duckAttackMs = 0;
            focused.kickReferenceHz = 53;
            focused.unmaskAttenuationDb = 24;
            Engine duck, combined;
            duck.prepare(sr);
            combined.prepare(sr);
            auto wet = bass, duckOnly = bass;
            render(combined, wet, &kick, focused);
            focused.unmask = false;
            render(duck, duckOnly, &kick, focused);
            require(std::abs(db(rms(wet, sr) / rms(duckOnly, sr))) < .05, "Unmask recut a fundamental already cleared by Duck");
        }
        std::cout << "PASS Unmask exact fundamental / light and severe harmonic overlap / post-Duck overlap\n";

        {
            constexpr int sr = 44100;
            const auto bass = tone(sr, 55, .42, sr);
            const auto quietKick = tone(sr, 55, .018, sr);
            const auto belowFloor = tone(sr, 55, .0004, sr);
            auto focused = bypass();
            focused.unmask = focused.unmaskDirect = true;
            focused.unmaskTrack = false;
            focused.unmaskFrequency = 55;
            focused.unmaskAttenuationDb = 24;
            focused.unmaskAttackMs = 0;
            Engine detector;
            detector.prepare(sr);
            auto wet = bass;
            render(detector, wet, &quietKick, focused, 63);
            require(db(rms(wet, sr/2)/rms(bass, sr/2)) < -23.5,
                    "Manual Unmask lost depth with a quiet sidechain");
            detector.prepare(sr);
            auto repartitioned = bass;
            render(detector, repartitioned, &quietKick, focused, 1024);
            require(wet == repartitioned, "Manual Unmask depends on host buffer size");
            detector.prepare(sr);
            wet = bass;
            render(detector, wet, &belowFloor, focused);
            require(wet == bass, "Manual Unmask attenuated a key below its noise floor");
        }

        // A short kick must produce a stronger cut only for a genuinely
        // competing bass. Its detector tail should let the note recover.
        for (double sr : {44100., 48000., 96000.}) {
            struct HitResponse { double peakDb = 0, tailDb = 0; int peakMs = 0; };
            constexpr double hz = 53;
            const int blockSize = static_cast<int>(sr / 100);
            const auto measureKick = [&](double bassLevelDb, double kickLevel = .42) {
                Engine detector;
                detector.prepare(sr);
                Settings focused = bypass();
                focused.unmask = focused.unmaskDirect = true;
                focused.kickReferenceHz = hz;
                focused.unmaskAttenuationDb = 6;
                Signal bass(blockSize), kick(blockSize);
                float* output[]{bass.data()};
                const float* sidechain[]{kick.data()};
                const double bassAmplitude = .42 * gain(bassLevelDb);
                HitResponse result;
                for (int block = 0; block < 250; ++block) {
                    for (int n = 0; n < blockSize; ++n) {
                        const double time = (block * blockSize + n) / sr;
                        const double age = std::fmod(time, .5);
                        bass[n] = float(bassAmplitude * std::sin(2 * pi * hz * time));
                        kick[n] = age < .08 ? float(kickLevel * std::exp(-age / .035) * std::sin(2 * pi * hz * age)) : 0;
                    }
                    detector.process(output, 1, sidechain, 1, blockSize, focused, 120);
                    if (block < 200) continue;
                    const int timeMs = (block - 199) * 10;
                    const double cut = detector.getMeters().unmaskReductionDb;
                    if (cut > result.peakDb) {
                        result.peakDb = cut;
                        result.peakMs = timeMs;
                    }
                    if (timeMs == 260) result.tailDb = cut;
                }
                return result;
            };
            const auto quiet = measureKick(-18);
            const auto strong = measureKick(0);
            const auto quietKey = measureKick(0, .02);
            require(quiet.peakDb > .1 && quiet.peakDb < 1.6 && strong.peakDb > 4,
                    "Auto Focus applied the same depth to quiet and competing bass");
            require(quiet.peakMs <= 80 && strong.peakMs <= 80 && quiet.tailDb < .35 && strong.tailDb < 1.5,
                    "Unmask deepened or held the cut after the kick");
            require(quietKey.peakDb > 5 && quietKey.peakMs <= 80 && quietKey.tailDb < 1.5,
                    "Auto Focus missed the fundamental of a quiet kick or held the cut into the next note");
        }
        std::cout << "PASS Unmask follows kick body and releases with depth proportional to overlap\n";

        // A recalled AGC amount must fade out when its module ceases to cut.
        for (int stop : {0, 1, 2}) {
            constexpr int sr = 44100;
            Engine makeup;
            makeup.prepare(sr);
            auto recalled = makeup.runtimeState();
            recalled[6] = 2.75;
            makeup.restoreRuntime(recalled);
            Settings focused = bypass();
            focused.unmask = focused.unmaskDirect = true;
            focused.unmaskTrack = false;
            focused.unmaskFrequency = 75;
            focused.unmaskAttenuationDb = 12;
            focused.agc[2] = {true, true};
            auto bass = tone(sr, 75, .4, sr);
            const auto activeKick = bass;
            render(makeup, bass, &activeKick, focused);
            require(makeup.getMeters().agcDb[2] > 2.7, "Unmask AGC did not apply the recalled compensation");
            if (stop == 2) focused.unmaskAttenuationDb = 0;
            const auto reference = tone(sr * 2, 75, .4, sr);
            const Signal silence(reference.size());
            auto wet = reference;
            const auto* kick = stop == 0 ? nullptr : stop == 1 ? &silence : &reference;
            render(makeup, wet, kick, focused);
            require(std::abs(makeup.getMeters().agcDb[2]) < .01 &&
                    std::abs(db(rms(wet, sr * 3 / 2) / rms(reference, sr * 3 / 2))) < .01,
                    "Unmask AGC still boosted the bass with the cut inactive");
            require(makeup.runtimeState()[6] == recalled[6], "Unmask AGC discarded its locked compensation");
        }
        std::cout << "PASS Unmask Auto Gain fades after lost overlap or zero attenuation\n";

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
        for (double reference : {42.,60.,92.}) for (double angle : {-180.,-90.,0.,45.,90.,180.}) {
            Settings dial;
            dial.glue=dial.unmask=false;
            dial.phase=true;dial.phaseReference=60;
            dial.phaseAngle=phaseDegreesAtReference(angle,reference,dial.phaseReference,48000.);
            auto shifted=tone(96000,reference,.3);
            Engine phaseDial;phaseDial.prepare(48000);render(phaseDial,shifted,nullptr,dial);
            double i=0,q=0;
            for (size_t n=48000;n<shifted.size();++n) {
                const auto rad=2*pi*reference*n/48000.;
                i+=shifted[n]*std::sin(rad);q+=shifted[n]*std::cos(rad);
            }
            require(std::abs(std::remainder(std::atan2(q,i)*180/pi-angle,360.))<.1,
                    "Phase dial angle differs from its reference");
            require(std::abs(std::hypot(i,q)*2/48000-.3)<1e-5,"Phase dial did not preserve sine amplitude");
        }
        std::cout << "PASS PHASE delay / polarity / all-pass response\n";

        for (double initialAngle : {-1.,1.}) {
            auto dial=bypass();
            dial.phase=dial.rotate=true;
            dial.phaseReference=55;
            dial.phaseAngle=initialAngle;
            Engine phaseDial;phaseDial.prepare(48000);
            auto warm=tone(2*48000,55,.3);
            render(phaseDial,warm,nullptr,dial);
            const auto nearDry=tone(12000,55,.3);
            auto acrossZero=nearDry;
            dial.phaseAngle=-initialAngle;
            render(phaseDial,acrossZero,nullptr,dial);
            for (size_t n=0;n<acrossZero.size();++n)
                require(std::abs(acrossZero[n]-nearDry[n]) < .018,
                        "A small Phase adjustment through zero reached the wrong all-pass branch");
        }
        std::cout << "PASS Phase rotation moves across zero without a large phase or gain change\n";

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
