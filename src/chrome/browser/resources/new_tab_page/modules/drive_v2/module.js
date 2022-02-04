// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {File} from '../../drive.mojom-webui.js';
import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {DriveProxy} from '../drive/drive_module_proxy.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';

/**
 * The Drive module, which serves as an inside look in to recent activity within
 * a user's Google Drive.
 * @polymer
 * @extends {PolymerElement}
 */
class DriveModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-drive-module-redesigned';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!File>} */
      files: Array,
    };
  }

  /**
   * @param {File} file
   * @return {string}
   * @private
   */
  getImageSrc_(file) {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }

  /** @private */
  onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesDriveSentence2')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onFileClick_(e) {
    const clickFileEvent = new Event('usage', {composed: true});
    this.dispatchEvent(clickFileEvent);
    const index = this.$.fileRepeat.indexForElement(e.target);
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Drive.FileClick', index);
  }

  /** @private */
  onInfoButtonClick_() {
    /** @type {InfoDialogElement} */
    (this.$.infoDialogRender.get()).showModal();
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

/** @return {!Promise<!DriveModuleElement>} */
async function createDriveElement() {
  const {files} = await DriveProxy.getHandler().getFiles();
  const element = new DriveModuleElement();
  element.files = files.slice(0, 2);
  return element;
}

/** @type {!ModuleDescriptorV2} */
export const driveDescriptor = new ModuleDescriptorV2(
    /*id*/ 'drive',
    /*name*/ loadTimeData.getString('modulesDriveSentence'),
    /*height*/ ModuleHeight.SHORT, createDriveElement);
