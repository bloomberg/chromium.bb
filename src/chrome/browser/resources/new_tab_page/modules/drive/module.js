// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {DriveProxy} from './drive_module_proxy.js';

/**
 * The Drive module, which serves as an inside look in to recent activity within
 * a user's Google Drive.
 * @polymer
 * @extends {PolymerElement}
 */
class DriveModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-drive-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!drive.mojom.File>} */
      files: Array,
    };
  }

  /** @private */
  onInfoButtonClick_() {
    /** @type {InfoDialogElement} */ (this.$.infoDialogRender.get())
        .showModal();
  }

  /** @private */
  onDismissButtonClick_() {
    DriveProxy.getInstance().handler.dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesDriveFilesSentence')),
        restoreCallback: () => DriveProxy.getInstance().handler.restoreModule(),
      },
    }));
  }

  /** @private */
  onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesDriveSentence2')),
      },
    }));
  }

  /**
   * @param {drive.mojom.File} file
   * @return {string}
   * @private
   */
  getImageSrc_(file) {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onFileClick_(e) {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    const index = this.$.fileRepeat.indexForElement(e.target);
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Drive.FileClick', index);
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

/**
 * @return {!Promise<DriveModuleElement>}
 */
async function createDriveElement() {
  const {files} = await DriveProxy.getInstance().handler.getFiles();
  if (files.length === 0) {
    return null;
  }
  const element = new DriveModuleElement();
  element.files = files;
  return element;
}

/** @type {!ModuleDescriptor} */
export const driveDescriptor = new ModuleDescriptor(
    /*id=*/ 'drive',
    /*name=*/ loadTimeData.getString('modulesDriveSentence'),
    createDriveElement);
