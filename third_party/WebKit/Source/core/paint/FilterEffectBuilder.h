/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterEffectBuilder_h
#define FilterEffectBuilder_h

#include "core/CoreExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CompositorFilterOperations;
class Filter;
class FilterEffect;
class FilterOperations;
class FloatRect;
class Node;
class ReferenceFilterOperation;
class SVGFilterElement;
class SVGFilterGraphNodeMap;

class CORE_EXPORT FilterEffectBuilder final {
  STACK_ALLOCATED();

 public:
  FilterEffectBuilder(const FloatRect& reference_box,
                      float zoom,
                      const PaintFlags* fill_flags = nullptr,
                      const PaintFlags* stroke_flags = nullptr);
  FilterEffectBuilder(Node*,
                      const FloatRect& reference_box,
                      float zoom,
                      const PaintFlags* fill_flags = nullptr,
                      const PaintFlags* stroke_flags = nullptr);

  Filter* BuildReferenceFilter(SVGFilterElement&,
                               FilterEffect* previous_effect,
                               SVGFilterGraphNodeMap* = nullptr) const;

  FilterEffect* BuildFilterEffect(const FilterOperations&) const;
  CompositorFilterOperations BuildFilterOperations(
      const FilterOperations&) const;

 private:
  Filter* BuildReferenceFilter(const ReferenceFilterOperation&,
                               FilterEffect* previous_effect) const;

  Member<Node> target_context_;
  FloatRect reference_box_;
  float zoom_;
  const PaintFlags* fill_flags_;
  const PaintFlags* stroke_flags_;
};

}  // namespace blink

#endif  // FilterEffectBuilder_h
