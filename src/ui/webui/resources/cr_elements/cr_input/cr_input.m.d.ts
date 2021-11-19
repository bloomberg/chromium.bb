// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrInputElement extends LegacyElementMixin, HTMLElement {
  ariaDescription: string|undefined;
  ariaLabel: string|null|undefined;
  autofocus: boolean;
  autoValidate: boolean|null|undefined;
  disabled: boolean|null|undefined;
  errorMessage: string|null|undefined;
  invalid: boolean;
  max: number|null|undefined;
  min: number|null|undefined;
  maxlength: number|null|undefined;
  minlength: number|null|undefined;
  pattern: string|null|undefined;
  inputmode: string|null|undefined;
  label: string|null|undefined;
  placeholder: string|null;
  readonly: boolean|null|undefined;
  required: boolean|null|undefined;
  tabindex: number|null;
  type: string|null|undefined;
  value: string;
  hostAttributes: object|null;
  readonly inputElement: HTMLInputElement;

  focusInput(): boolean;
  select(start?: number, end?: number): void;
  validate(): boolean;
}

export {CrInputElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-input': CrInputElement;
  }
}
