/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleBuilderConverter_h
#define StyleBuilderConverter_h

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleOffsetRotation.h"
#include "core/style/StyleReflection.h"
#include "core/style/TransformOrigin.h"
#include "platform/LengthSize.h"
#include "platform/fonts/FontDescription.h"
#include "platform/text/TabSize.h"
#include "platform/transforms/Rotation.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ClipPathOperation;
class RotateTransformOperation;
class ScaleTransformOperation;
class StylePath;
class TextSizeAdjust;
class TranslateTransformOperation;

class StyleBuilderConverterBase {
  STATIC_ONLY(StyleBuilderConverterBase);

 public:
  static FontWeight ConvertFontWeight(const CSSValue&, FontWeight);
  static FontDescription::FontVariantCaps ConvertFontVariantCaps(
      const CSSValue&);
  static FontDescription::FamilyDescription ConvertFontFamily(
      const CSSValue&,
      FontBuilder*,
      const Document* document_for_count);
  static FontDescription::Size ConvertFontSize(
      const CSSValue&,
      const CSSToLengthConversionData&,
      FontDescription::Size parent_size);
};

// Note that we assume the parser only allows valid CSSValue types.
class StyleBuilderConverter {
  STATIC_ONLY(StyleBuilderConverter);

 public:
  static PassRefPtr<StyleReflection> ConvertBoxReflect(StyleResolverState&,
                                                       const CSSValue&);
  static AtomicString ConvertFragmentIdentifier(StyleResolverState&,
                                                const CSSValue&);
  static Color ConvertColor(StyleResolverState&,
                            const CSSValue&,
                            bool for_visited_link = false);
  template <typename T>
  static T ConvertComputedLength(StyleResolverState&, const CSSValue&);
  static LengthBox ConvertClip(StyleResolverState&, const CSSValue&);
  static PassRefPtr<ClipPathOperation> ConvertClipPath(StyleResolverState&,
                                                       const CSSValue&);
  static FilterOperations ConvertFilterOperations(StyleResolverState&,
                                                  const CSSValue&);
  static FilterOperations ConvertOffscreenFilterOperations(const CSSValue&);
  template <typename T>
  static T ConvertFlags(StyleResolverState&, const CSSValue&);
  static FontDescription::FamilyDescription ConvertFontFamily(
      StyleResolverState&,
      const CSSValue&);
  static PassRefPtr<FontFeatureSettings> ConvertFontFeatureSettings(
      StyleResolverState&,
      const CSSValue&);
  static PassRefPtr<FontVariationSettings> ConvertFontVariationSettings(
      StyleResolverState&,
      const CSSValue&);
  static FontDescription::Size ConvertFontSize(StyleResolverState&,
                                               const CSSValue&);
  static float ConvertFontSizeAdjust(StyleResolverState&, const CSSValue&);
  static FontWeight ConvertFontWeight(StyleResolverState&, const CSSValue&);
  static FontDescription::FontVariantCaps ConvertFontVariantCaps(
      StyleResolverState&,
      const CSSValue&);
  static FontDescription::VariantLigatures ConvertFontVariantLigatures(
      StyleResolverState&,
      const CSSValue&);
  static FontVariantNumeric ConvertFontVariantNumeric(StyleResolverState&,
                                                      const CSSValue&);
  static StyleSelfAlignmentData ConvertSelfOrDefaultAlignmentData(
      StyleResolverState&,
      const CSSValue&);
  static StyleContentAlignmentData ConvertContentAlignmentData(
      StyleResolverState&,
      const CSSValue&);
  static GridAutoFlow ConvertGridAutoFlow(StyleResolverState&, const CSSValue&);
  static GridPosition ConvertGridPosition(StyleResolverState&, const CSSValue&);
  static GridTrackSize ConvertGridTrackSize(StyleResolverState&,
                                            const CSSValue&);
  static Vector<GridTrackSize> ConvertGridTrackSizeList(StyleResolverState&,
                                                        const CSSValue&);
  template <typename T>
  static T ConvertLineWidth(StyleResolverState&, const CSSValue&);
  static float ConvertBorderWidth(StyleResolverState&, const CSSValue&);
  static Length ConvertLength(const StyleResolverState&, const CSSValue&);
  static UnzoomedLength ConvertUnzoomedLength(const StyleResolverState&,
                                              const CSSValue&);
  static Length ConvertLengthOrAuto(const StyleResolverState&, const CSSValue&);
  static Length ConvertLengthSizing(StyleResolverState&, const CSSValue&);
  static Length ConvertLengthMaxSizing(StyleResolverState&, const CSSValue&);
  static TabSize ConvertLengthOrTabSpaces(StyleResolverState&, const CSSValue&);
  static Length ConvertLineHeight(StyleResolverState&, const CSSValue&);
  static float ConvertNumberOrPercentage(StyleResolverState&, const CSSValue&);
  static StyleOffsetRotation ConvertOffsetRotate(StyleResolverState&,
                                                 const CSSValue&);
  static LengthPoint ConvertPosition(StyleResolverState&, const CSSValue&);
  static LengthPoint ConvertPositionOrAuto(StyleResolverState&,
                                           const CSSValue&);
  static float ConvertPerspective(StyleResolverState&, const CSSValue&);
  static Length ConvertQuirkyLength(StyleResolverState&, const CSSValue&);
  static PassRefPtr<QuotesData> ConvertQuotes(StyleResolverState&,
                                              const CSSValue&);
  static LengthSize ConvertRadius(StyleResolverState&, const CSSValue&);
  static EPaintOrder ConvertPaintOrder(StyleResolverState&, const CSSValue&);
  static ShadowData ConvertShadow(const CSSToLengthConversionData&,
                                  StyleResolverState*,
                                  const CSSValue&);
  static double ConvertValueToNumber(const CSSFunctionValue*,
                                     const CSSPrimitiveValue*);
  static PassRefPtr<ShadowList> ConvertShadowList(StyleResolverState&,
                                                  const CSSValue&);
  static ShapeValue* ConvertShapeValue(StyleResolverState&, const CSSValue&);
  static float ConvertSpacing(StyleResolverState&, const CSSValue&);
  template <CSSValueID IdForNone>
  static AtomicString ConvertString(StyleResolverState&, const CSSValue&);
  static PassRefPtr<SVGDashArray> ConvertStrokeDasharray(StyleResolverState&,
                                                         const CSSValue&);
  static StyleColor ConvertStyleColor(StyleResolverState&,
                                      const CSSValue&,
                                      bool for_visited_link = false);
  static StyleAutoColor ConvertStyleAutoColor(StyleResolverState&,
                                              const CSSValue&,
                                              bool for_visited_link = false);
  static float ConvertTextStrokeWidth(StyleResolverState&, const CSSValue&);
  static TextSizeAdjust ConvertTextSizeAdjust(StyleResolverState&,
                                              const CSSValue&);
  static TransformOperations ConvertTransformOperations(StyleResolverState&,
                                                        const CSSValue&);
  static TransformOrigin ConvertTransformOrigin(StyleResolverState&,
                                                const CSSValue&);

  static void ConvertGridTrackList(
      const CSSValue&,
      Vector<GridTrackSize>&,
      NamedGridLinesMap&,
      OrderedNamedGridLines&,
      Vector<GridTrackSize>& auto_repeat_track_sizes,
      NamedGridLinesMap&,
      OrderedNamedGridLines&,
      size_t& auto_repeat_insertion_point,
      AutoRepeatType&,
      StyleResolverState&);
  static void CreateImplicitNamedGridLinesFromGridArea(
      const NamedGridAreaMap&,
      NamedGridLinesMap&,
      GridTrackSizingDirection);
  static void ConvertOrderedNamedGridLinesMapToNamedGridLinesMap(
      const OrderedNamedGridLines&,
      NamedGridLinesMap&);

  static ScrollSnapType ConvertSnapType(StyleResolverState&, const CSSValue&);
  static ScrollSnapAlign ConvertSnapAlign(StyleResolverState&, const CSSValue&);
  static PassRefPtr<TranslateTransformOperation> ConvertTranslate(
      StyleResolverState&,
      const CSSValue&);
  static PassRefPtr<RotateTransformOperation> ConvertRotate(StyleResolverState&,
                                                            const CSSValue&);
  static PassRefPtr<ScaleTransformOperation> ConvertScale(StyleResolverState&,
                                                          const CSSValue&);
  static RespectImageOrientationEnum ConvertImageOrientation(
      StyleResolverState&,
      const CSSValue&);
  static PassRefPtr<StylePath> ConvertPathOrNone(StyleResolverState&,
                                                 const CSSValue&);
  static PassRefPtr<BasicShape> ConvertOffsetPath(StyleResolverState&,
                                                  const CSSValue&);
  static StyleOffsetRotation ConvertOffsetRotate(const CSSValue&);
  template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
  static Length ConvertPositionLength(StyleResolverState&, const CSSValue&);
  static Rotation ConvertRotation(const CSSValue&);

  static const CSSValue& ConvertRegisteredPropertyInitialValue(const CSSValue&);
  static const CSSValue& ConvertRegisteredPropertyValue(
      const StyleResolverState&,
      const CSSValue&);
};

template <typename T>
T StyleBuilderConverter::ConvertComputedLength(StyleResolverState& state,
                                               const CSSValue& value) {
  return ToCSSPrimitiveValue(value).ComputeLength<T>(
      state.CssToLengthConversionData());
}

template <typename T>
T StyleBuilderConverter::ConvertFlags(StyleResolverState& state,
                                      const CSSValue& value) {
  T flags = static_cast<T>(0);
  if (value.IsIdentifierValue() &&
      ToCSSIdentifierValue(value).GetValueID() == CSSValueNone)
    return flags;
  for (auto& flag_value : ToCSSValueList(value))
    flags |= ToCSSIdentifierValue(*flag_value).ConvertTo<T>();
  return flags;
}

template <typename T>
T StyleBuilderConverter::ConvertLineWidth(StyleResolverState& state,
                                          const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    if (value_id == CSSValueThin)
      return 1;
    if (value_id == CSSValueMedium)
      return 3;
    if (value_id == CSSValueThick)
      return 5;
    NOTREACHED();
    return 0;
  }
  const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
  // FIXME: We are moving to use the full page zoom implementation to handle
  // high-dpi.  In that case specyfing a border-width of less than 1px would
  // result in a border that is one device pixel thick.  With this change that
  // would instead be rounded up to 2 device pixels.  Consider clamping it to
  // device pixels or zoom adjusted CSS pixels instead of raw CSS pixels.
  // Reference crbug.com/485650 and crbug.com/382483
  double result =
      primitive_value.ComputeLength<double>(state.CssToLengthConversionData());
  if (result > 0.0 && result < 1.0)
    return 1.0;
  return clampTo<T>(RoundForImpreciseConversion<T>(result),
                    defaultMinimumForClamp<T>(), defaultMaximumForClamp<T>());
}

template <CSSValueID cssValueFor0, CSSValueID cssValueFor100>
Length StyleBuilderConverter::ConvertPositionLength(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    Length length = StyleBuilderConverter::ConvertLength(state, pair.Second());
    if (ToCSSIdentifierValue(pair.First()).GetValueID() == cssValueFor0)
      return length;
    DCHECK_EQ(ToCSSIdentifierValue(pair.First()).GetValueID(), cssValueFor100);
    return length.SubtractFromOneHundredPercent();
  }

  if (value.IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value).GetValueID()) {
      case cssValueFor0:
        return Length(0, kPercent);
      case cssValueFor100:
        return Length(100, kPercent);
      case CSSValueCenter:
        return Length(50, kPercent);
      default:
        NOTREACHED();
    }
  }

  return StyleBuilderConverter::ConvertLength(state,
                                              ToCSSPrimitiveValue(value));
}

template <CSSValueID IdForNone>
AtomicString StyleBuilderConverter::ConvertString(StyleResolverState&,
                                                  const CSSValue& value) {
  if (value.IsStringValue())
    return AtomicString(ToCSSStringValue(value).Value());
  DCHECK_EQ(ToCSSIdentifierValue(value).GetValueID(), IdForNone);
  return g_null_atom;
}

}  // namespace blink

#endif
