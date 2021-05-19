// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Common from '../core/common/common.js';
import * as UI from '../ui/legacy/legacy.js';

export class ComboBoxOfCheckBoxes extends UI.Toolbar.ToolbarButton {
  private options = new Array<MenuOption>();
  private headers = new Array<MenuHeader>();
  private onOptionClicked = (): void => {};
  constructor(title: string) {
    super(title);
    this.turnIntoSelect();
    this.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this.showLevelContextMenu.bind(this));
    UI.ARIAUtils.markAsMenuButton(this.element);
  }

  addOption(option: string, value: string, defaultEnabled: boolean): void {
    this.options.push({'title': option, 'value': value, default: defaultEnabled, 'enabled': defaultEnabled});
  }

  setOptionEnabled(index: number, enabled: boolean): void {
    const option = this.options[index];
    if (!option) {
      return;
    }
    option.enabled = enabled;
    this.onOptionClicked();
  }

  addHeader(headerName: string, callback: (() => void)): void {
    this.headers.push({title: headerName, callback: callback});
  }

  setOnOptionClicked(onOptionClicked: (() => void)): void {
    this.onOptionClicked = onOptionClicked;
  }

  getOptions(): Array<MenuOption> {
    return this.options;
  }

  private showLevelContextMenu(event: Common.EventTarget.EventTargetEvent): void {
    const mouseEvent = /** @type {!Event} */ (event.data);
    const contextMenu = new UI.ContextMenu.ContextMenu(
        mouseEvent, true, this.element.totalOffsetLeft(),
        this.element.totalOffsetTop() +
            /** @type {!HTMLElement} */ (this.element).offsetHeight);

    for (const {title, callback} of this.headers) {
      contextMenu.headerSection().appendCheckboxItem(title, () => callback());
    }
    for (const [index, {title, enabled}] of this.options.entries()) {
      contextMenu.defaultSection().appendCheckboxItem(title, () => {
        this.setOptionEnabled(index, !enabled);
      }, enabled);
    }
    contextMenu.show();
  }
}

interface MenuOption {
  title: string;
  value: string;
  default: boolean;
  enabled: boolean;
}

interface MenuHeader {
  title: string;
  callback: () => void;
}
