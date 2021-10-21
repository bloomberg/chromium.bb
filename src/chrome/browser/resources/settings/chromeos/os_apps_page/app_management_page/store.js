// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {Store} from 'chrome://resources/js/cr/ui/store.m.js';

import {reduceAction} from './reducers.js';
import {createEmptyState} from './util.js';

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

export class AppManagementStore extends Store {
  constructor() {
    super(createEmptyState(), reduceAction);
  }
}

addSingletonGetter(AppManagementStore);
