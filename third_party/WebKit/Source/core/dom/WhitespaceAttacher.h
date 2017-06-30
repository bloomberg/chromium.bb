// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WhitespaceAttacher_h
#define WhitespaceAttacher_h

#include "core/CoreExport.h"
#include "platform/heap/Member.h"

namespace blink {

class Element;
class LayoutObject;
class Text;

// The WhitespaceAttacher is used during the layout tree rebuild to lazily re-
// attach whitespace LayoutObjects when necessary. For more details about white-
// space LayoutObjects, see the WhitespaceLayoutObjects.md file in this
// directory.
// TODO(rune@opera.com): Update WhitespaceLayoutObjects.md documentation.
//
// As RebuildLayoutTree walks from last to first child, we track the last text
// node, or the last skipped display:contents element we have seen. These are
// reset to null as soon as we encounter an in-flow element.
//
// If the tracked text node needed a (re-)attach, we call
// ReattachWhitespaceSiblings once we visit or re-attach the first preceding
// in-flow.
//
// If we re-attach a preceding in-flow, we also call ReattachWhitespaceSiblings
// since the need for a succeeding whitespace LayoutObject may change.

class CORE_EXPORT WhitespaceAttacher {
  STACK_ALLOCATED();

 public:
  WhitespaceAttacher() {}
  ~WhitespaceAttacher();

  void DidVisitText(Text*);
  void DidReattachText(Text*);
  void DidVisitElement(Element*);
  void DidReattachElement(Element*, LayoutObject*);
  bool LastTextNodeNeedsReattach() const {
    return last_text_node_needs_reattach_;
  }

 private:
  void DidReattach(Node*, LayoutObject*);
  void ReattachWhitespaceSiblings(LayoutObject* previous_in_flow);
  void ForceLastTextNodeNeedsReattach();
  void UpdateLastTextNodeFromDisplayContents();

  void SetLastTextNode(Text* text) {
    last_display_contents_ = nullptr;
    last_text_node_ = text;
    if (!text)
      last_text_node_needs_reattach_ = false;
  }

  // If we encounter a display:contents, without traversing its flat tree
  // children during layout tree rebuild, we store that element and start
  // traversing it for text nodes as needed if we re-attach a preceding node
  // without encountering a text node or an in-flow element first.
  //
  // If there is already a text node which needs re-attachment, we traverse into
  // the display:contents element instead as we need to find the last in-flow
  // descendant of that subtree which is used to check if the re-attached text
  // node needs a LayoutText or not.
  //
  // Invariants:
  // DCHECK(!last_display_contents_ || !last_text_node_needs_reattach_)
  // DCHECK(last_text_node_ || !last_text_node_needs_reattach_)
  Member<Element> last_display_contents_ = nullptr;

  // The last text node we've visited during rebuild for this attacher.
  Member<Text> last_text_node_ = nullptr;

  // Set to true if we need to re-attach last_text_node_ when:
  // 1. We visiting a previous in-flow sibling, or
  // 2. We get to the start of the sibling list during the rebuild.
  bool last_text_node_needs_reattach_ = false;
};

}  // namespace blink

#endif  // WhitespaceAttacher_h
