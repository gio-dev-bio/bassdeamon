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
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position, float start, float end, juce::Slider& slider) override {
        auto r=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),static_cast<float>(h)).reduced(std::min(12.0f,static_cast<float>(h)*0.1f));
        const float radius=std::min(r.getWidth(),r.getHeight())*0.5f, cx=r.getCentreX(),cy=r.getCentreY();
        const float angle=start+position*(end-start);
        for(int i=0;i<=12;++i) {
            const float a=start+static_cast<float>(i)/12*(end-start);
            const float inner=radius+4,outer=radius+(i%3==0 ? 11.0f:8.0f);
            g.setColour(cream.withAlpha(i%3==0 ? 0.8f:0.4f));
            g.drawLine(cx+std::sin(a)*inner,cy-std::cos(a)*inner,cx+std::sin(a)*outer,cy-std::cos(a)*outer,1.2f);
        }
        juce::Path active;active.addCentredArc(cx,cy,radius+2,radius+2,0,start,angle,true);
        g.setColour(amber.withAlpha(slider.isEnabled()?0.85f:0.2f));g.strokePath(active,juce::PathStrokeType(2.5f));
        g.setColour(juce::Colours::black.withAlpha(0.65f));g.fillEllipse(cx-radius+2,cy-radius+5,2*radius,2*radius);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8d8268),cx-radius,cy-radius,ink,cx+radius,cy+radius,false));
        g.fillEllipse(cx-radius,cy-radius,2*radius,2*radius);
        g.setColour(copper.withAlpha(0.85f));g.drawEllipse(cx-radius+3,cy-radius+3,2*radius-6,2*radius-6,1.5f);
        juce::Path grip;
        for(int i=0;i<72;++i) {
            const float a=static_cast<float>(i)*juce::MathConstants<float>::twoPi/72;
            const float size=radius-6+(i%6<3?1.5f:0);
            const float px=cx+std::sin(a)*size,py=cy-std::cos(a)*size;
            if(i==0)grip.startNewSubPath(px,py);else grip.lineTo(px,py);
        }
        grip.closeSubPath();g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3832),cx,cy-radius,juce::Colour(0xff191916),cx,cy+radius,false));g.fillPath(grip);
        g.setColour(juce::Colours::black.withAlpha(0.6f));g.strokePath(grip,juce::PathStrokeType(1.5f));
        const float tip=radius*0.78f,base=radius*0.28f;
        g.setColour(slider.isEnabled()?cream:muted);g.drawLine(cx+std::sin(angle)*base,cy-std::cos(angle)*base,cx+std::sin(angle)*tip,cy-std::cos(angle)*tip,radius>42?4.5f:3.0f);
        g.setColour(cream.withAlpha(0.06f));g.drawEllipse(cx-radius+9,cy-radius+9,2*radius-18,2*radius-18,1);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) override {button(g,b,hover,down,b.getToggleState());}
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
    static void button(juce::Graphics& g,juce::Button& b,bool hover,bool down,bool on) {
        auto r=b.getLocalBounds().toFloat().reduced(1);
        g.setColour(juce::Colours::black.withAlpha(0.6f));g.fillRoundedRectangle(r.translated(0,2),4);
        const auto top=on?amber:juce::Colour(0xff4a4238),bottom=on?juce::Colour(0xffa15c24):juce::Colour(0xff2b2620);
        g.setGradientFill(juce::ColourGradient((down?top.darker():top).brighter(hover?0.1f:0),0,r.getY(),bottom,0,r.getBottom(),false));
        g.fillRoundedRectangle(r,4);g.setColour(on?cream:copper);g.drawRoundedRectangle(r,4,1);
        g.setColour(on?ink:cream);g.setFont(font(r.getHeight()<28?12.5f:14.5f,true));
        g.drawFittedText(b.getButtonText(),r.reduced(5,0).toNearestInt(),juce::Justification::centred,1);
    }
};
} // namespace console
