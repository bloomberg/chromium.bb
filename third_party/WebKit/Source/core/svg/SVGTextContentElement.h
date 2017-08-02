/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGTextContentElement_h
#define SVGTextContentElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGraphicsElement.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class LineLayoutItem;
class SVGPointTearOff;

enum SVGLengthAdjustType {
  kSVGLengthAdjustUnknown,
  kSVGLengthAdjustSpacing,
  kSVGLengthAdjustSpacingAndGlyphs
};
template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGLengthAdjustType>();

class CORE_EXPORT SVGTextContentElement : public SVGGraphicsElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Forward declare enumerations in the W3C naming scheme, for IDL generation.
  enum {
    kLengthadjustUnknown = kSVGLengthAdjustUnknown,
    kLengthadjustSpacing = kSVGLengthAdjustSpacing,
    kLengthadjustSpacingandglyphs = kSVGLengthAdjustSpacingAndGlyphs
  };

  unsigned getNumberOfChars();
  float getComputedTextLength();
  float getSubStringLength(unsigned charnum, unsigned nchars, ExceptionState&);
  SVGPointTearOff* getStartPositionOfChar(unsigned charnum, ExceptionState&);
  SVGPointTearOff* getEndPositionOfChar(unsigned charnum, ExceptionState&);
  SVGRectTearOff* getExtentOfChar(unsigned charnum, ExceptionState&);
  float getRotationOfChar(unsigned charnum, ExceptionState&);
  int getCharNumAtPosition(SVGPointTearOff*, ExceptionState&);
  void selectSubString(unsigned charnum, unsigned nchars, ExceptionState&);

  static SVGTextContentElement* ElementFromLineLayoutItem(
      const LineLayoutItem&);

  SVGAnimatedLength* textLength() { return text_length_.Get(); }
  bool TextLengthIsSpecifiedByUser() {
    return text_length_is_specified_by_user_;
  }
  SVGAnimatedEnumeration<SVGLengthAdjustType>* lengthAdjust() {
    return length_adjust_.Get();
  }

  DECLARE_VIRTUAL_TRACE();

 protected:
  SVGTextContentElement(const QualifiedName&, Document&);

  bool IsPresentationAttribute(const QualifiedName&) const final;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) final;
  void SvgAttributeChanged(const QualifiedName&) override;

  bool SelfHasRelativeLengths() const override;

 private:
  bool IsTextContent() const final { return true; }

  Member<SVGAnimatedLength> text_length_;
  bool text_length_is_specified_by_user_;
  Member<SVGAnimatedEnumeration<SVGLengthAdjustType>> length_adjust_;
};

inline bool IsSVGTextContentElement(const SVGElement& element) {
  return element.IsTextContent();
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGTextContentElement);

}  // namespace blink

#endif  // SVGTextContentElement_h
