// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

class ErrorStore {
  constructor() {
    this.store_ = [];
    window.addEventListener('error', (e) => {
      this.store_.push(e);
    });
  }

  get length() {
    return this.store_.length;
  }
}

cr.addSingletonGetter(ErrorStore);
if (window.cr == undefined) {
  window.cr = {};
}
window.cr.ErrorStore = ErrorStore;
cr.ErrorStore.getInstance();
