// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_DIMMER_H_
#define ASH_WM_WINDOW_DIMMER_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_observer.h"

namespace ash {

// WindowDimmer creates a window whose opacity is optionally animated by way of
// SetDimOpacity() and whose size matches that of its parent. WindowDimmer is
// intended to be used in cases where a certain set of windows need to appear
// partially obscured. This is achieved by creating WindowDimmer, setting the
// opacity, and then stacking window() above the windows that are to appear
// obscured.
//
// WindowDimmer owns the window it creates, but supports having that window
// deleted out from under it (this generally happens if the parent of the
// window is deleted). If WindowDimmer is deleted and the window it created is
// still valid, then WindowDimmer deletes the window.
class ASH_EXPORT WindowDimmer : public aura::WindowObserver {
 public:
  // Defines an interface for an optional delegate to the WindowDimmer, which
  // will be notified with certain events happening to the window being dimmed.
  class Delegate {
   public:
    // Called when the window being dimmed |dimmed_window| is about to be
    // destroyed.
    // This can be used by the owner of the WindowDimmer to know when it's no
    // longer needed and can be destroyed since the window being dimmed itself
    // is destroying.
    virtual void OnDimmedWindowDestroying(aura::Window* dimmed_window) = 0;

    // Called when the window being dimmed |dimmed_window| changes its parent.
    virtual void OnDimmedWindowParentChanged(aura::Window* dimmed_window) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Creates a new WindowDimmer. The window() created by WindowDimmer is added
  // to |parent| and stacked above all other child windows. If |animate| is set
  // to false, the dimming |window_| created by |this| will not animate on its
  // visibility changing, otherwise it'll have a fade animation of a 200-ms
  // duration. |delegate| can be optionally specified to observe some events
  // happening to the window being dimmed (|parent|).
  explicit WindowDimmer(aura::Window* parent,
                        bool animate = true,
                        Delegate* delegate = nullptr);
  ~WindowDimmer() override;

  aura::Window* parent() { return parent_; }
  aura::Window* window() { return window_; }

  // Set the opacity value of the default dimming color which is Black. If it's
  // desired to specify a certain color with its alpha value, then use the below
  // SetDimColor().
  void SetDimOpacity(float target_opacity);

  // Sets the color of the dimming |window_|'s layer. This color must not be
  // opaque.
  void SetDimColor(SkColor dimming_color);

  // NOTE: WindowDimmer is an observer for both |parent_| and |window_|.
  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

 private:
  aura::Window* parent_;
  // See class description for details on ownership.
  aura::Window* window_;

  Delegate* delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WindowDimmer);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_DIMMER_H_
