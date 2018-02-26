// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticSelection_h
#define StaticSelection_h

#include "core/editing/Forward.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Node;

class StaticSelection final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StaticSelection* FromSelectionInFlatTree(const SelectionInFlatTree&);

  Node* anchorNode() const { return anchor_node_; }
  unsigned anchorOffset() const { return anchor_offset_; }
  Node* focusNode() const { return focus_node_; }
  unsigned focusOffset() const { return focus_offset_; }

  void Trace(blink::Visitor*);

 private:
  explicit StaticSelection(const SelectionInFlatTree&);

  const Member<Node> anchor_node_;
  const unsigned anchor_offset_;
  const Member<Node> focus_node_;
  const unsigned focus_offset_;

  DISALLOW_COPY_AND_ASSIGN(StaticSelection);
};

}  // namespace blink

#endif  // StaticSelection_h
