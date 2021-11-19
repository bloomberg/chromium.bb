// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './print_preview_shared_css.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {CrSearchFieldBehavior} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field_behavior.js';
import {stripDiacritics} from 'chrome://resources/js/search_highlight_utils.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

declare global {
  interface HTMLElementEventMap {
    'search-changed': CustomEvent<string>;
  }
}

const SANITIZE_REGEX: RegExp = /[-[\]{}()*+?.,\\^$|#\s]/g;

export interface PrintPreviewSearchBoxElement {
  $: {
    searchInput: CrInputElement,
  };
}

const PrintPreviewSearchBoxElementBase =
    mixinBehaviors(
        [CrSearchFieldBehavior], WebUIListenerMixin(PolymerElement)) as {
      new ():
          PolymerElement & WebUIListenerMixinInterface & CrSearchFieldBehavior
    };

export class PrintPreviewSearchBoxElement extends
    PrintPreviewSearchBoxElementBase {
  static get is() {
    return 'print-preview-search-box';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      autofocus: Boolean,

      searchQuery: {
        type: Object,
        notify: true,
      },
    };
  }

  autofocus: boolean;
  searchQuery: RegExp|null;
  private lastQuery_: string = '';

  ready() {
    super.ready();

    this.addEventListener('search-changed', e => this.onSearchChanged_(e));
  }

  getSearchInput(): CrInputElement {
    return this.$.searchInput;
  }

  focus() {
    this.$.searchInput.focus();
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    const strippedQuery = stripDiacritics(e.detail.trim());
    const safeQuery = strippedQuery.replace(SANITIZE_REGEX, '\\$&');
    if (safeQuery === this.lastQuery_) {
      return;
    }

    this.lastQuery_ = safeQuery;
    this.searchQuery =
        safeQuery.length > 0 ? new RegExp(`(${safeQuery})`, 'ig') : null;
  }

  private onClearClick_() {
    this.setValue('');
    this.$.searchInput.focus();
  }
}

customElements.define(
    PrintPreviewSearchBoxElement.is, PrintPreviewSearchBoxElement);
