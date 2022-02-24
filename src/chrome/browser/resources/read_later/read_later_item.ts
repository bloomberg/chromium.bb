// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';

import {MouseHoverableMixin} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterEntry} from './read_later.mojom-webui.js';
import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';

const navigationKeys: Set<string> =
    new Set([' ', 'Enter', 'ArrowRight', 'ArrowLeft']);

export interface ReadLaterItemElement {
  $: {
    updateStatusButton: HTMLElement,
    deleteButton: HTMLElement,
  },
}

const ReadLaterItemElementBase = MouseHoverableMixin(PolymerElement);

export class ReadLaterItemElement extends ReadLaterItemElementBase {
  static get is() {
    return 'read-later-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      data: Object,
      buttonRipples: Boolean,
    };
  }

  data: ReadLaterEntry;
  buttonRipples: boolean;
  private apiProxy_: ReadLaterApiProxy = ReadLaterApiProxyImpl.getInstance();

  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
    this.addEventListener('auxclick', this.onAuxClick_.bind(this));
    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  private onAuxClick_(e: MouseEvent) {
    if (e.button !== 1) {
      // Not a middle click.
      return;
    }

    this.apiProxy_.openURL(this.data.url, true, {
      middleButton: true,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  private onClick_(e: MouseEvent|KeyboardEvent) {
    this.apiProxy_.openURL(this.data.url, true, {
      middleButton: false,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  private onContextMenu_(e: MouseEvent) {
    this.apiProxy_.showContextMenuForURL(this.data.url, e.clientX, e.clientY);
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    switch (e.key) {
      case ' ':
      case 'Enter':
        this.onClick_(e);
        break;
      case 'ArrowRight':
        if (!this.shadowRoot!.activeElement) {
          this.$.updateStatusButton.focus();
        } else if (this.shadowRoot!.activeElement.nextElementSibling) {
          (this.shadowRoot!.activeElement.nextElementSibling as HTMLElement)
              .focus();
        } else {
          this.focus();
        }
        break;
      case 'ArrowLeft':
        if (!this.shadowRoot!.activeElement) {
          this.$.deleteButton.focus();
        } else if (this.shadowRoot!.activeElement.nextElementSibling) {
        } else if (this.shadowRoot!.activeElement.previousElementSibling) {
          (this.shadowRoot!.activeElement.previousElementSibling as HTMLElement)
              .focus();
        } else {
          this.focus();
        }
        break;
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  private onUpdateStatusClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.updateReadStatus(this.data.url, !this.data.read);
  }

  private onItemDeleteClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.removeEntry(this.data.url);
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  /**
   * @return The appropriate icon for the current state
   */
  private getUpdateStatusButtonIcon_(
      markAsUnreadIcon: string, markAsReadIcon: string): string {
    return this.data.read ? markAsUnreadIcon : markAsReadIcon;
  }

  /**
   * @return The appropriate tooltip for the current state
   */
  private getUpdateStatusButtonTooltip_(
      markAsUnreadTooltip: string, markAsReadTooltip: string): string {
    return this.data.read ? markAsUnreadTooltip : markAsReadTooltip;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-later-item': ReadLaterItemElement;
  }
}

customElements.define(ReadLaterItemElement.is, ReadLaterItemElement);
