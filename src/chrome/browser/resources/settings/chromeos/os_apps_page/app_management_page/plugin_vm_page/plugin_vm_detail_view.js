// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../pin_to_shelf_item.js';
import '//resources/cr_components/app_management/icons.js';
import '//resources/cr_components/app_management/permission_item.js';
import '../shared_style.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';

import {Router} from '../../../../router.js';
import {routes} from '../../../os_route.js';
import {AppManagementStoreClient, AppManagementStoreClientInterface} from '../store_client.js';

import {PluginVmBrowserProxy, PluginVmBrowserProxyImpl} from './plugin_vm_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const AppManagementPluginVmDetailViewElementBase = mixinBehaviors(
    [AppManagementStoreClient, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class AppManagementPluginVmDetailViewElement extends
    AppManagementPluginVmDetailViewElementBase {
  static get is() {
    return 'app-management-plugin-vm-detail-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {App}
       */
      app_: Object,

      /** @private {boolean} */
      showDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private {string} */
      dialogText_: String,

      /** @private {Element} */
      pendingPermissionItem_: Object,
    };
  }

  constructor() {
    super();

    /** @private {!PluginVmBrowserProxy}  */
    this.pluginVmBrowserProxy_ = PluginVmBrowserProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();

    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }

  /** @private */
  onSharedPathsClick_() {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS,
        new URLSearchParams({'id': this.app_.id}));
  }

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES,
        new URLSearchParams({'id': this.app_.id}));
  }

  /**
   * @param {!Event} e
   * @private
   */
  async onPermissionChanged_(e) {
    this.pendingPermissionItem_ =
        /** @type {AppManamentPermissionItemElement} */ (e.target);
    switch (e.target.permissionType) {
      case 'kCamera':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogCameraLabel');
        break;
      case 'kMicrophone':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogMicrophoneLabel');
        break;
      default:
        assertNotReached();
    }

    const requiresRelaunch =
        await this.pluginVmBrowserProxy_.isRelaunchNeededForNewPermissions();
    if (requiresRelaunch) {
      this.showDialog_ = true;
    } else {
      this.pendingPermissionItem_.syncPermission();
    }
  }

  /** @private */
  onRelaunchTap_() {
    this.pendingPermissionItem_.syncPermission();
    this.pluginVmBrowserProxy_.relaunchPluginVm();
    this.showDialog_ = false;
  }

  /** @private */
  onCancel_() {
    this.pendingPermissionItem_.resetToggle();
    this.showDialog_ = false;
  }
}

customElements.define(
    AppManagementPluginVmDetailViewElement.is,
    AppManagementPluginVmDetailViewElement);
