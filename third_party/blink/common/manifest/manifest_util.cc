// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include "base/strings/string_util.h"

namespace blink {

std::string WebDisplayModeToString(blink::WebDisplayMode display) {
  switch (display) {
    case blink::kWebDisplayModeUndefined:
      return "";
    case blink::kWebDisplayModeBrowser:
      return "browser";
    case blink::kWebDisplayModeMinimalUi:
      return "minimal-ui";
    case blink::kWebDisplayModeStandalone:
      return "standalone";
    case blink::kWebDisplayModeFullscreen:
      return "fullscreen";
  }
  return "";
}

blink::WebDisplayMode WebDisplayModeFromString(const std::string& display) {
  if (base::LowerCaseEqualsASCII(display, "browser"))
    return blink::kWebDisplayModeBrowser;
  if (base::LowerCaseEqualsASCII(display, "minimal-ui"))
    return blink::kWebDisplayModeMinimalUi;
  if (base::LowerCaseEqualsASCII(display, "standalone"))
    return blink::kWebDisplayModeStandalone;
  if (base::LowerCaseEqualsASCII(display, "fullscreen"))
    return blink::kWebDisplayModeFullscreen;
  return blink::kWebDisplayModeUndefined;
}

std::string WebScreenOrientationLockTypeToString(
    blink::WebScreenOrientationLockType orientation) {
  switch (orientation) {
    case blink::kWebScreenOrientationLockDefault:
      return "";
    case blink::kWebScreenOrientationLockPortraitPrimary:
      return "portrait-primary";
    case blink::kWebScreenOrientationLockPortraitSecondary:
      return "portrait-secondary";
    case blink::kWebScreenOrientationLockLandscapePrimary:
      return "landscape-primary";
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      return "landscape-secondary";
    case blink::kWebScreenOrientationLockAny:
      return "any";
    case blink::kWebScreenOrientationLockLandscape:
      return "landscape";
    case blink::kWebScreenOrientationLockPortrait:
      return "portrait";
    case blink::kWebScreenOrientationLockNatural:
      return "natural";
  }
  return "";
}

blink::WebScreenOrientationLockType WebScreenOrientationLockTypeFromString(
    const std::string& orientation) {
  if (base::LowerCaseEqualsASCII(orientation, "portrait-primary"))
    return blink::kWebScreenOrientationLockPortraitPrimary;
  if (base::LowerCaseEqualsASCII(orientation, "portrait-secondary"))
    return blink::kWebScreenOrientationLockPortraitSecondary;
  if (base::LowerCaseEqualsASCII(orientation, "landscape-primary"))
    return blink::kWebScreenOrientationLockLandscapePrimary;
  if (base::LowerCaseEqualsASCII(orientation, "landscape-secondary"))
    return blink::kWebScreenOrientationLockLandscapeSecondary;
  if (base::LowerCaseEqualsASCII(orientation, "any"))
    return blink::kWebScreenOrientationLockAny;
  if (base::LowerCaseEqualsASCII(orientation, "landscape"))
    return blink::kWebScreenOrientationLockLandscape;
  if (base::LowerCaseEqualsASCII(orientation, "portrait"))
    return blink::kWebScreenOrientationLockPortrait;
  if (base::LowerCaseEqualsASCII(orientation, "natural"))
    return blink::kWebScreenOrientationLockNatural;
  return blink::kWebScreenOrientationLockDefault;
}

}  // namespace blink
