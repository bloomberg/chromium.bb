// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {App} from './app_management.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementUserAction} from './constants.js';
import {recordAppManagementUserAction} from './util.js';

export class AppManagementMorePermissionsItemElement extends PolymerElement {
  static get is() {
    return 'app-management-more-permissions-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      app: Object,
      morePermissionsLabel: String,
    };
  }

  app: App;
  morePermissionsLabel: string;

  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  onClick_() {
    BrowserProxy.getInstance().handler.openNativeSettings(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NativeSettingsOpened);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-more-permissions-item':
        AppManagementMorePermissionsItemElement;
  }
}

customElements.define(
    AppManagementMorePermissionsItemElement.is,
    AppManagementMorePermissionsItemElement);
