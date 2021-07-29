// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from '../read_later_api_proxy.js';

import {BookmarksApiProxy} from './bookmarks_api_proxy.js';

/** Event interface for dom-repeat. */
interface RepeaterMouseEvent extends CustomEvent {
  clientX: number;
  clientY: number;
  model: {
    item: chrome.bookmarks.BookmarkTreeNode,
  };
}

export interface BookmarkFolderElement {
  $: {
    children: HTMLElement,
  };
}

// Event name for open state of a folder being changed.
export const FOLDER_OPEN_CHANGED_EVENT = 'bookmark-folder-open-changed';

export class BookmarkFolderElement extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      childDepth_: {
        type: Number,
        value: 1,
      },

      depth: {
        type: Number,
        observer: 'onDepthChanged_',
        value: 0,
      },

      folder: Object,

      open_: {
        type: Boolean,
        value: false,
      },

      openFolders: {
        type: Array,
        observer: 'onOpenFoldersChanged_',
      },
    };
  }

  private childDepth_: number;
  depth: number;
  folder: chrome.bookmarks.BookmarkTreeNode;
  private open_: boolean;
  openFolders: string[];
  private bookmarksApi_: BookmarksApiProxy = BookmarksApiProxy.getInstance();

  private onBookmarkClick_(event: RepeaterMouseEvent) {
    event.preventDefault();
    this.bookmarksApi_.openBookmark(event.model.item.url!, this.depth);
  }

  private onBookmarkContextMenu_(event: RepeaterMouseEvent) {
    event.preventDefault();
    this.bookmarksApi_.showContextMenu(
        event.model.item.id, event.clientX, event.clientY);
  }

  private getBookmarkIcon_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  private onDepthChanged_() {
    this.childDepth_ = this.depth + 1;
    this.style.setProperty('--node-depth', `${this.depth}`);
    this.$.children.style.setProperty('--node-depth', `${this.childDepth_}`);
  }

  private onFolderClick_() {
    if (!this.folder.children || this.folder.children.length === 0) {
      // No reason to open if there are no children to show.
      return;
    }

    this.open_ = !this.open_;
    this.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: this.folder.id,
        open: this.open_,
      }
    }));

    chrome.metricsPrivate.recordUserAction(
        this.open_ ? 'SidePanel.Bookmarks.FolderOpen' :
                     'SidePanel.Bookmarks.FolderClose');
  }

  private onOpenFoldersChanged_() {
    this.open_ =
        Boolean(this.openFolders) && this.openFolders.includes(this.folder.id);
  }

  private getFocusableRows_(): HTMLElement[] {
    return Array.from(
        this.shadowRoot!.querySelectorAll('.row, bookmark-folder'));
  }

  moveFocus(delta: -1|1): boolean {
    const currentFocus = this.shadowRoot!.activeElement;
    if (currentFocus instanceof BookmarkFolderElement &&
        currentFocus.moveFocus(delta)) {
      // If focus is already inside a nested folder, delegate the focus to the
      // nested folder and return early if successful.
      return true;
    }

    let moveFocusTo = null;
    const focusableRows = this.getFocusableRows_();
    if (currentFocus) {
      // If focus is in this folder, move focus to the next or previous
      // focusable row.
      const currentFocusIndex =
          focusableRows.indexOf(currentFocus as HTMLElement);
      moveFocusTo = focusableRows[currentFocusIndex + delta];
    } else {
      // If focus is not in this folder yet, move focus to either end.
      moveFocusTo = delta === 1 ? focusableRows[0] :
                                  focusableRows[focusableRows.length - 1];
    }

    if (moveFocusTo instanceof BookmarkFolderElement) {
      return moveFocusTo.moveFocus(delta);
    } else if (moveFocusTo) {
      moveFocusTo.focus();
      return true;
    } else {
      return false;
    }
  }
}

customElements.define(BookmarkFolderElement.is, BookmarkFolderElement);