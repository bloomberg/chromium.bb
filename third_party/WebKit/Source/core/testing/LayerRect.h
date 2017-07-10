/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
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

#ifndef LayerRect_h
#define LayerRect_h

#include "core/dom/Node.h"
#include "core/geometry/DOMRectReadOnly.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LayerRect final : public GarbageCollectedFinalized<LayerRect>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static LayerRect* Create(Node* node,
                           const String& layer_type,
                           int node_offset_x,
                           int node_offset_y,
                           DOMRectReadOnly* rect) {
    return new LayerRect(node, layer_type, node_offset_x, node_offset_y, rect);
  }

  Node* layerAssociatedNode() const { return layer_associated_node_.Get(); }
  String layerType() const { return layer_type_; }
  int associatedNodeOffsetX() const { return associated_node_offset_x_; }
  int associatedNodeOffsetY() const { return associated_node_offset_y_; }
  DOMRectReadOnly* layerRelativeRect() const { return rect_.Get(); }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(layer_associated_node_);
    visitor->Trace(rect_);
  }

 private:
  LayerRect(Node* node,
            const String& layer_name,
            int node_offset_x,
            int node_offset_y,
            DOMRectReadOnly* rect)
      : layer_associated_node_(node),
        layer_type_(layer_name),
        associated_node_offset_x_(node_offset_x),
        associated_node_offset_y_(node_offset_y),
        rect_(rect) {}

  Member<Node> layer_associated_node_;
  String layer_type_;
  int associated_node_offset_x_;
  int associated_node_offset_y_;
  Member<DOMRectReadOnly> rect_;
};

}  // namespace blink

#endif  // LayerRect_h
