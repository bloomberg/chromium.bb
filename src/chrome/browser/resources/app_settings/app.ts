// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/run_on_os_login_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_components/app_management/window_mode_item.js';
import 'chrome://resources/cr_components/app_management/icons.js';
import 'chrome://resources/cr_components/app_management/uninstall_button.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// TODO(crbug.com/1294060): Investigate end-to-end WebAppSettings tests
export class WebAppSettingsAppElement extends PolymerElement {
  static get is() {
    return 'web-app-settings-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      app_: Object,
      iconUrl_: {type: String, computed: 'getAppIcon_(app_)'},
      showSearch_: {type: Boolean, value: false, readonly: true},
    };
  }

  private app_: App|null;
  private iconUrl_: string;
  private showSearch_: boolean;

  connectedCallback() {
    super.connectedCallback();
    const urlPath = new URL(document.URL).pathname;
    if (urlPath.length <= 1) {
      return;
    }

    window.CrPolicyStrings = {
      controlledSettingPolicy: loadTimeData.getString('controlledSettingPolicy')
    };

    const appId = urlPath.substring(1);
    BrowserProxy.getInstance().handler.getApp(appId).then((result) => {
      this.app_ = result.app;
    });

    // Listens to app update.
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    callbackRouter.onAppChanged.addListener(this.onAppChanged_.bind(this));
  }

  private onAppChanged_(app: App) {
    if (this.app_ && app.id === this.app_.id) {
      this.app_ = app;
    }
  }

  private getAppIcon_(app: App|null): string {
    return app ? getAppIcon(app) : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'web-app-settings-app': WebAppSettingsAppElement;
  }

  interface Window {
    CrPolicyStrings: {[key: string]: string},
  }
}

customElements.define(WebAppSettingsAppElement.is, WebAppSettingsAppElement);
