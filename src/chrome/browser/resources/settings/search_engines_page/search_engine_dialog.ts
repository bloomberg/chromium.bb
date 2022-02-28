// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-dialog' is a component for adding
 * or editing a search engine entry.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './search_engines_browser_proxy.js';

export interface SettingsSearchEngineDialogElement {
  $: {
    actionButton: CrButtonElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    keyword: CrInputElement,
    queryUrl: CrInputElement,
    searchEngine: CrInputElement,
  };
}

const SettingsSearchEngineDialogElementBase =
    WebUIListenerMixin(PolymerElement);

export class SettingsSearchEngineDialogElement extends
    SettingsSearchEngineDialogElementBase {
  static get is() {
    return 'settings-search-engine-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The search engine to be edited. If not populated a new search engine
       * should be added.
       */
      model: Object,

      searchEngine_: String,
      keyword_: String,
      queryUrl_: String,
      dialogTitle_: String,
      actionButtonText_: String,

      isActiveSearchEnginesFlagEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isActiveSearchEnginesFlagEnabled'),
      },
    };
  }

  model: SearchEngine|null;
  private searchEngine_: string;
  private keyword_: string;
  private queryUrl_: string;
  private dialogTitle_: string;
  private actionButtonText_: string;
  private browserProxy_: SearchEnginesBrowserProxy =
      SearchEnginesBrowserProxyImpl.getInstance();
  DEFAULT_MODEL_INDEX: number;
  private isActiveSearchEnginesFlagEnabled_: boolean;

  constructor() {
    super();

    /**
     * The |modelIndex| to use when a new search engine is added. Must match
     * with kNewSearchEngineIndex constant specified at
     * chrome/browser/ui/webui/settings/search_engines_handler.cc
     */
    this.DEFAULT_MODEL_INDEX = -1;
  }

  ready() {
    super.ready();

    if (this.model) {
      this.dialogTitle_ =
          loadTimeData.getString('searchEnginesEditSearchEngine');
      this.actionButtonText_ = loadTimeData.getString('save');

      // If editing an existing search engine, pre-populate the input fields.
      this.searchEngine_ = this.model.name;
      this.keyword_ = this.model.keyword;
      this.queryUrl_ = this.model.url;
    } else {
      this.dialogTitle_ =
          loadTimeData.getString('searchEnginesAddSearchEngine');
      this.actionButtonText_ = loadTimeData.getString('add');
    }

    this.addEventListener('cancel', () => {
      this.browserProxy_.searchEngineEditCancelled();
    });

    this.addWebUIListener(
        'search-engines-changed', this.enginesChanged_.bind(this));
  }

  connectedCallback() {
    super.connectedCallback();

    microTask.run(() => this.updateActionButtonState_());
    this.browserProxy_.searchEngineEditStarted(
        this.model ? this.model.modelIndex : this.DEFAULT_MODEL_INDEX);
    this.$.dialog.showModal();
  }

  private enginesChanged_(searchEnginesInfo: SearchEnginesInfo) {
    if (this.model) {
      const engineWasRemoved =
          ['defaults', 'actives', 'others', 'extensions'].every(
              engineType => searchEnginesInfo[engineType].every(
                  e => e.id !== this.model!.id));
      if (engineWasRemoved) {
        this.cancel_();
        return;
      }
    }

    [this.$.searchEngine, this.$.keyword, this.$.queryUrl].forEach(
        element => this.validateElement_(element));
  }

  private cancel_() {
    this.$.dialog.cancel();
  }

  private onActionButtonTap_() {
    this.browserProxy_.searchEngineEditCompleted(
        this.searchEngine_, this.keyword_, this.queryUrl_);
    this.$.dialog.close();
  }

  private validateElement_(inputElement: CrInputElement) {
    // If element is empty, disable the action button, but don't show the red
    // invalid message.
    if (inputElement.value === '') {
      inputElement.invalid = false;
      this.updateActionButtonState_();
      return;
    }

    this.browserProxy_
        .validateSearchEngineInput(inputElement.id, inputElement.value)
        .then(isValid => {
          inputElement.invalid = !isValid;
          this.updateActionButtonState_();
        });
  }

  private validate_(event: Event) {
    const inputElement = event.target as CrInputElement;
    this.validateElement_(inputElement);
  }

  private updateActionButtonState_() {
    const allValid = [
      this.$.searchEngine, this.$.keyword, this.$.queryUrl
    ].every(function(inputElement) {
      return !inputElement.invalid && inputElement.value.length > 0;
    });
    this.$.actionButton.disabled = !allValid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-engine-dialog': SettingsSearchEngineDialogElement;
  }
}

customElements.define(
    SettingsSearchEngineDialogElement.is, SettingsSearchEngineDialogElement);
