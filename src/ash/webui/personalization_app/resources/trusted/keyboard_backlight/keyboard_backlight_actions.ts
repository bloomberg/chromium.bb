// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {BacklightColor} from '../personalization_app.mojom-webui.js';


/**
 * @fileoverview Defines the actions to update keyboard backlight settings.
 */

export enum KeyboardBacklightActionName {
  SET_BACKLIGHT_COLOR = 'set_backlight_color',
  SET_WALLPAPER_COLOR = 'set_wallpaper_color',
}

export type KeyboardBacklightActions =
    SetBacklightColorAction|SetWallpaperColorAction;

export type SetBacklightColorAction = Action&{
  name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
  backlightColor: BacklightColor,
};

export type SetWallpaperColorAction = Action&{
  name: KeyboardBacklightActionName.SET_WALLPAPER_COLOR,
  wallpaperColor: SkColor,
};

/**
 * Sets the current value of the backlight color.
 */
export function setBacklightColorAction(backlightColor: BacklightColor):
    SetBacklightColorAction {
  return {
    name: KeyboardBacklightActionName.SET_BACKLIGHT_COLOR,
    backlightColor
  };
}

/**
 * Sets the current value of the wallpaper extracted color.
 */
export function setWallpaperColorAction(wallpaperColor: SkColor):
    SetWallpaperColorAction {
  return {
    name: KeyboardBacklightActionName.SET_WALLPAPER_COLOR,
    wallpaperColor
  };
}
