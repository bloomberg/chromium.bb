/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ComputedStyle_h
#define ComputedStyle_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/style/BorderValue.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/CounterDirectives.h"
#include "core/style/DataRef.h"
#include "core/style/LineClampValue.h"
#include "core/style/NinePieceImage.h"
#include "core/style/SVGComputedStyle.h"
#include "core/style/StyleBackgroundData.h"
#include "core/style/StyleBoxData.h"
#include "core/style/StyleContentAlignmentData.h"
#include "core/style/StyleDeprecatedFlexibleBoxData.h"
#include "core/style/StyleDifference.h"
#include "core/style/StyleFilterData.h"
#include "core/style/StyleFlexibleBoxData.h"
#include "core/style/StyleGridData.h"
#include "core/style/StyleGridItemData.h"
#include "core/style/StyleInheritedData.h"
#include "core/style/StyleMotionRotation.h"
#include "core/style/StyleMultiColData.h"
#include "core/style/StyleRareInheritedData.h"
#include "core/style/StyleRareNonInheritedData.h"
#include "core/style/StyleReflection.h"
#include "core/style/StyleSelfAlignmentData.h"
#include "core/style/StyleSurroundData.h"
#include "core/style/StyleTransformData.h"
#include "core/style/StyleVisualData.h"
#include "core/style/StyleWillChangeData.h"
#include "core/style/TransformOrigin.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"
#include "platform/LengthPoint.h"
#include "platform/LengthSize.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextDirection.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/transforms/TransformOperations.h"
#include "wtf/Forward.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

template<typename T, typename U> inline bool compareEqual(const T& t, const U& u) { return t == static_cast<T>(u); }

#define SET_VAR(group, variable, value) \
    if (!compareEqual(group->variable, value)) \
        group.access()->variable = value

#define SET_NESTED_VAR(group, base, variable, value)            \
    if (!compareEqual(group->base->variable, value))            \
        group.access()->base.access()->variable = value

#define SET_VAR_WITH_SETTER(group, getter, setter, value)    \
    if (!compareEqual(group->getter(), value)) \
        group.access()->setter(value)

#define SET_BORDERVALUE_COLOR(group, variable, value) \
    if (!compareEqual(group->variable.color(), value)) \
        group.access()->variable.setColor(value)

namespace blink {

using std::max;

class FilterOperations;

class AppliedTextDecoration;
class BorderData;
struct BorderEdge;
class CSSAnimationData;
class CSSTransitionData;
class CSSVariableData;
class Font;
class FontMetrics;
class Hyphenation;
class RotateTransformOperation;
class ScaleTransformOperation;
class ShadowList;
class ShapeValue;
class StyleImage;
class StyleInheritedData;
class StylePath;
class StyleResolver;
class TransformationMatrix;
class TranslateTransformOperation;

class ContentData;

typedef Vector<RefPtr<ComputedStyle>, 4> PseudoStyleCache;

class CORE_EXPORT ComputedStyle: public RefCounted<ComputedStyle> {
    friend class AnimatedStyleBuilder; // Used by Web Animations CSS. Sets the color styles
    friend class CSSAnimatableValueFactory; // Used by Web Animations CSS. Gets visited and unvisited colors separately.
    friend class CSSPropertyEquality; // Used by CSS animations. We can't allow them to animate based off visited colors.
    friend class ApplyStyleCommand; // Editing has to only reveal unvisited info.
    friend class EditingStyle; // Editing has to only reveal unvisited info.
    friend class ComputedStyleCSSValueMapping; // Needs to be able to see visited and unvisited colors for devtools.
    friend class StyleBuilderFunctions; // Sets color styles
    friend class CachedUAStyle; // Saves Border/Background information for later comparison.
    friend class ColorPropertyFunctions; // Accesses visited and unvisited colors.

    // FIXME: When we stop resolving currentColor at style time, these can be removed.
    friend class CSSToStyleMap;
    friend class FilterOperationResolver;
    friend class StyleBuilderConverter;
    friend class StyleResolverState;
    friend class StyleResolver;
protected:

    // non-inherited attributes
    DataRef<StyleBoxData> m_box;
    DataRef<StyleVisualData> m_visual;
    DataRef<StyleBackgroundData> m_background;
    DataRef<StyleSurroundData> m_surround;
    DataRef<StyleRareNonInheritedData> m_rareNonInheritedData;

    // inherited attributes
    DataRef<StyleRareInheritedData> m_rareInheritedData;
    DataRef<StyleInheritedData> m_styleInheritedData;

    // list of associated pseudo styles
    std::unique_ptr<PseudoStyleCache> m_cachedPseudoStyles;

    DataRef<SVGComputedStyle> m_svgStyle;

// !START SYNC!: Keep this in sync with the copy constructor in ComputedStyle.cpp and implicitlyInherited() in StyleResolver.cpp

    // inherit
    struct InheritedData {
        bool operator==(const InheritedData& other) const
        {
            return (m_emptyCells == other.m_emptyCells)
                && (m_captionSide == other.m_captionSide)
                && (m_listStyleType == other.m_listStyleType)
                && (m_listStylePosition == other.m_listStylePosition)
                && (m_visibility == other.m_visibility)
                && (m_textAlign == other.m_textAlign)
                && (m_textTransform == other.m_textTransform)
                && (m_textUnderline == other.m_textUnderline)
                && (m_cursorStyle == other.m_cursorStyle)
                && (m_direction == other.m_direction)
                && (m_whiteSpace == other.m_whiteSpace)
                && (m_borderCollapse == other.m_borderCollapse)
                && (m_boxDirection == other.m_boxDirection)
                && (m_rtlOrdering == other.m_rtlOrdering)
                && (m_printColorAdjust == other.m_printColorAdjust)
                && (m_pointerEvents == other.m_pointerEvents)
                && (m_insideLink == other.m_insideLink)
                && (m_writingMode == other.m_writingMode);
        }

        bool operator!=(const InheritedData& other) const { return !(*this == other); }

        unsigned m_emptyCells : 1; // EEmptyCells
        unsigned m_captionSide : 2; // ECaptionSide
        unsigned m_listStyleType : 7; // EListStyleType
        unsigned m_listStylePosition : 1; // EListStylePosition
        unsigned m_visibility : 2; // EVisibility
        unsigned m_textAlign : 4; // ETextAlign
        unsigned m_textTransform : 2; // ETextTransform
        unsigned m_textUnderline : 1;
        unsigned m_cursorStyle : 6; // ECursor
        unsigned m_direction : 1; // TextDirection
        unsigned m_whiteSpace : 3; // EWhiteSpace
        unsigned m_borderCollapse : 1; // EBorderCollapse
        unsigned m_boxDirection : 1; // EBoxDirection (CSS3 box_direction property, flexible box layout module)
        // 32 bits

        // non CSS2 inherited
        unsigned m_rtlOrdering : 1; // Order
        unsigned m_printColorAdjust : PrintColorAdjustBits;
        unsigned m_pointerEvents : 4; // EPointerEvents
        unsigned m_insideLink : 2; // EInsideLink

        // CSS Text Layout Module Level 3: Vertical writing support
        unsigned m_writingMode : 2; // WritingMode
        // 42 bits
    } m_inheritedData;

// don't inherit
    struct NonInheritedData {
        // Compare computed styles, differences in other flags should not cause an inequality.
        bool operator==(const NonInheritedData& other) const
        {
            return m_effectiveDisplay == other.m_effectiveDisplay
                && m_originalDisplay == other.m_originalDisplay
                && m_overflowAnchor == other.m_overflowAnchor
                && m_overflowX == other.m_overflowX
                && m_overflowY == other.m_overflowY
                && m_verticalAlign == other.m_verticalAlign
                && m_clear == other.m_clear
                && m_position == other.m_position
                && m_floating == other.m_floating
                && m_tableLayout == other.m_tableLayout
                && m_unicodeBidi == other.m_unicodeBidi
                // hasViewportUnits
                && m_breakBefore == other.m_breakBefore
                && m_breakAfter == other.m_breakAfter
                && m_breakInside == other.m_breakInside;
                // styleType
                // pseudoBits
                // explicitInheritance
                // unique
                // emptyState
                // affectedByFocus
                // affectedByHover
                // affectedByActive
                // affectedByDrag
                // isLink
        }

        bool operator!=(const NonInheritedData& other) const { return !(*this == other); }

        unsigned m_effectiveDisplay : 5; // EDisplay
        unsigned m_originalDisplay : 5; // EDisplay
        unsigned m_overflowAnchor : 2; // EOverflowAnchor
        unsigned m_overflowX : 3; // EOverflow
        unsigned m_overflowY : 3; // EOverflow
        unsigned m_verticalAlign : 4; // EVerticalAlign
        unsigned m_clear : 2; // EClear
        unsigned m_position : 3; // EPosition
        unsigned m_floating : 2; // EFloat
        unsigned m_tableLayout : 1; // ETableLayout
        unsigned m_unicodeBidi : 3; // EUnicodeBidi

        // This is set if we used viewport units when resolving a length.
        // It is mutable so we can pass around const ComputedStyles to resolve lengths.
        mutable unsigned m_hasViewportUnits : 1;

        // 32 bits

        unsigned m_breakBefore : 4; // EBreak
        unsigned m_breakAfter : 4; // EBreak
        unsigned m_breakInside : 2; // EBreak

        unsigned m_styleType : 6; // PseudoId
        unsigned m_pseudoBits : 8;
        unsigned m_explicitInheritance : 1; // Explicitly inherits a non-inherited property
        unsigned m_variableReference : 1; // A non-inherited property references a variable.
        unsigned m_unique : 1; // Style can not be shared.

        unsigned m_emptyState : 1;

        unsigned m_affectedByFocus : 1;
        unsigned m_affectedByHover : 1;
        unsigned m_affectedByActive : 1;
        unsigned m_affectedByDrag : 1;

        // 64 bits

        unsigned m_isLink : 1;

        mutable unsigned m_hasRemUnits : 1;
        // If you add more style bits here, you will also need to update ComputedStyle::copyNonInheritedFromCached()
        // 66 bits
    } m_nonInheritedData;

// !END SYNC!

protected:
    void setBitDefaults()
    {
        m_inheritedData.m_emptyCells = initialEmptyCells();
        m_inheritedData.m_captionSide = initialCaptionSide();
        m_inheritedData.m_listStyleType = initialListStyleType();
        m_inheritedData.m_listStylePosition = initialListStylePosition();
        m_inheritedData.m_visibility = initialVisibility();
        m_inheritedData.m_textAlign = initialTextAlign();
        m_inheritedData.m_textTransform = initialTextTransform();
        m_inheritedData.m_textUnderline = false;
        m_inheritedData.m_cursorStyle = initialCursor();
        m_inheritedData.m_direction = initialDirection();
        m_inheritedData.m_whiteSpace = initialWhiteSpace();
        m_inheritedData.m_borderCollapse = initialBorderCollapse();
        m_inheritedData.m_rtlOrdering = initialRTLOrdering();
        m_inheritedData.m_boxDirection = initialBoxDirection();
        m_inheritedData.m_printColorAdjust = initialPrintColorAdjust();
        m_inheritedData.m_pointerEvents = initialPointerEvents();
        m_inheritedData.m_insideLink = NotInsideLink;
        m_inheritedData.m_writingMode = initialWritingMode();

        m_nonInheritedData.m_effectiveDisplay = m_nonInheritedData.m_originalDisplay = initialDisplay();
        m_nonInheritedData.m_overflowAnchor = initialOverflowAnchor();
        m_nonInheritedData.m_overflowX = initialOverflowX();
        m_nonInheritedData.m_overflowY = initialOverflowY();
        m_nonInheritedData.m_verticalAlign = initialVerticalAlign();
        m_nonInheritedData.m_clear = initialClear();
        m_nonInheritedData.m_position = initialPosition();
        m_nonInheritedData.m_floating = initialFloating();
        m_nonInheritedData.m_tableLayout = initialTableLayout();
        m_nonInheritedData.m_unicodeBidi = initialUnicodeBidi();
        m_nonInheritedData.m_breakBefore = initialBreakBefore();
        m_nonInheritedData.m_breakAfter = initialBreakAfter();
        m_nonInheritedData.m_breakInside = initialBreakInside();
        m_nonInheritedData.m_styleType = PseudoIdNone;
        m_nonInheritedData.m_pseudoBits = 0;
        m_nonInheritedData.m_explicitInheritance = false;
        m_nonInheritedData.m_variableReference = false;
        m_nonInheritedData.m_unique = false;
        m_nonInheritedData.m_emptyState = false;
        m_nonInheritedData.m_hasViewportUnits = false;
        m_nonInheritedData.m_affectedByFocus = false;
        m_nonInheritedData.m_affectedByHover = false;
        m_nonInheritedData.m_affectedByActive = false;
        m_nonInheritedData.m_affectedByDrag = false;
        m_nonInheritedData.m_isLink = false;
        m_nonInheritedData.m_hasRemUnits = false;
    }

private:
    ALWAYS_INLINE ComputedStyle();

    enum InitialStyleTag {
        InitialStyle
    };
    ALWAYS_INLINE explicit ComputedStyle(InitialStyleTag);
    ALWAYS_INLINE ComputedStyle(const ComputedStyle&);

    static PassRefPtr<ComputedStyle> createInitialStyle();
    static inline ComputedStyle& mutableInitialStyle()
    {
        LEAK_SANITIZER_DISABLED_SCOPE;
        DEFINE_STATIC_REF(ComputedStyle, s_initialStyle, (ComputedStyle::createInitialStyle()));
        return *s_initialStyle;
    }

public:
    static PassRefPtr<ComputedStyle> create();
    static PassRefPtr<ComputedStyle> createAnonymousStyleWithDisplay(const ComputedStyle& parentStyle, EDisplay);
    static PassRefPtr<ComputedStyle> clone(const ComputedStyle&);
    static const ComputedStyle& initialStyle() { return mutableInitialStyle(); }
    static void invalidateInitialStyle();

    // Computes how the style change should be propagated down the tree.
    static StyleRecalcChange stylePropagationDiff(const ComputedStyle* oldStyle, const ComputedStyle* newStyle);

    ContentPosition resolvedJustifyContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistributionType resolvedJustifyContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentPosition resolvedAlignContentPosition(const StyleContentAlignmentData& normalValueBehavior) const;
    ContentDistributionType resolvedAlignContentDistribution(const StyleContentAlignmentData& normalValueBehavior) const;
    const StyleSelfAlignmentData resolvedAlignment(const ComputedStyle& parentStyle, ItemPosition resolvedAutoPositionForLayoutObject) const;
    static ItemPosition resolveAlignment(const ComputedStyle& parentStyle, const ComputedStyle& childStyle, ItemPosition resolvedAutoPositionForLayoutObject);
    static ItemPosition resolveJustification(const ComputedStyle& parentStyle, const ComputedStyle& childStyle, ItemPosition resolvedAutoPositionForLayoutObject);

    StyleDifference visualInvalidationDiff(const ComputedStyle&) const;

    enum IsAtShadowBoundary {
        AtShadowBoundary,
        NotAtShadowBoundary,
    };

    void inheritFrom(const ComputedStyle& inheritParent, IsAtShadowBoundary = NotAtShadowBoundary);
    void copyNonInheritedFromCached(const ComputedStyle&);

    PseudoId styleType() const { return static_cast<PseudoId>(m_nonInheritedData.m_styleType); }
    void setStyleType(PseudoId styleType) { m_nonInheritedData.m_styleType = styleType; }

    ComputedStyle* getCachedPseudoStyle(PseudoId) const;
    ComputedStyle* addCachedPseudoStyle(PassRefPtr<ComputedStyle>);
    void removeCachedPseudoStyle(PseudoId);

    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    void setHasViewportUnits(bool hasViewportUnits = true) const { m_nonInheritedData.m_hasViewportUnits = hasViewportUnits; }
    bool hasViewportUnits() const { return m_nonInheritedData.m_hasViewportUnits; }

    void setHasRemUnits() const { m_nonInheritedData.m_hasRemUnits = true; }
    bool hasRemUnits() const { return m_nonInheritedData.m_hasRemUnits; }

    bool affectedByFocus() const { return m_nonInheritedData.m_affectedByFocus; }
    bool affectedByHover() const { return m_nonInheritedData.m_affectedByHover; }
    bool affectedByActive() const { return m_nonInheritedData.m_affectedByActive; }
    bool affectedByDrag() const { return m_nonInheritedData.m_affectedByDrag; }

    void setAffectedByFocus() { m_nonInheritedData.m_affectedByFocus = true; }
    void setAffectedByHover() { m_nonInheritedData.m_affectedByHover = true; }
    void setAffectedByActive() { m_nonInheritedData.m_affectedByActive = true; }
    void setAffectedByDrag() { m_nonInheritedData.m_affectedByDrag = true; }

    bool operator==(const ComputedStyle& other) const;
    bool operator!=(const ComputedStyle& other) const { return !(*this == other); }
    bool isFloating() const { return m_nonInheritedData.m_floating != NoFloat; }
    bool hasMargin() const { return m_surround->margin.nonZero(); }
    bool hasBorderFill() const { return m_surround->border.hasBorderFill(); }
    bool hasBorder() const { return m_surround->border.hasBorder(); }
    bool hasBorderDecoration() const { return hasBorder() || hasBorderFill(); }
    bool hasPadding() const { return m_surround->padding.nonZero(); }
    bool hasMarginBeforeQuirk() const { return marginBefore().quirk(); }
    bool hasMarginAfterQuirk() const { return marginAfter().quirk(); }

    bool hasBackgroundImage() const { return m_background->background().hasImage(); }
    bool hasFixedBackgroundImage() const { return m_background->background().hasFixedImage(); }

    bool hasEntirelyFixedBackground() const;

    bool hasAppearance() const { return appearance() != NoControlPart; }

    bool hasBackgroundRelatedColorReferencingCurrentColor() const
    {
        if (backgroundColor().isCurrentColor() || visitedLinkBackgroundColor().isCurrentColor())
            return true;
        if (!boxShadow())
            return false;
        return shadowListHasCurrentColor(boxShadow());
    }

    bool hasBackground() const
    {
        Color color = visitedDependentColor(CSSPropertyBackgroundColor);
        if (color.alpha())
            return true;
        return hasBackgroundImage();
    }

    LayoutRectOutsets imageOutsets(const NinePieceImage&) const;
    bool hasBorderImageOutsets() const
    {
        return borderImage().hasImage() && borderImage().outset().nonZero();
    }
    LayoutRectOutsets borderImageOutsets() const
    {
        return imageOutsets(borderImage());
    }

    // Returns |true| if any property that renders using filter operations is
    // used (including, but not limited to, 'filter').
    bool hasFilterInducingProperty() const { return hasFilter() || (RuntimeEnabledFeatures::cssBoxReflectFilterEnabled() && hasBoxReflect()); }

    // Returns |true| if opacity should be considered to have non-initial value for the purpose
    // of creating stacking contexts.
    bool hasNonInitialOpacity() const { return hasOpacity() || hasWillChangeOpacityHint() || hasCurrentOpacityAnimation(); }

    // Returns whether this style contains any grouping property as defined by [css-transforms].
    // The main purpose of this is to adjust the used value of transform-style property.
    // Note: We currently don't include every grouping property on the spec to maintain
    //       backward compatibility.
    // [css-transforms] https://drafts.csswg.org/css-transforms/#grouping-property-values
    bool hasGroupingProperty() const { return !isOverflowVisible() || hasFilterInducingProperty() || hasNonInitialOpacity(); }

    Order rtlOrdering() const { return static_cast<Order>(m_inheritedData.m_rtlOrdering); }
    void setRTLOrdering(Order o) { m_inheritedData.m_rtlOrdering = o; }

    bool isStyleAvailable() const;

    bool hasAnyPublicPseudoStyles() const;
    bool hasPseudoStyle(PseudoId) const;
    void setHasPseudoStyle(PseudoId);
    bool hasUniquePseudoStyle() const;
    bool hasPseudoElementStyle() const;

    // Note: canContainAbsolutePositionObjects should return true if canContainFixedPositionObjects.
    // We currently never use this value directly, always OR'ing it with canContainFixedPositionObjects.
    bool canContainAbsolutePositionObjects() const { return position() != StaticPosition; }
    bool canContainFixedPositionObjects() const { return hasTransformRelatedProperty() || containsPaint();}

    // attribute getter methods

    EDisplay display() const { return static_cast<EDisplay>(m_nonInheritedData.m_effectiveDisplay); }
    EDisplay originalDisplay() const { return static_cast<EDisplay>(m_nonInheritedData.m_originalDisplay); }

    const Length& left() const { return m_surround->offset.left(); }
    const Length& right() const { return m_surround->offset.right(); }
    const Length& top() const { return m_surround->offset.top(); }
    const Length& bottom() const { return m_surround->offset.bottom(); }

    // Accessors for positioned object edges that take into account writing mode.
    const Length& logicalLeft() const { return m_surround->offset.logicalLeft(getWritingMode()); }
    const Length& logicalRight() const { return m_surround->offset.logicalRight(getWritingMode()); }
    const Length& logicalTop() const { return m_surround->offset.before(getWritingMode()); }
    const Length& logicalBottom() const { return m_surround->offset.after(getWritingMode()); }

    // Whether or not a positioned element requires normal flow x/y to be computed
    // to determine its position.
    bool hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
    bool hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }
    bool hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
    bool hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }

    EPosition position() const { return static_cast<EPosition>(m_nonInheritedData.m_position); }
    bool hasOutOfFlowPosition() const { return position() == AbsolutePosition || position() == FixedPosition; }
    bool hasInFlowPosition() const { return position() == RelativePosition || position() == StickyPosition; }
    bool hasViewportConstrainedPosition() const { return position() == FixedPosition || position() == StickyPosition; }
    EFloat floating() const { return static_cast<EFloat>(m_nonInheritedData.m_floating); }

    const Length& width() const { return m_box->width(); }
    const Length& height() const { return m_box->height(); }
    const Length& minWidth() const { return m_box->minWidth(); }
    const Length& maxWidth() const { return m_box->maxWidth(); }
    const Length& minHeight() const { return m_box->minHeight(); }
    const Length& maxHeight() const { return m_box->maxHeight(); }

    const Length& logicalWidth() const { return isHorizontalWritingMode() ? width() : height(); }
    const Length& logicalHeight() const { return isHorizontalWritingMode() ? height() : width(); }
    const Length& logicalMinWidth() const { return isHorizontalWritingMode() ? minWidth() : minHeight(); }
    const Length& logicalMaxWidth() const { return isHorizontalWritingMode() ? maxWidth() : maxHeight(); }
    const Length& logicalMinHeight() const { return isHorizontalWritingMode() ? minHeight() : minWidth(); }
    const Length& logicalMaxHeight() const { return isHorizontalWritingMode() ? maxHeight() : maxWidth(); }

    const BorderData& border() const { return m_surround->border; }
    const BorderValue& borderLeft() const { return m_surround->border.left(); }
    const BorderValue& borderRight() const { return m_surround->border.right(); }
    const BorderValue& borderTop() const { return m_surround->border.top(); }
    const BorderValue& borderBottom() const { return m_surround->border.bottom(); }

    const BorderValue& borderBefore() const;
    const BorderValue& borderAfter() const;
    const BorderValue& borderStart() const;
    const BorderValue& borderEnd() const;

    const NinePieceImage& borderImage() const { return m_surround->border.image(); }
    StyleImage* borderImageSource() const { return m_surround->border.image().image(); }
    const LengthBox& borderImageSlices() const { return m_surround->border.image().imageSlices(); }
    bool borderImageSlicesFill() const { return m_surround->border.image().fill(); }
    const BorderImageLengthBox& borderImageWidth() const { return m_surround->border.image().borderSlices(); }
    const BorderImageLengthBox& borderImageOutset() const { return m_surround->border.image().outset(); }

    const LengthSize& borderTopLeftRadius() const { return m_surround->border.topLeft(); }
    const LengthSize& borderTopRightRadius() const { return m_surround->border.topRight(); }
    const LengthSize& borderBottomLeftRadius() const { return m_surround->border.bottomLeft(); }
    const LengthSize& borderBottomRightRadius() const { return m_surround->border.bottomRight(); }
    bool hasBorderRadius() const { return m_surround->border.hasBorderRadius(); }

    int borderLeftWidth() const { return m_surround->border.borderLeftWidth(); }
    EBorderStyle borderLeftStyle() const { return m_surround->border.left().style(); }
    int borderRightWidth() const { return m_surround->border.borderRightWidth(); }
    EBorderStyle borderRightStyle() const { return m_surround->border.right().style(); }
    int borderTopWidth() const { return m_surround->border.borderTopWidth(); }
    EBorderStyle borderTopStyle() const { return m_surround->border.top().style(); }
    int borderBottomWidth() const { return m_surround->border.borderBottomWidth(); }
    EBorderStyle borderBottomStyle() const { return m_surround->border.bottom().style(); }

    int borderBeforeWidth() const;
    int borderAfterWidth() const;
    int borderStartWidth() const;
    int borderEndWidth() const;
    int borderOverWidth() const;
    int borderUnderWidth() const;

    int outlineWidth() const
    {
        if (m_background->outline().style() == BorderStyleNone)
            return 0;
        return m_background->outline().width();
    }
    bool hasOutline() const { return outlineWidth() > 0 && outlineStyle() > BorderStyleHidden; }
    EBorderStyle outlineStyle() const { return m_background->outline().style(); }
    OutlineIsAuto outlineStyleIsAuto() const { return static_cast<OutlineIsAuto>(m_background->outline().isAuto()); }
    int outlineOutsetExtent() const;

    EOverflowAnchor overflowAnchor() const { return static_cast<EOverflowAnchor>(m_nonInheritedData.m_overflowAnchor); }
    EOverflow overflowX() const { return static_cast<EOverflow>(m_nonInheritedData.m_overflowX); }
    EOverflow overflowY() const { return static_cast<EOverflow>(m_nonInheritedData.m_overflowY); }
    // It's sufficient to just check one direction, since it's illegal to have visible on only one overflow value.
    bool isOverflowVisible() const { ASSERT(overflowX() != OverflowVisible || overflowX() == overflowY()); return overflowX() == OverflowVisible; }
    bool isOverflowPaged() const { return overflowY() == OverflowPagedX || overflowY() == OverflowPagedY; }

    EVisibility visibility() const { return static_cast<EVisibility>(m_inheritedData.m_visibility); }
    EVerticalAlign verticalAlign() const { return static_cast<EVerticalAlign>(m_nonInheritedData.m_verticalAlign); }
    const Length& getVerticalAlignLength() const { return m_box->verticalAlign(); }

    const Length& clipLeft() const { return m_visual->clip.left(); }
    const Length& clipRight() const { return m_visual->clip.right(); }
    const Length& clipTop() const { return m_visual->clip.top(); }
    const Length& clipBottom() const { return m_visual->clip.bottom(); }
    const LengthBox& clip() const { return m_visual->clip; }
    bool hasAutoClip() const { return m_visual->hasAutoClip; }

    EUnicodeBidi unicodeBidi() const { return static_cast<EUnicodeBidi>(m_nonInheritedData.m_unicodeBidi); }

    EClear clear() const { return static_cast<EClear>(m_nonInheritedData.m_clear); }
    ETableLayout tableLayout() const { return static_cast<ETableLayout>(m_nonInheritedData.m_tableLayout); }
    bool isFixedTableLayout() const { return tableLayout() == TableLayoutFixed && !logicalWidth().isAuto(); }

    const Font& font() const;
    const FontMetrics& getFontMetrics() const;
    const FontDescription& getFontDescription() const;
    float specifiedFontSize() const;
    float computedFontSize() const;
    int fontSize() const;
    float fontSizeAdjust() const;
    bool hasFontSizeAdjust() const;
    FontWeight fontWeight() const;
    FontStretch fontStretch() const;

    float textAutosizingMultiplier() const { return m_styleInheritedData->textAutosizingMultiplier; }

    const Length& textIndent() const { return m_rareInheritedData->indent; }
    TextIndentLine getTextIndentLine() const { return static_cast<TextIndentLine>(m_rareInheritedData->m_textIndentLine); }
    TextIndentType getTextIndentType() const { return static_cast<TextIndentType>(m_rareInheritedData->m_textIndentType); }
    ETextAlign textAlign() const { return static_cast<ETextAlign>(m_inheritedData.m_textAlign); }
    TextAlignLast getTextAlignLast() const { return static_cast<TextAlignLast>(m_rareInheritedData->m_textAlignLast); }
    TextJustify getTextJustify() const { return static_cast<TextJustify>(m_rareInheritedData->m_textJustify); }
    ETextTransform textTransform() const { return static_cast<ETextTransform>(m_inheritedData.m_textTransform); }
    TextDecoration textDecorationsInEffect() const;
    const Vector<AppliedTextDecoration>& appliedTextDecorations() const;
    TextDecoration getTextDecoration() const { return static_cast<TextDecoration>(m_visual->textDecoration); }
    TextUnderlinePosition getTextUnderlinePosition() const { return static_cast<TextUnderlinePosition>(m_rareInheritedData->m_textUnderlinePosition); }
    TextDecorationStyle getTextDecorationStyle() const { return static_cast<TextDecorationStyle>(m_rareNonInheritedData->m_textDecorationStyle); }
    float wordSpacing() const;
    float letterSpacing() const;
    StyleVariableData* variables() const;

    void setVariable(const AtomicString&, PassRefPtr<CSSVariableData>);
    void removeVariable(const AtomicString&);

    float zoom() const { return m_visual->m_zoom; }
    float effectiveZoom() const { return m_rareInheritedData->m_effectiveZoom; }

    TextDirection direction() const { return static_cast<TextDirection>(m_inheritedData.m_direction); }
    bool isLeftToRightDirection() const { return direction() == LTR; }
    bool selfOrAncestorHasDirAutoAttribute() const { return m_rareInheritedData->m_selfOrAncestorHasDirAutoAttribute; }

    const Length& specifiedLineHeight() const;
    Length lineHeight() const;
    int computedLineHeight() const;

    EWhiteSpace whiteSpace() const { return static_cast<EWhiteSpace>(m_inheritedData.m_whiteSpace); }
    static bool autoWrap(EWhiteSpace ws)
    {
        // Nowrap and pre don't automatically wrap.
        return ws != NOWRAP && ws != PRE;
    }

    bool autoWrap() const
    {
        return autoWrap(whiteSpace());
    }

    static bool preserveNewline(EWhiteSpace ws)
    {
        // Normal and nowrap do not preserve newlines.
        return ws != NORMAL && ws != NOWRAP;
    }

    bool preserveNewline() const
    {
        return preserveNewline(whiteSpace());
    }

    static bool collapseWhiteSpace(EWhiteSpace ws)
    {
        // Pre and prewrap do not collapse whitespace.
        return ws != PRE && ws != PRE_WRAP;
    }

    bool collapseWhiteSpace() const
    {
        return collapseWhiteSpace(whiteSpace());
    }

    bool isCollapsibleWhiteSpace(UChar c) const
    {
        switch (c) {
        case ' ':
        case '\t':
            return collapseWhiteSpace();
        case '\n':
            return !preserveNewline();
        }
        return false;
    }

    bool breakOnlyAfterWhiteSpace() const
    {
        return whiteSpace() == PRE_WRAP || getLineBreak() == LineBreakAfterWhiteSpace;
    }

    bool breakWords() const
    {
        return (wordBreak() == BreakWordBreak || overflowWrap() == BreakOverflowWrap)
            && whiteSpace() != PRE && whiteSpace() != NOWRAP;
    }

    EFillBox backgroundClip() const { return static_cast<EFillBox>(m_background->background().clip()); }
    FillLayer& accessBackgroundLayers() { return m_background.access()->m_background; }
    const FillLayer& backgroundLayers() const { return m_background->background(); }

    StyleImage* maskImage() const { return m_rareNonInheritedData->m_mask.image(); }
    FillLayer& accessMaskLayers() { return m_rareNonInheritedData.access()->m_mask; }
    const FillLayer& maskLayers() const { return m_rareNonInheritedData->m_mask; }

    const NinePieceImage& maskBoxImage() const { return m_rareNonInheritedData->m_maskBoxImage; }
    StyleImage* maskBoxImageSource() const { return m_rareNonInheritedData->m_maskBoxImage.image(); }
    const LengthBox& maskBoxImageSlices() const { return m_rareNonInheritedData->m_maskBoxImage.imageSlices(); }
    bool maskBoxImageSlicesFill() const { return m_rareNonInheritedData->m_maskBoxImage.fill(); }
    const BorderImageLengthBox& maskBoxImageWidth() const { return m_rareNonInheritedData->m_maskBoxImage.borderSlices(); }
    const BorderImageLengthBox& maskBoxImageOutset() const { return m_rareNonInheritedData->m_maskBoxImage.outset(); }

    EBorderCollapse borderCollapse() const { return static_cast<EBorderCollapse>(m_inheritedData.m_borderCollapse); }
    short horizontalBorderSpacing() const;
    short verticalBorderSpacing() const;
    EEmptyCells emptyCells() const { return static_cast<EEmptyCells>(m_inheritedData.m_emptyCells); }
    ECaptionSide captionSide() const { return static_cast<ECaptionSide>(m_inheritedData.m_captionSide); }

    EListStyleType listStyleType() const { return static_cast<EListStyleType>(m_inheritedData.m_listStyleType); }
    StyleImage* listStyleImage() const;
    EListStylePosition listStylePosition() const { return static_cast<EListStylePosition>(m_inheritedData.m_listStylePosition); }

    const Length& marginTop() const { return m_surround->margin.top(); }
    const Length& marginBottom() const { return m_surround->margin.bottom(); }
    const Length& marginLeft() const { return m_surround->margin.left(); }
    const Length& marginRight() const { return m_surround->margin.right(); }
    const Length& marginBefore() const { return m_surround->margin.before(getWritingMode()); }
    const Length& marginAfter() const { return m_surround->margin.after(getWritingMode()); }
    const Length& marginStart() const { return m_surround->margin.start(getWritingMode(), direction()); }
    const Length& marginEnd() const { return m_surround->margin.end(getWritingMode(), direction()); }
    const Length& marginOver() const { return m_surround->margin.over(getWritingMode()); }
    const Length& marginUnder() const { return m_surround->margin.under(getWritingMode()); }
    const Length& marginStartUsing(const ComputedStyle* otherStyle) const { return m_surround->margin.start(otherStyle->getWritingMode(), otherStyle->direction()); }
    const Length& marginEndUsing(const ComputedStyle* otherStyle) const { return m_surround->margin.end(otherStyle->getWritingMode(), otherStyle->direction()); }
    const Length& marginBeforeUsing(const ComputedStyle* otherStyle) const { return m_surround->margin.before(otherStyle->getWritingMode()); }
    const Length& marginAfterUsing(const ComputedStyle* otherStyle) const { return m_surround->margin.after(otherStyle->getWritingMode()); }

    const LengthBox& paddingBox() const { return m_surround->padding; }
    const Length& paddingTop() const { return m_surround->padding.top(); }
    const Length& paddingBottom() const { return m_surround->padding.bottom(); }
    const Length& paddingLeft() const { return m_surround->padding.left(); }
    const Length& paddingRight() const { return m_surround->padding.right(); }
    const Length& paddingBefore() const { return m_surround->padding.before(getWritingMode()); }
    const Length& paddingAfter() const { return m_surround->padding.after(getWritingMode()); }
    const Length& paddingStart() const { return m_surround->padding.start(getWritingMode(), direction()); }
    const Length& paddingEnd() const { return m_surround->padding.end(getWritingMode(), direction()); }
    const Length& paddingOver() const { return m_surround->padding.over(getWritingMode()); }
    const Length& paddingUnder() const { return m_surround->padding.under(getWritingMode()); }

    ECursor cursor() const { return static_cast<ECursor>(m_inheritedData.m_cursorStyle); }
    CursorList* cursors() const { return m_rareInheritedData->cursorData.get(); }

    EInsideLink insideLink() const { return static_cast<EInsideLink>(m_inheritedData.m_insideLink); }
    bool isLink() const { return m_nonInheritedData.m_isLink; }

    short widows() const { return m_rareInheritedData->widows; }
    short orphans() const { return m_rareInheritedData->orphans; }
    EBreak breakAfter() const { return static_cast<EBreak>(m_nonInheritedData.m_breakAfter); }
    EBreak breakBefore() const { return static_cast<EBreak>(m_nonInheritedData.m_breakBefore); }
    EBreak breakInside() const { return static_cast<EBreak>(m_nonInheritedData.m_breakInside); }

    TextSizeAdjust getTextSizeAdjust() const { return m_rareInheritedData->m_textSizeAdjust; }

    // CSS3 Getter Methods

    int outlineOffset() const
    {
        if (m_background->outline().style() == BorderStyleNone)
            return 0;
        return m_background->outline().offset();
    }

    ShadowList* textShadow() const { return m_rareInheritedData->textShadow.get(); }

    float textStrokeWidth() const { return m_rareInheritedData->textStrokeWidth; }
    float opacity() const { return m_rareNonInheritedData->opacity; }
    bool hasOpacity() const { return opacity() < 1.0f; }
    ControlPart appearance() const { return static_cast<ControlPart>(m_rareNonInheritedData->m_appearance); }
    EBoxAlignment boxAlign() const { return static_cast<EBoxAlignment>(m_rareNonInheritedData->m_deprecatedFlexibleBox->align); }
    EBoxDirection boxDirection() const { return static_cast<EBoxDirection>(m_inheritedData.m_boxDirection); }
    float boxFlex() const { return m_rareNonInheritedData->m_deprecatedFlexibleBox->flex; }
    unsigned boxFlexGroup() const { return m_rareNonInheritedData->m_deprecatedFlexibleBox->flexGroup; }
    EBoxLines boxLines() const { return static_cast<EBoxLines>(m_rareNonInheritedData->m_deprecatedFlexibleBox->lines); }
    unsigned boxOrdinalGroup() const { return m_rareNonInheritedData->m_deprecatedFlexibleBox->ordinalGroup; }
    EBoxOrient boxOrient() const { return static_cast<EBoxOrient>(m_rareNonInheritedData->m_deprecatedFlexibleBox->orient); }
    EBoxPack boxPack() const { return static_cast<EBoxPack>(m_rareNonInheritedData->m_deprecatedFlexibleBox->pack); }

    int order() const { return m_rareNonInheritedData->m_order; }
    const Vector<String>& callbackSelectors() const { return m_rareNonInheritedData->m_callbackSelectors; }
    float flexGrow() const { return m_rareNonInheritedData->m_flexibleBox->m_flexGrow; }
    float flexShrink() const { return m_rareNonInheritedData->m_flexibleBox->m_flexShrink; }
    const Length& flexBasis() const { return m_rareNonInheritedData->m_flexibleBox->m_flexBasis; }
    const StyleContentAlignmentData& alignContent() const { return m_rareNonInheritedData->m_alignContent; }
    ContentPosition alignContentPosition() const { return m_rareNonInheritedData->m_alignContent.position(); }
    ContentDistributionType alignContentDistribution() const { return m_rareNonInheritedData->m_alignContent.distribution(); }
    OverflowAlignment alignContentOverflowAlignment() const { return m_rareNonInheritedData->m_alignContent.overflow(); }
    const StyleSelfAlignmentData& alignItems() const { return m_rareNonInheritedData->m_alignItems; }
    ItemPosition alignItemsPosition() const { return m_rareNonInheritedData->m_alignItems.position(); }
    OverflowAlignment alignItemsOverflowAlignment() const { return m_rareNonInheritedData->m_alignItems.overflow(); }
    const StyleSelfAlignmentData& alignSelf() const { return m_rareNonInheritedData->m_alignSelf; }
    ItemPosition alignSelfPosition() const { return m_rareNonInheritedData->m_alignSelf.position(); }
    OverflowAlignment alignSelfOverflowAlignment() const { return m_rareNonInheritedData->m_alignSelf.overflow(); }
    EFlexDirection flexDirection() const { return static_cast<EFlexDirection>(m_rareNonInheritedData->m_flexibleBox->m_flexDirection); }
    bool isColumnFlexDirection() const { return flexDirection() == FlowColumn || flexDirection() == FlowColumnReverse; }
    bool isReverseFlexDirection() const { return flexDirection() == FlowRowReverse || flexDirection() == FlowColumnReverse; }
    EFlexWrap flexWrap() const { return static_cast<EFlexWrap>(m_rareNonInheritedData->m_flexibleBox->m_flexWrap); }
    const StyleContentAlignmentData& justifyContent() const { return m_rareNonInheritedData->m_justifyContent; }
    ContentPosition justifyContentPosition() const { return m_rareNonInheritedData->m_justifyContent.position(); }
    ContentDistributionType justifyContentDistribution() const { return m_rareNonInheritedData->m_justifyContent.distribution(); }
    OverflowAlignment justifyContentOverflowAlignment() const { return m_rareNonInheritedData->m_justifyContent.overflow(); }
    const StyleSelfAlignmentData& justifyItems() const { return m_rareNonInheritedData->m_justifyItems; }
    ItemPosition justifyItemsPosition() const { return m_rareNonInheritedData->m_justifyItems.position(); }
    OverflowAlignment justifyItemsOverflowAlignment() const { return m_rareNonInheritedData->m_justifyItems.overflow(); }
    ItemPositionType justifyItemsPositionType() const { return m_rareNonInheritedData->m_justifyItems.positionType(); }
    const StyleSelfAlignmentData& justifySelf() const { return m_rareNonInheritedData->m_justifySelf; }
    ItemPosition justifySelfPosition() const { return m_rareNonInheritedData->m_justifySelf.position(); }
    OverflowAlignment justifySelfOverflowAlignment() const { return m_rareNonInheritedData->m_justifySelf.overflow(); }

    const Vector<GridTrackSize>& gridTemplateColumns() const { return m_rareNonInheritedData->m_grid->m_gridTemplateColumns; }
    const Vector<GridTrackSize>& gridTemplateRows() const { return m_rareNonInheritedData->m_grid->m_gridTemplateRows; }
    const Vector<GridTrackSize>& gridAutoRepeatColumns() const { return m_rareNonInheritedData->m_grid->m_gridAutoRepeatColumns; }
    const Vector<GridTrackSize>& gridAutoRepeatRows() const { return m_rareNonInheritedData->m_grid->m_gridAutoRepeatRows; }
    size_t gridAutoRepeatColumnsInsertionPoint() const { return m_rareNonInheritedData->m_grid->m_autoRepeatColumnsInsertionPoint; }
    size_t gridAutoRepeatRowsInsertionPoint() const { return m_rareNonInheritedData->m_grid->m_autoRepeatRowsInsertionPoint; }
    AutoRepeatType gridAutoRepeatColumnsType() const  { return m_rareNonInheritedData->m_grid->m_autoRepeatColumnsType; }
    AutoRepeatType gridAutoRepeatRowsType() const  { return m_rareNonInheritedData->m_grid->m_autoRepeatRowsType; }
    const NamedGridLinesMap& namedGridColumnLines() const { return m_rareNonInheritedData->m_grid->m_namedGridColumnLines; }
    const NamedGridLinesMap& namedGridRowLines() const { return m_rareNonInheritedData->m_grid->m_namedGridRowLines; }
    const OrderedNamedGridLines& orderedNamedGridColumnLines() const { return m_rareNonInheritedData->m_grid->m_orderedNamedGridColumnLines; }
    const OrderedNamedGridLines& orderedNamedGridRowLines() const { return m_rareNonInheritedData->m_grid->m_orderedNamedGridRowLines; }
    const NamedGridLinesMap& autoRepeatNamedGridColumnLines() const { return m_rareNonInheritedData->m_grid->m_autoRepeatNamedGridColumnLines; }
    const NamedGridLinesMap& autoRepeatNamedGridRowLines() const { return m_rareNonInheritedData->m_grid->m_autoRepeatNamedGridRowLines; }
    const OrderedNamedGridLines& autoRepeatOrderedNamedGridColumnLines() const { return m_rareNonInheritedData->m_grid->m_autoRepeatOrderedNamedGridColumnLines; }
    const OrderedNamedGridLines& autoRepeatOrderedNamedGridRowLines() const { return m_rareNonInheritedData->m_grid->m_autoRepeatOrderedNamedGridRowLines; }
    const NamedGridAreaMap& namedGridArea() const { return m_rareNonInheritedData->m_grid->m_namedGridArea; }
    size_t namedGridAreaRowCount() const { return m_rareNonInheritedData->m_grid->m_namedGridAreaRowCount; }
    size_t namedGridAreaColumnCount() const { return m_rareNonInheritedData->m_grid->m_namedGridAreaColumnCount; }
    GridAutoFlow getGridAutoFlow() const { return static_cast<GridAutoFlow>(m_rareNonInheritedData->m_grid->m_gridAutoFlow); }
    bool isGridAutoFlowDirectionRow() const { return (m_rareNonInheritedData->m_grid->m_gridAutoFlow & InternalAutoFlowDirectionRow) == InternalAutoFlowDirectionRow; }
    bool isGridAutoFlowDirectionColumn() const { return (m_rareNonInheritedData->m_grid->m_gridAutoFlow & InternalAutoFlowDirectionColumn) == InternalAutoFlowDirectionColumn; }
    bool isGridAutoFlowAlgorithmSparse() const { return (m_rareNonInheritedData->m_grid->m_gridAutoFlow & InternalAutoFlowAlgorithmSparse) == InternalAutoFlowAlgorithmSparse; }
    bool isGridAutoFlowAlgorithmDense() const { return (m_rareNonInheritedData->m_grid->m_gridAutoFlow & InternalAutoFlowAlgorithmDense) == InternalAutoFlowAlgorithmDense; }
    const Vector<GridTrackSize>& gridAutoColumns() const { return m_rareNonInheritedData->m_grid->m_gridAutoColumns; }
    const Vector<GridTrackSize>& gridAutoRows() const { return m_rareNonInheritedData->m_grid->m_gridAutoRows; }
    const Length& gridColumnGap() const { return m_rareNonInheritedData->m_grid->m_gridColumnGap; }
    const Length& gridRowGap() const { return m_rareNonInheritedData->m_grid->m_gridRowGap; }

    const GridPosition& gridColumnStart() const { return m_rareNonInheritedData->m_gridItem->m_gridColumnStart; }
    const GridPosition& gridColumnEnd() const { return m_rareNonInheritedData->m_gridItem->m_gridColumnEnd; }
    const GridPosition& gridRowStart() const { return m_rareNonInheritedData->m_gridItem->m_gridRowStart; }
    const GridPosition& gridRowEnd() const { return m_rareNonInheritedData->m_gridItem->m_gridRowEnd; }

    ShadowList* boxShadow() const { return m_rareNonInheritedData->m_boxShadow.get(); }

    EBoxDecorationBreak boxDecorationBreak() const { return m_box->boxDecorationBreak(); }
    StyleReflection* boxReflect() const { return m_rareNonInheritedData->m_boxReflect.get(); }
    bool hasBoxReflect() const { return boxReflect(); }
    bool reflectionDataEquivalent(const ComputedStyle* otherStyle) const { return m_rareNonInheritedData->reflectionDataEquivalent(*otherStyle->m_rareNonInheritedData); }

    // FIXME: reflections should belong to this helper function but they are currently handled
    // through their self-painting layers. So the layout code doesn't account for them.
    bool hasVisualOverflowingEffect() const { return boxShadow() || hasBorderImageOutsets() || hasOutline(); }

    Containment contain() const { return static_cast<Containment>(m_rareNonInheritedData->m_contain); }
    bool containsPaint() const { return m_rareNonInheritedData->m_contain & ContainsPaint; }
    bool containsStyle() const { return m_rareNonInheritedData->m_contain & ContainsStyle; }
    bool containsLayout() const { return m_rareNonInheritedData->m_contain & ContainsLayout; }
    bool containsSize() const { return m_rareNonInheritedData->m_contain & ContainsSize; }

    EBoxSizing boxSizing() const { return m_box->boxSizing(); }
    EUserModify userModify() const { return static_cast<EUserModify>(m_rareInheritedData->userModify); }
    EUserDrag userDrag() const { return static_cast<EUserDrag>(m_rareNonInheritedData->userDrag); }
    EUserSelect userSelect() const { return static_cast<EUserSelect>(m_rareInheritedData->userSelect); }
    TextOverflow getTextOverflow() const { return static_cast<TextOverflow>(m_rareNonInheritedData->textOverflow); }
    EMarginCollapse marginBeforeCollapse() const { return static_cast<EMarginCollapse>(m_rareNonInheritedData->marginBeforeCollapse); }
    EMarginCollapse marginAfterCollapse() const { return static_cast<EMarginCollapse>(m_rareNonInheritedData->marginAfterCollapse); }
    EWordBreak wordBreak() const { return static_cast<EWordBreak>(m_rareInheritedData->wordBreak); }
    EOverflowWrap overflowWrap() const { return static_cast<EOverflowWrap>(m_rareInheritedData->overflowWrap); }
    LineBreak getLineBreak() const { return static_cast<LineBreak>(m_rareInheritedData->lineBreak); }
    const AtomicString& highlight() const { return m_rareInheritedData->highlight; }
    Hyphens getHyphens() const { return static_cast<Hyphens>(m_rareInheritedData->hyphens); }
    const AtomicString& hyphenationString() const { return m_rareInheritedData->hyphenationString; }
    const AtomicString& locale() const { return LayoutLocale::localeString(getFontDescription().locale()); }
    EResize resize() const { return static_cast<EResize>(m_rareNonInheritedData->m_resize); }
    bool hasInlinePaginationAxis() const
    {
        // If the pagination axis is parallel with the writing mode inline axis, columns may be laid
        // out along the inline axis, just like for regular multicol. Otherwise, we need to lay out
        // along the block axis.
        if (isOverflowPaged())
            return (overflowY() == OverflowPagedX) == isHorizontalWritingMode();
        return false;
    }
    float columnWidth() const { return m_rareNonInheritedData->m_multiCol->m_width; }
    bool hasAutoColumnWidth() const { return m_rareNonInheritedData->m_multiCol->m_autoWidth; }
    unsigned short columnCount() const { return m_rareNonInheritedData->m_multiCol->m_count; }
    bool hasAutoColumnCount() const { return m_rareNonInheritedData->m_multiCol->m_autoCount; }
    bool specifiesColumns() const { return !hasAutoColumnCount() || !hasAutoColumnWidth(); }
    ColumnFill getColumnFill() const { return static_cast<ColumnFill>(m_rareNonInheritedData->m_multiCol->m_fill); }
    float columnGap() const { return m_rareNonInheritedData->m_multiCol->m_gap; }
    bool hasNormalColumnGap() const { return m_rareNonInheritedData->m_multiCol->m_normalGap; }
    EBorderStyle columnRuleStyle() const { return m_rareNonInheritedData->m_multiCol->m_rule.style(); }
    unsigned short columnRuleWidth() const { return m_rareNonInheritedData->m_multiCol->ruleWidth(); }
    bool columnRuleIsTransparent() const { return m_rareNonInheritedData->m_multiCol->m_rule.isTransparent(); }
    bool columnRuleEquivalent(const ComputedStyle* otherStyle) const;
    ColumnSpan getColumnSpan() const { return static_cast<ColumnSpan>(m_rareNonInheritedData->m_multiCol->m_columnSpan); }
    bool hasInlineTransform() const { return m_rareNonInheritedData->m_hasInlineTransform; }
    bool hasCompositorProxy() const { return m_rareNonInheritedData->m_hasCompositorProxy; }
    const TransformOperations& transform() const { return m_rareNonInheritedData->m_transform->m_operations; }
    const TransformOrigin& transformOrigin() const { return m_rareNonInheritedData->m_transform->m_origin; }
    const Length& transformOriginX() const { return transformOrigin().x(); }
    const Length& transformOriginY() const { return transformOrigin().y(); }
    TranslateTransformOperation* translate() const { return m_rareNonInheritedData->m_transform->m_translate.get(); }
    RotateTransformOperation* rotate() const { return m_rareNonInheritedData->m_transform->m_rotate.get(); }
    ScaleTransformOperation* scale() const { return m_rareNonInheritedData->m_transform->m_scale.get(); }
    float transformOriginZ() const { return transformOrigin().z(); }
    bool has3DTransform() const { return m_rareNonInheritedData->m_transform->has3DTransform(); }
    bool hasTransform() const { return hasTransformOperations() || hasMotionPath() || hasCurrentTransformAnimation() || translate() || rotate() || scale(); }
    bool hasTransformOperations() const { return !m_rareNonInheritedData->m_transform->m_operations.operations().isEmpty(); }
    bool transformDataEquivalent(const ComputedStyle& otherStyle) const { return m_rareNonInheritedData->m_transform == otherStyle.m_rareNonInheritedData->m_transform; }

    StylePath* motionPath() const { return m_rareNonInheritedData->m_transform->m_motion.m_path.get(); }
    bool hasMotionPath() const { return motionPath(); }
    const Length& motionOffset() const { return m_rareNonInheritedData->m_transform->m_motion.m_offset; }
    const StyleMotionRotation& motionRotation() const { return m_rareNonInheritedData->m_transform->m_motion.m_rotation; }

    TextEmphasisFill getTextEmphasisFill() const { return static_cast<TextEmphasisFill>(m_rareInheritedData->textEmphasisFill); }
    TextEmphasisMark getTextEmphasisMark() const;
    const AtomicString& textEmphasisCustomMark() const { return m_rareInheritedData->textEmphasisCustomMark; }
    TextEmphasisPosition getTextEmphasisPosition() const { return static_cast<TextEmphasisPosition>(m_rareInheritedData->textEmphasisPosition); }
    const AtomicString& textEmphasisMarkString() const;

    RubyPosition getRubyPosition() const { return static_cast<RubyPosition>(m_rareInheritedData->m_rubyPosition); }

    TextOrientation getTextOrientation() const { return static_cast<TextOrientation>(m_rareInheritedData->m_textOrientation); }

    ObjectFit getObjectFit() const { return static_cast<ObjectFit>(m_rareNonInheritedData->m_objectFit); }
    LengthPoint objectPosition() const { return m_rareNonInheritedData->m_objectPosition; }

    // Return true if any transform related property (currently transform/motionPath, transformStyle3D, perspective,
    // or will-change:transform) indicates that we are transforming. will-change:transform should result in
    // the same rendering behavior as having a transform, including the creation of a containing block
    // for fixed position descendants.
    bool hasTransformRelatedProperty() const { return hasTransform() || preserves3D() || hasPerspective() || hasWillChangeTransformHint(); }

    enum ApplyTransformOrigin { IncludeTransformOrigin, ExcludeTransformOrigin };
    enum ApplyMotionPath { IncludeMotionPath, ExcludeMotionPath };
    enum ApplyIndependentTransformProperties { IncludeIndependentTransformProperties , ExcludeIndependentTransformProperties };
    void applyTransform(TransformationMatrix&, const LayoutSize& borderBoxSize, ApplyTransformOrigin, ApplyMotionPath, ApplyIndependentTransformProperties) const;
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, ApplyTransformOrigin, ApplyMotionPath, ApplyIndependentTransformProperties) const;
    bool hasMask() const { return m_rareNonInheritedData->m_mask.hasImage() || m_rareNonInheritedData->m_maskBoxImage.hasImage(); }

    TextCombine getTextCombine() const { return static_cast<TextCombine>(m_rareInheritedData->m_textCombine); }
    bool hasTextCombine() const { return getTextCombine() != TextCombineNone; }

    uint8_t snapHeightPosition() const { return m_rareInheritedData->m_snapHeightPosition; }
    uint8_t snapHeightUnit() const { return m_rareInheritedData->m_snapHeightUnit; }

    TabSize getTabSize() const { return m_rareInheritedData->m_tabSize; }

    RespectImageOrientationEnum respectImageOrientation() const { return static_cast<RespectImageOrientationEnum>(m_rareInheritedData->m_respectImageOrientation); }

    // End CSS3 Getters

    // Apple-specific property getter methods
    EPointerEvents pointerEvents() const { return static_cast<EPointerEvents>(m_inheritedData.m_pointerEvents); }
    const CSSAnimationData* animations() const { return m_rareNonInheritedData->m_animations.get(); }
    const CSSTransitionData* transitions() const { return m_rareNonInheritedData->m_transitions.get(); }

    CSSAnimationData& accessAnimations();
    CSSTransitionData& accessTransitions();

    ETransformStyle3D transformStyle3D() const { return static_cast<ETransformStyle3D>(m_rareNonInheritedData->m_transformStyle3D); }
    ETransformStyle3D usedTransformStyle3D() const { return hasGroupingProperty() ? TransformStyle3DFlat : transformStyle3D(); }
    bool preserves3D() const { return usedTransformStyle3D() != TransformStyle3DFlat; }

    EBackfaceVisibility backfaceVisibility() const { return static_cast<EBackfaceVisibility>(m_rareNonInheritedData->m_backfaceVisibility); }
    float perspective() const { return m_rareNonInheritedData->m_perspective; }
    bool hasPerspective() const { return m_rareNonInheritedData->m_perspective > 0; }
    const LengthPoint& perspectiveOrigin() const { return m_rareNonInheritedData->m_perspectiveOrigin; }
    const Length& perspectiveOriginX() const { return perspectiveOrigin().x(); }
    const Length& perspectiveOriginY() const { return perspectiveOrigin().y(); }
    const FloatSize& pageSize() const { return m_rareNonInheritedData->m_pageSize; }
    PageSizeType getPageSizeType() const { return static_cast<PageSizeType>(m_rareNonInheritedData->m_pageSizeType); }

    bool hasCurrentOpacityAnimation() const { return m_rareNonInheritedData->m_hasCurrentOpacityAnimation; }
    bool hasCurrentTransformAnimation() const { return m_rareNonInheritedData->m_hasCurrentTransformAnimation; }
    bool hasCurrentFilterAnimation() const { return m_rareNonInheritedData->m_hasCurrentFilterAnimation; }
    bool hasCurrentBackdropFilterAnimation() const { return m_rareNonInheritedData->m_hasCurrentBackdropFilterAnimation; }
    bool shouldCompositeForCurrentAnimations() const { return hasCurrentOpacityAnimation() || hasCurrentTransformAnimation() || hasCurrentFilterAnimation() || hasCurrentBackdropFilterAnimation(); }

    bool isRunningOpacityAnimationOnCompositor() const { return m_rareNonInheritedData->m_runningOpacityAnimationOnCompositor; }
    bool isRunningTransformAnimationOnCompositor() const { return m_rareNonInheritedData->m_runningTransformAnimationOnCompositor; }
    bool isRunningFilterAnimationOnCompositor() const { return m_rareNonInheritedData->m_runningFilterAnimationOnCompositor; }
    bool isRunningBackdropFilterAnimationOnCompositor() const { return m_rareNonInheritedData->m_runningBackdropFilterAnimationOnCompositor; }
    bool isRunningAnimationOnCompositor() const { return isRunningOpacityAnimationOnCompositor() || isRunningTransformAnimationOnCompositor() || isRunningFilterAnimationOnCompositor() || isRunningBackdropFilterAnimationOnCompositor(); }

    const LineClampValue& lineClamp() const { return m_rareNonInheritedData->lineClamp; }
    Color tapHighlightColor() const { return m_rareInheritedData->tapHighlightColor; }
    ETextSecurity textSecurity() const { return static_cast<ETextSecurity>(m_rareInheritedData->textSecurity); }

    WritingMode getWritingMode() const { return static_cast<WritingMode>(m_inheritedData.m_writingMode); }
    bool isHorizontalWritingMode() const { return blink::isHorizontalWritingMode(getWritingMode()); }
    bool isFlippedLinesWritingMode() const { return blink::isFlippedLinesWritingMode(getWritingMode()); }
    bool isFlippedBlocksWritingMode() const { return blink::isFlippedBlocksWritingMode(getWritingMode()); }

    EImageRendering imageRendering() const { return static_cast<EImageRendering>(m_rareInheritedData->m_imageRendering); }

    ESpeak speak() const { return static_cast<ESpeak>(m_rareInheritedData->speak); }

    FilterOperations& mutableFilter() { return m_rareNonInheritedData.access()->m_filter.access()->m_operations; }
    const FilterOperations& filter() const { return m_rareNonInheritedData->m_filter->m_operations; }
    bool hasFilter() const { return !m_rareNonInheritedData->m_filter->m_operations.operations().isEmpty(); }

    FilterOperations& mutableBackdropFilter() { return m_rareNonInheritedData.access()->m_backdropFilter.access()->m_operations; }
    const FilterOperations& backdropFilter() const { return m_rareNonInheritedData->m_backdropFilter->m_operations; }
    bool hasBackdropFilter() const { return !m_rareNonInheritedData->m_backdropFilter->m_operations.operations().isEmpty(); }

    WebBlendMode blendMode() const { return static_cast<WebBlendMode>(m_rareNonInheritedData->m_effectiveBlendMode); }
    void setBlendMode(WebBlendMode v) { m_rareNonInheritedData.access()->m_effectiveBlendMode = v; }
    bool hasBlendMode() const { return blendMode() != WebBlendModeNormal; }

    EIsolation isolation() const { return static_cast<EIsolation>(m_rareNonInheritedData->m_isolation); }
    void setIsolation(EIsolation v) { m_rareNonInheritedData.access()->m_isolation = v; }
    bool hasIsolation() const { return isolation() != IsolationAuto; }

    bool shouldPlaceBlockDirectionScrollbarOnLogicalLeft() const { return !isLeftToRightDirection() && isHorizontalWritingMode(); }

    TouchAction getTouchAction() const { return static_cast<TouchAction>(m_rareNonInheritedData->m_touchAction); }

    ScrollBehavior getScrollBehavior() const { return static_cast<ScrollBehavior>(m_rareNonInheritedData->m_scrollBehavior); }

    ScrollSnapType getScrollSnapType() const { return static_cast<ScrollSnapType>(m_rareNonInheritedData->m_scrollSnapType); }
    const ScrollSnapPoints& scrollSnapPointsX() const { return m_rareNonInheritedData->m_scrollSnap->m_xPoints; }
    const ScrollSnapPoints& scrollSnapPointsY() const { return m_rareNonInheritedData->m_scrollSnap->m_yPoints; }
    const Vector<LengthPoint>& scrollSnapCoordinate() const { return m_rareNonInheritedData->m_scrollSnap->m_coordinates; }
    const LengthPoint& scrollSnapDestination() const { return m_rareNonInheritedData->m_scrollSnap->m_destination; }

    const Vector<CSSPropertyID>& willChangeProperties() const { return m_rareNonInheritedData->m_willChange->m_properties; }
    bool willChangeContents() const { return m_rareNonInheritedData->m_willChange->m_contents; }
    bool willChangeScrollPosition() const { return m_rareNonInheritedData->m_willChange->m_scrollPosition; }
    bool hasWillChangeCompositingHint() const;
    bool hasWillChangeOpacityHint() const { return willChangeProperties().contains(CSSPropertyOpacity); }
    bool hasWillChangeTransformHint() const;
    bool subtreeWillChangeContents() const { return m_rareInheritedData->m_subtreeWillChangeContents; }

// attribute setter methods

    void setDisplay(EDisplay v) { m_nonInheritedData.m_effectiveDisplay = v; }
    void setOriginalDisplay(EDisplay v) { m_nonInheritedData.m_originalDisplay = v; }
    void setPosition(EPosition v) { m_nonInheritedData.m_position = v; }
    void setFloating(EFloat v) { m_nonInheritedData.m_floating = v; }

    void setLeft(const Length& v) { SET_VAR(m_surround, offset.m_left, v); }
    void setRight(const Length& v) { SET_VAR(m_surround, offset.m_right, v); }
    void setTop(const Length& v) { SET_VAR(m_surround, offset.m_top, v); }
    void setBottom(const Length& v) { SET_VAR(m_surround, offset.m_bottom, v); }

    void setWidth(const Length& v) { SET_VAR(m_box, m_width, v); }
    void setHeight(const Length& v) { SET_VAR(m_box, m_height, v); }

    void setLogicalWidth(const Length& v)
    {
        if (isHorizontalWritingMode()) {
            SET_VAR(m_box, m_width, v);
        } else {
            SET_VAR(m_box, m_height, v);
        }
    }

    void setLogicalHeight(const Length& v)
    {
        if (isHorizontalWritingMode()) {
            SET_VAR(m_box, m_height, v);
        } else {
            SET_VAR(m_box, m_width, v);
        }
    }

    void setMinWidth(const Length& v) { SET_VAR(m_box, m_minWidth, v); }
    void setMaxWidth(const Length& v) { SET_VAR(m_box, m_maxWidth, v); }
    void setMinHeight(const Length& v) { SET_VAR(m_box, m_minHeight, v); }
    void setMaxHeight(const Length& v) { SET_VAR(m_box, m_maxHeight, v); }

    DraggableRegionMode getDraggableRegionMode() const { return m_rareNonInheritedData->m_draggableRegionMode; }
    void setDraggableRegionMode(DraggableRegionMode v) { SET_VAR(m_rareNonInheritedData, m_draggableRegionMode, v); }

    void resetBorder()
    {
        resetBorderImage();
        resetBorderTop();
        resetBorderRight();
        resetBorderBottom();
        resetBorderLeft();
        resetBorderTopLeftRadius();
        resetBorderTopRightRadius();
        resetBorderBottomLeftRadius();
        resetBorderBottomRightRadius();
    }
    void resetBorderTop() { SET_VAR(m_surround, border.m_top, BorderValue()); }
    void resetBorderRight() { SET_VAR(m_surround, border.m_right, BorderValue()); }
    void resetBorderBottom() { SET_VAR(m_surround, border.m_bottom, BorderValue()); }
    void resetBorderLeft() { SET_VAR(m_surround, border.m_left, BorderValue()); }
    void resetBorderImage() { SET_VAR(m_surround, border.m_image, NinePieceImage()); }
    void resetBorderTopLeftRadius() { SET_VAR(m_surround, border.m_topLeft, initialBorderRadius()); }
    void resetBorderTopRightRadius() { SET_VAR(m_surround, border.m_topRight, initialBorderRadius()); }
    void resetBorderBottomLeftRadius() { SET_VAR(m_surround, border.m_bottomLeft, initialBorderRadius()); }
    void resetBorderBottomRightRadius() { SET_VAR(m_surround, border.m_bottomRight, initialBorderRadius()); }

    void setBackgroundColor(const StyleColor& v) { SET_VAR(m_background, m_color, v); }

    void setBorderImage(const NinePieceImage& b) { SET_VAR(m_surround, border.m_image, b); }
    void setBorderImageSource(StyleImage*);
    void setBorderImageSlices(const LengthBox&);
    void setBorderImageSlicesFill(bool);
    void setBorderImageWidth(const BorderImageLengthBox&);
    void setBorderImageOutset(const BorderImageLengthBox&);

    void setBorderTopLeftRadius(const LengthSize& s) { SET_VAR(m_surround, border.m_topLeft, s); }
    void setBorderTopRightRadius(const LengthSize& s) { SET_VAR(m_surround, border.m_topRight, s); }
    void setBorderBottomLeftRadius(const LengthSize& s) { SET_VAR(m_surround, border.m_bottomLeft, s); }
    void setBorderBottomRightRadius(const LengthSize& s) { SET_VAR(m_surround, border.m_bottomRight, s); }

    void setBorderRadius(const LengthSize& s)
    {
        setBorderTopLeftRadius(s);
        setBorderTopRightRadius(s);
        setBorderBottomLeftRadius(s);
        setBorderBottomRightRadius(s);
    }
    void setBorderRadius(const IntSize& s)
    {
        setBorderRadius(LengthSize(Length(s.width(), Fixed), Length(s.height(), Fixed)));
    }

    FloatRoundedRect getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true,
        bool includeLogicalRightEdge = true) const;
    FloatRoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    FloatRoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect,
        const LayoutRectOutsets insets, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;

    void setBorderLeftWidth(unsigned v) { SET_VAR(m_surround, border.m_left.m_width, v); }
    void setBorderLeftStyle(EBorderStyle v) { SET_VAR(m_surround, border.m_left.m_style, v); }
    void setBorderLeftColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surround, border.m_left, v); }
    void setBorderRightWidth(unsigned v) { SET_VAR(m_surround, border.m_right.m_width, v); }
    void setBorderRightStyle(EBorderStyle v) { SET_VAR(m_surround, border.m_right.m_style, v); }
    void setBorderRightColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surround, border.m_right, v); }
    void setBorderTopWidth(unsigned v) { SET_VAR(m_surround, border.m_top.m_width, v); }
    void setBorderTopStyle(EBorderStyle v) { SET_VAR(m_surround, border.m_top.m_style, v); }
    void setBorderTopColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surround, border.m_top, v); }
    void setBorderBottomWidth(unsigned v) { SET_VAR(m_surround, border.m_bottom.m_width, v); }
    void setBorderBottomStyle(EBorderStyle v) { SET_VAR(m_surround, border.m_bottom.m_style, v); }
    void setBorderBottomColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_surround, border.m_bottom, v); }

    void setOutlineWidth(unsigned short v) { SET_VAR(m_background, m_outline.m_width, v); }
    void setOutlineStyleIsAuto(OutlineIsAuto isAuto) { SET_VAR(m_background, m_outline.m_isAuto, isAuto); }
    void setOutlineStyle(EBorderStyle v) { SET_VAR(m_background, m_outline.m_style, v); }
    void setOutlineColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_background, m_outline, v); }
    bool isOutlineEquivalent(const ComputedStyle* otherStyle) const
    {
        // No other style, so we don't have an outline then we consider them to be the same.
        if (!otherStyle)
            return !hasOutline();
        return m_background->outline().visuallyEqual(otherStyle->m_background->outline());
    }
    void setOutlineFromStyle(const ComputedStyle& o)
    {
        ASSERT(!isOutlineEquivalent(&o));
        m_background.access()->m_outline = o.m_background->m_outline;
    }

    void setOverflowAnchor(EOverflowAnchor v) { m_nonInheritedData.m_overflowAnchor = v; }
    void setOverflowX(EOverflow v) { m_nonInheritedData.m_overflowX = v; }
    void setOverflowY(EOverflow v) { m_nonInheritedData.m_overflowY = v; }
    void setVisibility(EVisibility v) { m_inheritedData.m_visibility = v; }
    void setVerticalAlign(EVerticalAlign v) { m_nonInheritedData.m_verticalAlign = v; }
    void setVerticalAlignLength(const Length& length) { setVerticalAlign(VerticalAlignLength); SET_VAR(m_box, m_verticalAlign, length); }

    void setHasAutoClip() { SET_VAR(m_visual, hasAutoClip, true); SET_VAR(m_visual, clip, ComputedStyle::initialClip()); }
    void setClip(const LengthBox& box) { SET_VAR(m_visual, hasAutoClip, false); SET_VAR(m_visual, clip, box); }

    void setUnicodeBidi(EUnicodeBidi b) { m_nonInheritedData.m_unicodeBidi = b; }

    void setClear(EClear v) { m_nonInheritedData.m_clear = v; }
    void setTableLayout(ETableLayout v) { m_nonInheritedData.m_tableLayout = v; }

    bool setFontDescription(const FontDescription&);
    void setFont(const Font&);

    void setTextAutosizingMultiplier(float);

    void setColor(const Color&);
    void setTextIndent(const Length& v) { SET_VAR(m_rareInheritedData, indent, v); }
    void setTextIndentLine(TextIndentLine v) { SET_VAR(m_rareInheritedData, m_textIndentLine, v); }
    void setTextIndentType(TextIndentType v) { SET_VAR(m_rareInheritedData, m_textIndentType, v); }
    void setTextAlign(ETextAlign v) { m_inheritedData.m_textAlign = v; }
    void setTextAlignLast(TextAlignLast v) { SET_VAR(m_rareInheritedData, m_textAlignLast, v); }
    void setTextJustify(TextJustify v) { SET_VAR(m_rareInheritedData, m_textJustify, v); }
    void setTextTransform(ETextTransform v) { m_inheritedData.m_textTransform = v; }
    void applyTextDecorations();
    void clearAppliedTextDecorations();
    void setTextDecoration(TextDecoration v) { SET_VAR(m_visual, textDecoration, v); }
    void setTextUnderlinePosition(TextUnderlinePosition v) { SET_VAR(m_rareInheritedData, m_textUnderlinePosition, v); }
    void setTextDecorationStyle(TextDecorationStyle v) { SET_VAR(m_rareNonInheritedData, m_textDecorationStyle, v); }
    void setDirection(TextDirection v) { m_inheritedData.m_direction = v; }
    void setSelfOrAncestorHasDirAutoAttribute(bool v) { SET_VAR(m_rareInheritedData, m_selfOrAncestorHasDirAutoAttribute, v); }
    void setLineHeight(const Length& specifiedLineHeight);
    bool setZoom(float);
    bool setEffectiveZoom(float);
    void clearMultiCol();

    void setImageRendering(EImageRendering v) { SET_VAR(m_rareInheritedData, m_imageRendering, v); }

    void setWhiteSpace(EWhiteSpace v) { m_inheritedData.m_whiteSpace = v; }

    // FIXME: Remove these two and replace them with respective FontBuilder calls.
    void setWordSpacing(float);
    void setLetterSpacing(float);

    void adjustBackgroundLayers()
    {
        if (backgroundLayers().next()) {
            accessBackgroundLayers().cullEmptyLayers();
            accessBackgroundLayers().fillUnsetProperties();
        }
    }

    void adjustMaskLayers()
    {
        if (maskLayers().next()) {
            accessMaskLayers().cullEmptyLayers();
            accessMaskLayers().fillUnsetProperties();
        }
    }

    void setMaskBoxImage(const NinePieceImage& b) { SET_VAR(m_rareNonInheritedData, m_maskBoxImage, b); }
    void setMaskBoxImageSource(StyleImage* v) { m_rareNonInheritedData.access()->m_maskBoxImage.setImage(v); }
    void setMaskBoxImageSlices(const LengthBox& slices)
    {
        m_rareNonInheritedData.access()->m_maskBoxImage.setImageSlices(slices);
    }
    void setMaskBoxImageSlicesFill(bool fill)
    {
        m_rareNonInheritedData.access()->m_maskBoxImage.setFill(fill);
    }
    void setMaskBoxImageWidth(const BorderImageLengthBox& slices)
    {
        m_rareNonInheritedData.access()->m_maskBoxImage.setBorderSlices(slices);
    }
    void setMaskBoxImageOutset(const BorderImageLengthBox& outset)
    {
        m_rareNonInheritedData.access()->m_maskBoxImage.setOutset(outset);
    }

    void setBorderCollapse(EBorderCollapse collapse) { m_inheritedData.m_borderCollapse = collapse; }
    void setHorizontalBorderSpacing(short);
    void setVerticalBorderSpacing(short);
    void setEmptyCells(EEmptyCells v) { m_inheritedData.m_emptyCells = v; }
    void setCaptionSide(ECaptionSide v) { m_inheritedData.m_captionSide = v; }

    void setListStyleType(EListStyleType v) { m_inheritedData.m_listStyleType = v; }
    void setListStyleImage(StyleImage*);
    void setListStylePosition(EListStylePosition v) { m_inheritedData.m_listStylePosition = v; }

    void setMarginTop(const Length& v) { SET_VAR(m_surround, margin.m_top, v); }
    void setMarginBottom(const Length& v) { SET_VAR(m_surround, margin.m_bottom, v); }
    void setMarginLeft(const Length& v) { SET_VAR(m_surround, margin.m_left, v); }
    void setMarginRight(const Length& v) { SET_VAR(m_surround, margin.m_right, v); }
    void setMarginStart(const Length&);
    void setMarginEnd(const Length&);

    void resetPadding() { SET_VAR(m_surround, padding, LengthBox(Fixed)); }
    void setPaddingBox(const LengthBox& b) { SET_VAR(m_surround, padding, b); }
    void setPaddingTop(const Length& v) { SET_VAR(m_surround, padding.m_top, v); }
    void setPaddingBottom(const Length& v) { SET_VAR(m_surround, padding.m_bottom, v); }
    void setPaddingLeft(const Length& v) { SET_VAR(m_surround, padding.m_left, v); }
    void setPaddingRight(const Length& v) { SET_VAR(m_surround, padding.m_right, v); }

    void setCursor(ECursor c) { m_inheritedData.m_cursorStyle = c; }
    void addCursor(StyleImage*, bool hotSpotSpecified, const IntPoint& hotSpot = IntPoint());
    void setCursorList(CursorList*);
    void clearCursorList();

    void setInsideLink(EInsideLink insideLink) { m_inheritedData.m_insideLink = insideLink; }
    void setIsLink(bool b) { m_nonInheritedData.m_isLink = b; }

    PrintColorAdjust getPrintColorAdjust() const { return static_cast<PrintColorAdjust>(m_inheritedData.m_printColorAdjust); }
    void setPrintColorAdjust(PrintColorAdjust value) { m_inheritedData.m_printColorAdjust = value; }

    // A stacking context is painted atomically and defines a stacking order, whereas
    // a containing stacking context defines in which order the stacking contexts
    // below are painted.
    // See CSS 2.1, Appendix E (https://www.w3.org/TR/CSS21/zindex.html) for more details.
    bool isStackingContext() const { return m_rareNonInheritedData->m_isStackingContext; }

    void updateIsStackingContext(bool isDocumentElement, bool isInTopLayer);
    void setIsStackingContext(bool b) { SET_VAR(m_rareNonInheritedData, m_isStackingContext, b); }

    // Stacking contexts and positioned elements[1] are stacked (sorted in negZOrderList
    // and posZOrderList) in their enclosing stacking contexts.
    //
    // [1] According to CSS2.1, Appendix E.2.8 (https://www.w3.org/TR/CSS21/zindex.html),
    // positioned elements with 'z-index: auto' are "treated as if it created a new
    // stacking context" and z-ordered together with other elements with 'z-index: 0'.
    // The difference of them from normal stacking contexts is that they don't determine
    // the stacking of the elements underneath them.
    // (Note: There are also other elements treated as stacking context during painting,
    // but not managed in stacks. See ObjectPainter::paintAllPhasesAtomically().)
    bool isStacked() const { return isStackingContext() || position() != StaticPosition; }

    bool hasAutoZIndex() const { return m_box->hasAutoZIndex(); }
    void setHasAutoZIndex() { SET_VAR(m_box, m_hasAutoZIndex, true); SET_VAR(m_box, m_zIndex, 0); }
    int zIndex() const { return m_box->zIndex(); }
    void setZIndex(int v) { SET_VAR(m_box, m_hasAutoZIndex, false); SET_VAR(m_box, m_zIndex, v); }
    void setWidows(short w) { SET_VAR(m_rareInheritedData, widows, w); }
    void setOrphans(short o) { SET_VAR(m_rareInheritedData, orphans, o); }
    void setBreakAfter(EBreak b) { DCHECK_LE(b, BreakValueLastAllowedForBreakAfterAndBefore); m_nonInheritedData.m_breakAfter = b; }
    void setBreakBefore(EBreak b) { DCHECK_LE(b, BreakValueLastAllowedForBreakAfterAndBefore); m_nonInheritedData.m_breakBefore = b;  }
    void setBreakInside(EBreak b) { DCHECK_LE(b, BreakValueLastAllowedForBreakInside); m_nonInheritedData.m_breakInside = b;  }

    void setTextSizeAdjust(TextSizeAdjust sizeAdjust) { SET_VAR(m_rareInheritedData, m_textSizeAdjust, sizeAdjust); }

    // CSS3 Setters
    void setOutlineOffset(int v) { SET_VAR(m_background, m_outline.m_offset, v); }
    void setTextShadow(PassRefPtr<ShadowList>);
    void setTextStrokeColor(const StyleColor& c) { SET_VAR_WITH_SETTER(m_rareInheritedData, textStrokeColor, setTextStrokeColor, c); }
    void setTextStrokeWidth(float w) { SET_VAR(m_rareInheritedData, textStrokeWidth, w); }
    void setTextFillColor(const StyleColor& c) { SET_VAR_WITH_SETTER(m_rareInheritedData, textFillColor, setTextFillColor, c); }
    void setOpacity(float f) { float v = clampTo<float>(f, 0, 1); SET_VAR(m_rareNonInheritedData, opacity, v); }
    void setAppearance(ControlPart a) { SET_VAR(m_rareNonInheritedData, m_appearance, a); }
    // For valid values of box-align see http://www.w3.org/TR/2009/WD-css3-flexbox-20090723/#alignment
    void setBoxAlign(EBoxAlignment a) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, align, a); }
    void setBoxDecorationBreak(EBoxDecorationBreak b) { SET_VAR(m_box, m_boxDecorationBreak, b); }
    void setBoxDirection(EBoxDirection d) { m_inheritedData.m_boxDirection = d; }
    void setBoxFlex(float f) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, flex, f); }
    void setBoxFlexGroup(unsigned fg) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, flexGroup, fg); }
    void setBoxLines(EBoxLines lines) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, lines, lines); }
    void setBoxOrdinalGroup(unsigned og) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, ordinalGroup, og); }
    void setBoxOrient(EBoxOrient o) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, orient, o); }
    void setBoxPack(EBoxPack p) { SET_NESTED_VAR(m_rareNonInheritedData, m_deprecatedFlexibleBox, pack, p); }
    void setBoxShadow(PassRefPtr<ShadowList>);
    void setBoxReflect(PassRefPtr<StyleReflection> reflect)
    {
        if (m_rareNonInheritedData->m_boxReflect != reflect)
            m_rareNonInheritedData.access()->m_boxReflect = reflect;
    }
    void setBoxSizing(EBoxSizing s) { SET_VAR(m_box, m_boxSizing, s); }
    void setContain(Containment contain) { SET_VAR(m_rareNonInheritedData, m_contain, contain); }
    void setFlexGrow(float f) { SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexGrow, f); }
    void setFlexShrink(float f) { SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexShrink, f); }
    void setFlexBasis(const Length& length) { SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexBasis, length); }
    // We restrict the smallest value to int min + 2 because we use int min and int min + 1 as special values in a hash set.
    void setOrder(int o) { SET_VAR(m_rareNonInheritedData, m_order, max(std::numeric_limits<int>::min() + 2, o)); }
    void addCallbackSelector(const String& selector);
    void setAlignContent(const StyleContentAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_alignContent, data); }
    void setAlignContentPosition(ContentPosition position) { m_rareNonInheritedData.access()->m_alignContent.setPosition(position); }
    void setAlignContentDistribution(ContentDistributionType distribution) { m_rareNonInheritedData.access()->m_alignContent.setDistribution(distribution); }
    void setAlignContentOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_alignContent.setOverflow(overflow); }
    void setAlignItems(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_alignItems, data); }
    void setAlignItemsPosition(ItemPosition position) { m_rareNonInheritedData.access()->m_alignItems.setPosition(position); }
    void setAlignItemsOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_alignItems.setOverflow(overflow); }
    void setAlignSelf(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_alignSelf, data); }
    void setAlignSelfPosition(ItemPosition position) { m_rareNonInheritedData.access()->m_alignSelf.setPosition(position); }
    void setAlignSelfOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_alignSelf.setOverflow(overflow); }
    void setFlexDirection(EFlexDirection direction) { SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexDirection, direction); }
    void setFlexWrap(EFlexWrap w) { SET_NESTED_VAR(m_rareNonInheritedData, m_flexibleBox, m_flexWrap, w); }
    void setJustifyContent(const StyleContentAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_justifyContent, data); }
    void setJustifyContentPosition(ContentPosition position) { m_rareNonInheritedData.access()->m_justifyContent.setPosition(position); }
    void setJustifyContentDistribution(ContentDistributionType distribution) { m_rareNonInheritedData.access()->m_justifyContent.setDistribution(distribution); }
    void setJustifyContentOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_justifyContent.setOverflow(overflow); }
    void setJustifyItems(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_justifyItems, data); }
    void setJustifyItemsPosition(ItemPosition position) { m_rareNonInheritedData.access()->m_justifyItems.setPosition(position); }
    void setJustifyItemsOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_justifyItems.setOverflow(overflow); }
    void setJustifyItemsPositionType(ItemPositionType positionType) { m_rareNonInheritedData.access()->m_justifyItems.setPositionType(positionType); }
    void setJustifySelf(const StyleSelfAlignmentData& data) { SET_VAR(m_rareNonInheritedData, m_justifySelf, data); }
    void setJustifySelfPosition(ItemPosition position) { m_rareNonInheritedData.access()->m_justifySelf.setPosition(position); }
    void setJustifySelfOverflow(OverflowAlignment overflow) { m_rareNonInheritedData.access()->m_justifySelf.setOverflow(overflow); }
    void setGridAutoColumns(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoColumns, trackSizeList); }
    void setGridAutoRows(const Vector<GridTrackSize>& trackSizeList) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRows, trackSizeList); }
    void setGridTemplateColumns(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridTemplateColumns, lengths); }
    void setGridTemplateRows(const Vector<GridTrackSize>& lengths) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridTemplateRows, lengths); }
    void setGridAutoRepeatColumns(const Vector<GridTrackSize>& trackSizes) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRepeatColumns, trackSizes); }
    void setGridAutoRepeatRows(const Vector<GridTrackSize>& trackSizes) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoRepeatRows, trackSizes); }
    void setGridAutoRepeatColumnsInsertionPoint(const size_t insertionPoint) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatColumnsInsertionPoint, insertionPoint); }
    void setGridAutoRepeatRowsInsertionPoint(const size_t insertionPoint) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatRowsInsertionPoint, insertionPoint); }
    void setGridAutoRepeatColumnsType(const AutoRepeatType autoRepeatType) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatColumnsType, autoRepeatType); }
    void setGridAutoRepeatRowsType(const AutoRepeatType autoRepeatType) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatRowsType, autoRepeatType); }
    void setNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridColumnLines, namedGridColumnLines); }
    void setNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridRowLines, namedGridRowLines); }
    void setOrderedNamedGridColumnLines(const OrderedNamedGridLines& orderedNamedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_orderedNamedGridColumnLines, orderedNamedGridColumnLines); }
    void setOrderedNamedGridRowLines(const OrderedNamedGridLines& orderedNamedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_orderedNamedGridRowLines, orderedNamedGridRowLines); }
    void setAutoRepeatNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatNamedGridColumnLines, namedGridColumnLines); }
    void setAutoRepeatNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatNamedGridRowLines, namedGridRowLines); }
    void setAutoRepeatOrderedNamedGridColumnLines(const OrderedNamedGridLines& orderedNamedGridColumnLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatOrderedNamedGridColumnLines, orderedNamedGridColumnLines); }
    void setAutoRepeatOrderedNamedGridRowLines(const OrderedNamedGridLines& orderedNamedGridRowLines) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_autoRepeatOrderedNamedGridRowLines, orderedNamedGridRowLines); }
    void setNamedGridArea(const NamedGridAreaMap& namedGridArea) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridArea, namedGridArea); }
    void setNamedGridAreaRowCount(size_t rowCount) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridAreaRowCount, rowCount); }
    void setNamedGridAreaColumnCount(size_t columnCount) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_namedGridAreaColumnCount, columnCount); }
    void setGridAutoFlow(GridAutoFlow flow) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridAutoFlow, flow); }

    void setGridColumnStart(const GridPosition& columnStartPosition) { SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridColumnStart, columnStartPosition); }
    void setGridColumnEnd(const GridPosition& columnEndPosition) { SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridColumnEnd, columnEndPosition); }
    void setGridRowStart(const GridPosition& rowStartPosition) { SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridRowStart, rowStartPosition); }
    void setGridRowEnd(const GridPosition& rowEndPosition) { SET_NESTED_VAR(m_rareNonInheritedData, m_gridItem, m_gridRowEnd, rowEndPosition); }
    void setGridColumnGap(const Length& v) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridColumnGap, v); }
    void setGridRowGap(const Length& v) { SET_NESTED_VAR(m_rareNonInheritedData, m_grid, m_gridRowGap, v); }

    void setUserModify(EUserModify u) { SET_VAR(m_rareInheritedData, userModify, u); }
    void setUserDrag(EUserDrag d) { SET_VAR(m_rareNonInheritedData, userDrag, d); }
    void setUserSelect(EUserSelect s) { SET_VAR(m_rareInheritedData, userSelect, s); }
    void setTextOverflow(TextOverflow overflow) { SET_VAR(m_rareNonInheritedData, textOverflow, overflow); }
    void setMarginBeforeCollapse(EMarginCollapse c) { SET_VAR(m_rareNonInheritedData, marginBeforeCollapse, c); }
    void setMarginAfterCollapse(EMarginCollapse c) { SET_VAR(m_rareNonInheritedData, marginAfterCollapse, c); }
    void setWordBreak(EWordBreak b) { SET_VAR(m_rareInheritedData, wordBreak, b); }
    void setOverflowWrap(EOverflowWrap b) { SET_VAR(m_rareInheritedData, overflowWrap, b); }
    void setLineBreak(LineBreak b) { SET_VAR(m_rareInheritedData, lineBreak, b); }
    void setHighlight(const AtomicString& h) { SET_VAR(m_rareInheritedData, highlight, h); }
    void setHyphens(Hyphens h) { SET_VAR(m_rareInheritedData, hyphens, h); }
    void setHyphenationString(const AtomicString& h) { SET_VAR(m_rareInheritedData, hyphenationString, h); }
    void setResize(EResize r) { SET_VAR(m_rareNonInheritedData, m_resize, r); }
    void setColumnWidth(float f) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoWidth, false); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_width, f); }
    void setHasAutoColumnWidth() { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoWidth, true); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_width, 0); }
    void setColumnCount(unsigned short c) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoCount, false); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_count, c); }
    void setHasAutoColumnCount() { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_autoCount, true); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_count, initialColumnCount()); }
    void setColumnFill(ColumnFill columnFill) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_fill, columnFill); }
    void setColumnGap(float f) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_normalGap, false); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_gap, f); }
    void setHasNormalColumnGap() { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_normalGap, true); SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_gap, 0); }
    void setColumnRuleColor(const StyleColor& c) { SET_BORDERVALUE_COLOR(m_rareNonInheritedData.access()->m_multiCol, m_rule, c); }
    void setColumnRuleStyle(EBorderStyle b) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_rule.m_style, b); }
    void setColumnRuleWidth(unsigned short w) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_rule.m_width, w); }
    void setColumnSpan(ColumnSpan columnSpan) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_columnSpan, columnSpan); }
    void inheritColumnPropertiesFrom(const ComputedStyle& parent) { m_rareNonInheritedData.access()->m_multiCol = parent.m_rareNonInheritedData->m_multiCol; }
    void setHasInlineTransform(bool b) { SET_VAR(m_rareNonInheritedData, m_hasInlineTransform, b); }
    void setHasCompositorProxy(bool b) { SET_VAR(m_rareNonInheritedData, m_hasCompositorProxy, b); }
    void setTransform(const TransformOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_operations, ops); }
    void setTransformOriginX(const Length& v) { setTransformOrigin(TransformOrigin(v, transformOriginY(), transformOriginZ())); }
    void setTransformOriginY(const Length& v) { setTransformOrigin(TransformOrigin(transformOriginX(), v, transformOriginZ())); }
    void setTransformOriginZ(float f) { setTransformOrigin(TransformOrigin(transformOriginX(), transformOriginY(), f)); }
    void setTransformOrigin(const TransformOrigin& o) { SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_origin, o); }
    void setTranslate(PassRefPtr<TranslateTransformOperation> v) { m_rareNonInheritedData.access()->m_transform.access()->m_translate = v; }
    void setRotate(PassRefPtr<RotateTransformOperation> v) { m_rareNonInheritedData.access()->m_transform.access()->m_rotate = v; }
    void setScale(PassRefPtr<ScaleTransformOperation> v) { m_rareNonInheritedData.access()->m_transform.access()->m_scale = v; }

    void setSpeak(ESpeak s) { SET_VAR(m_rareInheritedData, speak, s); }
    void setTextCombine(TextCombine v) { SET_VAR(m_rareInheritedData, m_textCombine, v); }
    void setTextDecorationColor(const StyleColor& c) { SET_VAR(m_rareNonInheritedData, m_textDecorationColor, c); }
    void setTextEmphasisColor(const StyleColor& c) { SET_VAR_WITH_SETTER(m_rareInheritedData, textEmphasisColor, setTextEmphasisColor, c); }
    void setTextEmphasisFill(TextEmphasisFill fill) { SET_VAR(m_rareInheritedData, textEmphasisFill, fill); }
    void setTextEmphasisMark(TextEmphasisMark mark) { SET_VAR(m_rareInheritedData, textEmphasisMark, mark); }
    void setTextEmphasisCustomMark(const AtomicString& mark) { SET_VAR(m_rareInheritedData, textEmphasisCustomMark, mark); }
    void setTextEmphasisPosition(TextEmphasisPosition position) { SET_VAR(m_rareInheritedData, textEmphasisPosition, position); }
    bool setTextOrientation(TextOrientation);

    void setMotionPath(PassRefPtr<StylePath>);
    void setMotionOffset(const Length& motionOffset) { SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_offset, motionOffset); }
    void setMotionRotation(const StyleMotionRotation& motionRotation) { SET_NESTED_VAR(m_rareNonInheritedData, m_transform, m_motion.m_rotation, motionRotation); }

    void setObjectFit(ObjectFit f) { SET_VAR(m_rareNonInheritedData, m_objectFit, f); }
    void setObjectPosition(LengthPoint position) { SET_VAR(m_rareNonInheritedData, m_objectPosition, position); }

    void setRubyPosition(RubyPosition position) { SET_VAR(m_rareInheritedData, m_rubyPosition, position); }

    void setFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, m_filter, m_operations, ops); }
    void setBackdropFilter(const FilterOperations& ops) { SET_NESTED_VAR(m_rareNonInheritedData, m_backdropFilter, m_operations, ops); }

    void setSnapHeightPosition(uint8_t position) { SET_VAR(m_rareInheritedData, m_snapHeightPosition, position); }
    void setSnapHeightUnit(uint8_t unit) { SET_VAR(m_rareInheritedData, m_snapHeightUnit, unit); }

    void setTabSize(TabSize size) { SET_VAR(m_rareInheritedData, m_tabSize, size); }

    void setRespectImageOrientation(RespectImageOrientationEnum v) { SET_VAR(m_rareInheritedData, m_respectImageOrientation, v); }

    // End CSS3 Setters

    void setWrapFlow(WrapFlow wrapFlow) { SET_VAR(m_rareNonInheritedData, m_wrapFlow, wrapFlow); }
    void setWrapThrough(WrapThrough wrapThrough) { SET_VAR(m_rareNonInheritedData, m_wrapThrough, wrapThrough); }

    // Apple-specific property setters
    void setPointerEvents(EPointerEvents p) { m_inheritedData.m_pointerEvents = p; }

    void setTransformStyle3D(ETransformStyle3D b) { SET_VAR(m_rareNonInheritedData, m_transformStyle3D, b); }
    void setBackfaceVisibility(EBackfaceVisibility b) { SET_VAR(m_rareNonInheritedData, m_backfaceVisibility, b); }
    void setPerspective(float p) { SET_VAR(m_rareNonInheritedData, m_perspective, p); }
    void setPerspectiveOriginX(const Length& v) { setPerspectiveOrigin(LengthPoint(v, perspectiveOriginY())); }
    void setPerspectiveOriginY(const Length& v) { setPerspectiveOrigin(LengthPoint(perspectiveOriginX(), v)); }
    void setPerspectiveOrigin(const LengthPoint& p) { SET_VAR(m_rareNonInheritedData, m_perspectiveOrigin, p); }
    void setPageSize(const FloatSize& s) { SET_VAR(m_rareNonInheritedData, m_pageSize, s); }
    void setPageSizeType(PageSizeType t) { SET_VAR(m_rareNonInheritedData, m_pageSizeType, t); }
    void resetPageSizeType() { SET_VAR(m_rareNonInheritedData, m_pageSizeType, PAGE_SIZE_AUTO); }

    void setHasCurrentOpacityAnimation(bool b = true) { SET_VAR(m_rareNonInheritedData, m_hasCurrentOpacityAnimation, b); }
    void setHasCurrentTransformAnimation(bool b = true) { SET_VAR(m_rareNonInheritedData, m_hasCurrentTransformAnimation, b); }
    void setHasCurrentFilterAnimation(bool b = true) { SET_VAR(m_rareNonInheritedData, m_hasCurrentFilterAnimation, b); }
    void setHasCurrentBackdropFilterAnimation(bool b = true) { SET_VAR(m_rareNonInheritedData, m_hasCurrentBackdropFilterAnimation, b); }

    void setIsRunningOpacityAnimationOnCompositor(bool b = true) { SET_VAR(m_rareNonInheritedData, m_runningOpacityAnimationOnCompositor, b); }
    void setIsRunningTransformAnimationOnCompositor(bool b = true) { SET_VAR(m_rareNonInheritedData, m_runningTransformAnimationOnCompositor, b); }
    void setIsRunningFilterAnimationOnCompositor(bool b = true) { SET_VAR(m_rareNonInheritedData, m_runningFilterAnimationOnCompositor, b); }
    void setIsRunningBackdropFilterAnimationOnCompositor(bool b = true) { SET_VAR(m_rareNonInheritedData, m_runningBackdropFilterAnimationOnCompositor, b); }

    void setLineClamp(LineClampValue c) { SET_VAR(m_rareNonInheritedData, lineClamp, c); }
    void setTapHighlightColor(const Color& c) { SET_VAR(m_rareInheritedData, tapHighlightColor, c); }
    void setTextSecurity(ETextSecurity aTextSecurity) { SET_VAR(m_rareInheritedData, textSecurity, aTextSecurity); }
    void setTouchAction(TouchAction t) { SET_VAR(m_rareNonInheritedData, m_touchAction, t); }

    void setScrollBehavior(ScrollBehavior b) { SET_VAR(m_rareNonInheritedData, m_scrollBehavior, b); }

    void setScrollSnapType(ScrollSnapType b) { SET_VAR(m_rareNonInheritedData, m_scrollSnapType, b); }
    void setScrollSnapPointsX(const ScrollSnapPoints& b) { SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_xPoints, b); }
    void setScrollSnapPointsY(const ScrollSnapPoints& b) { SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_yPoints, b); }
    void setScrollSnapDestination(const LengthPoint& b) { SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_destination, b); }
    void setScrollSnapCoordinate(const Vector<LengthPoint>& b) { SET_NESTED_VAR(m_rareNonInheritedData, m_scrollSnap, m_coordinates, b); }

    void setWillChangeProperties(const Vector<CSSPropertyID>& properties) { SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_properties, properties); }
    void setWillChangeContents(bool b) { SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_contents, b); }
    void setWillChangeScrollPosition(bool b) { SET_NESTED_VAR(m_rareNonInheritedData, m_willChange, m_scrollPosition, b); }
    void setSubtreeWillChangeContents(bool b) { SET_VAR(m_rareInheritedData, m_subtreeWillChangeContents, b); }

    bool requiresAcceleratedCompositingForExternalReasons(bool b) { return m_rareNonInheritedData->m_requiresAcceleratedCompositingForExternalReasons; }
    void setRequiresAcceleratedCompositingForExternalReasons(bool b) { SET_VAR(m_rareNonInheritedData, m_requiresAcceleratedCompositingForExternalReasons, b); }

    const SVGComputedStyle& svgStyle() const { return *m_svgStyle.get(); }
    SVGComputedStyle& accessSVGStyle() { return *m_svgStyle.access(); }

    const SVGPaintType& fillPaintType() const { return svgStyle().fillPaintType(); }
    Color fillPaintColor() const { return svgStyle().fillPaintColor(); }
    float fillOpacity() const { return svgStyle().fillOpacity(); }
    void setFillOpacity(float f) { accessSVGStyle().setFillOpacity(f); }

    const SVGPaintType& strokePaintType() const { return svgStyle().strokePaintType(); }
    Color strokePaintColor() const { return svgStyle().strokePaintColor(); }
    float strokeOpacity() const { return svgStyle().strokeOpacity(); }
    void setStrokeOpacity(float f) { accessSVGStyle().setStrokeOpacity(f); }
    const UnzoomedLength& strokeWidth() const { return svgStyle().strokeWidth(); }
    void setStrokeWidth(const UnzoomedLength& w) { accessSVGStyle().setStrokeWidth(w); }
    SVGDashArray* strokeDashArray() const { return svgStyle().strokeDashArray(); }
    void setStrokeDashArray(PassRefPtr<SVGDashArray> array) { accessSVGStyle().setStrokeDashArray(array); }
    const Length& strokeDashOffset() const { return svgStyle().strokeDashOffset(); }
    void setStrokeDashOffset(const Length& d) { accessSVGStyle().setStrokeDashOffset(d); }
    float strokeMiterLimit() const { return svgStyle().strokeMiterLimit(); }
    void setStrokeMiterLimit(float f) { accessSVGStyle().setStrokeMiterLimit(f); }

    void setD(PassRefPtr<StylePath> d) { accessSVGStyle().setD(d); }
    void setCx(const Length& cx) { accessSVGStyle().setCx(cx); }
    void setCy(const Length& cy) { accessSVGStyle().setCy(cy); }
    void setX(const Length& x) { accessSVGStyle().setX(x); }
    void setY(const Length& y) { accessSVGStyle().setY(y); }
    void setR(const Length& r) { accessSVGStyle().setR(r); }
    void setRx(const Length& rx) { accessSVGStyle().setRx(rx); }
    void setRy(const Length& ry) { accessSVGStyle().setRy(ry); }

    float floodOpacity() const { return svgStyle().floodOpacity(); }
    void setFloodOpacity(float f) { accessSVGStyle().setFloodOpacity(f); }

    float stopOpacity() const { return svgStyle().stopOpacity(); }
    void setStopOpacity(float f) { accessSVGStyle().setStopOpacity(f); }

    void setStopColor(const Color& c) { accessSVGStyle().setStopColor(c); }
    void setFloodColor(const Color& c) { accessSVGStyle().setFloodColor(c); }
    void setLightingColor(const Color& c) { accessSVGStyle().setLightingColor(c); }

    EBaselineShift baselineShift() const { return svgStyle().baselineShift(); }
    const Length& baselineShiftValue() const { return svgStyle().baselineShiftValue(); }
    void setBaselineShiftValue(const Length& value)
    {
        SVGComputedStyle& svgStyle = accessSVGStyle();
        svgStyle.setBaselineShift(BS_LENGTH);
        svgStyle.setBaselineShiftValue(value);
    }

    void setShapeOutside(ShapeValue* value)
    {
        if (m_rareNonInheritedData->m_shapeOutside == value)
            return;
        m_rareNonInheritedData.access()->m_shapeOutside = value;
    }
    ShapeValue* shapeOutside() const { return m_rareNonInheritedData->m_shapeOutside.get(); }

    static ShapeValue* initialShapeOutside() { return 0; }

    void setClipPath(PassRefPtr<ClipPathOperation> operation)
    {
        if (m_rareNonInheritedData->m_clipPath != operation)
            m_rareNonInheritedData.access()->m_clipPath = operation;
    }
    ClipPathOperation* clipPath() const { return m_rareNonInheritedData->m_clipPath.get(); }

    static ClipPathOperation* initialClipPath() { return 0; }

    const Length& shapeMargin() const { return m_rareNonInheritedData->m_shapeMargin; }
    void setShapeMargin(const Length& shapeMargin) { SET_VAR(m_rareNonInheritedData, m_shapeMargin, shapeMargin); }
    static Length initialShapeMargin() { return Length(0, Fixed); }

    float shapeImageThreshold() const { return m_rareNonInheritedData->m_shapeImageThreshold; }
    void setShapeImageThreshold(float shapeImageThreshold)
    {
        float clampedShapeImageThreshold = clampTo<float>(shapeImageThreshold, 0, 1);
        SET_VAR(m_rareNonInheritedData, m_shapeImageThreshold, clampedShapeImageThreshold);
    }
    static float initialShapeImageThreshold() { return 0; }

    bool hasContent() const { return contentData(); }
    ContentData* contentData() const { return m_rareNonInheritedData->m_content.get(); }
    bool contentDataEquivalent(const ComputedStyle* otherStyle) const { return const_cast<ComputedStyle*>(this)->m_rareNonInheritedData->contentDataEquivalent(*const_cast<ComputedStyle*>(otherStyle)->m_rareNonInheritedData); }
    void setContent(ContentData*);

    const CounterDirectiveMap* counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();
    const CounterDirectives getCounterDirectives(const AtomicString& identifier) const;
    void clearIncrementDirectives();
    void clearResetDirectives();

    QuotesData* quotes() const { return m_rareInheritedData->quotes.get(); }
    void setQuotes(PassRefPtr<QuotesData>);

    Hyphenation* getHyphenation() const;
    const AtomicString& hyphenString() const;

    bool inheritedEqual(const ComputedStyle&) const;
    bool nonInheritedEqual(const ComputedStyle&) const;
    bool loadingCustomFontsEqual(const ComputedStyle&) const;
    bool inheritedDataShared(const ComputedStyle&) const;

    bool isDisplayReplacedType() const { return isDisplayReplacedType(display()); }
    bool isDisplayInlineType() const { return isDisplayInlineType(display()); }
    bool isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
    bool isDisplayFlexibleOrGridBox() const { return isDisplayFlexibleBox(display()) || isDisplayGridBox(display()); }
    bool isDisplayFlexibleBox() const { return isDisplayFlexibleBox(display()); }


    bool setWritingMode(WritingMode v)
    {
        if (v == getWritingMode())
            return false;

        m_inheritedData.m_writingMode = v;
        return true;
    }

    // A unique style is one that has matches something that makes it impossible to share.
    bool unique() const { return m_nonInheritedData.m_unique; }
    void setUnique() { m_nonInheritedData.m_unique = true; }

    bool isSharable() const;

    bool emptyState() const { return m_nonInheritedData.m_emptyState; }
    void setEmptyState(bool b) { setUnique(); m_nonInheritedData.m_emptyState = b; }

    Color visitedDependentColor(int colorProperty) const;

    void setHasExplicitlyInheritedProperties() { m_nonInheritedData.m_explicitInheritance = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedData.m_explicitInheritance; }

    void setHasVariableReferenceFromNonInheritedProperty() { m_nonInheritedData.m_variableReference = true; }
    bool hasVariableReferenceFromNonInheritedProperty() const { return m_nonInheritedData.m_variableReference; }

    bool hasChildDependentFlags() const { return emptyState() || hasExplicitlyInheritedProperties(); }
    void copyChildDependentFlagsFrom(const ComputedStyle&);

    bool hasBoxDecorations() const
    {
        return hasBorderDecoration()
            || hasBorderRadius()
            || hasOutline()
            || hasAppearance()
            || boxShadow()
            || hasFilterInducingProperty()
            || hasBackdropFilter()
            || resize() != RESIZE_NONE;
    }

    bool borderObscuresBackground() const;
    void getBorderEdgeInfo(BorderEdge edges[], bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    void setHasAuthorBackground(bool authorBackground) { SET_VAR(m_rareNonInheritedData, m_hasAuthorBackground, authorBackground); }
    void setHasAuthorBorder(bool authorBorder) { SET_VAR(m_rareNonInheritedData, m_hasAuthorBorder, authorBorder); }
    bool hasAuthorBackground() const { return m_rareNonInheritedData->m_hasAuthorBackground; };
    bool hasAuthorBorder() const { return m_rareNonInheritedData->m_hasAuthorBorder; };

    void addPaintImage(StyleImage*);

    // Initial values for all the properties
    static EBorderCollapse initialBorderCollapse() { return BorderCollapseSeparate; }
    static EBorderStyle initialBorderStyle() { return BorderStyleNone; }
    static OutlineIsAuto initialOutlineStyleIsAuto() { return OutlineIsAutoOff; }
    static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
    static LengthSize initialBorderRadius() { return LengthSize(Length(0, Fixed), Length(0, Fixed)); }
    static ECaptionSide initialCaptionSide() { return CaptionSideTop; }
    static EClear initialClear() { return ClearNone; }
    static LengthBox initialClip() { return LengthBox(); }
    static Containment initialContain() { return ContainsNone; }
    static TextDirection initialDirection() { return LTR; }
    static WritingMode initialWritingMode() { return TopToBottomWritingMode; }
    static TextCombine initialTextCombine() { return TextCombineNone; }
    static TextOrientation initialTextOrientation() { return TextOrientationMixed; }
    static ObjectFit initialObjectFit() { return ObjectFitFill; }
    static LengthPoint initialObjectPosition() { return LengthPoint(Length(50.0, Percent), Length(50.0, Percent)); }
    static EDisplay initialDisplay() { return INLINE; }
    static EEmptyCells initialEmptyCells() { return EmptyCellsShow; }
    static EFloat initialFloating() { return NoFloat; }
    static EListStylePosition initialListStylePosition() { return ListStylePositionOutside; }
    static EListStyleType initialListStyleType() { return Disc; }
    static EOverflowAnchor initialOverflowAnchor() { return AnchorAuto; }
    static EOverflow initialOverflowX() { return OverflowVisible; }
    static EOverflow initialOverflowY() { return OverflowVisible; }
    static EBreak initialBreakAfter() { return BreakAuto; }
    static EBreak initialBreakBefore() { return BreakAuto; }
    static EBreak initialBreakInside() { return BreakAuto; }
    static EPosition initialPosition() { return StaticPosition; }
    static ETableLayout initialTableLayout() { return TableLayoutAuto; }
    static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
    static ETextTransform initialTextTransform() { return TTNONE; }
    static EVisibility initialVisibility() { return VISIBLE; }
    static EWhiteSpace initialWhiteSpace() { return NORMAL; }
    static short initialHorizontalBorderSpacing() { return 0; }
    static short initialVerticalBorderSpacing() { return 0; }
    static ECursor initialCursor() { return CURSOR_AUTO; }
    static Color initialColor() { return Color::black; }
    static StyleImage* initialListStyleImage() { return 0; }
    static unsigned initialBorderWidth() { return 3; }
    static unsigned short initialColumnRuleWidth() { return 3; }
    static unsigned short initialOutlineWidth() { return 3; }
    static float initialLetterWordSpacing() { return 0.0f; }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(); }
    static Length initialMaxSize() { return Length(MaxSizeNone); }
    static Length initialOffset() { return Length(); }
    static Length initialMargin() { return Length(Fixed); }
    static Length initialPadding() { return Length(Fixed); }
    static Length initialTextIndent() { return Length(Fixed); }
    static TextIndentLine initialTextIndentLine() { return TextIndentFirstLine; }
    static TextIndentType initialTextIndentType() { return TextIndentNormal; }
    static EVerticalAlign initialVerticalAlign() { return VerticalAlignBaseline; }
    static short initialWidows() { return 2; }
    static short initialOrphans() { return 2; }
    static Length initialLineHeight() { return Length(-100.0, Percent); }
    static ETextAlign initialTextAlign() { return TASTART; }
    static TextAlignLast initialTextAlignLast() { return TextAlignLastAuto; }
    static TextJustify initialTextJustify() { return TextJustifyAuto; }
    static TextDecoration initialTextDecoration() { return TextDecorationNone; }
    static TextUnderlinePosition initialTextUnderlinePosition() { return TextUnderlinePositionAuto; }
    static TextDecorationStyle initialTextDecorationStyle() { return TextDecorationStyleSolid; }
    static float initialZoom() { return 1.0f; }
    static int initialOutlineOffset() { return 0; }
    static float initialOpacity() { return 1.0f; }
    static EBoxAlignment initialBoxAlign() { return BSTRETCH; }
    static EBoxDecorationBreak initialBoxDecorationBreak() { return BoxDecorationBreakSlice; }
    static EBoxDirection initialBoxDirection() { return BNORMAL; }
    static EBoxLines initialBoxLines() { return SINGLE; }
    static EBoxOrient initialBoxOrient() { return HORIZONTAL; }
    static EBoxPack initialBoxPack() { return BoxPackStart; }
    static float initialBoxFlex() { return 0.0f; }
    static unsigned initialBoxFlexGroup() { return 1; }
    static unsigned initialBoxOrdinalGroup() { return 1; }
    static EBoxSizing initialBoxSizing() { return BoxSizingContentBox; }
    static StyleReflection* initialBoxReflect() { return 0; }
    static float initialFlexGrow() { return 0; }
    static float initialFlexShrink() { return 1; }
    static Length initialFlexBasis() { return Length(Auto); }
    static int initialOrder() { return 0; }
    static StyleContentAlignmentData initialContentAlignment() { return StyleContentAlignmentData(ContentPositionNormal, ContentDistributionDefault, OverflowAlignmentDefault); }
    static StyleSelfAlignmentData initialSelfAlignment() { return StyleSelfAlignmentData(ItemPositionAuto, OverflowAlignmentDefault); }
    static EFlexDirection initialFlexDirection() { return FlowRow; }
    static EFlexWrap initialFlexWrap() { return FlexNoWrap; }
    static EUserModify initialUserModify() { return READ_ONLY; }
    static EUserDrag initialUserDrag() { return DRAG_AUTO; }
    static EUserSelect initialUserSelect() { return SELECT_TEXT; }
    static TextOverflow initialTextOverflow() { return TextOverflowClip; }
    static EMarginCollapse initialMarginBeforeCollapse() { return MarginCollapseCollapse; }
    static EMarginCollapse initialMarginAfterCollapse() { return MarginCollapseCollapse; }
    static EWordBreak initialWordBreak() { return NormalWordBreak; }
    static EOverflowWrap initialOverflowWrap() { return NormalOverflowWrap; }
    static LineBreak initialLineBreak() { return LineBreakAuto; }
    static const AtomicString& initialHighlight() { return nullAtom; }
    static ESpeak initialSpeak() { return SpeakNormal; }
    static Hyphens initialHyphens() { return HyphensManual; }
    static const AtomicString& initialHyphenationString() { return nullAtom; }
    static EResize initialResize() { return RESIZE_NONE; }
    static ControlPart initialAppearance() { return NoControlPart; }
    static Order initialRTLOrdering() { return LogicalOrder; }
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static ColumnFill initialColumnFill() { return ColumnFillBalance; }
    static ColumnSpan initialColumnSpan() { return ColumnSpanNone; }
    static EmptyTransformOperations initialTransform() { return EmptyTransformOperations(); }
    static PassRefPtr<TranslateTransformOperation> initialTranslate() { return TranslateTransformOperation::create(Length(0, Fixed), Length(0, Fixed), 0, TransformOperation::Translate3D); }
    static PassRefPtr<RotateTransformOperation> initialRotate() { return RotateTransformOperation::create(0, 0, 1, 0, TransformOperation::Rotate3D); }
    static PassRefPtr<ScaleTransformOperation> initialScale() { return ScaleTransformOperation::create(1, 1, 1, TransformOperation::Scale3D); }
    static Length initialTransformOriginX() { return Length(50.0, Percent); }
    static Length initialTransformOriginY() { return Length(50.0, Percent); }
    static float initialTransformOriginZ() { return 0; }
    static TransformOrigin initialTransformOrigin() { return TransformOrigin(Length(50.0, Percent), Length(50.0, Percent), 0); }
    static EPointerEvents initialPointerEvents() { return PE_AUTO; }
    static ETransformStyle3D initialTransformStyle3D() { return TransformStyle3DFlat; }
    static StylePath* initialMotionPath() { return nullptr; }
    static Length initialMotionOffset() { return Length(0, Fixed); }
    static StyleMotionRotation initialMotionRotation() { return StyleMotionRotation(0, MotionRotationAuto); }
    static EBackfaceVisibility initialBackfaceVisibility() { return BackfaceVisibilityVisible; }
    static float initialPerspective() { return 0; }
    static Length initialPerspectiveOriginX() { return Length(50.0, Percent); }
    static Length initialPerspectiveOriginY() { return Length(50.0, Percent); }
    static LengthPoint initialPerspectiveOrigin() { return LengthPoint(Length(50.0, Percent), Length(50.0, Percent)); }
    static Color initialBackgroundColor() { return Color::transparent; }
    static TextEmphasisFill initialTextEmphasisFill() { return TextEmphasisFillFilled; }
    static TextEmphasisMark initialTextEmphasisMark() { return TextEmphasisMarkNone; }
    static const AtomicString& initialTextEmphasisCustomMark() { return nullAtom; }
    static TextEmphasisPosition initialTextEmphasisPosition() { return TextEmphasisPositionOver; }
    static RubyPosition initialRubyPosition() { return RubyPositionBefore; }
    static ImageOrientationEnum initialImageOrientation() { return OriginTopLeft; }
    static RespectImageOrientationEnum initialRespectImageOrientation() { return DoNotRespectImageOrientation; }
    static EImageRendering initialImageRendering() { return ImageRenderingAuto; }
    static ImageResolutionSource initialImageResolutionSource() { return ImageResolutionSpecified; }
    static ImageResolutionSnap initialImageResolutionSnap() { return ImageResolutionNoSnap; }
    static float initialImageResolution() { return 1; }
    static StyleImage* initialBorderImageSource() { return 0; }
    static StyleImage* initialMaskBoxImageSource() { return 0; }
    static PrintColorAdjust initialPrintColorAdjust() { return PrintColorAdjustEconomy; }
    static TouchAction initialTouchAction() { return TouchActionAuto; }
    static ShadowList* initialBoxShadow() { return 0; }
    static ShadowList* initialTextShadow() { return 0; }
    static StyleVariableData* initialVariables() { return nullptr; }
    static ScrollBehavior initialScrollBehavior() { return ScrollBehaviorAuto; }
    static ScrollSnapType initialScrollSnapType() { return ScrollSnapTypeNone; }
    static ScrollSnapPoints initialScrollSnapPointsX() { return ScrollSnapPoints(); }
    static ScrollSnapPoints initialScrollSnapPointsY() { return ScrollSnapPoints(); }
    static LengthPoint initialScrollSnapDestination() { return LengthPoint(Length(0, Fixed), Length(0, Fixed)); }
    static Vector<LengthPoint> initialScrollSnapCoordinate() { return Vector<LengthPoint>(); }

    // The initial value is 'none' for grid tracks.
    static Vector<GridTrackSize> initialGridTemplateColumns() { return Vector<GridTrackSize>(); }
    static Vector<GridTrackSize> initialGridTemplateRows() { return Vector<GridTrackSize>(); }
    static Vector<GridTrackSize> initialGridAutoRepeatTracks() { return Vector<GridTrackSize>(); }
    static size_t initialGridAutoRepeatInsertionPoint() { return 0; }
    static AutoRepeatType initialGridAutoRepeatType() { return NoAutoRepeat; }

    static GridAutoFlow initialGridAutoFlow() { return AutoFlowRow; }

    static Vector<GridTrackSize> initialGridAutoColumns();
    static Vector<GridTrackSize> initialGridAutoRows();

    static NamedGridLinesMap initialNamedGridColumnLines() { return NamedGridLinesMap(); }
    static NamedGridLinesMap initialNamedGridRowLines() { return NamedGridLinesMap(); }

    static OrderedNamedGridLines initialOrderedNamedGridColumnLines() { return OrderedNamedGridLines(); }
    static OrderedNamedGridLines initialOrderedNamedGridRowLines() { return OrderedNamedGridLines(); }

    static NamedGridAreaMap initialNamedGridArea() { return NamedGridAreaMap(); }
    static size_t initialNamedGridAreaCount() { return 0; }

    static Length initialGridColumnGap() { return Length(Fixed); }
    static Length initialGridRowGap() { return Length(Fixed); }

    // 'auto' is the default.
    static GridPosition initialGridColumnStart() { return GridPosition(); }
    static GridPosition initialGridColumnEnd() { return GridPosition(); }
    static GridPosition initialGridRowStart() { return GridPosition(); }
    static GridPosition initialGridRowEnd() { return GridPosition(); }

    static TabSize initialTabSize() { return TabSize(8); }

    static TextSizeAdjust initialTextSizeAdjust() { return TextSizeAdjust::adjustAuto(); }

    static WrapFlow initialWrapFlow() { return WrapFlowAuto; }
    static WrapThrough initialWrapThrough() { return WrapThroughWrap; }

    static QuotesData* initialQuotes() { return 0; }

    // Keep these at the end.
    // FIXME: Why? Seems these should all be one big sorted list.
    static LineClampValue initialLineClamp() { return LineClampValue(); }
    static ETextSecurity initialTextSecurity() { return TSNONE; }
    static Color initialTapHighlightColor();
    static const FilterOperations& initialFilter();
    static const FilterOperations& initialBackdropFilter();
    static WebBlendMode initialBlendMode() { return WebBlendModeNormal; }
    static EIsolation initialIsolation() { return IsolationAuto; }
private:
    void setVisitedLinkColor(const Color&);
    void setVisitedLinkBackgroundColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkBackgroundColor, v); }
    void setVisitedLinkBorderLeftColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderLeftColor, v); }
    void setVisitedLinkBorderRightColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderRightColor, v); }
    void setVisitedLinkBorderBottomColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderBottomColor, v); }
    void setVisitedLinkBorderTopColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkBorderTopColor, v); }
    void setVisitedLinkOutlineColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkOutlineColor, v); }
    void setVisitedLinkColumnRuleColor(const StyleColor& v) { SET_NESTED_VAR(m_rareNonInheritedData, m_multiCol, m_visitedLinkColumnRuleColor, v); }
    void setVisitedLinkTextDecorationColor(const StyleColor& v) { SET_VAR(m_rareNonInheritedData, m_visitedLinkTextDecorationColor, v); }
    void setVisitedLinkTextEmphasisColor(const StyleColor& v) { SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextEmphasisColor, setVisitedLinkTextEmphasisColor, v); }
    void setVisitedLinkTextFillColor(const StyleColor& v) { SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextFillColor, setVisitedLinkTextFillColor, v); }
    void setVisitedLinkTextStrokeColor(const StyleColor& v) { SET_VAR_WITH_SETTER(m_rareInheritedData, visitedLinkTextStrokeColor, setVisitedLinkTextStrokeColor, v); }

    void inheritUnicodeBidiFrom(const ComputedStyle& parent) { m_nonInheritedData.m_unicodeBidi = parent.m_nonInheritedData.m_unicodeBidi; }

    bool isDisplayFlexibleBox(EDisplay display) const
    {
        return display == FLEX || display == INLINE_FLEX;
    }

    bool isDisplayGridBox(EDisplay display) const
    {
        return display == GRID || display == INLINE_GRID;
    }

    bool isDisplayReplacedType(EDisplay display) const
    {
        return display == INLINE_BLOCK || display == INLINE_BOX || display == INLINE_FLEX
            || display == INLINE_TABLE || display == INLINE_GRID;
    }

    bool isDisplayInlineType(EDisplay display) const
    {
        return display == INLINE || isDisplayReplacedType(display);
    }

    // Color accessors are all private to make sure callers use visitedDependentColor instead to access them.
    StyleColor borderLeftColor() const { return m_surround->border.left().color(); }
    StyleColor borderRightColor() const { return m_surround->border.right().color(); }
    StyleColor borderTopColor() const { return m_surround->border.top().color(); }
    StyleColor borderBottomColor() const { return m_surround->border.bottom().color(); }
    StyleColor backgroundColor() const { return m_background->color(); }
    Color color() const;
    StyleColor columnRuleColor() const { return m_rareNonInheritedData->m_multiCol->m_rule.color(); }
    StyleColor outlineColor() const { return m_background->outline().color(); }
    StyleColor textEmphasisColor() const { return m_rareInheritedData->textEmphasisColor(); }
    StyleColor textFillColor() const { return m_rareInheritedData->textFillColor(); }
    StyleColor textStrokeColor() const { return m_rareInheritedData->textStrokeColor(); }
    Color visitedLinkColor() const;
    StyleColor visitedLinkBackgroundColor() const { return m_rareNonInheritedData->m_visitedLinkBackgroundColor; }
    StyleColor visitedLinkBorderLeftColor() const { return m_rareNonInheritedData->m_visitedLinkBorderLeftColor; }
    StyleColor visitedLinkBorderRightColor() const { return m_rareNonInheritedData->m_visitedLinkBorderRightColor; }
    StyleColor visitedLinkBorderBottomColor() const { return m_rareNonInheritedData->m_visitedLinkBorderBottomColor; }
    StyleColor visitedLinkBorderTopColor() const { return m_rareNonInheritedData->m_visitedLinkBorderTopColor; }
    StyleColor visitedLinkOutlineColor() const { return m_rareNonInheritedData->m_visitedLinkOutlineColor; }
    StyleColor visitedLinkColumnRuleColor() const { return m_rareNonInheritedData->m_multiCol->m_visitedLinkColumnRuleColor; }
    StyleColor textDecorationColor() const { return m_rareNonInheritedData->m_textDecorationColor; }
    StyleColor visitedLinkTextDecorationColor() const { return m_rareNonInheritedData->m_visitedLinkTextDecorationColor; }
    StyleColor visitedLinkTextEmphasisColor() const { return m_rareInheritedData->visitedLinkTextEmphasisColor(); }
    StyleColor visitedLinkTextFillColor() const { return m_rareInheritedData->visitedLinkTextFillColor(); }
    StyleColor visitedLinkTextStrokeColor() const { return m_rareInheritedData->visitedLinkTextStrokeColor(); }

    StyleColor decorationColorIncludingFallback(bool visitedLink) const;
    Color colorIncludingFallback(int colorProperty, bool visitedLink) const;

    Color stopColor() const { return svgStyle().stopColor(); }
    Color floodColor() const { return svgStyle().floodColor(); }
    Color lightingColor() const { return svgStyle().lightingColor(); }

    void addAppliedTextDecoration(const AppliedTextDecoration&);
    void applyMotionPathTransform(float originX, float originY, TransformationMatrix&) const;

    bool diffNeedsFullLayoutAndPaintInvalidation(const ComputedStyle& other) const;
    bool diffNeedsFullLayout(const ComputedStyle& other) const;
    bool diffNeedsPaintInvalidationSubtree(const ComputedStyle& other) const;
    bool diffNeedsPaintInvalidationObject(const ComputedStyle& other) const;
    bool diffNeedsPaintInvalidationObjectForPaintImage(const StyleImage*, const ComputedStyle& other) const;
    void updatePropertySpecificDifferences(const ComputedStyle& other, StyleDifference&) const;

    bool requireTransformOrigin(ApplyTransformOrigin applyOrigin, ApplyMotionPath) const;
    static bool shadowListHasCurrentColor(const ShadowList*);
};

// FIXME: Reduce/remove the dependency on zoom adjusted int values.
// The float or LayoutUnit versions of layout values should be used.
int adjustForAbsoluteZoom(int value, float zoomFactor);

inline int adjustForAbsoluteZoom(int value, const ComputedStyle* style)
{
    float zoomFactor = style->effectiveZoom();
    if (zoomFactor == 1)
        return value;
    return adjustForAbsoluteZoom(value, zoomFactor);
}

inline float adjustFloatForAbsoluteZoom(float value, const ComputedStyle& style)
{
    return value / style.effectiveZoom();
}

inline double adjustDoubleForAbsoluteZoom(double value, const ComputedStyle& style)
{
    return value / style.effectiveZoom();
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const ComputedStyle& style)
{
    return LayoutUnit(value / style.effectiveZoom());
}

inline double adjustScrollForAbsoluteZoom(double scrollOffset, float zoomFactor)
{
    return scrollOffset / zoomFactor;
}

inline double adjustScrollForAbsoluteZoom(double scrollOffset, const ComputedStyle& style)
{
    return adjustScrollForAbsoluteZoom(scrollOffset, style.effectiveZoom());
}

inline bool ComputedStyle::setZoom(float f)
{
    if (compareEqual(m_visual->m_zoom, f))
        return false;
    m_visual.access()->m_zoom = f;
    setEffectiveZoom(effectiveZoom() * zoom());
    return true;
}

inline bool ComputedStyle::setEffectiveZoom(float f)
{
    // Clamp the effective zoom value to a smaller (but hopeful still large
    // enough) range, to avoid overflow in derived computations.
    float clampedEffectiveZoom = clampTo<float>(f, 1e-6, 1e6);
    if (compareEqual(m_rareInheritedData->m_effectiveZoom, clampedEffectiveZoom))
        return false;
    m_rareInheritedData.access()->m_effectiveZoom = clampedEffectiveZoom;
    return true;
}

inline bool ComputedStyle::isSharable() const
{
    if (unique())
        return false;
    if (hasUniquePseudoStyle())
        return false;
    return true;
}

inline bool ComputedStyle::setTextOrientation(TextOrientation textOrientation)
{
    if (compareEqual(m_rareInheritedData->m_textOrientation, textOrientation))
        return false;

    m_rareInheritedData.access()->m_textOrientation = textOrientation;
    return true;
}

inline bool ComputedStyle::hasAnyPublicPseudoStyles() const
{
    return PublicPseudoIdMask & m_nonInheritedData.m_pseudoBits;
}

inline bool ComputedStyle::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > PseudoIdNone);
    ASSERT(pseudo < FirstInternalPseudoId);
    return (1 << (pseudo - 1)) & m_nonInheritedData.m_pseudoBits;
}

inline void ComputedStyle::setHasPseudoStyle(PseudoId pseudo)
{
    ASSERT(pseudo > PseudoIdNone);
    ASSERT(pseudo < FirstInternalPseudoId);
    m_nonInheritedData.m_pseudoBits |= 1 << (pseudo - 1);
}

inline bool ComputedStyle::hasPseudoElementStyle() const
{
    return m_nonInheritedData.m_pseudoBits & ElementPseudoIdMask;
}

} // namespace blink

#endif // ComputedStyle_h
