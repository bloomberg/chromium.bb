// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'data-point' shows a single piece of information related to a component. It
 *  consists of a header and value.
 */
Polymer({
  is: 'data-point',

  _template: html`{__html_template__}`,

  properties: {
    header: {
      type: String,
    },

    value: {
      type: String,
      value: '',
    },
  },
});
