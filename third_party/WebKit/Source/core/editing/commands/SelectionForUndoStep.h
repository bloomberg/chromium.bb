// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionForUndoStep_h
#define SelectionForUndoStep_h

#include "base/macros.h"
#include "core/editing/Forward.h"
#include "core/editing/Position.h"
#include "core/editing/TextAffinity.h"
#include "platform/heap/Handle.h"

namespace blink {

// This class represents selection in |UndoStep|.
class SelectionForUndoStep final {
  DISALLOW_NEW();

 public:
  class Builder;

  // Returns newly constructed |SelectionForUndoStep| from |SelectionInDOMTree|
  // with computing direction of selection by base <= extent. Thus, computation
  // time depends O(depth of tree).
  static SelectionForUndoStep From(const SelectionInDOMTree&);

  SelectionForUndoStep(const SelectionForUndoStep&);
  SelectionForUndoStep();

  SelectionForUndoStep& operator=(const SelectionForUndoStep&);

  bool operator==(const SelectionForUndoStep&) const;
  bool operator!=(const SelectionForUndoStep&) const;

  TextAffinity Affinity() const { return affinity_; }
  Position Base() const { return base_; }
  Position Extent() const { return extent_; }
  bool IsBaseFirst() const { return is_base_first_; }
  bool IsDirectional() const { return is_directional_; }

  SelectionInDOMTree AsSelection() const;

  // Selection type predicates
  bool IsCaret() const;
  bool IsNone() const;
  bool IsRange() const;

  // Returns |base_| if |base_ <= extent| at construction time, otherwise
  // |extent_|.
  Position Start() const;
  // Returns |extent_| if |base_ <= extent| at construction time, otherwise
  // |base_|.
  Position End() const;

  bool IsValidFor(const Document&) const;

  void Trace(blink::Visitor*);

 private:
  // |base_| and |extent_| can be disconnected from document.
  Position base_;
  Position extent_;
  TextAffinity affinity_ = TextAffinity::kDownstream;
  // Note: We should compute |is_base_first_| as construction otherwise we
  // fail "backward and forward delete" case in "undo-delete-boundary.html".
  bool is_base_first_ = true;
  bool is_directional_ = false;
};

// Builds |SelectionForUndoStep| object with disconnected position. You should
// use |SelectionForUndoStep::From()| if positions are connected.
class SelectionForUndoStep::Builder final {
  DISALLOW_NEW();

 public:
  Builder();

  const SelectionForUndoStep& Build() const { return selection_; }

  // |base| and |extent| can be disconnected.
  Builder& SetBaseAndExtentAsBackwardSelection(const Position& base,
                                               const Position& extent);

  // |base| and |extent| can be disconnected.
  Builder& SetBaseAndExtentAsForwardSelection(const Position& base,
                                              const Position& extent);

  void Trace(blink::Visitor*);

 private:
  SelectionForUndoStep selection_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

VisibleSelection CreateVisibleSelection(const SelectionForUndoStep&);

}  // namespace blink

#endif  // SelectionForUndoStep_h
