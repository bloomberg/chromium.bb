// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_TRACKER_MAC_H_
#define UI_BASE_INTERACTION_ELEMENT_TRACKER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

// Tracked element representing a native Mac visual element (typically a menu or
// menu item, since we use Views for everything else).
class COMPONENT_EXPORT(UI_BASE) TrackedElementMac : public TrackedElement {
 public:
  TrackedElementMac(ElementIdentifier identifier,
                    ElementContext context,
                    const gfx::Rect& screen_bounds);
  ~TrackedElementMac() override;

  gfx::Rect screen_bounds() const { return screen_bounds_; }

  DECLARE_ELEMENT_TRACKER_METADATA()

 private:
  const gfx::Rect screen_bounds_;
};

// Helper class for translating between Mac visual elements and TrackedElements.
// Largely used to track native menus and menu items, as almost all other
// surfaces are rendered using Views.
class COMPONENT_EXPORT(UI_BASE) ElementTrackerMac {
 public:
  ElementTrackerMac(const ElementTrackerMac& other) = delete;
  void operator=(const ElementTrackerMac& other) = delete;

  // Gets the global instance of the tracker for native Mac elements.
  static ElementTrackerMac* GetInstance();

  // Called before a root menu will be displayed.
  void NotifyMenuWillShow(NSMenu* menu, ElementContext context);

  // Called after a root menu fully fades out.
  void NotifyMenuDoneShowing(NSMenu* menu);

  // Called when a specific menu item becomes visible.
  void NotifyMenuItemShown(NSMenu* menu,
                           ElementIdentifier identifier,
                           const gfx::Rect& screen_bounds);

  // Called when a specific menu item is hidden.
  void NotifyMenuItemHidden(NSMenu* menu, ElementIdentifier identifier);

  // Called when the user clicks on a menu item. This callback will happen after
  // all of the Hidden() calls happen, but before NotifyMenuDoneShowing() is
  // called.
  void NotifyMenuItemActivated(NSMenu* menu, ElementIdentifier identifier);

 protected:
  ElementTrackerMac();
  virtual ~ElementTrackerMac();

  // Returns the root menu for a given `menu` (which might be the same menu if
  // it is the root). Override for tests if you want to use fake NSMenu objects.
  virtual NSMenu* GetRootMenu(NSMenu* menu) const;

  // Used in testing to determine if all data has been properly cleared out.
  bool is_tracking_any_menus() const { return !root_menu_to_context_.empty(); }

 private:
  friend class base::NoDestructor<ElementTrackerMac>;
  class ContextData;

  ElementContext GetContextForMenu(NSMenu* menu) const;

  std::map<NSMenu*, ElementContext> root_menu_to_context_;
  std::map<ElementContext, std::unique_ptr<ContextData>> context_to_data_;
};

}  // namespace ui

#endif  // UI_BASE_INTERACTION_ELEMENT_TRACKER_MAC_H_
