// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './autocomplete_action_predictor.js';
import './resource_prefetch_predictor.js';
import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {isWindows} from 'chrome://resources/js/cr.m.js';

if (isWindows) {
  document.documentElement.setAttribute('os', 'win');
}
