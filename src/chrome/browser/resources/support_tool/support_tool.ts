// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './data_collectors.js';
import './issue_details.js';
import './spinner_page.js';
import './pii_selection.js';
import './data_export_done.js';
import './support_tool_shared_css.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, PIIDataItem} from './browser_proxy.js';
import {DataCollectorsElement} from './data_collectors.js';
import {DataExportDoneElement} from './data_export_done.js';
import {IssueDetailsElement} from './issue_details.js';
import {PIISelectionElement} from './pii_selection.js';
import {SpinnerPageElement} from './spinner_page.js';
import {getTemplate} from './support_tool.html.js';

export enum SupportToolPageIndex {
  ISSUE_DETAILS,
  DATA_COLLECTOR_SELECTION,
  SPINNER,
  PII_SELECTION,
  EXPORT_SPINNER,
  DATA_EXPORT_DONE,
}

export type DataExportResult = {
  success: boolean,
  path: string,
  error: string,
};

export interface SupportToolElement {
  $: {
    issueDetails: IssueDetailsElement,
    dataCollectors: DataCollectorsElement,
    spinnerPage: SpinnerPageElement,
    piiSelection: PIISelectionElement,
    dataExportDone: DataExportDoneElement,
    errorMessageToast: CrToastElement,
  };
}

const SupportToolElementBase = WebUIListenerMixin(PolymerElement);

export class SupportToolElement extends SupportToolElementBase {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: {
        type: SupportToolPageIndex,
        value: SupportToolPageIndex.ISSUE_DETAILS,
      },
      supportToolPageIndex_: {
        readonly: true,
        type: Object,
        value: SupportToolPageIndex,
      },
      // The error message shown in errorMessageToast element when it's shown.
      errorMessage_: {
        type: String,
        value: '',
      }
    };
  }

  private errorMessage_: string;
  private selectedPage_: SupportToolPageIndex;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'data-collection-completed',
        this.onDataCollectionCompleted_.bind(this));
    this.addWebUIListener(
        'data-collection-cancelled',
        this.onDataCollectionCancelled_.bind(this));
    this.addWebUIListener(
        'support-data-export-started', this.onDataExportStarted_.bind(this));
    this.addWebUIListener(
        'data-export-completed', this.onDataExportCompleted_.bind(this));
  }

  private onDataExportStarted_() {
    this.selectedPage_ = SupportToolPageIndex.EXPORT_SPINNER;
  }

  private onDataCollectionCompleted_(piiItems: PIIDataItem[]) {
    this.$.piiSelection.updateDetectedPIIItems(piiItems);
    this.selectedPage_ = SupportToolPageIndex.PII_SELECTION;
  }

  private onDataCollectionCancelled_() {
    // Change the selected page into issue details page so they user can restart
    // data collection if they want.
    this.selectedPage_ = SupportToolPageIndex.ISSUE_DETAILS;
  }

  private displayError_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  private onDataExportCompleted_(result: DataExportResult) {
    if (result.success) {
      // Show the exported data path to user in data export page if the data
      // export is successful.
      this.$.dataExportDone.setPath(result.path);
      this.selectedPage_ = SupportToolPageIndex.DATA_EXPORT_DONE;
    } else {
      // Show a toast with error message and turn back to the PII selection page
      // so the user can retry exporting their data.
      this.selectedPage_ = SupportToolPageIndex.PII_SELECTION;
      this.displayError_(result.error);
    }
  }

  private onErrorMessageToastCloseClicked_() {
    this.$.errorMessageToast.hide();
  }

  private onContinueClick_() {
    // If we are currently on data collectors selection page, send signal to
    // start data collection.
    if (this.selectedPage_ === SupportToolPageIndex.DATA_COLLECTOR_SELECTION) {
      this.browserProxy_.startDataCollection(
          this.$.issueDetails.getIssueDetails(),
          this.$.dataCollectors.getDataCollectors());
    }
    this.selectedPage_ = this.selectedPage_ + 1;
  }

  private onBackClick_() {
    this.selectedPage_ = this.selectedPage_ - 1;
  }

  private shouldHideBackButton_(): boolean {
    // Back button will only be shown on data collectors selection page.
    return this.selectedPage_ !== SupportToolPageIndex.DATA_COLLECTOR_SELECTION;
  }

  private shouldHideContinueButtonContainer_(): boolean {
    // Continue button container will only be shown in issue details page and
    // data collectors selection page.
    return this.selectedPage_ >= SupportToolPageIndex.SPINNER;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'support-tool': SupportToolElement;
  }
}

customElements.define(SupportToolElement.is, SupportToolElement);