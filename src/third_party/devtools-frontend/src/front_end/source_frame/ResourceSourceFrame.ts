// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) IBM Corp. 2009  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* eslint-disable rulesdir/no_underscored_properties */

import * as i18n from '../core/i18n/i18n.js';
import * as TextUtils from '../models/text_utils/text_utils.js';  // eslint-disable-line no-unused-vars
import * as UI from '../ui/legacy/legacy.js';

import {SourceFrameImpl} from './SourceFrame.js';

const UIStrings = {
  /**
  *@description Text to find an item
  */
  find: 'Find',
};
const str_ = i18n.i18n.registerUIStrings('source_frame/ResourceSourceFrame.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export class ResourceSourceFrame extends SourceFrameImpl {
  _resource: TextUtils.ContentProvider.ContentProvider;

  constructor(
      resource: TextUtils.ContentProvider.ContentProvider, autoPrettyPrint?: boolean,
      codeMirrorOptions?: UI.TextEditor.Options) {
    super(async () => {
      let content: string = (await resource.requestContent()).content || '';
      if (await resource.contentEncoded()) {
        content = window.atob(content);
      }
      return {content, isEncoded: false};
    }, codeMirrorOptions);
    this._resource = resource;
    this.setCanPrettyPrint(this._resource.contentType().isDocumentOrScriptOrStyleSheet(), autoPrettyPrint);
  }

  static createSearchableView(
      resource: TextUtils.ContentProvider.ContentProvider, highlighterType: string,
      autoPrettyPrint?: boolean): UI.Widget.Widget {
    return new SearchableContainer(resource, highlighterType, autoPrettyPrint);
  }

  get resource(): TextUtils.ContentProvider.ContentProvider {
    return this._resource;
  }

  populateTextAreaContextMenu(contextMenu: UI.ContextMenu.ContextMenu, _lineNumber: number, _columnNumber: number):
      Promise<void> {
    contextMenu.appendApplicableItems(this._resource);
    return Promise.resolve();
  }
}

export class SearchableContainer extends UI.Widget.VBox {
  _sourceFrame: ResourceSourceFrame;

  constructor(resource: TextUtils.ContentProvider.ContentProvider, highlighterType: string, autoPrettyPrint?: boolean) {
    super(true);
    this.registerRequiredCSS('source_frame/resourceSourceFrame.css', {enableLegacyPatching: true});
    const sourceFrame = new ResourceSourceFrame(resource, autoPrettyPrint);
    this._sourceFrame = sourceFrame;
    sourceFrame.setHighlighterType(highlighterType);
    const searchableView = new UI.SearchableView.SearchableView(sourceFrame, sourceFrame);
    searchableView.element.classList.add('searchable-view');
    searchableView.setPlaceholder(i18nString(UIStrings.find));
    sourceFrame.show(searchableView.element);
    sourceFrame.setSearchableView(searchableView);
    searchableView.show(this.contentElement);

    const toolbar = new UI.Toolbar.Toolbar('toolbar', this.contentElement);
    sourceFrame.toolbarItems().then(items => {
      items.map(item => toolbar.appendToolbarItem(item));
    });
  }

  async revealPosition(lineNumber: number, columnNumber?: number): Promise<void> {
    this._sourceFrame.revealPosition(lineNumber, columnNumber, true);
  }
}
