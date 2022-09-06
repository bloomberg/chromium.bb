// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Actions} from '../personalization_actions.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {ThemeActionName} from './theme_actions.js';
import {ThemeState} from './theme_state.js';

export function darkModeEnabledReducer(
    state: ThemeState['darkModeEnabled'], action: Actions,
    _: PersonalizationState): ThemeState['darkModeEnabled'] {
  switch (action.name) {
    case ThemeActionName.SET_DARK_MODE_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

export function colorModeAutoScheduleEnabledReducer(
    state: ThemeState['colorModeAutoScheduleEnabled'], action: Actions,
    _: PersonalizationState): ThemeState['colorModeAutoScheduleEnabled'] {
  switch (action.name) {
    case ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

export const themeReducers:
    {[K in keyof ThemeState]: ReducerFunction<ThemeState[K]>} = {
      colorModeAutoScheduleEnabled: colorModeAutoScheduleEnabledReducer,
      darkModeEnabled: darkModeEnabledReducer,
    };
