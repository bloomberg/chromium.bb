// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-button/paper-button.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class HelloPolymer3Element extends PolymerElement {
  static get template() {
    return html`
      <div>Hello Polymer3 [[time]]</div>
      <paper-button on-click="onClick_">Update time</paper-button>
    `;
  }

  static get properties() {
    return {
      time: {
        type: Number,
        value: Date.now(),
      },
    };
  }

  onClick_() {
    this.time = Date.now();
  }
}  // class HelloPolymer3

customElements.define('hello-polymer3', HelloPolymer3Element);
