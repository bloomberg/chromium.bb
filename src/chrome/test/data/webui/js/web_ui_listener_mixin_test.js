// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from '../chai_assert.js';

const TestElementBase = WebUIListenerMixin(PolymerElement);
class TestElement extends TestElementBase {}
customElements.define('test-element', TestElement);

suite('WebUIListenerMixinTest', function() {
  let testElement;

  setup(function() {
    document.body.innerHTML = '';
    testElement = document.createElement('test-element');
    document.body.appendChild(testElement);
  });

  test('addRemoveListener', function() {
    const eventName = 'dummyEvent';
    let counter = 0;

    testElement.addWebUIListener(eventName, () => counter++);
    webUIListenerCallback(eventName);
    assertEquals(1, counter);

    webUIListenerCallback(eventName);
    assertEquals(2, counter);

    testElement.remove();
    webUIListenerCallback(eventName);
    assertEquals(2, counter);
  });
});
