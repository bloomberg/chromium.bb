// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../../assert.js';
import * as barcodeChip from '../../barcode_chip.js';
import {CameraManager, CameraUI} from '../../device/index.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import {BarcodeScanner} from '../../models/barcode.js';
import * as state from '../../state.js';
import {Mode, PreviewVideo} from '../../type.js';
import {assertEnumVariant} from '../../util.js';

import {DocumentCornerOverlay} from './document_corner_overlay.js';

enum ScanType {
  BARCODE = 'barcode',
  DOCUMENT = 'document',
}

function getScanTypeFromElement(el: HTMLInputElement): ScanType {
  return assertEnumVariant(ScanType, el.dataset['scantype']);
}

function getElemetFromScanType(type: ScanType): HTMLInputElement {
  return dom.get(`input[data-scantype=${type}]`, HTMLInputElement);
}

const DEFAULT_SCAN_TYPE = ScanType.DOCUMENT;

/**
 * Controller for the scan options of Camera view.
 */
export class ScanOptions implements CameraUI {
  /**
   * Togglable barcode option in photo mode.
   */
  private readonly photoBarcodeOption =
      dom.get('#toggle-barcode', HTMLInputElement);

  private readonly scanOptions =
      [...dom.getAll('#scan-modes-group [data-scantype]', HTMLInputElement)];

  private video: PreviewVideo|null = null;

  /**
   * May be null if preview is not ready.
   */
  private barcodeScanner: BarcodeScanner|null = null;

  private readonly documentCornerOverylay: DocumentCornerOverlay;

  /**
   * Called when scan option changed.
   * TODO(pihsun): Change to use a setter function to set this callback,
   * instead of a public property.
   */
  onChange = (): void => {
    // Do nothing.
  };

  /*
   * @param updatePointOfInterest function to update point of interest on the
   *     stream.
   */
  constructor(private readonly cameraManager: CameraManager) {
    this.cameraManager.registerCameraUI(this);

    this.documentCornerOverylay = new DocumentCornerOverlay(
        (p) => this.cameraManager.setPointOfInterest(p));

    [this.photoBarcodeOption, ...this.scanOptions].forEach((opt) => {
      opt.addEventListener('click', (evt) => {
        if (state.get(state.State.CAMERA_CONFIGURING)) {
          evt.preventDefault();
        }
      });
    });
    this.photoBarcodeOption.addEventListener('change', () => {
      this.updateOption(
          this.photoBarcodeOption.checked ? ScanType.BARCODE : null);
    });
    this.scanOptions.forEach((opt) => {
      opt.addEventListener('change', () => {
        if (opt.checked) {
          this.updateOption(this.getToggledScanOption());
        }
      });
    });
  }

  /**
   * Whether preview have attached as scan frame source.
   */
  private previewAvailable(): boolean {
    return this.video?.isExpired() === false;
  }

  // Overrides |CameraUI|.
  async onUpdateConfig(): Promise<void> {
    assert(!this.previewAvailable());

    const video = this.cameraManager.getPreviewVideo();
    this.video = video;
    this.barcodeScanner = new BarcodeScanner(video.video, (value) => {
      barcodeChip.show(value);
    });
    const {deviceId} = video.getVideoSettings();
    this.documentCornerOverylay.attach(deviceId);
    const scanType = state.get(Mode.SCAN) ? this.getToggledScanOption() : null;
    (async () => {
      await video.onExpired;
      this.detachPreview();
    })();
    await this.updateOption(scanType);
  }

  /**
   * @return Returns scan type of checked radio buttons in scan type option
   *     groups.
   */
  private getToggledScanOption(): ScanType {
    const checkedEl = this.scanOptions.find(({checked}) => checked);
    return checkedEl === undefined ? DEFAULT_SCAN_TYPE :
                                     getScanTypeFromElement(checkedEl);
  }

  isDocumentModeEanbled(): boolean {
    return this.documentCornerOverylay.isEnabled();
  }

  /**
   * @param scanType Scan type to be enabled, null for no type is
   *     enabled.
   */
  private async updateOption(scanType: ScanType|null) {
    if (!this.previewAvailable()) {
      return;
    }
    assert(this.barcodeScanner !== null);

    this.updateOptionsUI(scanType);
    const mode = state.get(state.State.SHOW_SCAN_MODE) ? Mode.SCAN : Mode.PHOTO;
    if (state.get(mode) && scanType === ScanType.BARCODE) {
      sendBarcodeEnabledEvent();
      this.barcodeScanner.start();
      state.set(state.State.ENABLE_SCAN_BARCODE, true);
    } else {
      this.stopBarcodeScanner();
    }

    if (state.get(Mode.SCAN) && scanType === ScanType.DOCUMENT) {
      await this.documentCornerOverylay.start();
    } else {
      await this.documentCornerOverylay.stop();
    }

    this.onChange();
  }

  private stopBarcodeScanner() {
    assert(this.barcodeScanner !== null);
    this.barcodeScanner.stop();
    barcodeChip.dismiss();
    state.set(state.State.ENABLE_SCAN_BARCODE, false);
  }

  private updateOptionsUI(scanType: ScanType|null) {
    if (state.get(Mode.SCAN)) {
      assert(scanType !== null);
      getElemetFromScanType(scanType).checked = true;
    } else if (state.get(Mode.PHOTO)) {
      this.photoBarcodeOption.checked = scanType === ScanType.BARCODE;
    }
  }

  /**
   * Stops all scanner and detach from current preview.
   */
  private async detachPreview(): Promise<void> {
    if (this.barcodeScanner !== null) {
      this.stopBarcodeScanner();
      this.barcodeScanner = null;
    }
    await this.documentCornerOverylay.detach();
  }
}
