// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor} from '../personalization_app.mojom-webui.js';

/**
 * Stores keyboard backlight related states.
 */
export interface KeyboardBacklightState {
  backlightColor: BacklightColor|null;
}

export function emptyState(): KeyboardBacklightState {
  return {
    backlightColor: null,
  };
}
