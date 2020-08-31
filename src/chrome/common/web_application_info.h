// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEB_APPLICATION_INFO_H_
#define CHROME_COMMON_WEB_APPLICATION_INFO_H_

#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using SquareSizePx = int;

struct WebApplicationIconInfo {
  WebApplicationIconInfo();
  WebApplicationIconInfo(const WebApplicationIconInfo&);
  WebApplicationIconInfo(WebApplicationIconInfo&&);
  ~WebApplicationIconInfo();
  WebApplicationIconInfo& operator=(const WebApplicationIconInfo&);
  WebApplicationIconInfo& operator=(WebApplicationIconInfo&&);

  GURL url;
  SquareSizePx square_size_px;
};

// Structure used when creating app icon shortcuts menu and for downloading
// associated shortcut icons when supported by OS platform (eg. Windows).
struct WebApplicationShortcutInfo {
  WebApplicationShortcutInfo();
  WebApplicationShortcutInfo(const WebApplicationShortcutInfo&);
  WebApplicationShortcutInfo(WebApplicationShortcutInfo&&) noexcept;
  ~WebApplicationShortcutInfo();
  WebApplicationShortcutInfo& operator=(const WebApplicationShortcutInfo&);
  WebApplicationShortcutInfo& operator=(WebApplicationShortcutInfo&&) noexcept;

  // Title of shortcut item in App Icon Shortcut Menu.
  base::string16 name;

  // URL launched when shortcut item is selected.
  GURL url;

  // List of shortcut icon URLs with associated square size.
  std::vector<WebApplicationIconInfo> shortcut_icon_infos;

  // Shortcut icon bitmaps keyed by their square size.
  std::map<SquareSizePx, SkBitmap> shortcut_icon_bitmaps;
};

// Structure used when installing a web page as an app.
struct WebApplicationInfo {
  enum MobileCapable {
    MOBILE_CAPABLE_UNSPECIFIED,
    MOBILE_CAPABLE,
    MOBILE_CAPABLE_APPLE
  };

  WebApplicationInfo();
  WebApplicationInfo(const WebApplicationInfo& other);
  ~WebApplicationInfo();

  // Title of the application.
  base::string16 title;

  // Description of the application.
  base::string16 description;

  // The launch URL for the app.
  GURL app_url;

  // Scope for the app. Dictates what URLs will be opened in the app.
  GURL scope;

  // List of icon URLs with associated square size.
  std::vector<WebApplicationIconInfo> icon_infos;

  // Icon bitmaps keyed by their square size.
  std::map<SquareSizePx, SkBitmap> icon_bitmaps;

  // Whether the page is marked as mobile-capable, including apple specific meta
  // tag.
  MobileCapable mobile_capable = MOBILE_CAPABLE_UNSPECIFIED;

  // The color to use if an icon needs to be generated for the web app.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;

  // The color to use for the web app frame.
  base::Optional<SkColor> theme_color;

  // App preference regarding whether the app should be opened in a tab,
  // in a window (with or without minimal-ui buttons), or full screen. Defaults
  // to browser display mode as specified in
  // https://w3c.github.io/manifest/#display-modes
  blink::mojom::DisplayMode display_mode = blink::mojom::DisplayMode::kBrowser;

  // User preference as to whether the app should be opened in a window.
  // If false, the app will be opened in a tab.
  // If true, the app will be opened in a window, with minimal-ui buttons
  // if display_mode is kBrowser or kMinimalUi.
  bool open_as_window = false;

  // Whether standalone app windows should have a tab strip. Currently a user
  // preference for the sake of experimental exploration.
  bool enable_experimental_tabbed_window = false;

  // The extensions and mime types the app can handle.
  std::vector<blink::Manifest::FileHandler> file_handlers;

  // Additional search terms that users can use to find the app.
  std::vector<std::string> additional_search_terms;

  // Set of shortcut infos populated using shortcuts specified in the manifest.
  std::vector<WebApplicationShortcutInfo> shortcut_infos;
};

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationIconInfo& icon_info);

bool operator==(const WebApplicationIconInfo& icon_info1,
                const WebApplicationIconInfo& icon_info2);

#endif  // CHROME_COMMON_WEB_APPLICATION_INFO_H_
