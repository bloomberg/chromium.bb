// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_
#define UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"

namespace views {
class AXViewStore;

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes.
class VIEWS_EXPORT AXTreeSourceViews : public ui::AXTreeSource<View*>,
                                       public WidgetObserver,
                                       public WidgetRemovalsObserver {
 public:
  explicit AXTreeSourceViews(Widget* root);
  virtual ~AXTreeSourceViews();

  // AXTreeSource implementation.
  virtual View* GetRoot() const OVERRIDE;
  virtual View* GetFromId(int32 id) const OVERRIDE;
  virtual int32 GetId(View* node) const OVERRIDE;
  virtual void GetChildren(View* node,
                           std::vector<View*>* out_children) const OVERRIDE;
  virtual View* GetParent(View* node) const OVERRIDE;
  virtual bool IsValid(View* node) const OVERRIDE;
  virtual bool IsEqual(View* node1,
                       View* node2) const OVERRIDE;
  virtual View* GetNull() const OVERRIDE;
  virtual void SerializeNode(
      View* node, ui::AXNodeData* out_data) const OVERRIDE;

  // WidgetObserver overrides.
  virtual void OnWidgetDestroyed(Widget* widget) OVERRIDE;

  // WidgetRemovalsObserver overrides.
  virtual void OnWillRemoveView(Widget* widget, View* view) OVERRIDE;

 private:
  scoped_ptr<AXViewStore> view_store_;

  views::Widget* root_;
};

} // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_
