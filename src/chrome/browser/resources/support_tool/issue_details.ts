// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import './support_tool_shared_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, IssueDetails} from './browser_proxy.js';
import {getTemplate} from './issue_details.html.js';

export class IssueDetailsElement extends PolymerElement {
  static get is() {
    return 'issue-details';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      caseId_: {
        type: String,
        value: () => loadTimeData.getString('caseId'),
      },
      emails_: {
        type: Array,
        value: () => [],
      },
      issueDescription_: {
        type: String,
        value: '',
      },
      selectedEmail_: {
        type: String,
        value: '',
      },
    };
  }

  private caseId_: string;
  private emails_: string[];
  private issueDescription_: string;
  private selectedEmail_: string;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getEmailAddresses().then((emails: string[]) => {
      this.emails_ = emails;
    });
  }

  getIssueDetails(): IssueDetails {
    return {
      caseId: this.caseId_,
      emailAddress: this.selectedEmail_,
      issueDescription: this.issueDescription_
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'issue-details': IssueDetailsElement;
  }
}

customElements.define(IssueDetailsElement.is, IssueDetailsElement);