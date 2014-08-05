// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TARGETER_H_
#define UI_VIEWS_VIEW_TARGETER_H_

#include "ui/events/event_targeter.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class ViewTargeterDelegate;

// A ViewTargeter is installed on a View that wishes to use the custom
// hit-testing or event-targeting behaviour defined by |delegate|.
class VIEWS_EXPORT ViewTargeter : public ui::EventTargeter {
 public:
  explicit ViewTargeter(ViewTargeterDelegate* delegate);
  virtual ~ViewTargeter();

  // A call-through to DoesIntersectRect() on |delegate_|.
  bool DoesIntersectRect(const View* target, const gfx::Rect& rect) const;

  // A call-through to TargetForRect() on |delegate_|.
  View* TargetForRect(View* root, const gfx::Rect& rect) const;

 protected:
  // ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;
  virtual ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                              ui::Event* event) OVERRIDE;
  virtual bool SubtreeCanAcceptEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE;
  virtual bool EventLocationInsideBounds(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE;

 private:
  View* FindTargetForKeyEvent(View* root, const ui::KeyEvent& key);
  View* FindTargetForScrollEvent(View* root, const ui::ScrollEvent& scroll);

  // ViewTargeter does not own the |delegate_|, but |delegate_| must
  // outlive the targeter.
  ViewTargeterDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewTargeter);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TARGETER_H_
