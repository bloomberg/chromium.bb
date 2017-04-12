/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef LayoutTreeAsText_h
#define LayoutTreeAsText_h

#include "core/CoreExport.h"
#include "platform/text/TextStream.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Color;
class PaintLayer;
class Element;
class LayoutRect;
class LocalFrame;
class LayoutBlockFlow;
class LayoutObject;
class TextStream;

enum LayoutAsTextBehaviorFlags {
  kLayoutAsTextBehaviorNormal = 0,
  kLayoutAsTextShowAllLayers =
      1 << 0,  // Dump all layers, not just those that would paint.
  kLayoutAsTextShowLayerNesting = 1 << 1,  // Annotate the layer lists.
  kLayoutAsTextShowCompositedLayers =
      1 << 2,  // Show which layers are composited.
  kLayoutAsTextShowAddresses = 1
                               << 3,  // Show layer and layoutObject addresses.
  kLayoutAsTextShowIDAndClass = 1 << 4,  // Show id and class attributes
  kLayoutAsTextPrintingMode = 1 << 5,    // Dump the tree in printing mode.
  kLayoutAsTextDontUpdateLayout =
      1 << 6,  // Don't update layout, to make it safe to call showLayerTree()
               // from the debugger inside layout or painting code.
  kLayoutAsTextShowLayoutState =
      1 << 7,  // Print the various 'needs layout' bits on layoutObjects.
  kLayoutAsTextShowLineTrees =
      1 << 8  // Dump the line trees for each LayoutBlockFlow.
};
typedef unsigned LayoutAsTextBehavior;

// You don't need pageWidthInPixels if you don't specify
// LayoutAsTextInPrintingMode.
CORE_EXPORT String
ExternalRepresentation(LocalFrame*,
                       LayoutAsTextBehavior = kLayoutAsTextBehaviorNormal,
                       const PaintLayer* marked_layer = nullptr);
CORE_EXPORT String
ExternalRepresentation(Element*,
                       LayoutAsTextBehavior = kLayoutAsTextBehaviorNormal);
void Write(TextStream&,
           const LayoutObject&,
           int indent = 0,
           LayoutAsTextBehavior = kLayoutAsTextBehaviorNormal);

class LayoutTreeAsText {
  STATIC_ONLY(LayoutTreeAsText);
  // FIXME: This is a cheesy hack to allow easy access to ComputedStyle colors.
  // It won't be needed if we convert it to use visitedDependentColor instead.
  // (This just involves rebaselining many results though, so for now it's
  // not being done).
 public:
  static void WriteLayoutObject(TextStream&,
                                const LayoutObject&,
                                LayoutAsTextBehavior);
  static void WriteLayers(TextStream&,
                          const PaintLayer* root_layer,
                          PaintLayer*,
                          const LayoutRect& paint_dirty_rect,
                          int indent = 0,
                          LayoutAsTextBehavior = kLayoutAsTextBehaviorNormal,
                          const PaintLayer* marked_layer = nullptr);
  static void WriteLineBoxTree(TextStream&,
                               const LayoutBlockFlow&,
                               int indent = 0);
};

// Helper function shared with SVGLayoutTreeAsText
String QuoteAndEscapeNonPrintables(const String&);

CORE_EXPORT String CounterValueForElement(Element*);

CORE_EXPORT String MarkerTextForListItem(Element*);

TextStream& operator<<(TextStream&, const Color&);

}  // namespace blink

#endif  // LayoutTreeAsText_h
