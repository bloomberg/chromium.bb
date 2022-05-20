// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EnterpriseProfileInfo, EnterpriseProfileWelcomeBrowserProxy, EnterpriseProfileWelcomeBrowserProxyImpl} from './enterprise_profile_welcome_browser_proxy.js';

document.addEventListener('DOMContentLoaded', () => {
  const enterpriseProfileWelcomeBrowserProxyImpl =
      EnterpriseProfileWelcomeBrowserProxyImpl.getInstance();
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // page even when the page zoom in Chrome is different than 100%.
  enterpriseProfileWelcomeBrowserProxyImpl.initializedWithSize(
      document.body.offsetHeight);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
});

export interface EnterpriseProfileWelcomeAppElement {
  $: {
    cancelButton: CrButtonElement,
    proceedButton: CrButtonElement,
  };
}

const EnterpriseProfileWelcomeAppElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class EnterpriseProfileWelcomeAppElement extends
    EnterpriseProfileWelcomeAppElementBase {
  static get is() {
    return 'enterprise-profile-welcome-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Whether the account is managed */
      showEnterpriseBadge_: {
        type: Boolean,
        value: false,
      },

      /** URL for the profile picture */
      pictureUrl_: String,

      /** The title and subtitle of the screen */
      title_: String,
      subtitle_: String,

      /** The detailed info about enterprise management */
      enterpriseInfo_: String,

      isModalDialog_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('isModalDialog');
        }
      },

      showLinkDataCheckbox_: {
        type: String,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('showLinkDataCheckbox');
        }
      },

      /** The label for the button to proceed with the flow */
      proceedLabel_: String,

      /** Whether to show the cancel button on the screen */
      showCancelButton_: {
        type: Boolean,
        value: true,
      },

      disableProceedButton_: {
        type: Boolean,
        value: false,
      },

      linkData_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'linkDataChanged_'
      },
    };
  }

  private showEnterpriseBadge_: boolean;
  private pictureUrl_: string;
  private title_: string;
  private subtitle_: string;
  private enterpriseInfo_: string;
  private isModalDialog_: boolean;
  private proceedLabel_: string;
  private disableProceedButton_: boolean;
  private linkData_: boolean;
  private showCancelButton_: boolean;
  private defaultProceedLabel_: string;
  private enterpriseProfileWelcomeBrowserProxy_:
      EnterpriseProfileWelcomeBrowserProxy =
          EnterpriseProfileWelcomeBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUIListener(
        'on-profile-info-changed',
        (info: EnterpriseProfileInfo) => this.setProfileInfo_(info));
    this.enterpriseProfileWelcomeBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  }

  private linkDataChanged_(linkData: boolean) {
    this.proceedLabel_ = linkData ? this.i18n('proceedAlternateLabel') :
                                    this.defaultProceedLabel_;
  }

  /** Called when the proceed button is clicked. */
  private onProceed_() {
    this.disableProceedButton_ = true;
    this.enterpriseProfileWelcomeBrowserProxy_.proceed(this.linkData_);
  }

  /** Called when the cancel button is clicked. */
  private onCancel_() {
    this.enterpriseProfileWelcomeBrowserProxy_.cancel();
  }

  private setProfileInfo_(info: EnterpriseProfileInfo) {
    this.style.setProperty('--header-background-color', info.backgroundColor);
    this.pictureUrl_ = info.pictureUrl;
    this.showEnterpriseBadge_ = info.showEnterpriseBadge;
    this.title_ = info.title;
    this.subtitle_ = info.subtitle;
    this.enterpriseInfo_ = info.enterpriseInfo;
    this.defaultProceedLabel_ = info.proceedLabel;
    this.proceedLabel_ = this.defaultProceedLabel_;
    this.showCancelButton_ = info.showCancelButton;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'enterprise-profile-welcome-app': EnterpriseProfileWelcomeAppElement;
  }
}

customElements.define(
    EnterpriseProfileWelcomeAppElement.is, EnterpriseProfileWelcomeAppElement);
