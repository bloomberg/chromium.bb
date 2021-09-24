// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../../../core/common/common.js';

export class ContrastInfo extends Common.ObjectWrapper.ObjectWrapper<EventTypes> {
  private readonly isNullInternal: boolean;
  private contrastRatioInternal: number|null;
  private contrastRatioAPCAInternal: number|null;
  private contrastRatioThresholds: {
    [x: string]: number,
  }|null;
  private readonly contrastRationAPCAThreshold: number|null;
  private fgColor: Common.Color.Color|null;
  private bgColorInternal: Common.Color.Color|null;
  private colorFormatInternal!: string|undefined;
  constructor(contrastInfo: ContrastInfoType|null) {
    super();
    this.isNullInternal = true;
    this.contrastRatioInternal = null;
    this.contrastRatioAPCAInternal = null;
    this.contrastRatioThresholds = null;
    this.contrastRationAPCAThreshold = 0;
    this.fgColor = null;
    this.bgColorInternal = null;

    if (!contrastInfo) {
      return;
    }

    if (!contrastInfo.computedFontSize || !contrastInfo.computedFontWeight || !contrastInfo.backgroundColors ||
        contrastInfo.backgroundColors.length !== 1) {
      return;
    }

    this.isNullInternal = false;
    this.contrastRatioThresholds =
        Common.ColorUtils.getContrastThreshold(contrastInfo.computedFontSize, contrastInfo.computedFontWeight);
    this.contrastRationAPCAThreshold =
        Common.ColorUtils.getAPCAThreshold(contrastInfo.computedFontSize, contrastInfo.computedFontWeight);
    const bgColorText = contrastInfo.backgroundColors[0];
    const bgColor = Common.Color.Color.parse(bgColorText);
    if (bgColor) {
      this.setBgColorInternal(bgColor);
    }
  }

  isNull(): boolean {
    return this.isNullInternal;
  }

  setColor(fgColor: Common.Color.Color, colorFormat?: string): void {
    this.fgColor = fgColor;
    this.colorFormatInternal = colorFormat;
    this.updateContrastRatio();
    this.dispatchEventToListeners(Events.ContrastInfoUpdated);
  }

  colorFormat(): string|undefined {
    return this.colorFormatInternal;
  }

  color(): Common.Color.Color|null {
    return this.fgColor;
  }

  contrastRatio(): number|null {
    return this.contrastRatioInternal;
  }

  contrastRatioAPCA(): number|null {
    return this.contrastRatioAPCAInternal;
  }

  contrastRatioAPCAThreshold(): number|null {
    return this.contrastRationAPCAThreshold;
  }

  setBgColor(bgColor: Common.Color.Color): void {
    this.setBgColorInternal(bgColor);
    this.dispatchEventToListeners(Events.ContrastInfoUpdated);
  }

  private setBgColorInternal(bgColor: Common.Color.Color): void {
    this.bgColorInternal = bgColor;

    if (!this.fgColor) {
      return;
    }

    const fgRGBA = this.fgColor.rgba();

    // If we have a semi-transparent background color over an unknown
    // background, draw the line for the "worst case" scenario: where
    // the unknown background is the same color as the text.
    if (bgColor.hasAlpha()) {
      const blendedRGBA: number[] = Common.ColorUtils.blendColors(bgColor.rgba(), fgRGBA);
      this.bgColorInternal = new Common.Color.Color(blendedRGBA, Common.Color.Format.RGBA);
    }

    this.contrastRatioInternal = Common.ColorUtils.contrastRatio(fgRGBA, this.bgColorInternal.rgba());
    this.contrastRatioAPCAInternal =
        Common.ColorUtils.contrastRatioAPCA(this.fgColor.rgba(), this.bgColorInternal.rgba());
  }

  bgColor(): Common.Color.Color|null {
    return this.bgColorInternal;
  }

  private updateContrastRatio(): void {
    if (!this.bgColorInternal || !this.fgColor) {
      return;
    }
    this.contrastRatioInternal = Common.ColorUtils.contrastRatio(this.fgColor.rgba(), this.bgColorInternal.rgba());
    this.contrastRatioAPCAInternal =
        Common.ColorUtils.contrastRatioAPCA(this.fgColor.rgba(), this.bgColorInternal.rgba());
  }

  contrastRatioThreshold(level: string): number|null {
    if (!this.contrastRatioThresholds) {
      return null;
    }
    return this.contrastRatioThresholds[level];
  }
}

export const enum Events {
  ContrastInfoUpdated = 'ContrastInfoUpdated',
}

export type EventTypes = {
  [Events.ContrastInfoUpdated]: void,
};

export interface ContrastInfoType {
  backgroundColors: string[]|null;
  computedFontSize: string;
  computedFontWeight: string;
}
