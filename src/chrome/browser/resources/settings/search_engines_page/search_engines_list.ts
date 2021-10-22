// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
import './search_engine_entry.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine} from './search_engines_browser_proxy.js';

export class SettingsSearchEnginesListElement extends PolymerElement {
  static get is() {
    return 'settings-search-engines-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      engines: Array,

      /**
       * Whether the active search engines feature flag is enabled.
       */
      isActiveSearchEnginesFlagEnabled: Boolean,

      /**
       * The scroll target that this list should use.
       */
      scrollTarget: Object,

      showShortcut: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showQueryUrl: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      collapseList: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      nameColumnHeader: {
        type: String,
        reflectToAttribute: true,
      },

      /**
       * The number of engines visible when the list is collapsed.
       * This is currently gated behind the #omnibox-active-search-engines-flag.
       */
      visibleEnginesSize: {
        type: Number,
        value: 5,
      },

      /**
       * An array of the first 'visibleEnginesSize' engines in the `engines`
       * array.  These engines are visible even when 'collapsedEngines' is
       * collapsed. This is currently gated behind the
       * #omnibox-active-search-engines flag.
       */
      visibleEngines:
          {type: Array, computed: 'computeVisibleEngines_(engines)'},

      /**
       * An array of all remaining engines not in the `visibleEngines` array.
       * These engines' visibility can be toggled by expanding or collapsing the
       * engines list. This is currently gated behind the
       * #omnibox-active-search-engines flag.
       */
      collapsedEngines:
          {type: Array, computed: 'computeCollapsedEngines_(engines)'},

      /** Used to fix scrolling glitch when list is not top most element. */
      scrollOffset: Number,

      lastFocused_: Object,

      listBlurred_: Boolean,

      expandListText: {
        type: String,
        reflectToAttribute: true,
      },

      fixedHeight: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  engines: Array<SearchEngine>;
  visibleEngines: Array<SearchEngine>;
  collapsedEngines: Array<SearchEngine>;
  visibleEnginesSize: number;
  scrollTarget: HTMLElement|null;
  scrollOffset: number;
  fixedHeight: boolean;
  showShortcut: boolean;
  showQueryUrl: boolean;
  collapseList: boolean;
  nameColumnHeader: String;
  expandListText: String;
  private lastFocused_: HTMLElement;
  private listBlurred_: boolean;

  computeVisibleEngines_(engines: Array<SearchEngine>) {
    if (!engines || !engines.length) {
      return;
    }

    return engines.slice(0, this.visibleEnginesSize);
  }

  computeCollapsedEngines_(engines: Array<SearchEngine>) {
    if (!engines || !engines.length) {
      return;
    }

    return engines.slice(this.visibleEnginesSize);
  }
}

customElements.define(
    SettingsSearchEnginesListElement.is, SettingsSearchEnginesListElement);
