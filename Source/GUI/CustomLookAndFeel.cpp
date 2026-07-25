#include "CustomLookAndFeel.h"

// Define static color constants
const juce::Colour CustomLookAndFeel::shredDarkBg          (0xff0d0e12);
const juce::Colour CustomLookAndFeel::shredWidgetBg        (0xff161822);
const juce::Colour CustomLookAndFeel::shredMenuBg          (0xff12141c);
const juce::Colour CustomLookAndFeel::shredOutline         (0xff2d3142);
const juce::Colour CustomLookAndFeel::shredAccent          (0xffff3c00);
const juce::Colour CustomLookAndFeel::shredText            (0xffe6e8ea);
const juce::Colour CustomLookAndFeel::shredHighlightText   (0xffffffff);
const juce::Colour CustomLookAndFeel::shredHighlightFill   (0xff3d425c);
const juce::Colour CustomLookAndFeel::shredMutedText       (0xff7a8296);

CustomLookAndFeel::CustomLookAndFeel()
{
    // Apply ColourScheme-style dark palette
    setColourScheme (juce::LookAndFeel_V4::ColourScheme {
        shredDarkBg,
        shredWidgetBg,
        shredMenuBg,
        shredOutline,
        shredText,
        shredAccent,
        shredHighlightText,
        shredHighlightFill,
        shredText
    });

    // Explicit setColour calls for juce::Slider, juce::Label, juce::TextButton, juce::ComboBox, and juce::PopupMenu colour IDs
    setColour (juce::Slider::thumbColourId, shredAccent);
    setColour (juce::Slider::trackColourId, shredAccent);
    setColour (juce::Slider::backgroundColourId, shredWidgetBg);
    setColour (juce::Slider::rotarySliderOutlineColourId, shredOutline);
    setColour (juce::Slider::rotarySliderFillColourId, shredAccent);

    setColour (juce::Label::textColourId, shredText);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId, shredWidgetBg);
    setColour (juce::TextButton::buttonOnColourId, shredAccent);
    setColour (juce::TextButton::textColourOffId, shredText);
    setColour (juce::TextButton::textColourOnId, shredHighlightText);

    setColour (juce::ComboBox::backgroundColourId, shredWidgetBg);
    setColour (juce::ComboBox::outlineColourId, shredOutline);
    setColour (juce::ComboBox::textColourId, shredText);
    setColour (juce::ComboBox::buttonColourId, shredWidgetBg);
    setColour (juce::ComboBox::arrowColourId, shredAccent);

    setColour (juce::PopupMenu::backgroundColourId, shredMenuBg);
    setColour (juce::PopupMenu::textColourId, shredText);
    setColour (juce::PopupMenu::headerTextColourId, shredMutedText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, shredHighlightFill);
    setColour (juce::PopupMenu::highlightedTextColourId, shredHighlightText);
}

CustomLookAndFeel::~CustomLookAndFeel() = default;

juce::Font CustomLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return withDefaultMetrics (juce::FontOptions { juce::jmin (15.0f, (float) buttonHeight * 0.55f) });
}

void CustomLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto cornerSize = 4.0f;
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f, 0.5f);

    auto baseColour = backgroundColour.withMultipliedSaturation (button.hasKeyboardFocus (true) ? 1.2f : 1.0f)
                                      .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    if (shouldDrawButtonAsDown)
        baseColour = shredAccent.withAlpha (0.4f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter (0.1f);

    g.setColour (baseColour);

    auto flatOnLeft   = button.isConnectedOnLeft();
    auto flatOnRight  = button.isConnectedOnRight();
    auto flatOnTop    = button.isConnectedOnTop();
    auto flatOnBottom = button.isConnectedOnBottom();

    juce::Path path;
    path.addRoundedRectangle (bounds.getX(), bounds.getY(),
                              bounds.getWidth(), bounds.getHeight(),
                              cornerSize, cornerSize,
                              ! (flatOnLeft  || flatOnTop),
                              ! (flatOnRight || flatOnTop),
                              ! (flatOnLeft  || flatOnBottom),
                              ! (flatOnRight || flatOnBottom));

    g.fillPath (path);

    g.setColour (shouldDrawButtonAsHighlighted ? shredAccent : shredOutline);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void CustomLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                          const juce::String& text, const juce::String& shortcutKeyText,
                                          const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced (5, 0);
        r.removeFromTop (juce::roundToInt (((float) r.getHeight() * 0.5f) - 0.5f));

        g.setColour (shredOutline);
        g.fillRect (r.removeFromTop (1));
    }
    else
    {
        auto textColour = (textColourToUse == nullptr ? findColour (juce::PopupMenu::textColourId)
                                                      : *textColourToUse);

        auto r = area.reduced (1);

        if (isHighlighted && isActive)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect (r);

            // Left highlighted indicator line
            g.setColour (shredAccent);
            g.fillRect (r.getX(), r.getY(), 3, r.getHeight());

            g.setColour (findColour (juce::PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour (textColour.withMultipliedAlpha (isActive ? 1.0f : 0.5f));
        }

        r.reduce (juce::jmin (5, area.getWidth() / 20), 0);

        auto font = getPopupMenuFont();
        auto maxFontHeight = (float) r.getHeight() / 1.3f;

        if (font.getHeight() > maxFontHeight)
            font.setHeight (maxFontHeight);

        g.setFont (font);

        auto iconArea = r.removeFromLeft (juce::roundToInt (maxFontHeight)).toFloat();

        if (icon != nullptr)
        {
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            r.removeFromLeft (juce::roundToInt (maxFontHeight * 0.5f));
        }
        else if (isTicked)
        {
            auto tick = getTickShape (1.0f);
            g.fillPath (tick, tick.getTransformToScaleToFit (iconArea.reduced (iconArea.getWidth() / 5.0f, 0.0f), true));
        }

        if (hasSubMenu)
        {
            auto arrowH = 0.6f * getPopupMenuFont().getAscent();

            auto x = static_cast<float> (r.removeFromRight ((int) arrowH).getX());
            auto halfH = static_cast<float> (r.getCentreY());

            juce::Path path;
            path.startNewSubPath (x, halfH - arrowH * 0.5f);
            path.lineTo (x + arrowH * 0.6f, halfH);
            path.lineTo (x, halfH + arrowH * 0.5f);

            g.strokePath (path, juce::PathStrokeType (1.5f));
        }

        r.removeFromRight (10);
        g.drawFittedText (text, r, juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            auto fontHeight = font.getHeight();
            g.setFont (fontHeight * 0.85f);
            g.drawFittedText (shortcutKeyText, r, juce::Justification::centredRight, 1);
            g.setFont (font);
        }
    }
}

void CustomLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto cornerSize = 3.0f;
    juce::Rectangle<int> boxBounds (0, 0, width, height);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

    juce::Rectangle<int> arrowZone (width - 25, 0, 20, height);
    juce::Path path;
    path.startNewSubPath ((float) arrowZone.getX() + 3.0f, (float) arrowZone.getCentreY() - 2.0f);
    path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
    path.lineTo ((float) arrowZone.getRight() - 3.0f, (float) arrowZone.getCentreY() - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

void CustomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.isBar())
    {
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRect (slider.isHorizontal() ? juce::Rectangle<float> (static_cast<float> (x), (float) y + 0.5f, sliderPos - (float) x, (float) height - 1.0f)
                                          : juce::Rectangle<float> ((float) x + 0.5f, sliderPos, (float) width - 1.0f, (float) y + ((float) height - sliderPos)));

        drawLinearSliderOutline (g, x, y, width, height, style, slider);
    }
    else
    {
        auto isTwoVal   = (style == juce::Slider::SliderStyle::TwoValueVertical   || style == juce::Slider::SliderStyle::TwoValueHorizontal);
        auto isThreeVal = (style == juce::Slider::SliderStyle::ThreeValueVertical || style == juce::Slider::SliderStyle::ThreeValueHorizontal);

        auto trackWidth = 4.0f;

        juce::Point<float> startPoint (slider.isHorizontal() ? (float) x : (float) x + (float) width * 0.5f,
                                       slider.isHorizontal() ? (float) y + (float) height * 0.5f : (float) (height + y));

        juce::Point<float> endPoint (slider.isHorizontal() ? (float) (width + x) : startPoint.x,
                                     slider.isHorizontal() ? startPoint.y : (float) y);

        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath (startPoint);
        backgroundTrack.lineTo (endPoint);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.strokePath (backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        juce::Path valueTrack;
        juce::Point<float> minPoint, maxPoint, thumbPoint;

        if (isTwoVal || isThreeVal)
        {
            minPoint = { slider.isHorizontal() ? minSliderPos : (float) width * 0.5f,
                         slider.isHorizontal() ? (float) height * 0.5f : minSliderPos };

            if (isThreeVal)
                thumbPoint = { slider.isHorizontal() ? sliderPos : (float) width * 0.5f,
                               slider.isHorizontal() ? (float) height * 0.5f : sliderPos };

            maxPoint = { slider.isHorizontal() ? maxSliderPos : (float) width * 0.5f,
                         slider.isHorizontal() ? (float) height * 0.5f : maxSliderPos };
        }
        else
        {
            auto kx = slider.isHorizontal() ? sliderPos : ((float) x + (float) width * 0.5f);
            auto ky = slider.isHorizontal() ? ((float) y + (float) height * 0.5f) : sliderPos;

            minPoint = startPoint;
            maxPoint = { kx, ky };
        }

        valueTrack.startNewSubPath (minPoint);
        valueTrack.lineTo (isThreeVal ? thumbPoint : maxPoint);
        g.setColour (slider.isEnabled() ? slider.findColour (juce::Slider::trackColourId) : shredMutedText);
        g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        if (! isTwoVal)
        {
            auto centre = isThreeVal ? thumbPoint : maxPoint;
            auto thumbRadius = 8.0f;

            g.setColour (shredOutline);
            g.drawEllipse (centre.x - thumbRadius, centre.y - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 1.5f);

            g.setColour (slider.isEnabled() ? slider.findColour (juce::Slider::thumbColourId) : shredMutedText);
            g.fillEllipse (centre.x - (thumbRadius - 2.0f), centre.y - (thumbRadius - 2.0f),
                           (thumbRadius - 2.0f) * 2.0f, (thumbRadius - 2.0f) * 2.0f);
        }
    }
}

void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
    auto fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 6.0f;
    auto arcRadius = radius - lineW * 1.5f;

    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (centreX - radius + 2.0f, centreY - radius + 2.0f, radius * 2.0f, radius * 2.0f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (outline);
    g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ColourGradient knobGrad (shredWidgetBg.brighter (0.05f), centreX - radius, centreY - radius,
                                   shredWidgetBg.darker (0.1f), centreX + radius, centreY + radius, true);
    g.setGradientFill (knobGrad);
    g.fillEllipse (centreX - (radius - lineW - 2.0f), centreY - (radius - lineW - 2.0f),
                   (radius - lineW - 2.0f) * 2.0f, (radius - lineW - 2.0f) * 2.0f);

    g.setColour (shredOutline);
    g.drawEllipse (centreX - (radius - lineW - 2.0f), centreY - (radius - lineW - 2.0f),
                   (radius - lineW - 2.0f) * 2.0f, (radius - lineW - 2.0f) * 2.0f, 1.0f);

    if (slider.isEnabled())
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour (fill);
        g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    auto indicatorLength = juce::jmax (2.0f, radius - lineW - 6.0f);
    auto indicatorThickness = 2.5f;
    juce::Path p;
    p.addRoundedRectangle (-indicatorThickness * 0.5f, -indicatorLength, indicatorThickness, indicatorLength * 0.7f, 1.0f);
    p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centreX, centreY));

    g.setColour (slider.isEnabled() ? shredAccent.brighter (0.2f) : shredMutedText);
    g.fillPath (p);
}

void CustomLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font (getLabelFont (label));

        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);

        auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());

        g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());

        auto outlineColour = label.findColour (juce::Label::outlineColourId);
        if (outlineColour.isOpaque() || outlineColour.getAlpha() > 0)
        {
            g.setColour (outlineColour.withMultipliedAlpha (alpha));
            g.drawRect (label.getLocalBounds());
        }
    }
    else if (label.isEnabled())
    {
        auto outlineColour = label.findColour (juce::Label::outlineColourId);
        if (outlineColour.isOpaque() || outlineColour.getAlpha() > 0)
        {
            g.setColour (outlineColour);
            g.drawRect (label.getLocalBounds());
        }
    }
}
