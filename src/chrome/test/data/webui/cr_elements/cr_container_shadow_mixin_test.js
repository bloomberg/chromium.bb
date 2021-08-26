// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrContainerShadowMixin, CrContainerShadowMixinInterface} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('CrContainerShadowBehavior', function() {
  /**
   * @constructor
   * @extends {PolymerElement}
   * @implements {CrContainerShadowMixinInterface}
   */
  const TestElementBase = CrContainerShadowMixin(PolymerElement);

  /** @polymer */
  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
         <style>
           #container {
             height: 50px;
           }
         </style>
         <div id="before"></div>
         <div id="container" show-bottom-shadow$="[[showBottomShadow]]"></div>
         <div id="after"></div>
       `;
    }

    static get properties() {
      return {
        showBottomShadow: Boolean,
      };
    }
  }

  customElements.define(TestElement.is, TestElement);

  setup(function() {
    document.body.innerHTML = '';
  });

  test('no bottom shadow', function() {
    const element =
        /** @type {!TestElement} */ (document.createElement('test-element'));
    document.body.appendChild(element);

    // Should not have a bottom shadow div.
    assertFalse(
        !!element.shadowRoot.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot.querySelector('#cr-container-shadow-top'));

    element.showBottomShadow = true;

    // Still no bottom shadow since this is only checked in attached();
    assertFalse(
        !!element.shadowRoot.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot.querySelector('#cr-container-shadow-top'));
  });

  test('show bottom shadow', function() {
    const element = /** @type {TestElementElement} */ (
        document.createElement('test-element'));
    element.showBottomShadow = true;
    document.body.appendChild(element);

    // Has both shadows.
    assertTrue(
        !!element.shadowRoot.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot.querySelector('#cr-container-shadow-top'));
  });
});
