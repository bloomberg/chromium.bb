// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const TestElementBase = I18nMixin(PolymerElement);
class TestElement extends TestElementBase {}
customElements.define('test-element', TestElement);

suite('I18nMixinTest', function() {
  const allowedByDefault = '<a href="https://google.com">Google!</a>';
  const text = 'I\'m just text, nobody should have a problem with me!';
  const nonBreakingSpace = 'A\u00a0B\u00a0C';  // \u00a0 is a unicode nbsp.

  let testElement;

  suiteSetup(function() {
    window.loadTimeData.data = {
      'allowedByDefault': allowedByDefault,
      'customAttr': '<a is="action-link">Take action!</a>',
      'optionalTag': '<img>',
      'javascriptHref': '<a href="javascript:alert(1)">teh hax</a>',
      'script': '<script>alert(/xss/)</scr' +
          'ipt>',
      'text': text,
      'nonBreakingSpace': nonBreakingSpace,
    };
  });

  setup(function() {
    document.body.innerHTML = '';
    testElement = document.createElement('test-element');
    document.body.appendChild(testElement);
  });

  test('i18n', function() {
    assertEquals(text, testElement.i18n('text'));
    assertEquals(nonBreakingSpace, testElement.i18n('nonBreakingSpace'));

    assertThrows(function() {
      testElement.i18n('customAttr');
    });
    assertThrows(function() {
      testElement.i18n('optionalTag');
    });
    assertThrows(function() {
      testElement.i18n('javascriptHref');
    });
    assertThrows(function() {
      testElement.i18n('script');
    });
  });

  test('i18n advanced', function() {
    assertEquals(
        allowedByDefault, testElement.i18nAdvanced('allowedByDefault'));
    testElement.i18nAdvanced('customAttr', {attrs: ['is']});
    testElement.i18nAdvanced('optionalTag', {tags: ['img']});
  });

  test('i18n dynamic', function() {
    assertEquals(text, testElement.i18nDynamic('en', 'text'));
  });

  test('i18n exists', function() {
    assertTrue(testElement.i18nExists('text'));
    assertFalse(testElement.i18nExists('missingText'));
  });
});
