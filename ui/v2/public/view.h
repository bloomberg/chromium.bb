// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_VIEW_H_
#define UI_V2_PUBLIC_VIEW_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/rect.h"
#include "ui/v2/public/layout.h"
#include "ui/v2/public/painter.h"
#include "ui/v2/public/v2_export.h"

namespace v2 {

class Layout;
class Painter;
class ViewObserver;

class V2_EXPORT View /* : public EventTarget */ {
 public:
  typedef std::vector<View*> Children;

  View();
  virtual ~View();

  // Configuration.

  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }

  // View takes ownership.
  void set_painter(Painter* painter) { painter_.reset(painter); }

  // View takes ownership.
  void set_layout(Layout* layout) { layout_.reset(layout); }

  // Observation.

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);

  // Disposition.

  void SetBounds(const gfx::Rect& bounds);
  gfx::Rect bounds() const { return bounds_; }

  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Tree.

  View* parent() { return parent_; }
  const View* parent() const { return parent_; }
  const Children& children() const { return children_; }

  void AddChild(View* child);
  void RemoveChild(View* child);

  bool Contains(View* child) const;

  void StackChildAtTop(View* child);
  void StackChildAtBottom(View* child);
  void StackChildAbove(View* child, View* other);
  void StackChildBelow(View* child, View* other);

 private:
  friend class Layout;
  friend class ViewObserversAccessor;

  void SetBoundsInternal(const gfx::Rect& bounds);

  // Disposition attributes.
  gfx::Rect bounds_;
  bool visible_;

  scoped_ptr<Painter> painter_;
  scoped_ptr<Layout> layout_;

  // Tree.
  bool owned_by_parent_;
  View* parent_;
  Children children_;

  ObserverList<ViewObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_VIEW_H_
