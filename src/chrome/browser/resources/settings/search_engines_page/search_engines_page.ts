// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/cr.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_radio_button.js';
import '../controls/settings_radio_group.js';
import './search_engine_dialog.js';
import './search_engines_list.js';
import './omnibox_extension_entry.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo, SearchEnginesInteractions} from './search_engines_browser_proxy.js';

type SearchEngineEditEvent = CustomEvent<{
  engine: SearchEngine,
  anchorElement: HTMLElement,
}>;

export interface SettingsSearchEnginesPageElement {
  $: {
    addSearchEngine: HTMLElement,
    extensions: IronListElement,
    keyboardShortcutSettingGroup: SettingsRadioGroupElement,
  };
}

const SettingsSearchEnginesPageElementBase =
    GlobalScrollTargetMixin(WebUIListenerMixin(PolymerElement)) as
    {new (): PolymerElement & WebUIListenerMixinInterface};

export class SettingsSearchEnginesPageElement extends
    SettingsSearchEnginesPageElementBase {
  static get is() {
    return 'settings-search-engines-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      defaultEngines: Array,
      activeEngines: Array,
      otherEngines: Array,
      extensions: Array,

      /**
       * Needed by GlobalScrollTargetMixin.
       */
      subpageRoute: {
        type: Object,
        value: routes.SEARCH_ENGINES,
      },

      showExtensionsList_: {
        type: Boolean,
        computed: 'computeShowExtensionsList_(extensions)',
      },

      /** Filters out all search engines that do not match. */
      filter: {
        type: String,
        value: '',
      },

      matchingDefaultEngines_: {
        type: Array,
        computed: 'computeMatchingEngines_(defaultEngines, filter)',
      },

      matchingActiveEngines_: {
        type: Array,
        computed: 'computeMatchingEngines_(activeEngines, filter)',
      },

      matchingOtherEngines_: {
        type: Array,
        computed: 'computeMatchingEngines_(otherEngines, filter)',
      },

      matchingExtensions_: {
        type: Array,
        computed: 'computeMatchingEngines_(extensions, filter)',
      },

      omniboxExtensionlastFocused_: Object,
      omniboxExtensionListBlurred_: Boolean,

      dialogModel_: {
        type: Object,
        value: null,
      },

      dialogAnchorElement_: {
        type: Object,
        value: null,
      },

      showDialog_: {
        type: Boolean,
        value: false,
      },

      showKeywordTriggerSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showKeywordTriggerSetting'),
      },

      isActiveSearchEnginesFlagEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isActiveSearchEnginesFlagEnabled'),
      },
    };
  }

  static get observers() {
    return ['extensionsChanged_(extensions, showExtensionsList_)'];
  }

  defaultEngines: Array<SearchEngine>;
  activeEngines: Array<SearchEngine>;
  otherEngines: Array<SearchEngine>;
  extensions: Array<SearchEngine>;
  private showExtensionsList_: boolean;
  filter: string;
  private matchingDefaultEngines_: Array<SearchEngine>;
  private matchingActiveEngines_: Array<SearchEngine>;
  private matchingOtherEngines_: Array<SearchEngine>;
  private matchingExtensions_: Array<SearchEngine>;
  private omniboxExtensionlastFocused_: HTMLElement;
  private omniboxExtensionListBlurred_: boolean;
  private dialogModel_: SearchEngine|null;
  private dialogAnchorElement_: HTMLElement|null;
  private showDialog_: boolean;
  private showKeywordTriggerSetting_: boolean;
  private isActiveSearchEnginesFlagEnabled_: boolean;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  ready() {
    super.ready();

    this.browserProxy_.getSearchEnginesList().then(
        this.enginesChanged_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.enginesChanged_.bind(this));

    this.addEventListener(
        'edit-search-engine',
        e => this.onEditSearchEngine_(e as SearchEngineEditEvent));
  }

  private openDialog_(
      searchEngine: SearchEngine|null, anchorElement: HTMLElement) {
    this.dialogModel_ = searchEngine;
    this.dialogAnchorElement_ = anchorElement;
    this.showDialog_ = true;
  }

  private onCloseDialog_() {
    this.showDialog_ = false;
    focusWithoutInk(this.dialogAnchorElement_ as HTMLElement);
    this.dialogModel_ = null;
    this.dialogAnchorElement_ = null;
  }

  private onEditSearchEngine_(e: SearchEngineEditEvent) {
    this.openDialog_(e.detail.engine, e.detail.anchorElement);
  }

  private extensionsChanged_() {
    if (this.showExtensionsList_ && this.$.extensions) {
      this.$.extensions.notifyResize();
    }
  }

  private enginesChanged_(searchEnginesInfo: SearchEnginesInfo) {
    this.defaultEngines = searchEnginesInfo.defaults;

    // Sort |activeEngines| and |otherEngines| in alphabetical order.
    this.activeEngines = searchEnginesInfo.actives.sort(
        (a, b) => a.name.toLocaleLowerCase().localeCompare(
            b.name.toLocaleLowerCase()));
    this.otherEngines = searchEnginesInfo.others.sort(
        (a, b) => a.name.toLocaleLowerCase().localeCompare(
            b.name.toLocaleLowerCase()));

    this.extensions = searchEnginesInfo.extensions;
  }

  private onAddSearchEngineTap_(e: Event) {
    e.preventDefault();
    this.openDialog_(null, this.$.addSearchEngine);
  }

  private computeShowExtensionsList_(): boolean {
    return this.extensions.length > 0;
  }

  /**
   * Filters the given list based on the currently existing filter string.
   */
  private computeMatchingEngines_(list: Array<SearchEngine>):
      Array<SearchEngine> {
    if (this.filter === '') {
      return list;
    }

    const filter = this.filter.toLowerCase();
    return list.filter(e => {
      return [e.displayName, e.name, e.keyword, e.url].some(
          term => term.toLowerCase().includes(filter));
    });
  }

  /**
   * @param list The original list.
   * @param filteredList The filtered list.
   * @return Whether to show the "no results" message.
   */
  private showNoResultsMessage_(
      list: Array<SearchEngine>, filteredList: Array<SearchEngine>): boolean {
    return list.length > 0 && filteredList.length === 0;
  }

  private onKeyboardShortcutSettingChange_() {
    const spaceEnabled =
        this.$.keyboardShortcutSettingGroup.selected === 'true';

    this.browserProxy_.recordSearchEnginesPageHistogram(
        spaceEnabled ?
            SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB :
            SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engines-page': SettingsSearchEnginesPageElement;
  }
}

customElements.define(
    SettingsSearchEnginesPageElement.is, SettingsSearchEnginesPageElement);
