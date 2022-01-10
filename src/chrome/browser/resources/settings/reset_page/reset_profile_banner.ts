// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-banner' is the banner shown for prompting the user to
 * clear profile settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import {ResetBrowserProxyImpl} from './reset_browser_proxy.js';

export interface SettingsResetProfileBannerElement {
  $: {
    dialog: CrDialogElement,
    ok: HTMLElement,
    reset: HTMLElement,
  };
}

export class SettingsResetProfileBannerElement extends PolymerElement {
  static get is() {
    return 'settings-reset-profile-banner';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onOkTap_() {
    this.$.dialog.cancel();
  }

  private onCancel_() {
    ResetBrowserProxyImpl.getInstance().onHideResetProfileBanner();
  }

  private onResetTap_() {
    this.$.dialog.close();
    Router.getInstance().navigateTo(routes.RESET_DIALOG);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-profile-banner': SettingsResetProfileBannerElement;
  }
}

customElements.define(
    SettingsResetProfileBannerElement.is, SettingsResetProfileBannerElement);
