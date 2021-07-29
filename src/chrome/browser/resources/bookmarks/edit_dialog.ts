// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import {BookmarkNode} from './types.js';

export interface BookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
    url: CrInputElement,
  }
}

export class BookmarksEditDialogElement extends PolymerElement {
  static get is() {
    return 'bookmarks-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      isFolder_: Boolean,
      isEdit_: Boolean,

      /**
       * Item that is being edited, or null when adding.
       */
      editItem_: Object,

      /**
       * Parent node for the item being added, or null when editing.
       */
      parentId_: String,
      titleValue_: String,
      urlValue_: String,
    };
  }

  private isFolder_: boolean;
  private isEdit_: boolean;
  private editItem_: BookmarkNode|null;
  private parentId_: string|null;
  private titleValue_: string;
  private urlValue_: string;

  /**
   * Show the dialog to add a new folder (if |isFolder|) or item, which will be
   * inserted into the tree as a child of |parentId|.
   */
  showAddDialog(isFolder: boolean, parentId: string) {
    this.reset_();
    this.isEdit_ = false;
    this.isFolder_ = isFolder;
    this.parentId_ = parentId;

    DialogFocusManager.getInstance().showDialog(this.$.dialog);
  }

  /** Show the edit dialog for |editItem|. */
  showEditDialog(editItem: BookmarkNode) {
    this.reset_();
    this.isEdit_ = true;
    this.isFolder_ = !editItem.url;
    this.editItem_ = editItem;

    this.titleValue_ = editItem.title;
    if (!this.isFolder_) {
      this.urlValue_ = assert(editItem.url!);
    }

    DialogFocusManager.getInstance().showDialog(this.$.dialog);
  }

  /**
   * Clear out existing values from the dialog, allowing it to be reused.
   */
  private reset_() {
    this.editItem_ = null;
    this.parentId_ = null;
    this.$.url.invalid = false;
    this.titleValue_ = '';
    this.urlValue_ = '';
  }

  private getDialogTitle_(isFolder: boolean, isEdit: boolean): string {
    let title;
    if (isEdit) {
      title = isFolder ? 'renameFolderTitle' : 'editBookmarkTitle';
    } else {
      title = isFolder ? 'addFolderTitle' : 'addBookmarkTitle';
    }

    return loadTimeData.getString(title);
  }

  /**
   * Validates the value of the URL field, returning true if it is a valid URL.
   * May modify the value by prepending 'http://' in order to make it valid.
   */
  private validateUrl_(): boolean {
    const urlInput = this.$.url;
    const originalValue = this.urlValue_;

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = 'http://' + originalValue;

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = originalValue;
    return false;
  }

  private onSaveButtonTap_() {
    const edit: { title: string, url?: string, parentId?: string|null } =
        { 'title': this.titleValue_ };
    if (!this.isFolder_) {
      if (!this.validateUrl_()) {
        return;
      }

      edit['url'] = this.urlValue_;
    }

    if (this.isEdit_) {
      chrome.bookmarks.update(this.editItem_!.id, edit);
    } else {
      edit['parentId'] = this.parentId_;
      trackUpdatedItems();
      chrome.bookmarks.create(edit, highlightUpdatedItems);
    }
    this.$.dialog.close();
  }

  private onCancelButtonTap_() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    BookmarksEditDialogElement.is, BookmarksEditDialogElement);
