#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace console {
inline const juce::Colour cream{0xffe6d5b5}, amber{0xffffb54f}, copper{0xff9d7350};
inline const juce::Colour ink{0xff141412}, muted{0xff9a9c8c}, orange{0xffe76931};
inline juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
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

    void paintPanel(juce::Graphics& g, juce::Rectangle<float> r, bool dark=false) const {
        if (!raw || !steel.isValid()) {
            panel(g,r,dark);
            return;
        }
        g.setColour(juce::Colours::black.withAlpha(.78f));
        g.fillRoundedRectangle(r.translated(0,3),6);
        juce::Graphics::ScopedSaveState scope(g);
        juce::Path edge;
        edge.addRoundedRectangle(r,6);
        g.reduceClipRegion(edge);
        g.setGradientFill(juce::ColourGradient(juce::Colour(dark?0xff181918:0xff393630),0,r.getY(),
            juce::Colour(dark?0xff111312:0xff242320),0,r.getBottom(),false));
        g.fillRect(r);
        g.setTiledImageFill(steel,0,0,dark?.13f:.22f);
        g.fillRect(r);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(dark?0.f:.035f),0,r.getY(),
            juce::Colours::black.withAlpha(dark?.34f:.21f),0,r.getBottom(),false));
        g.fillRect(r);
        g.setColour(juce::Colour(0xff9a8065).withAlpha(.82f));
        g.drawRoundedRectangle(r.reduced(.75f),5,1.5f);
        g.setColour(juce::Colours::black.withAlpha(.7f));
        g.drawRoundedRectangle(r.reduced(2.5f),4,1);
        for (float x:{r.getX()+10,r.getRight()-10})
            for (float y:{r.getY()+10,r.getBottom()-10}) screw(g,x,y);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position, float start, float end, juce::Slider& slider) override {
        juce::Graphics::ScopedSaveState state(g);
        const float cx=static_cast<float>(x)+static_cast<float>(w)*.5f;
        const float cy=static_cast<float>(y)+static_cast<float>(h)*.5f;
        const float radius=std::max(1.f,static_cast<float>(std::min(w,h))*.5f-10.f);
        const float angle=start+position*(end-start);
        const auto polar=[&](float theta,float distance) {
            return juce::Point<float>{cx+std::sin(theta)*distance,cy-std::cos(theta)*distance};
        };
        const auto disc=[&](float r) { return juce::Rectangle<float>{cx-r,cy-r,2*r,2*r}; };
        const bool enabled=slider.isEnabled();
        const auto accent=enabled?amber:muted;
        g.setColour(juce::Colours::black.withAlpha(.7f));
        g.fillEllipse(disc(radius+2).translated(1.5f,3.5f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa48f6d),cx,cy-radius,
            juce::Colour(0xff29221b),cx,cy+radius,false));
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
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff63584a),cx,cy-skirt,
            juce::Colour(0xff211e1a),cx,cy+skirt,false));
        g.fillEllipse(disc(skirt));
        if (radius>=24.f) {
            for (int i=0;i<40;++i) {
                const float a=angle+static_cast<float>(i)*juce::MathConstants<float>::twoPi/40.f;
                g.setColour(juce::Colours::black.withAlpha(.68f));
                g.drawLine(juce::Line<float>{polar(a,radius*.76f),polar(a,skirt)},2.f);
            }
        }
        const float cap=radius*.76f;
        {
            juce::Graphics::ScopedSaveState capState(g);
            juce::Path clip;
            clip.addEllipse(disc(cap));
            g.reduceClipRegion(clip);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff514b40),cx-cap*.7f,cy-cap,
                juce::Colour(0xff201e1a),cx+cap*.3f,cy+cap,false));
            g.fillEllipse(disc(cap));
            if (raw && knob.isValid()) {
                g.setOpacity(.38f);
                g.drawImage(knob,disc(radius));
                g.setOpacity(1.f);
            }
            g.setColour(cream.withAlpha(.15f));
            g.drawEllipse(disc(cap-1),1.f);
        }
        const auto pointerBase=polar(angle,radius*.27f);
        const auto pointerTip=polar(angle,radius*.70f);
        const float weight=radius>39?4.f:2.8f;
        g.setColour(ink);
        g.drawLine(juce::Line<float>{pointerBase.translated(0,1),pointerTip.translated(0,1)},weight+1.6f);
        g.setColour(enabled?cream.brighter(.12f):muted);
        g.drawLine(juce::Line<float>{pointerBase,pointerTip},weight);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) override {toggle(g,b,hover,down);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down) override {button(g,b,hover,down,b.getToggleState());}
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
    void toggle(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) const {
        const auto bounds=b.getLocalBounds().toFloat();
        const bool on=b.getToggleState();
        const auto centre=juce::Point<float>{14,bounds.getCentreY()};
        const float radius=std::min(8.f,bounds.getHeight()*.43f);
        const juce::Rectangle<float> socket(centre.x-radius,centre.y-radius,2*radius,2*radius);
        g.setColour(ink);
        g.fillRoundedRectangle(bounds.reduced(1),4);
        g.setColour(copper.withAlpha(hover?.9f:.45f));
        g.drawRoundedRectangle(bounds.reduced(1),4,1);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffb5b1a5),0,socket.getY(),
            juce::Colour(0xff4a4943),0,socket.getBottom(),false));
        g.fillEllipse(socket);
        g.setColour(ink);
        g.fillEllipse(socket.reduced(radius*.26f));
        const auto tip=centre.translated(radius*.43f,(on?-1.f:1.f)*radius*(down?.44f:.64f));
        g.setColour(juce::Colour(0xff484743));
        g.drawLine(juce::Line<float>{centre,tip},radius*.67f);
        g.setColour(juce::Colour(0xffdbd8cd));
        g.drawLine(juce::Line<float>{centre.translated(-1,0),tip.translated(-1,0)},radius*.30f);
        const auto led=juce::Rectangle<float>(3,3,3,3);
        g.setColour(on?amber:muted.darker(.7f));
        g.fillEllipse(led);
        const auto label=b.getButtonText()=="ON" || b.getButtonText()=="OFF"
            ? juce::String(on?"ON":"OFF") : b.getButtonText();
        g.setColour(cream.withAlpha(b.isEnabled()?1.f:.45f));
        g.setFont(font(bounds.getHeight()<23?11.f:13.f,true));
        g.drawFittedText(label,bounds.withTrimmedLeft(25).withTrimmedRight(4).toNearestInt(),
                         juce::Justification::centred,1);
    }
    void button(juce::Graphics& g,juce::Button& b,bool hover,bool down,bool on) const {
        auto r=b.getLocalBounds().toFloat().reduced(1);
        g.setColour(juce::Colours::black.withAlpha(0.6f));g.fillRoundedRectangle(r.translated(0,2),4);
        if (raw) {
            const auto top=on?juce::Colour(0xff87592f):juce::Colour(0xff474139);
            const auto bottom=on?juce::Colour(0xff49301e):juce::Colour(0xff211e1a);
            g.setGradientFill(juce::ColourGradient(hover?top.brighter(.12f):top,0,r.getY(),
                down?bottom.darker(.2f):bottom,0,r.getBottom(),false));
            g.fillRoundedRectangle(r,4);
            g.setColour((on?amber:copper).withAlpha(b.isEnabled()?.68f:.35f));
            g.drawRoundedRectangle(r,4,1);
            if (on) g.fillRoundedRectangle(r.getX()+5,r.getY()+5,2,r.getHeight()-10,1);
            g.setColour(cream.withAlpha(b.isEnabled()?1.f:.45f));
            g.setFont(font(r.getHeight()<28?12.5f:14.5f,true));
            g.drawFittedText(b.getButtonText(),r.reduced(7,0).toNearestInt(),juce::Justification::centred,1);
            return;
        }
        const auto top=on?amber:juce::Colour(0xff4a4238),bottom=on?juce::Colour(0xffa15c24):juce::Colour(0xff2b2620);
        g.setGradientFill(juce::ColourGradient((down?top.darker():top).brighter(hover?0.1f:0),0,r.getY(),bottom,0,r.getBottom(),false));
        g.fillRoundedRectangle(r,4);g.setColour(on?cream:copper);g.drawRoundedRectangle(r,4,1);
        g.setColour(on?ink:cream);g.setFont(font(r.getHeight()<28?12.5f:14.5f,true));
        g.drawFittedText(b.getButtonText(),r.reduced(5,0).toNearestInt(),juce::Justification::centred,1);
    }
};
} // namespace console
