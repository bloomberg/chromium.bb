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

#include "core/testing/LayerRectList.h"

#include "core/dom/Node.h"
#include "core/geometry/DOMRectReadOnly.h"
#include "core/testing/LayerRect.h"

namespace blink {

LayerRectList::LayerRectList() {}

unsigned LayerRectList::length() const {
  return list_.size();
}

LayerRect* LayerRectList::item(unsigned index) {
  if (index >= list_.size())
    return 0;

  return list_[index].Get();
}

void LayerRectList::Append(Node* layer_root_node,
                           const String& layer_type,
                           int layer_offset_x,
                           int layer_offset_y,
                           DOMRectReadOnly* layer_relative_rect) {
  list_.push_back(LayerRect::Create(layer_root_node, layer_type, layer_offset_x,
                                    layer_offset_y, layer_relative_rect));
}

DEFINE_TRACE(LayerRectList) {
  visitor->Trace(list_);
}

}  // namespace blink
