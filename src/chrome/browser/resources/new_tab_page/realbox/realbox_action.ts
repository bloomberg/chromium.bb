// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Action} from '../realbox.mojom-webui.js';
import {decodeString16} from '../utils.js';

import {getTemplate} from './realbox_action.html.js';

// Displays an action associated with AutocompleteMatch (i.e. Clear
// Browsing History, etc.)
class RealboxActionElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-action';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================
      action: {
        type: Object,
      },

      /**
       * Index of the match in the autocomplete result. Used to inform embedder
       * of events such as click, keyboard events etc.
       */
      matchIndex: {
        type: Number,
        value: -1,
      },

      //========================================================================
      // Private properties
      //========================================================================
      /** Element's 'aria-label' attribute. */
      ariaLabel: {
        type: String,
        computed: `computeAriaLabel_(action)`,
        reflectToAttribute: true,
      },

      /** Rendered hint from action. */
      hintHtml_: {
        type: String,
        computed: `computeHintHtml_(action)`,
      },

      /** Rendered tooltip from action. */
      tooltip_: {
        type: String,
        computed: `computeTooltip_(action)`,
      },
    };
  }

  action: Action;
  matchIndex: number;
  override ariaLabel: string;
  private hintHtml_: string;
  private tooltip_: string;

  //============================================================================
  // Helpers
  //============================================================================

  private computeAriaLabel_(): string {
    if (this.action.a11yLabel) {
      return decodeString16(this.action.a11yLabel);
    }
    return '';
  }

  private computeHintHtml_(): string {
    if (this.action.hint) {
      return decodeString16(this.action.hint);
    }
    return '';
  }

  private computeTooltip_(): string {
    if (this.action.suggestionContents) {
      return decodeString16(this.action.suggestionContents);
    }
    return '';
  }
}

customElements.define(RealboxActionElement.is, RealboxActionElement);
