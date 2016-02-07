// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SCREEN_MUS_H_
#define UI_VIEWS_MUS_SCREEN_MUS_H_

#include <vector>

#include "base/observer_list.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/mus/mus_export.h"

namespace mojo {
class Shell;
}

namespace views {

class ScreenMusDelegate;

// Screen implementation backed by mus::mojom::DisplayManager.
class VIEWS_MUS_EXPORT ScreenMus
    : public gfx::Screen,
      public NON_EXPORTED_BASE(mus::mojom::DisplayManagerObserver) {
 public:
  explicit ScreenMus(ScreenMusDelegate* delegate);
  ~ScreenMus() override;

  void Init(mojo::Shell* shell);

 private:
  int FindDisplayIndexById(int64_t id) const;

  // Invoked when a display changed in some weay, including being added.
  // If |is_primary| is true, |changed_display| is the primary display.
  void ProcessDisplayChanged(const gfx::Display& changed_display,
                             bool is_primary);

  // gfx::Screen:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::Display GetPrimaryDisplay() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

  // mus::mojom::DisplayManager:
  void OnDisplays(mojo::Array<mus::mojom::DisplayPtr> displays) override;
  void OnDisplaysChanged(mojo::Array<mus::mojom::DisplayPtr> display) override;
  void OnDisplayRemoved(int64_t id) override;

  ScreenMusDelegate* delegate_;
  mus::mojom::DisplayManagerPtr display_manager_;
  std::vector<gfx::Display> displays_;
  int primary_display_index_;
  mojo::Binding<mus::mojom::DisplayManagerObserver>
      display_manager_observer_binding_;
  base::ObserverList<gfx::DisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SCREEN_MUS_H_
