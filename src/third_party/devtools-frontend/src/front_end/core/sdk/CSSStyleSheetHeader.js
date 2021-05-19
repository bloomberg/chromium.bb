// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as TextUtils from '../../models/text_utils/text_utils.js';
import * as Common from '../common/common.js';
import * as i18n from '../i18n/i18n.js';

import {CSSModel} from './CSSModel.js';  // eslint-disable-line no-unused-vars
import {DeferredDOMNode} from './DOMModel.js';
import {FrameAssociated} from './FrameAssociated.js';               // eslint-disable-line no-unused-vars
import {PageResourceLoadInitiator} from './PageResourceLoader.js';  // eslint-disable-line no-unused-vars
import {ResourceTreeModel} from './ResourceTreeModel.js';

const UIStrings = {
  /**
  *@description Error message for when a CSS file can't be loaded
  */
  couldNotFindTheOriginalStyle: 'Could not find the original style sheet.',
  /**
  *@description Error message to display when a source CSS file could not be retrieved.
  */
  thereWasAnErrorRetrievingThe: 'There was an error retrieving the source styles.',
};
const str_ = i18n.i18n.registerUIStrings('core/sdk/CSSStyleSheetHeader.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
/**
 * @implements {TextUtils.ContentProvider.ContentProvider}
 * TODO(chromium:1011811): make `implements {FrameAssociated}` annotation work here.
 */
export class CSSStyleSheetHeader {
  /**
   * @param {!CSSModel} cssModel
   * @param {!Protocol.CSS.CSSStyleSheetHeader} payload
   */
  constructor(cssModel, payload) {
    this._cssModel = cssModel;
    this.id = payload.styleSheetId;
    this.frameId = payload.frameId;
    this.sourceURL = payload.sourceURL;
    this.hasSourceURL = Boolean(payload.hasSourceURL);
    this.origin = payload.origin;
    this.title = payload.title;
    this.disabled = payload.disabled;
    this.isInline = payload.isInline;
    this.isMutable = payload.isMutable;
    // TODO(alexrudenko): Needs a roll of the browser_protocol.pdl.
    this.isConstructed = /** @type {*} */ (payload).isConstructed;
    this.startLine = payload.startLine;
    this.startColumn = payload.startColumn;
    this.endLine = payload.endLine;
    this.endColumn = payload.endColumn;
    this.contentLength = payload.length;
    if (payload.ownerNode) {
      this.ownerNode = new DeferredDOMNode(cssModel.target(), payload.ownerNode);
    }
    this.sourceMapURL = payload.sourceMapURL;
    this._originalContentProvider = null;
  }

  /**
   * @return {!TextUtils.ContentProvider.ContentProvider}
   */
  originalContentProvider() {
    if (!this._originalContentProvider) {
      const lazyContent = /** @type {function():!Promise<!TextUtils.ContentProvider.DeferredContent>} */ (async () => {
        const originalText = await this._cssModel.originalStyleSheetText(this);
        // originalText might be an empty string which should not trigger the error
        if (originalText === null) {
          return {content: null, error: i18nString(UIStrings.couldNotFindTheOriginalStyle), isEncoded: false};
        }
        return {content: originalText, isEncoded: false};
      });
      this._originalContentProvider =
          new TextUtils.StaticContentProvider.StaticContentProvider(this.contentURL(), this.contentType(), lazyContent);
    }
    return this._originalContentProvider;
  }

  /**
   * @param {string=} sourceMapURL
   */
  setSourceMapURL(sourceMapURL) {
    this.sourceMapURL = sourceMapURL;
  }

  /**
   * @return {!CSSModel}
   */
  cssModel() {
    return this._cssModel;
  }

  /**
   * @return {boolean}
   */
  isAnonymousInlineStyleSheet() {
    return !this.resourceURL() && !this._cssModel.sourceMapManager().sourceMapForClient(this);
  }

  /**
   * @return {string}
   */
  resourceURL() {
    return this.isViaInspector() ? this._viaInspectorResourceURL() : this.sourceURL;
  }

  /**
   * @return {string}
   */
  _viaInspectorResourceURL() {
    const model = this._cssModel.target().model(ResourceTreeModel);
    console.assert(Boolean(model));
    if (!model) {
      return '';
    }
    const frame = model.frameForId(this.frameId);
    if (!frame) {
      return '';
    }
    console.assert(Boolean(frame));
    const parsedURL = new Common.ParsedURL.ParsedURL(frame.url);
    let fakeURL = 'inspector://' + parsedURL.host + parsedURL.folderPathComponents;
    if (!fakeURL.endsWith('/')) {
      fakeURL += '/';
    }
    fakeURL += 'inspector-stylesheet';
    return fakeURL;
  }

  /**
   * @param {number} lineNumberInStyleSheet
   * @return {number}
   */
  lineNumberInSource(lineNumberInStyleSheet) {
    return this.startLine + lineNumberInStyleSheet;
  }

  /**
   * @param {number} lineNumberInStyleSheet
   * @param {number} columnNumberInStyleSheet
   * @return {number|undefined}
   */
  columnNumberInSource(lineNumberInStyleSheet, columnNumberInStyleSheet) {
    return (lineNumberInStyleSheet ? 0 : this.startColumn) + columnNumberInStyleSheet;
  }

  /**
   * Checks whether the position is in this style sheet. Assumes that the
   * position's columnNumber is consistent with line endings.
   * @param {number} lineNumber
   * @param {number} columnNumber
   * @return {boolean}
   */
  containsLocation(lineNumber, columnNumber) {
    const afterStart =
        (lineNumber === this.startLine && columnNumber >= this.startColumn) || lineNumber > this.startLine;
    const beforeEnd = lineNumber < this.endLine || (lineNumber === this.endLine && columnNumber <= this.endColumn);
    return afterStart && beforeEnd;
  }

  /**
   * @return {string}
   */
  contentURL() {
    return this.resourceURL();
  }

  /**
   * @return {!Common.ResourceType.ResourceType}
   */
  contentType() {
    return Common.ResourceType.resourceTypes.Stylesheet;
  }

  /**
   * @return {!Promise<boolean>}
   */
  contentEncoded() {
    return Promise.resolve(false);
  }

  /**
   * @return {!Promise<!TextUtils.ContentProvider.DeferredContent>}
   */
  async requestContent() {
    try {
      const cssText = await this._cssModel.getStyleSheetText(this.id);
      return {content: /** @type{string} */ (cssText), isEncoded: false};
    } catch (err) {
      return {
        content: null,
        error: i18nString(UIStrings.thereWasAnErrorRetrievingThe),
        isEncoded: false,
      };
    }
  }

  /**
   * @param {string} query
   * @param {boolean} caseSensitive
   * @param {boolean} isRegex
   * @return {!Promise<!Array<!TextUtils.ContentProvider.SearchMatch>>}
   */
  async searchInContent(query, caseSensitive, isRegex) {
    const requestedContent = await this.requestContent();
    if (requestedContent.content === null) {
      return [];
    }
    return TextUtils.TextUtils.performSearchInContent(requestedContent.content, query, caseSensitive, isRegex);
  }

  /**
   * @return {boolean}
   */
  isViaInspector() {
    return this.origin === 'inspector';
  }

  /**
   * @returns {!PageResourceLoadInitiator}
   */
  createPageResourceLoadInitiator() {
    return {target: null, frameId: this.frameId, initiatorUrl: this.hasSourceURL ? '' : this.sourceURL};
  }
}
