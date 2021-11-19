// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-reset-profile-dialog' is the dialog shown for clearing profile
 * settings. A triggered variant of this dialog can be shown under certain
 * circumstances. See triggered_profile_resetter.h for when the triggered
 * variant will be used.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {ResetBrowserProxy, ResetBrowserProxyImpl} from './reset_browser_proxy.js';

export interface SettingsResetProfileDialogElement {
  $: {
    dialog: CrDialogElement,
    sendSettings: CrCheckboxElement,
  };
}

const SettingsResetProfileDialogElementBase = I18nMixin(PolymerElement);

export class SettingsResetProfileDialogElement extends
    SettingsResetProfileDialogElementBase {
  static get is() {
    return 'settings-reset-profile-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // TODO(dpapad): Evaluate whether this needs to be synced across different
      // settings tabs.

      isTriggered_: {
        type: Boolean,
        value: false,
      },

      triggeredResetToolName_: {
        type: String,
        value: '',
      },

      resetRequestOrigin_: String,

      clearingInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isTriggered_: boolean;
  private triggeredResetToolName_: string;
  private resetRequestOrigin_: string;
  private clearingInProgress_: boolean;
  private browserProxy_: ResetBrowserProxy =
      ResetBrowserProxyImpl.getInstance();

  private getExplanationText_(): string {
    if (this.isTriggered_) {
      return loadTimeData.getStringF(
          'triggeredResetPageExplanation', this.triggeredResetToolName_);
    }

    if (loadTimeData.getBoolean('showExplanationWithBulletPoints')) {
      return this.i18nAdvanced('resetPageExplanationBulletPoints', {
        tags: ['LINE_BREAKS', 'LINE_BREAK'],
      });
    }

    return loadTimeData.getStringF('resetPageExplanation');
  }

  private getPageTitle_(): string {
    if (this.isTriggered_) {
      return loadTimeData.getStringF(
          'triggeredResetPageTitle', this.triggeredResetToolName_);
    }
    return loadTimeData.getStringF('resetDialogTitle');
  }

  ready() {
    super.ready();

    this.addEventListener('cancel', () => {
      this.browserProxy_.onHideResetProfileDialog();
    });

    this.shadowRoot!.querySelector('cr-checkbox a')!.addEventListener(
        'click', this.onShowReportedSettingsTap_.bind(this));
  }

  private showDialog_() {
    if (!this.$.dialog.open) {
      this.$.dialog.showModal();
    }
    this.browserProxy_.onShowResetProfileDialog();
  }

  show() {
    this.isTriggered_ = Router.getInstance().getCurrentRoute() ===
        routes.TRIGGERED_RESET_DIALOG;
    if (this.isTriggered_) {
      this.browserProxy_.getTriggeredResetToolName().then(name => {
        this.resetRequestOrigin_ = 'triggeredreset';
        this.triggeredResetToolName_ = name;
        this.showDialog_();
      });
    } else {
      // For the non-triggered reset dialog, a '#cct' hash indicates that the
      // reset request came from the Chrome Cleanup Tool by launching Chrome
      // with the startup URL chrome://settings/resetProfileSettings#cct.
      const origin = window.location.hash.slice(1).toLowerCase() === 'cct' ?
          'cct' :
          Router.getInstance().getQueryParameters().get('origin');
      this.resetRequestOrigin_ = origin || '';
      this.showDialog_();
    }
  }

  private onCancelTap_() {
    this.cancel();
  }

  cancel() {
    if (this.$.dialog.open) {
      this.$.dialog.cancel();
    }
  }

  private onResetTap_() {
    this.clearingInProgress_ = true;
    this.browserProxy_
        .performResetProfileSettings(
            this.$.sendSettings.checked, this.resetRequestOrigin_)
        .then(() => {
          this.clearingInProgress_ = false;
          if (this.$.dialog.open) {
            this.$.dialog.close();
          }
          this.dispatchEvent(
              new CustomEvent('reset-done', {bubbles: true, composed: true}));
        });
  }

  /**
   * Displays the settings that will be reported in a new tab.
   */
  private onShowReportedSettingsTap_(e: Event) {
    this.browserProxy_.showReportedSettings();
    e.stopPropagation();
  }
}

customElements.define(
    SettingsResetProfileDialogElement.is, SettingsResetProfileDialogElement);
