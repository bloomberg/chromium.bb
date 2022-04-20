// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ThemeProviderInterface} from '../personalization_app.mojom-webui.js';
import {PersonalizationStore} from '../personalization_store.js';
import {setColorModeAutoScheduleEnabledAction, setDarkModeEnabledAction} from './theme_actions.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 */

export async function initializeData(
    provider: ThemeProviderInterface,
    store: PersonalizationStore): Promise<void> {
  store.beginBatchUpdate();
  const {enabled} = await provider.isColorModeAutoScheduleEnabled();
  store.dispatch(setColorModeAutoScheduleEnabledAction(enabled));
  const {darkModeEnabled} = await provider.isDarkModeEnabled();
  store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
  store.endBatchUpdate();
}

// Disables or enables dark color mode.
export function setColorModePref(
    darkModeEnabled: boolean, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setColorModePref(darkModeEnabled);
  // Dispatch action to highlight color mode.
  store.dispatch(setDarkModeEnabledAction(darkModeEnabled));
}

// Disables or enables color mode auto schedule.
export function setColorModeAutoSchedule(
    enabled: boolean, provider: ThemeProviderInterface,
    store: PersonalizationStore) {
  provider.setColorModeAutoScheduleEnabled(enabled);
  // Dispatch action to highlight auto color mode.
  store.dispatch(setColorModeAutoScheduleEnabledAction(enabled));
}
