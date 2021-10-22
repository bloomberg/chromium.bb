// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TroubleshootingInfo} from './diagnostics_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NetworkTroubleshootingElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NetworkTroubleshootingElement extends
    NetworkTroubleshootingElementBase {
  static get is() {
    return 'network-troubleshooting';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {TroubleshootingInfo} */
      troubleshootingInfo: {
        type: Object,
      },
    }
  }

  /** @protected */
  onLinkTextClicked_() {
    window.open(this.troubleshootingInfo.url);
  }
}

customElements.define(
    NetworkTroubleshootingElement.is, NetworkTroubleshootingElement);
