// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_VIEW_MODEL_H_
#define UI_AURA_SHELL_LAUNCHER_VIEW_MODEL_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/rect.h"

namespace views {
class View;
}

namespace aura_shell {

// ViewModel is used to track an 'interesting' set of a views. Often times
// during animations views are removed after a delay, which makes for tricky
// coordinate conversion as you have to account for the possibility of the
// indices from the model not lining up with those you expect. This class lets
// you define the 'interesting' views and operate on those views.
class AURA_SHELL_EXPORT ViewModel {
 public:
  ViewModel();
  ~ViewModel();

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void Add(views::View* view, int index);

  // Removes the view at the specified index. This does not actually remove the
  // view from the view hierarchy.
  void Remove(int index);

  // Returns the number of views.
  int view_size() const { return static_cast<int>(entries_.size()); }

  // Removes and deletes all the views.
  void Clear();

  // Returns the view at the specified index.
  views::View* view_at(int index) {
    return entries_[index].view;
  }

  void set_ideal_bounds(int index, const gfx::Rect& bounds) {
    entries_[index].ideal_bounds = bounds;
  }

  const gfx::Rect& ideal_bounds(int index) const {
    return entries_[index].ideal_bounds;
  }

  // Sets the bounds of each view to its ideal bounds.
  void SetViewBoundsToIdealBounds();

  // Returns the index of the specified view, or -1 if the view isn't in the
  // model.
  int GetIndexOfView(views::View* view);

 private:
  struct Entry {
    Entry() : view(NULL) {}

    views::View* view;
    gfx::Rect ideal_bounds;
  };
  typedef std::vector<Entry> Entries;

  Entries entries_;

  DISALLOW_COPY_AND_ASSIGN(ViewModel);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_MODEL_H_
