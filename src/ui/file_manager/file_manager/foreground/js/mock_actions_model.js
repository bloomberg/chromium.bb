// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
// #import {dispatchSimpleEvent} from 'chrome://resources/js/cr.m.js';
// clang-format on

/* #export */ class MockActionModel extends cr.EventTarget {
  /**
   * @param {string} title
   * @param {Array<!Entry>} entries
   */
  constructor(title, entries) {
    super();

    this.title = title;
    this.entries = entries;
    this.actionsModel = null;
  }

  getTitle() {
    return this.title;
  }

  onCanExecute() {}

  onExecute() {
    cr.dispatchSimpleEvent(this, 'invalidated', true);
  }
}

/* #export */ class MockActionsModel extends cr.EventTarget {
  constructor(actions) {
    super();

    this.actions_ = actions;
    Object.keys(actions).forEach(function(key) {
      actions[key].actionsModel = this;
    });
  }

  initialize() {
    return Promise.resolve();
  }

  getActions() {
    return this.actions_;
  }
}
