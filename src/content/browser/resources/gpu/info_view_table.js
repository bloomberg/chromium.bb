// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_view_table_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

export class InfoViewTableElement extends CustomElement {
  static get template() {
    return getTrustedHTML`{__html_template__}`;
  }

  setData(dataArray) {
    dataArray.forEach(data => {
      const row = document.createElement('info-view-table-row');
      row.setData(data);
      this.shadowRoot.querySelector('#info-view-table').appendChild(row);
    });
  }
}

customElements.define('info-view-table', InfoViewTableElement);
