/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef CSSToStyleMap_h
#define CSSToStyleMap_h

#include "core/CSSPropertyNames.h"
#include "core/animation/Timing.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Document;
class FillLayer;
class CSSValue;
class StyleResolverState;
class NinePieceImage;
class BorderImageLengthBox;

class CSSToStyleMap {
  STATIC_ONLY(CSSToStyleMap);

 public:
  static void MapFillAttachment(StyleResolverState&,
                                FillLayer*,
                                const CSSValue&);
  static void MapFillClip(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillComposite(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillBlendMode(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillOrigin(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillImage(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillRepeatX(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillRepeatY(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillSize(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillXPosition(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillYPosition(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillMaskSourceType(StyleResolverState&,
                                    FillLayer*,
                                    const CSSValue&);

  static double MapAnimationDelay(const CSSValue&);
  static Timing::PlaybackDirection MapAnimationDirection(const CSSValue&);
  static double MapAnimationDuration(const CSSValue&);
  static Timing::FillMode MapAnimationFillMode(const CSSValue&);
  static double MapAnimationIterationCount(const CSSValue&);
  static AtomicString MapAnimationName(const CSSValue&);
  static EAnimPlayState MapAnimationPlayState(const CSSValue&);
  static CSSTransitionData::TransitionProperty MapAnimationProperty(
      const CSSValue&);

  // Pass a Document* if allow_step_middle is true so that the usage can be
  // counted.
  static RefPtr<TimingFunction> MapAnimationTimingFunction(
      const CSSValue&,
      bool allow_step_middle = false,
      Document* = nullptr);

  static void MapNinePieceImage(StyleResolverState&,
                                CSSPropertyID,
                                const CSSValue&,
                                NinePieceImage&);
  static void MapNinePieceImageSlice(StyleResolverState&,
                                     const CSSValue&,
                                     NinePieceImage&);
  static BorderImageLengthBox MapNinePieceImageQuad(StyleResolverState&,
                                                    const CSSValue&);
  static void MapNinePieceImageRepeat(StyleResolverState&,
                                      const CSSValue&,
                                      NinePieceImage&);
};

}  // namespace blink

#endif
