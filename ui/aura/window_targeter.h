// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TARGETER_H_
#define UI_AURA_WINDOW_TARGETER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/events/event_targeter.h"
#include "ui/gfx/geometry/insets.h"

namespace gfx {
class Rect;
}

namespace ui {
class KeyEvent;
class LocatedEvent;
}  // namespace ui

namespace aura {

class Window;

class AURA_EXPORT WindowTargeter : public ui::EventTargeter {
 public:
  WindowTargeter();
  ~WindowTargeter() override;

  using HitTestRects = std::vector<gfx::Rect>;

  // Returns true if |window| or one of its descendants can be a target of
  // |event|. This requires that |window| and its descendants are not
  // prohibited from accepting the event, and that the event is within an
  // actionable region of the target's bounds. Note that the location etc. of
  // |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeShouldBeExploredForEvent(Window* window,
                                               const ui::LocatedEvent& event);

  // Returns true if the |target| is accepting LocatedEvents, false otherwise.
  // |hit_test_rect_mouse| and |hit_test_rect_touch| must be not null and return
  // the bounds that can be used for hit testing. The default implementation
  // extends the |target|'s |bounds()| by insets provided with SetInsets().
  // This can be used to extend the hit-test area for touch events and make
  // targeting windows with imprecise input devices easier.
  // Returned rectangles are in |target|'s parent's coordinates.
  virtual bool GetHitTestRects(Window* target,
                               gfx::Rect* hit_test_rect_mouse,
                               gfx::Rect* hit_test_rect_touch) const;

  // Returns additional hit-test areas or nullptr when there are none. Used when
  // a window needs a complex shape hit-test area. This additional area is
  // clipped to |hit_test_rect_mouse| returned by GetHitTestRects or the window
  // bounds when GetHitTestRects is not overridden.
  // Returned rectangles are in |target|'s coordinates.
  virtual std::unique_ptr<HitTestRects> GetExtraHitTestShapeRects(
      Window* target) const;

  // If there is a target that takes priority over normal WindowTargeter (such
  // as a capture window) this returns it.
  Window* GetPriorityTargetInRootWindow(Window* root_window,
                                        const ui::LocatedEvent& event);

  Window* FindTargetInRootWindow(Window* root_window,
                                 const ui::LocatedEvent& event);

  // If |target| is not a child of |root_window|, then converts |event| to
  // be relative to |root_window| and dispatches the event to |root_window|.
  // Returns false if the |target| is a child of |root_window|.
  bool ProcessEventIfTargetsDifferentRootWindow(Window* root_window,
                                                Window* target,
                                                ui::Event* event);

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;

 protected:
  // Same as FindTargetForEvent(), but used for positional events. The location
  // etc. of |event| are in |root|'s coordinate system. When finding the target
  // for the event, the targeter can mutate the |event| (e.g. change the
  // coordinate to be in the returned target's coordinate system) so that it can
  // be dispatched to the target without any further modification.
  virtual Window* FindTargetForLocatedEvent(Window* window,
                                            ui::LocatedEvent* event);

  // Returns false if neither |window| nor any of its descendants are allowed
  // to accept |event| for reasons unrelated to the event's location or the
  // target's bounds. For example, overrides of this function may consider
  // attributes such as the visibility or enabledness of |window|. Note that
  // the location etc. of |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeCanAcceptEvent(Window* window,
                                     const ui::LocatedEvent& event) const;

  // Returns whether the location of the event is in an actionable region of the
  // target. Note that the location etc. of |event| is in the |window|'s
  // parent's coordinate system.
  // Deprecated. As an alternative, override GetHitTestRects.
  // TODO(varkha): Make this non-overridable.
  virtual bool EventLocationInsideBounds(Window* target,
                                         const ui::LocatedEvent& event) const;

  // Returns true if the hit testing (GetHitTestRects()) should use the
  // extended bounds.
  virtual bool ShouldUseExtendedBounds(const aura::Window* window) const;

  // Called after the hit-test area has been extended with SetInsets(). The
  // supplied insets are the values before the call to SetInsets().
  virtual void OnSetInsets(const gfx::Insets& last_mouse_extend,
                           const gfx::Insets& last_touch_extend);

  // Sets additional mouse and touch insets that are factored into the hit-test
  // regions returned by GetHitTestRects.
  void SetInsets(const gfx::Insets& mouse_extend,
                 const gfx::Insets& touch_extend);

  const gfx::Insets& mouse_extend() const { return mouse_extend_; }
  const gfx::Insets& touch_extend() const { return touch_extend_; }

 private:
  Window* FindTargetForKeyEvent(Window* root_window, const ui::KeyEvent& event);
  Window* FindTargetForNonKeyEvent(Window* root_window, ui::Event* event);
  Window* FindTargetForLocatedEventRecursively(Window* root_window,
                                               ui::LocatedEvent* event);

  gfx::Insets mouse_extend_;
  gfx::Insets touch_extend_;

  DISALLOW_COPY_AND_ASSIGN(WindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TARGETER_H_
