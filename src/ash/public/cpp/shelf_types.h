// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_TYPES_H_
#define ASH_PUBLIC_CPP_SHELF_TYPES_H_

#include <cstdint>
#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum ShelfAlignment {
  SHELF_ALIGNMENT_BOTTOM,
  SHELF_ALIGNMENT_LEFT,
  SHELF_ALIGNMENT_RIGHT,
  // Top has never been supported.

  // The locked alignment is set temporarily and not saved to preferences.
  SHELF_ALIGNMENT_BOTTOM_LOCKED,
};

enum ShelfAutoHideBehavior {
  // Always auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,

  // Never auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_NEVER,

  // Always hide.
  SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
};

enum ShelfAutoHideState {
  SHELF_AUTO_HIDE_SHOWN,
  SHELF_AUTO_HIDE_HIDDEN,
};

enum ShelfVisibilityState {
  // Always visible.
  SHELF_VISIBLE,

  // A couple of pixels are reserved at the bottom for the shelf.
  SHELF_AUTO_HIDE,

  // Nothing is shown. Used for fullscreen windows.
  SHELF_HIDDEN,
};

enum ShelfBackgroundType {
  // The default transparent background.
  SHELF_BACKGROUND_DEFAULT,

  // The background when a window is overlapping.
  SHELF_BACKGROUND_OVERLAP,

  // The background when a window is maximized.
  SHELF_BACKGROUND_MAXIMIZED,

  // The background when fullscreen app list is visible.
  SHELF_BACKGROUND_APP_LIST,

  // The background when split view mode is active.
  SHELF_BACKGROUND_SPLIT_VIEW,

  // The background when OOBE is active.
  SHELF_BACKGROUND_OOBE,

  // The background when login/lock/user-add is active.
  SHELF_BACKGROUND_LOGIN,

  // The background when login/lock/user-add is active and the wallpaper is not
  // blurred.
  SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER,
};

// Source of the launch or activation request, for tracking.
enum ShelfLaunchSource {
  // The item was launched from an unknown source (ie. not the app list).
  LAUNCH_FROM_UNKNOWN,

  // The item was launched from a generic app list view.
  LAUNCH_FROM_APP_LIST,

  // The item was launched from an app list search view.
  LAUNCH_FROM_APP_LIST_SEARCH,
};

// The actions that may be performed when a shelf item is selected.
enum ShelfAction {
  // No action was taken.
  SHELF_ACTION_NONE,

  // A new window was created.
  SHELF_ACTION_NEW_WINDOW_CREATED,

  // An existing inactive window was activated.
  SHELF_ACTION_WINDOW_ACTIVATED,

  // The currently active window was minimized.
  SHELF_ACTION_WINDOW_MINIMIZED,

  // The app list launcher menu was shown.
  SHELF_ACTION_APP_LIST_SHOWN,
};

// The type of a shelf item.
enum ShelfItemType {
  // Represents a pinned shortcut to an app, the app may be running or not.
  TYPE_PINNED_APP,

  // Toggles visiblity of the app list.
  TYPE_APP_LIST,

  // The browser shortcut button, the browser may be running or not.
  TYPE_BROWSER_SHORTCUT,

  // Represents an unpinned running app window. Supports these app types:
  // - Extension "V1" (legacy packaged and hosted) apps,
  // - Extension "V2" (platform) apps,
  // - ARC (App Runtime for Chrome - Android Play Store) apps.
  TYPE_APP,

  // Represents an open dialog.
  TYPE_DIALOG,

  // Represents the back button, which is shown in tablet mode.
  TYPE_BACK_BUTTON,

  // Default value.
  TYPE_UNDEFINED,
};

// Returns true if |type| is a valid ShelfItemType.
ASH_PUBLIC_EXPORT bool IsValidShelfItemType(int64_t type);

// Represents the status of applications in the shelf.
enum ShelfItemStatus {
  // A closed shelf item, i.e. has no live instance.
  STATUS_CLOSED,
  // A shelf item that has live instance.
  STATUS_RUNNING,
  // A shelf item that needs user's attention.
  STATUS_ATTENTION,
};

// A unique shelf item id composed of an |app_id| and a |launch_id|.
// |app_id| is the non-empty application id associated with a set of windows.
// |launch_id| is passed on app launch, to support multiple shelf items per app.
// As an example, a remote desktop client may want each remote application to
// have its own icon.
struct ASH_PUBLIC_EXPORT ShelfID {
  explicit ShelfID(const std::string& app_id = std::string(),
                   const std::string& launch_id = std::string());
  ~ShelfID();

  ShelfID(const ShelfID& other);
  ShelfID(ShelfID&& other);
  ShelfID& operator=(const ShelfID& other);
  bool operator==(const ShelfID& other) const;
  bool operator!=(const ShelfID& other) const;
  bool operator<(const ShelfID& other) const;

  // Returns true if both the application id and launch id are empty.
  // This is often used to determine if the id is invalid.
  bool IsNull() const;

  // Functions to [de]serialize ids as a string for window property usage, etc.
  // Serialization appends ids with a delimeter that must not be used in ids.
  // Deserialization returns an empty/null/default id for a null string input.
  std::string Serialize() const;
  static ShelfID Deserialize(const std::string* string);

  // The application id associated with a set of windows.
  std::string app_id;
  // An id passed on app launch, to support multiple shelf items per app.
  std::string launch_id;
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& o, const ShelfID& id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_TYPES_H_
