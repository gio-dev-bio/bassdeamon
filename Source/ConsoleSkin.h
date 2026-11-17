#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace console {
inline const juce::Colour cream{0xffe6d5b5}, amber{0xffffb54f}, copper{0xff9d7350};
inline const juce::Colour ink{0xff141412}, muted{0xff9a9c8c}, orange{0xffe76931};
inline juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions("Helvetica Neue", size, bold ? juce::Font::bold : juce::Font::plain));
}
inline void text(juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                 float size = 15, juce::Colour colour = cream, juce::Justification align = juce::Justification::centredLeft) {
    g.setColour(colour); g.setFont(font(size)); g.drawText(s, r, align, false);
}
inline void screw(juce::Graphics& g, float x, float y) {
    g.setColour(juce::Colours::black.withAlpha(0.7f)); g.fillEllipse(x-6,y-5,14,14);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777568), x-4,y-5, juce::Colour(0xff20241f),x+5,y+6,false));
    g.fillEllipse(x-5,y-5,10,10); g.setColour(ink); g.drawLine(x-2.4f,y-1.5f,x+2.4f,y+1.5f,1.8f);
}
inline void panel(juce::Graphics& g, juce::Rectangle<float> r, bool dark = false) {
    g.setColour(juce::Colours::black.withAlpha(0.7f)); g.fillRoundedRectangle(r.translated(0,3),9);
    g.setGradientFill(juce::ColourGradient(dark ? ink : juce::Colour(0xff34332f),r.getX(),r.getY(),
        dark ? juce::Colour(0xff191c16) : juce::Colour(0xff22221f),r.getX(),r.getBottom(),false));
    g.fillRoundedRectangle(r,9); g.setColour(copper.withAlpha(0.75f)); g.drawRoundedRectangle(r.reduced(0.5f),9,1.3f);
    g.setColour(juce::Colours::black.withAlpha(0.8f)); g.drawRoundedRectangle(r.reduced(3),7,1);
    for (float x : {r.getX()+10,r.getRight()-10}) for(float y : {r.getY()+10,r.getBottom()-10}) screw(g,x,y);
}
inline void demon(juce::Graphics& g, juce::Rectangle<float> area) {
    juce::Graphics::ScopedSaveState scope(g);
    g.addTransform(juce::AffineTransform::scale(area.getWidth()/100,area.getHeight()/100).translated(area.getX(),area.getY()));
    juce::Path face;
    face.startNewSubPath(23,34); face.lineTo(37,24); face.lineTo(50,30); face.lineTo(63,24); face.lineTo(77,34);
    face.lineTo(74,65); face.lineTo(64,80); face.lineTo(50,94); face.lineTo(36,80); face.lineTo(26,65); face.closeSubPath();
    g.setGradientFill(juce::ColourGradient(amber,50,28,copper.darker(0.6f),50,90,false)); g.fillPath(face);
    g.setColour(ink); g.strokePath(face,juce::PathStrokeType(2));
    juce::Path horns;
    horns.startNewSubPath(26,41); horns.cubicTo(5,26,16,8,25,2); horns.cubicTo(16,20,29,19,36,29); horns.closeSubPath();
    horns.startNewSubPath(74,41); horns.cubicTo(95,26,84,8,75,2); horns.cubicTo(84,20,71,19,64,29); horns.closeSubPath();
    g.setColour(cream); g.fillPath(horns); g.setColour(copper); g.strokePath(horns,juce::PathStrokeType(2));
    juce::Path brow; brow.startNewSubPath(30,45); brow.lineTo(45,51); brow.lineTo(39,58); brow.closeSubPath();
    brow.startNewSubPath(70,45); brow.lineTo(55,51); brow.lineTo(61,58); brow.closeSubPath();
    g.setColour(ink); g.fillPath(brow);
    g.setColour(cream.brighter()); g.drawLine(32,49,41,53,2); g.drawLine(68,49,59,53,2);
    juce::Path nose; nose.startNewSubPath(50,51); nose.lineTo(43,68); nose.lineTo(57,68); nose.closeSubPath();
    g.setColour(ink.withAlpha(0.65f)); g.fillPath(nose);
    g.drawLine(37,73,50,80,2); g.drawLine(50,80,63,73,2);
    g.setColour(cream); g.drawLine(42,76,50,79,1.5f); g.drawLine(50,79,58,76,1.5f);
    juce::Path headphones; headphones.addCentredArc(50,50,37,37,0,-juce::MathConstants<float>::halfPi,juce::MathConstants<float>::halfPi,true);
    g.setColour(ink); g.strokePath(headphones,juce::PathStrokeType(8));
    g.setColour(copper); g.strokePath(headphones,juce::PathStrokeType(2));
    for (float x : {10.0f,79.0f}) {g.setColour(ink);g.fillRoundedRectangle(x,43,11,25,3);g.setColour(copper);g.drawRoundedRectangle(x,43,11,25,3,1.5f);}
}

class Skin final : public juce::LookAndFeel_V4 {
public:
    Skin() {
        setColour(juce::Slider::thumbColourId,amber); setColour(juce::Slider::trackColourId,copper);
        setColour(juce::Slider::backgroundColourId,ink);
        setColour(juce::Slider::textBoxTextColourId,cream); setColour(juce::Slider::textBoxBackgroundColourId,ink);
        setColour(juce::Slider::textBoxOutlineColourId,copper.withAlpha(0.5f));
        setColour(juce::ComboBox::textColourId,cream); setColour(juce::ComboBox::backgroundColourId,ink);
        setColour(juce::PopupMenu::backgroundColourId,ink);setColour(juce::PopupMenu::textColourId,cream);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,copper.darker());
        setColour(juce::Label::textColourId,cream);
        setColour(juce::TooltipWindow::backgroundColourId,juce::Colour(0xffe8d6af));
        setColour(juce::TooltipWindow::textColourId,ink);
    }
    bool isRaw() const noexcept { return raw; }

    bool setRaw(bool useRaw);

    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,
                          float position,float minimum,float maximum,
                          juce::Slider::SliderStyle style,juce::Slider& slider) override {
        if (!raw || style!=juce::Slider::LinearBar) {
            juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimum,maximum,style,slider);
            return;
        }
        const auto area=slider.getLocalBounds().toFloat().reduced(1);
        g.setColour(ink);
        g.fillRoundedRectangle(area,2);
        g.setColour(juce::Colour(0xff797b79));
        g.drawRoundedRectangle(area,2,1);
    }

    void paintPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark=false) const {
        if (!raw) { panel(g,r,dark); return; }
        juce::Graphics::ScopedSaveState scope(g);
        g.setColour(juce::Colours::black.withAlpha(.85f));
        g.fillRoundedRectangle(r.translated(0,2),8);
        // Continuous steel extrusion around a recessed graphite face.
        juce::ColourGradient rim(juce::Colour(0xffa5a7a7),r.getX(),r.getY(),
                                juce::Colour(0xff373a3d),r.getX(),r.getBottom(),false);
        rim.addColour(.12,juce::Colour(0xff61666a));
        rim.addColour(.50,juce::Colour(0xff4b5054));
        rim.addColour(.91,juce::Colour(0xff858a8d));
        g.setGradientFill(rim);g.fillRoundedRectangle(r,8);
        g.setColour(juce::Colour(0xff1a1c1e));
        g.drawRoundedRectangle(r.reduced(.6f),8,1.2f);
        g.setColour(juce::Colour(0xffc1c4c5).withAlpha(.45f));
        g.drawRoundedRectangle(r.reduced(1.7f),6.5f,.8f);
        const auto face=r.reduced(5);
        g.setGradientFill(juce::ColourGradient(juce::Colour(dark?0xff151d20:0xff37393c),
            face.getX(),face.getY(),juce::Colour(dark?0xff10181b:0xff292c2f),
            face.getX(),face.getBottom(),false));
        g.fillRoundedRectangle(face,4);
        if (steel.isValid()) {
            juce::Graphics::ScopedSaveState texture(g);
            juce::Path clip;clip.addRoundedRectangle(face,4);g.reduceClipRegion(clip);
            g.setTiledImageFill(steel,0,0,dark?.025f:.045f);g.fillRect(face);
        }
        g.setColour(juce::Colours::black.withAlpha(.85f));
        g.drawRoundedRectangle(face,4,1.4f);
        g.setColour(juce::Colours::white.withAlpha(.10f));
        g.drawHorizontalLine(juce::roundToInt(face.getY()+1),face.getX()+6,face.getRight()-6);
        for (float x:{r.getX()+10,r.getRight()-10})
            for (float y:{r.getY()+10,r.getBottom()-10}) screw(g,x,y);
    }

    // The approved cap is smooth turned steel; no fan-shaped stock bitmap.
    void rawKnob(juce::Graphics& g,float cx,float cy,float radius,float angle,bool enabled) const {
        const auto disc=[&](float r){return juce::Rectangle<float>(cx-r,cy-r,2*r,2*r);};
        const auto point=[&](float a,float r){return juce::Point<float>(cx+std::sin(a)*r,cy-std::cos(a)*r);};
        g.setColour(juce::Colours::black.withAlpha(.8f));g.fillEllipse(disc(radius+3).translated(1,3));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd4d3cb),cx-radius,cy-radius,
            juce::Colour(0xff383a39),cx+radius,cy+radius,false));g.fillEllipse(disc(radius));
        g.setColour(juce::Colour(0xff171a1a));g.fillEllipse(disc(radius-2));
        const float cap=radius-4;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffbcb9ad),cx-cap,cy-cap,
            juce::Colour(0xff555751),cx+cap,cy+cap,false));g.fillEllipse(disc(cap));
        // Wide, low-contrast reflected light, rather than alternating spokes.
        {
            juce::Graphics::ScopedSaveState clip(g);
            juce::Path circle;circle.addEllipse(disc(cap));g.reduceClipRegion(circle);
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.30f),cx-cap*.5f,cy-cap*.6f,
                juce::Colours::transparentWhite,cx+cap*.7f,cy+cap*.7f,true));g.fillEllipse(disc(cap));
            for(int i=1;i<22;++i) {
                g.setColour(juce::Colours::black.withAlpha(i%2?.025f:.045f));
                g.drawEllipse(disc(cap*float(i)/22.f),.35f);
            }
        }
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffece8da),cx,cy-cap,
            juce::Colour(0xff343733),cx,cy+cap,false));g.drawEllipse(disc(cap-.8f),1.2f);
        g.setColour(juce::Colour(0xff121616));g.drawLine({point(angle,cap*.57f),point(angle,cap*.94f)},radius>25?3.f:2.f);
        if(!enabled) {g.setColour(juce::Colours::black.withAlpha(.20f));g.fillEllipse(disc(radius));}
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position, float start, float end, juce::Slider& slider) override {
        juce::Graphics::ScopedSaveState state(g);
        const float cx=static_cast<float>(x)+static_cast<float>(w)*.5f;
        const float cy=static_cast<float>(y)+static_cast<float>(h)*.5f;
        const float radius=std::max(1.f,static_cast<float>(std::min(w,h))*.5f-(raw?3.f:10.f));
        const float angle=start+position*(end-start);
        const auto polar=[&](float theta,float distance) {
            return juce::Point<float>{cx+std::sin(theta)*distance,cy-std::cos(theta)*distance};
        };
        const auto disc=[&](float r) { return juce::Rectangle<float>{cx-r,cy-r,2*r,2*r}; };
        const bool enabled=slider.isEnabled();
        const auto accent=enabled?amber:muted;
        if (raw) {
            for(int i=0;i<=12;++i) {
                const float mark=start+float(i)*(end-start)/12.f;
                g.setColour(i%3==0?cream.withAlpha(.9f):amber.withAlpha(.7f));
                g.drawLine({polar(mark,radius+3),polar(mark,radius+(i%3==0?6.f:4.5f))},i%3==0?1.1f:.8f);
            }
            rawKnob(g,cx,cy,radius,angle,enabled);
            return;
        }
        g.setColour(juce::Colours::black.withAlpha(.7f));
        g.fillEllipse(disc(radius+2).translated(1.5f,3.5f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb8bab9),cx,cy-radius,
            juce::Colour(0xff343737),cx,cy+radius,false));
        g.fillEllipse(disc(radius));
        g.setColour(ink);
        g.fillEllipse(disc(radius*.91f));

        juce::Path arc;
        arc.addCentredArc(cx,cy,radius+3,radius+3,0,start,end,true);
        g.setColour(ink);
        g.strokePath(arc,juce::PathStrokeType(2.5f));
        if (position>0) {
            juce::Path active;
            active.addCentredArc(cx,cy,radius+3,radius+3,0,start,angle,true);
            g.setColour(accent.withAlpha(.9f));
            g.strokePath(active,juce::PathStrokeType(2.3f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        for(int i=0;i<=12;++i) {
            const bool major=i%3==0;
            const float mark=start+static_cast<float>(i)*(end-start)/12.f;
            g.setColour(cream.withAlpha(major?.84f:.4f));
            g.drawLine(juce::Line<float>{polar(mark,radius+5),polar(mark,radius+(major?9.f:7.5f))},major?1.35f:1.f);
        }

        const float skirt=radius*.88f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7b7e7c),cx,cy-skirt,
            juce::Colour(0xff2a2c2b),cx,cy+skirt,false));
        g.fillEllipse(disc(skirt));
        if (!raw && radius>=24.f) {
            for (int i=0;i<40;++i) {
                const float a=angle+static_cast<float>(i)*juce::MathConstants<float>::twoPi/40.f;
                g.setColour(juce::Colours::black.withAlpha(.68f));
                g.drawLine(juce::Line<float>{polar(a,radius*.76f),polar(a,skirt)},2.f);
            }
        }
        const float cap=radius*(raw?.91f:.80f);
        {
            juce::Graphics::ScopedSaveState capState(g);
            juce::Path clip;
            clip.addEllipse(disc(cap));
            g.reduceClipRegion(clip);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777a79),cx-cap*.7f,cy-cap,
                juce::Colour(0xff3c3e3d),cx+cap*.3f,cy+cap,false));
            g.fillEllipse(disc(cap));
            if (knob.isValid()) {
                g.setOpacity(enabled?.75f:.5f);
                g.drawImage(knob,disc(cap));
                g.setOpacity(1.f);
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe6e9e8).withAlpha(.72f),cx,cy-cap,
                juce::Colour(0xff171919).withAlpha(.83f),cx,cy+cap,false));
            g.drawEllipse(disc(cap-1),1.35f);
        }
        const auto pointerBase=polar(angle,radius*.27f);
        const auto pointerTip=polar(angle,radius*.70f);
        const float weight=radius>39?4.f:2.8f;
        g.setColour(ink);
        g.drawLine(juce::Line<float>{pointerBase.translated(0,1),pointerTip.translated(0,1)},weight+1.6f);
        g.setColour(juce::Colour(enabled?0xff1c1e1d:0xff3e4240));
        g.drawLine(juce::Line<float>{pointerBase,pointerTip},weight);
    }
    void drawLabel(juce::Graphics& g,juce::Label& label) override {
        if (!raw || dynamic_cast<juce::Slider*>(label.getParentComponent())==nullptr) {
            juce::LookAndFeel_V4::drawLabel(g,label);return;
        }
        const auto r=label.getLocalBounds().toFloat().reduced(.7f);
        g.setColour(juce::Colours::black.withAlpha(.7f));g.fillRoundedRectangle(r.translated(0,1),3);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff101314),0,r.getY(),
            juce::Colour(0xff232526),0,r.getBottom(),false));g.fillRoundedRectangle(r,3);
        g.setColour(juce::Colour(0xff777b7b));g.drawRoundedRectangle(r,3,.75f);
        g.setColour(juce::Colours::black);g.drawHorizontalLine(juce::roundToInt(r.getY()+1),r.getX()+3,r.getRight()-3);
        if(!label.isBeingEdited()) {
            g.setColour(label.isEnabled()?juce::Colour(0xffeee8da):muted);
            g.setFont(font(14));
            g.drawFittedText(label.getText(),r.reduced(3,0).toNearestInt(),label.getJustificationType(),1);
        }
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) override {metalToggle(g,b,hover,down,b.getComponentID()=="duck.multiband");}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down) override {
        if (b.getComponentID()=="view-mode") metalToggle(g,b,hover,down,true);
        else button(g,b,hover,down,b.getToggleState());
    }
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override {}
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox&) override {
        auto r=juce::Rectangle<float>(0,0,static_cast<float>(w),static_cast<float>(h)).reduced(1);
        g.setColour(ink);g.fillRoundedRectangle(r,4);g.setColour(copper.withAlpha(0.65f));g.drawRoundedRectangle(r,4,1);
        juce::Path p;p.addTriangle(static_cast<float>(w)-17,static_cast<float>(h)*0.43f,static_cast<float>(w)-9,static_cast<float>(h)*0.43f,static_cast<float>(w)-13,static_cast<float>(h)*0.61f);
        g.setColour(cream);g.fillPath(p);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return font(16);}
    juce::Font getLabelFont(juce::Label&) override {return font(14);}
private:
    bool raw=false;
    juce::Image steel,knob;
    // Mechanical steel toggle: purely visual, JUCE ButtonAttachment and accessibility unchanged.
    void metalToggle(juce::Graphics& g, juce::Button& b, bool hover, bool down, bool viewMode=false) const {
        const auto r=b.getLocalBounds().toFloat().reduced(1.0f);
        if (r.getWidth()<33.0f || r.getHeight()<14.0f) {
            button(g,b,hover,down,b.getToggleState());
            return;
        }
        const bool on=b.getToggleState();
        constexpr float radius=7.5f;
        constexpr float shaftLength=9.5f;
        constexpr float tipRadius=3.0f;
        const float cx=viewMode?r.getCentreX():r.getRight()-radius-5.0f;
        const float cy=r.getCentreY();
        g.setColour(juce::Colours::black.withAlpha(0.66f));
        g.fillEllipse(cx-radius+1,cy-radius+2,radius*2,radius*2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffdfe3e3),cx-radius,cy-radius,
                                               juce::Colour(0xff555b60),cx+radius,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,radius*2,radius*2);
        g.setColour(juce::Colour(0xff211e1b));g.drawEllipse(cx-radius,cy-radius,radius*2,radius*2,1.2f);
        g.setColour(juce::Colour(0xff24211e));g.fillEllipse(cx-radius*.67f,cy-radius*.67f,radius*1.34f,radius*1.34f);
        const float sx=cx+(on?shaftLength:-shaftLength)*.66f;
        const float sy=cy+(on?-shaftLength:shaftLength)*.73f+(down?1.0f:0.0f);
        g.setColour(juce::Colours::black.withAlpha(.7f));
        g.drawLine(cx+1,cy+1,sx+1,sy+1,5.2f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff0f2ef),cx-radius,cy-radius,
                                               juce::Colour(0xff817c72),cx+radius,cy+radius,false));
        g.drawLine(cx,cy,sx,sy,4.5f);
        g.setColour(juce::Colour(0xffe0e1da));
        g.fillEllipse(sx-tipRadius,sy-tipRadius,2*tipRadius,2*tipRadius);
        const float lampX=cx-18.0f;
        if (!viewMode && r.getWidth()>62.0f) {
            g.setColour(on?amber:juce::Colour(0xff40362c));
            g.fillEllipse(lampX-2.4f,cy-2.4f,4.8f,4.8f);
            if (on) {g.setColour(amber.withAlpha(.16f));g.fillEllipse(lampX-5,cy-5,10,10);}
        }
        if (viewMode) {
            const auto labelWidth=r.getWidth()*.5f-radius-5;
            const bool band=b.getComponentID()=="duck.multiband";
            const auto drawLabel=[&](const char* label,juce::Rectangle<float> bounds,bool selected) {
                g.setColour(selected?cream:muted);
                g.setFont(font(11,!raw));
                g.drawFittedText(label,bounds.toNearestInt(),juce::Justification::centred,1);
            };
            drawLabel(band?"FULL-BAND":"MAIN",r.withWidth(labelWidth),!on);
            drawLabel(band?"LOW-BAND":"ADV",r.withX(cx+radius+5).withWidth(labelWidth),on);
        } else {
            const auto labelArea=juce::Rectangle<float>(r.getX()+3,r.getY(),juce::jmax(1.0f,(r.getWidth()>62.f?lampX-9:cx-radius-3)-r.getX()),r.getHeight());
            g.setColour(b.isEnabled()?cream:muted.withAlpha(.5f));
            g.setFont(font(r.getHeight()<25?11.0f:12.5f,!raw));
            const auto label=b.getButtonText()=="ON" || b.getButtonText()=="OFF"
                ? juce::String(on?"ON":"OFF") : b.getButtonText();
            g.drawFittedText(label,labelArea.toNearestInt(),juce::Justification::centred,1);
        }
        if (hover && b.isEnabled()) {
            g.setColour(cream.withAlpha(.15f));
            g.drawEllipse(cx-radius-2,cy-radius-2,2*radius+4,2*radius+4,1.0f);
        }
    }
    void button(juce::Graphics& g,juce::Button& b,bool hover,bool down,bool on) const {
        auto r=b.getLocalBounds().toFloat().reduced(1);
        g.setColour(juce::Colours::black.withAlpha(0.6f));g.fillRoundedRectangle(r.translated(0,2),4);
        if (raw) {
            const auto top=on?juce::Colour(0xff9e662c):juce::Colour(0xff45484a);
            const auto bottom=on?juce::Colour(0xff533216):juce::Colour(0xff242628);
            g.setGradientFill(juce::ColourGradient(hover?top.brighter(.12f):top,0,r.getY(),
                down?bottom.darker(.2f):bottom,0,r.getBottom(),false));
            g.fillRoundedRectangle(r,4);
            g.setColour((on?amber:copper).withAlpha(b.isEnabled()?.68f:.35f));
            g.drawRoundedRectangle(r,4,1);
            g.setColour(juce::Colours::white.withAlpha(on?.20f:.12f));
            g.drawHorizontalLine(juce::roundToInt(r.getY()+1),r.getX()+4,r.getRight()-4);
            g.setColour(cream.withAlpha(b.isEnabled()?1.f:.45f));
            g.setFont(font(r.getHeight()<28?12.5f:14.5f,!raw));
            g.drawFittedText(b.getButtonText(),r.reduced(7,0).toNearestInt(),juce::Justification::centred,1);
            return;
        }
        const auto top=on?amber:juce::Colour(0xff4a4238),bottom=on?juce::Colour(0xffa15c24):juce::Colour(0xff2b2620);
        g.setGradientFill(juce::ColourGradient((down?top.darker():top).brighter(hover?0.1f:0),0,r.getY(),bottom,0,r.getBottom(),false));
        g.fillRoundedRectangle(r,4);g.setColour(on?cream:copper);g.drawRoundedRectangle(r,4,1);
        g.setColour(on?ink:cream);g.setFont(font(r.getHeight()<28?12.5f:14.5f,!raw));
        g.drawFittedText(b.getButtonText(),r.reduced(5,0).toNearestInt(),juce::Justification::centred,1);
    }
};
} // namespace console
