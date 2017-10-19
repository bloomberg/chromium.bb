/*
 * Copyright (C) 2011 Google Inc.
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

#ifndef DragState_h
#define DragState_h

#include "core/page/DragActions.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class DataTransfer;
class Node;

class DragState final : public GarbageCollected<DragState> {
  WTF_MAKE_NONCOPYABLE(DragState);

 public:
  DragState() {}

  // Element that may be a drag source, for the current mouse gesture.
  Member<Node> drag_src_;
  DragSourceAction drag_type_;
  // Used on only the source side of dragging.
  Member<DataTransfer> drag_data_transfer_;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(drag_src_);
    visitor->Trace(drag_data_transfer_);
  }
};

}  // namespace blink

#endif  // DragState_h
