// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to ChromeOS.

#ifndef ASH_CONSTANTS_ASH_CONSTANTS_H_
#define ASH_CONSTANTS_ASH_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kDriveCacheDirname[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssCertDbPath[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const base::FilePath::CharType kNssKeyDbPath[];

// Background color used for the Chrome OS boot splash screen.
constexpr SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

// The border thickness of keyboard focus for launcher items and system tray.
constexpr int kFocusBorderThickness = 2;

// The thickness of the focus bar for launcher search.
constexpr int kFocusBarThickness = 3;

constexpr int kDefaultLargeCursorSize = 64;

constexpr SkColor kDefaultCursorColor = SK_ColorBLACK;

// If the window's maximum size (one of width/height) is bigger than this,
// the window become maximizable/snappable.
constexpr int kAllowMaximizeThreshold = 30720;

// These device types are a subset of ui::InputDeviceType. These strings are
// also used in Switch Access webui.
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessInternalDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessUsbDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kSwitchAccessBluetoothDevice[];
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kSwitchAccessUnknownDevice[];

// The default delay before Switch Access automatically moves to the next
// element on the page that is interesting, based on the Switch Access
// predicates. This value is mostly overridden by the setup guide's default
// value.
constexpr base::TimeDelta kDefaultSwitchAccessAutoScanSpeed =
    base::Milliseconds(1800);

// The default speed in dips per second that the gliding point scan cursor
// in switch access moves across the screen.
constexpr int kDefaultSwitchAccessPointScanSpeedDipsPerSecond = 50;

// The default wait time between last mouse movement and sending autoclick.
constexpr int kDefaultAutoclickDelayMs = 1000;

// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
constexpr int kDefaultAutoclickMovementThreshold = 20;

// Whether keyboard auto repeat is enabled by default.
constexpr bool kDefaultKeyAutoRepeatEnabled = true;

// Whether dark mode is enabled by default.
constexpr bool kDefaultDarkModeEnabled = false;

// Whether color mode is themed by default.
constexpr bool kDefaultColorModeThemed = true;

// The default delay before a held keypress will start to auto repeat.
constexpr base::TimeDelta kDefaultKeyAutoRepeatDelay = base::Milliseconds(500);

// The default interval between auto-repeated key events.
constexpr base::TimeDelta kDefaultKeyAutoRepeatInterval =
    base::Milliseconds(50);

}  // namespace ash

#endif  // ASH_CONSTANTS_ASH_CONSTANTS_H_
