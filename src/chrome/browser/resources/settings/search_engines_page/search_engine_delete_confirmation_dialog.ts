// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-delete-confirmation-dialog' is a
 * component for confirming that the user wants to delete a search engine.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './search_engine_delete_confirmation_dialog.html.js';
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineDeleteConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
    cancelButton: CrButtonElement,
    deleteButton: CrButtonElement,
  };
}

const SettingsSearchEngineDeleteConfirmationDialogElementBase =
    WebUIListenerMixin(PolymerElement);

export class SettingsSearchEngineDeleteConfirmationDialogElement extends
    SettingsSearchEngineDeleteConfirmationDialogElementBase {
  static get is() {
    return 'settings-search-engine-delete-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,

      isActiveSearchEnginesFlagEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isActiveSearchEnginesFlagEnabled'),
      },
    };
  }

  model: SearchEngine|null;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();

  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private cancel_() {
    this.$.dialog.cancel();
  }

  private delete_() {
    if (this.model) {
      this.browserProxy_.removeSearchEngine(this.model.modelIndex);
    }
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-delete-confirmation-dialog':
        SettingsSearchEngineDeleteConfirmationDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineDeleteConfirmationDialogElement.is,
    SettingsSearchEngineDeleteConfirmationDialogElement);
