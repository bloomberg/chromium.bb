// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const htmlTemplate = html`{__html_template__}`;

/**
 * A button used inside PanelItem with varying display characteristics.
 */
export class PanelButton extends HTMLElement {
  constructor() {
    super();
    this.createElement_();
  }

  /**
   * Creates a PanelButton.
   * @private
   */
  createElement_() {
    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'data-category',
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue === newValue) {
      return;
    }
    /** @type {Element} */
    const iconButton = this.shadowRoot.querySelector('cr-icon-button');
    if (name === 'data-category') {
      switch (newValue) {
        case 'cancel':
          iconButton.setAttribute('iron-icon', 'cr:clear');
          break;
        case 'collapse':
        case 'expand':
          iconButton.setAttribute('iron-icon', 'cr:expand-less');
          break;
      }
    }
  }
}

window.customElements.define('xf-button', PanelButton);

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/xf_button.js
