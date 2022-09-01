// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MenuItem extends HTMLElement {
  disabled: boolean;
  hidden: any;
  selected: boolean;
  checked: boolean;
  checkable: boolean;
  handleEvent(e: Event): void;
}
