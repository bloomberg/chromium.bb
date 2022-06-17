// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

/**
 * @typedef {{
 *    level: string,
 *    scope: string,
 *    source: string,
 *    value: any,
 * }}
 */
export let Conflict;

export class PolicyConflictElement extends CustomElement {
  static get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  connectedCallback() {
    this.setAttribute('role', 'rowgroup');
  }

  /**
   * @param {Conflict} conflict
   * @param {string} rowLabel
   */
  initialize(conflict, rowLabel) {
    this.shadowRoot.querySelector('.scope').textContent =
        loadTimeData.getString(
            conflict.scope === 'user' ? 'scopeUser' : 'scopeDevice');
    this.shadowRoot.querySelector('.level').textContent =
        loadTimeData.getString(
            conflict.level === 'recommended' ? 'levelRecommended' :
                                               'levelMandatory');
    this.shadowRoot.querySelector('.source').textContent =
        loadTimeData.getString(conflict.source);
    this.shadowRoot.querySelector('.value').textContent = conflict.value;
    this.shadowRoot.querySelector('.name').textContent =
        loadTimeData.getString(rowLabel);
  }
}

customElements.define('policy-conflict', PolicyConflictElement);
