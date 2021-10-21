// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../controls/extension_controlled_indicator.js';
import './search_engine_entry_css.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl} from './search_engines_browser_proxy.js';

const SettingsSearchEngineEntryElementBase =
    mixinBehaviors([FocusRowBehavior], PolymerElement) as
    {new (): PolymerElement & FocusRowBehavior};

class SettingsSearchEngineEntryElement extends
    SettingsSearchEngineEntryElementBase {
  static get is() {
    return 'settings-search-engine-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      engine: Object,

      showShortcut: {type: Boolean, value: false, reflectToAttribute: true},

      showQueryUrl: {type: Boolean, value: false, reflectToAttribute: true},

      isActiveSearchEnginesFlagEnabled: Boolean,

      isDefault: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsDefault_(engine)'
      },

      showMenuButton: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeShowMenuButton_(engine)'
      },

    };
  }

  engine: SearchEngine;
  showShortcut: boolean;
  showQueryUrl: boolean;
  isActiveSearchEnginesFlagEnabled: boolean;
  isDefault: boolean;
  showMenuButton: boolean;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  private closePopupMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private computeIsDefault_(): boolean {
    return this.engine.default;
  }

  private computeShowMenuButton_(): boolean {
    return !this.isActiveSearchEnginesFlagEnabled || !this.engine.default;
  }

  private onDeleteTap_() {
    this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
    this.closePopupMenu_();
  }

  private onDotsTap_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(
        assert(this.shadowRoot!.querySelector('cr-icon-button.icon-more-vert')!
               ),
        {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  }

  private onEditTap_(e: Event) {
    e.preventDefault();
    this.closePopupMenu_();
    this.dispatchEvent(new CustomEvent('edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine: this.engine,
        anchorElement:
            assert(this.shadowRoot!.querySelector('cr-icon-button')!),
      },
    }));
  }

  private onMakeDefaultTap_() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(this.engine.modelIndex);
  }

  private onActivateTap_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.modelIndex, /*is_active=*/ true);
  }

  private onDeactivateTap_() {
    this.closePopupMenu_();
    this.browserProxy_.setIsActiveSearchEngine(
        this.engine.modelIndex, /*is_active=*/ false);
  }
}

customElements.define(
    SettingsSearchEngineEntryElement.is, SettingsSearchEngineEntryElement);
