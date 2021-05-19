/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../core/common/common.js';
import * as Host from '../core/host/host.js';
import * as i18n from '../core/i18n/i18n.js';
import * as SDK from '../core/sdk/sdk.js';
import * as Bindings from '../models/bindings/bindings.js';
import * as TextUtils from '../models/text_utils/text_utils.js';
import * as UI from '../ui/legacy/legacy.js';
import * as Workspace from '../workspace/workspace.js';  // eslint-disable-line no-unused-vars

const UIStrings = {
  /**
  *@description Text in Linkifier
  */
  unknown: '(unknown)',
  /**
  *@description Text short for automatic
  */
  auto: 'auto',
  /**
  *@description Text in Linkifier
  *@example {Sources panel} PH1
  */
  revealInS: 'Reveal in {PH1}',
  /**
  *@description Text for revealing an item in its destination
  */
  reveal: 'Reveal',
  /**
  *@description A context menu item in the Linkifier
  *@example {Extension} PH1
  */
  openUsingS: 'Open using {PH1}',
  /**
  * @description The name of a setting which controls how links are handled in the UI. 'Handling'
  * refers to the ability of extensions to DevTools to be able to intercept link clicks so that they
  * can react to them.
  */
  linkHandling: 'Link handling:',
};
const str_ = i18n.i18n.registerUIStrings('components/Linkifier.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
/** @type {!Set<!Linkifier>} */
const instances = new Set();

/** @type {?LinkDecorator} */
let decorator = null;

/** @type {!WeakMap<!Workspace.UISourceCode.UISourceCode, !Set<!Element>>} */
const anchorsByUISourceCode = new WeakMap();

/** @type {!WeakMap<!Node, _LinkInfo>} */
const infoByAnchor = new WeakMap();

/** @type {!WeakMap<!Node, string>} */
const textByAnchor = new WeakMap();

/** @type {!Map<string, !LinkHandler>} */
const linkHandlers = new Map();

/** @type {!Common.Settings.Setting<string>} */
let linkHandlerSettingInstance;

/**
 * @implements {SDK.SDKModel.Observer}
 */
export class Linkifier {
  /**
   * @param {number=} maxLengthForDisplayedURLs
   * @param {boolean=} useLinkDecorator
   * @param {function():void=} onLiveLocationUpdate
   */
  constructor(maxLengthForDisplayedURLs, useLinkDecorator, onLiveLocationUpdate = () => {}) {
    this._maxLength = maxLengthForDisplayedURLs || UI.UIUtils.MaxLengthForDisplayedURLs;
    /** @type {!Map<!SDK.SDKModel.Target, !Array<!Element>>} */
    this._anchorsByTarget = new Map();
    /** @type {!Map<!SDK.SDKModel.Target, !Bindings.LiveLocation.LiveLocationPool>} */
    this._locationPoolByTarget = new Map();
    this._onLiveLocationUpdate = onLiveLocationUpdate;
    this._useLinkDecorator = Boolean(useLinkDecorator);
    instances.add(this);
    SDK.SDKModel.TargetManager.instance().observeTargets(this);
  }

  /**
   * @param {!LinkDecorator} linkDecorator
   */
  static setLinkDecorator(linkDecorator) {
    console.assert(!decorator, 'Cannot re-register link decorator.');
    decorator = linkDecorator;
    linkDecorator.addEventListener(LinkDecorator.Events.LinkIconChanged, onLinkIconChanged);
    for (const linkifier of instances) {
      linkifier._updateAllAnchorDecorations();
    }

    /**
     * @param {!Common.EventTarget.EventTargetEvent} event
     */
    function onLinkIconChanged(event) {
      const uiSourceCode = /** @type {!Workspace.UISourceCode.UISourceCode} */ (event.data);
      const links = anchorsByUISourceCode.get(uiSourceCode) || [];
      for (const link of links) {
        Linkifier._updateLinkDecorations(link);
      }
    }
  }

  _updateAllAnchorDecorations() {
    for (const anchors of this._anchorsByTarget.values()) {
      for (const anchor of anchors) {
        Linkifier._updateLinkDecorations(anchor);
      }
    }
  }

  /**
   * @param {!Element} anchor
   * @param {!Workspace.UISourceCode.UILocation} uiLocation
   */
  static _bindUILocation(anchor, uiLocation) {
    const linkInfo = Linkifier.linkInfo(anchor);
    if (!linkInfo) {
      return;
    }
    linkInfo.uiLocation = uiLocation;
    if (!uiLocation) {
      return;
    }
    const uiSourceCode = uiLocation.uiSourceCode;
    let sourceCodeAnchors = anchorsByUISourceCode.get(uiSourceCode);
    if (!sourceCodeAnchors) {
      sourceCodeAnchors = new Set();
      anchorsByUISourceCode.set(uiSourceCode, sourceCodeAnchors);
    }
    sourceCodeAnchors.add(anchor);
  }

  /**
   * @param {!Element} anchor
   */
  static _unbindUILocation(anchor) {
    const info = Linkifier.linkInfo(anchor);
    if (!info || !info.uiLocation) {
      return;
    }

    const uiSourceCode = info.uiLocation.uiSourceCode;
    info.uiLocation = null;
    const sourceCodeAnchors = anchorsByUISourceCode.get(uiSourceCode);
    if (sourceCodeAnchors) {
      sourceCodeAnchors.delete(anchor);
    }
  }

  /**
   * @override
   * @param {!SDK.SDKModel.Target} target
   */
  targetAdded(target) {
    this._anchorsByTarget.set(target, []);
    this._locationPoolByTarget.set(target, new Bindings.LiveLocation.LiveLocationPool());
  }

  /**
   * @override
   * @param {!SDK.SDKModel.Target} target
   */
  targetRemoved(target) {
    const locationPool = this._locationPoolByTarget.get(target);
    this._locationPoolByTarget.delete(target);
    if (!locationPool) {
      return;
    }
    locationPool.disposeAll();
    const anchors = /** @type {?Array<!HTMLElement>} */ (this._anchorsByTarget.get(target));
    if (!anchors) {
      return;
    }
    this._anchorsByTarget.delete(target);
    for (const anchor of anchors) {
      const info = Linkifier.linkInfo(anchor);
      if (!info) {
        continue;
      }
      info.liveLocation = null;
      Linkifier._unbindUILocation(anchor);
      const fallback = /** @type {?HTMLElement} */ (info.fallback);
      if (fallback) {
        // @ts-ignore
        anchor.href = fallback.href;
        UI.Tooltip.Tooltip.install(anchor, UI.Tooltip.Tooltip.getContent(fallback));
        anchor.className = fallback.className;
        anchor.textContent = fallback.textContent;
        const fallbackInfo = infoByAnchor.get(fallback);
        if (fallbackInfo) {
          infoByAnchor.set(anchor, fallbackInfo);
        }
      }
    }
  }

  /**
   * @param {?SDK.SDKModel.Target} target
   * @param {?string} scriptId
   * @param {string} sourceURL
   * @param {number|undefined} lineNumber
   * @param {!LinkifyOptions=} options
   * @return {?HTMLElement}
   */
  maybeLinkifyScriptLocation(target, scriptId, sourceURL, lineNumber, options) {
    let fallbackAnchor = null;
    const linkifyURLOptions = {
      lineNumber,
      maxLength: this._maxLength,
      columnNumber: options ? options.columnNumber : undefined,
      className: options ? options.className : undefined,
      tabStop: options ? options.tabStop : undefined,
      inlineFrameIndex: options ? options.inlineFrameIndex : 0,
      text: undefined,
      preventClick: undefined,
      bypassURLTrimming: undefined
    };
    const {columnNumber, className = ''} = linkifyURLOptions;
    if (sourceURL) {
      fallbackAnchor = Linkifier.linkifyURL(sourceURL, linkifyURLOptions);
    }
    if (!target || target.isDisposed()) {
      return fallbackAnchor;
    }
    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
    if (!debuggerModel) {
      return fallbackAnchor;
    }

    let rawLocation;
    if (scriptId) {
      rawLocation = debuggerModel.createRawLocationByScriptId(
          scriptId, lineNumber || 0, columnNumber, linkifyURLOptions.inlineFrameIndex);
    }
    // The function createRawLocationByScriptId will always return a raw location. Normally
    // we rely on the live location that is created from it to update missing information
    // to create the link. If we, however, already have a similar script with the same source url,
    // use that one.
    if (!rawLocation?.script()) {
      rawLocation = debuggerModel.createRawLocationByURL(
                        sourceURL, lineNumber || 0, columnNumber, linkifyURLOptions.inlineFrameIndex) ||
          rawLocation;
    }

    if (!rawLocation) {
      return fallbackAnchor;
    }

    const createLinkOptions = {
      maxLength: undefined,
      title: undefined,
      href: undefined,
      preventClick: undefined,
      bypassURLTrimming: undefined,
      tabStop: options ? options.tabStop : undefined,
    };
    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const anchor = Linkifier._createLink(
        fallbackAnchor && fallbackAnchor.textContent ? fallbackAnchor.textContent : '\u200b', className,
        createLinkOptions);
    const info = Linkifier.linkInfo(anchor);
    if (!info) {
      return fallbackAnchor;
    }
    info.enableDecorator = this._useLinkDecorator;
    info.fallback = fallbackAnchor;

    const pool = this._locationPoolByTarget.get(rawLocation.debuggerModel.target());
    if (!pool) {
      return fallbackAnchor;
    }
    const currentOnLiveLocationUpdate = this._onLiveLocationUpdate;
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance()
        .createLiveLocation(rawLocation, this._updateAnchor.bind(this, anchor), pool)
        .then(liveLocation => {
          if (liveLocation) {
            info.liveLocation = liveLocation;
            currentOnLiveLocationUpdate();
          }
        });

    const anchors = /** @type {!Array<!Element>} */ (this._anchorsByTarget.get(rawLocation.debuggerModel.target()));
    anchors.push(anchor);
    return anchor;
  }

  /**
   * @param {?SDK.SDKModel.Target} target
   * @param {?string} scriptId
   * @param {string} sourceURL
   * @param {number|undefined} lineNumber
   * @param {!LinkifyOptions=} options
   * @return {!HTMLElement}
   */
  linkifyScriptLocation(target, scriptId, sourceURL, lineNumber, options) {
    const scriptLink = this.maybeLinkifyScriptLocation(target, scriptId, sourceURL, lineNumber, options);
    const linkifyURLOptions = {
      lineNumber,
      maxLength: this._maxLength,
      className: options ? options.className : undefined,
      columnNumber: options ? options.columnNumber : undefined,
      inlineFrameIndex: options ? options.inlineFrameIndex : 0,
      tabStop: options ? options.tabStop : undefined,
      text: undefined,
      preventClick: undefined,
      bypassURLTrimming: undefined
    };

    return scriptLink || Linkifier.linkifyURL(sourceURL, linkifyURLOptions);
  }

  /**
   * @param {!SDK.DebuggerModel.Location} rawLocation
   * @param {string} fallbackUrl
   * @param {string=} className
   * @return {!Element}
   */
  linkifyRawLocation(rawLocation, fallbackUrl, className) {
    return this.linkifyScriptLocation(
        rawLocation.debuggerModel.target(), rawLocation.scriptId, fallbackUrl, rawLocation.lineNumber, {
          columnNumber: rawLocation.columnNumber,
          className,
          tabStop: undefined,
          inlineFrameIndex: rawLocation.inlineFrameIndex
        });
  }

  /**
   * @param {?SDK.SDKModel.Target} target
   * @param {!Protocol.Runtime.CallFrame} callFrame
   * @param {!LinkifyOptions=} options
   * @return {?HTMLElement}
   */
  maybeLinkifyConsoleCallFrame(target, callFrame, options) {
    const linkifyOptions = {
      columnNumber: callFrame.columnNumber,
      inlineFrameIndex: options ? options.inlineFrameIndex : 0,
      tabStop: options ? options.tabStop : undefined,
      className: options ? options.className : undefined,
    };
    return this.maybeLinkifyScriptLocation(
        target, callFrame.scriptId, callFrame.url, callFrame.lineNumber, linkifyOptions);
  }

  /**
   * @param {!SDK.SDKModel.Target} target
   * @param {!Protocol.Runtime.StackTrace} stackTrace
   * @param {string=} classes
   * @return {!HTMLElement}
   */
  linkifyStackTraceTopFrame(target, stackTrace, classes) {
    console.assert(Boolean(stackTrace.callFrames) && Boolean(stackTrace.callFrames.length));

    const topFrame = stackTrace.callFrames[0];
    const fallbackAnchor = Linkifier.linkifyURL(topFrame.url, {
      className: classes,
      lineNumber: topFrame.lineNumber,
      columnNumber: topFrame.columnNumber,
      inlineFrameIndex: 0,
      maxLength: this._maxLength,
      text: undefined,
      preventClick: undefined,
      tabStop: undefined,
      bypassURLTrimming: undefined
    });
    if (target.isDisposed()) {
      return fallbackAnchor;
    }

    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel);
    if (!debuggerModel) {
      return fallbackAnchor;
    }
    const rawLocations = debuggerModel.createRawLocationsByStackTrace(stackTrace);
    if (rawLocations.length === 0) {
      return fallbackAnchor;
    }

    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const anchor = Linkifier._createLink('\u200b', classes || '');
    const info = Linkifier.linkInfo(anchor);
    if (!info) {
      return fallbackAnchor;
    }
    info.enableDecorator = this._useLinkDecorator;
    info.fallback = fallbackAnchor;

    const pool = this._locationPoolByTarget.get(target);
    if (!pool) {
      return fallbackAnchor;
    }
    const currentOnLiveLocationUpdate = this._onLiveLocationUpdate;
    Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance()
        .createStackTraceTopFrameLiveLocation(rawLocations, this._updateAnchor.bind(this, anchor), pool)
        .then(liveLocation => {
          info.liveLocation = liveLocation;
          currentOnLiveLocationUpdate();
        });

    const anchors = /** @type {!Array<!Element>} */ (this._anchorsByTarget.get(target));
    anchors.push(anchor);
    return anchor;
  }

  /**
   * @param {!SDK.CSSModel.CSSLocation} rawLocation
   * @param {string=} classes
   * @return {!Element}
   */
  linkifyCSSLocation(rawLocation, classes) {
    const createLinkOptions = {
      maxLength: undefined,
      title: undefined,
      href: undefined,
      preventClick: undefined,
      bypassURLTrimming: undefined,
      tabStop: true
    };
    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const anchor = /** @type {!HTMLElement} */ (Linkifier._createLink('\u200b', classes || '', createLinkOptions));
    const info = Linkifier.linkInfo(anchor);
    if (!info) {
      return anchor;
    }
    info.enableDecorator = this._useLinkDecorator;

    const pool = this._locationPoolByTarget.get(rawLocation.cssModel().target());
    if (!pool) {
      return anchor;
    }
    const currentOnLiveLocationUpdate = this._onLiveLocationUpdate;
    Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance()
        .createLiveLocation(rawLocation, this._updateAnchor.bind(this, anchor), pool)
        .then(liveLocation => {
          info.liveLocation = liveLocation;
          currentOnLiveLocationUpdate();
        });

    const anchors = /** @type {!Array<!Element>} */ (this._anchorsByTarget.get(rawLocation.cssModel().target()));
    anchors.push(anchor);
    return anchor;
  }

  reset() {
    // Create a copy of {keys} so {targetRemoved} can safely modify the map.
    for (const target of [...this._anchorsByTarget.keys()]) {
      this.targetRemoved(target);
      this.targetAdded(target);
    }
  }

  dispose() {
    // Create a copy of {keys} so {targetRemoved} can safely modify the map.
    for (const target of [...this._anchorsByTarget.keys()]) {
      this.targetRemoved(target);
    }
    SDK.SDKModel.TargetManager.instance().unobserveTargets(this);
    instances.delete(this);
  }

  /**
   * @param {!HTMLElement} anchor
   * @param {!Bindings.LiveLocation.LiveLocation} liveLocation
   */
  async _updateAnchor(anchor, liveLocation) {
    Linkifier._unbindUILocation(anchor);
    const uiLocation = await liveLocation.uiLocation();
    if (!uiLocation) {
      if (liveLocation instanceof Bindings.CSSWorkspaceBinding.LiveLocation) {
        const header = (/** @type {!Bindings.CSSWorkspaceBinding.LiveLocation} */ (liveLocation)).header();
        if (header && header.ownerNode) {
          anchor.addEventListener('click', event => {
            event.consume(true);
            Common.Revealer.reveal(header.ownerNode || null);
          }, false);
          Linkifier._setTrimmedText(anchor, '<style>');
        }
      }
      return;
    }

    Linkifier._bindUILocation(anchor, uiLocation);
    const text = uiLocation.linkText(true /* skipTrim */);
    Linkifier._setTrimmedText(anchor, text, this._maxLength);

    let titleText = uiLocation.uiSourceCode.url();
    if (uiLocation.uiSourceCode.mimeType() === 'application/wasm') {
      // For WebAssembly locations, we follow the conventions described in
      // github.com/WebAssembly/design/blob/master/Web.md#developer-facing-display-conventions
      if (typeof uiLocation.columnNumber === 'number') {
        titleText += `:0x${uiLocation.columnNumber.toString(16)}`;
      }
    } else if (typeof uiLocation.lineNumber === 'number') {
      titleText += ':' + (uiLocation.lineNumber + 1);
    }
    UI.Tooltip.Tooltip.install(anchor, titleText);
    anchor.classList.toggle('ignore-list-link', await liveLocation.isIgnoreListed());
    Linkifier._updateLinkDecorations(anchor);
  }

  /**
   * @param {function():void} callback
   */
  setLiveLocationUpdateCallback(callback) {
    this._onLiveLocationUpdate = callback;
  }

  /**
   * @param {!Element} anchor
   */
  static _updateLinkDecorations(anchor) {
    const info = Linkifier.linkInfo(anchor);
    if (!info || !info.enableDecorator) {
      return;
    }
    if (!decorator || !info.uiLocation) {
      return;
    }
    if (info.icon && info.icon.parentElement) {
      anchor.removeChild(info.icon);
    }
    const icon = decorator.linkIcon(info.uiLocation.uiSourceCode);
    if (icon) {
      icon.style.setProperty('margin-right', '2px');
      anchor.insertBefore(icon, anchor.firstChild);
    }
    info.icon = icon;
  }

  /**
   * @param {string} url
   * @param  {!LinkifyURLOptions=} options
   * @return {!HTMLElement}
   */
  static linkifyURL(url, options) {
    options = options || {
      text: undefined,
      className: undefined,
      lineNumber: undefined,
      columnNumber: undefined,
      inlineFrameIndex: 0,
      preventClick: undefined,
      maxLength: undefined,
      tabStop: undefined,
      bypassURLTrimming: undefined
    };
    const text = options.text;
    const className = options.className || '';
    const lineNumber = options.lineNumber;
    const columnNumber = options.columnNumber;
    const preventClick = options.preventClick;
    const maxLength = options.maxLength || UI.UIUtils.MaxLengthForDisplayedURLs;
    const bypassURLTrimming = options.bypassURLTrimming;
    if (!url || url.trim().toLowerCase().startsWith('javascript:')) {
      const element = /** @type {!HTMLElement} */ (document.createElement('span'));
      if (className) {
        element.className = className;
      }
      element.textContent = text || url || i18nString(UIStrings.unknown);
      return element;
    }

    let linkText = text || Bindings.ResourceUtils.displayNameForURL(url);
    if (typeof lineNumber === 'number' && !text) {
      linkText += ':' + (lineNumber + 1);
    }
    const title = linkText !== url ? url : '';
    const linkOptions = {maxLength, title, href: url, preventClick, tabStop: options.tabStop, bypassURLTrimming};
    const link = Linkifier._createLink(linkText, className, linkOptions);
    const info = Linkifier.linkInfo(link);
    if (!info) {
      return link;
    }
    if (lineNumber) {
      info.lineNumber = lineNumber;
    }
    if (columnNumber) {
      info.columnNumber = columnNumber;
    }
    return link;
  }

  /**
   * @param {!Object} revealable
   * @param {string|!HTMLElement} text
   * @param {string=} fallbackHref
   * @param {string=} title
   * @param {string=} className
   * @return {!HTMLElement}
   */
  static linkifyRevealable(revealable, text, fallbackHref, title, className) {
    const createLinkOptions = {
      maxLength: UI.UIUtils.MaxLengthForDisplayedURLs,
      href: fallbackHref,
      title,
      preventClick: undefined,
      tabStop: undefined,
      bypassURLTrimming: undefined
    };
    const link = Linkifier._createLink(text, className || '', createLinkOptions);
    const linkInfo = Linkifier.linkInfo(link);
    if (!linkInfo) {
      return link;
    }
    linkInfo.revealable = revealable;
    return link;
  }

  /**
   * @param {string|!HTMLElement} text
   * @param {string} className
   * @param {!_CreateLinkOptions=} options
   * @returns {!HTMLElement}
   */
  static _createLink(text, className, options) {
    options = options || {
      maxLength: undefined,
      title: undefined,
      href: undefined,
      preventClick: undefined,
      tabStop: undefined,
      bypassURLTrimming: undefined
    };
    const {maxLength, title, href, preventClick, tabStop, bypassURLTrimming} = options;
    const link = /** @type {!HTMLElement}*/ (document.createElement('span'));
    if (className) {
      link.className = className;
    }
    link.classList.add('devtools-link');
    if (title) {
      UI.Tooltip.Tooltip.install(link, title);
    }
    if (href) {
      // @ts-ignore
      link.href = href;
    }

    if (text instanceof HTMLElement) {
      link.appendChild(text);
    } else {
      if (bypassURLTrimming) {
        link.classList.add('devtools-link-styled-trim');
        Linkifier._appendTextWithoutHashes(link, text);
      } else {
        Linkifier._setTrimmedText(link, text, maxLength);
      }
    }

    const linkInfo = {
      icon: null,
      enableDecorator: false,
      uiLocation: null,
      liveLocation: null,
      url: href || null,
      lineNumber: null,
      columnNumber: null,
      inlineFrameIndex: 0,
      revealable: null,
      fallback: null
    };
    infoByAnchor.set(link, linkInfo);
    if (!preventClick) {
      link.addEventListener('click', event => {
        if (Linkifier._handleClick(event)) {
          event.consume(true);
        }
      }, false);
      link.addEventListener('keydown', event => {
        if (event.key === 'Enter' && Linkifier._handleClick(event)) {
          event.consume(true);
        }
      }, false);
    } else {
      link.classList.add('devtools-link-prevent-click');
    }
    UI.ARIAUtils.markAsLink(link);
    link.tabIndex = tabStop ? 0 : -1;
    return link;
  }

  /**
   * @param {!Element} link
   * @param {string} text
   * @param {number=} maxLength
   */
  static _setTrimmedText(link, text, maxLength) {
    link.removeChildren();
    if (maxLength && text.length > maxLength) {
      const middleSplit = splitMiddle(text, maxLength);
      Linkifier._appendTextWithoutHashes(link, middleSplit[0]);
      Linkifier._appendHiddenText(link, middleSplit[1]);
      Linkifier._appendTextWithoutHashes(link, middleSplit[2]);
    } else {
      Linkifier._appendTextWithoutHashes(link, text);
    }

    /**
     * @param {string} string
     * @param {number} maxLength
     * @return {!Array<string>}
     */
    function splitMiddle(string, maxLength) {
      let leftIndex = Math.floor(maxLength / 2);
      let rightIndex = string.length - Math.ceil(maxLength / 2) + 1;

      const codePointAtRightIndex = string.codePointAt(rightIndex - 1);
      // Do not truncate between characters that use multiple code points (emojis).
      if (typeof codePointAtRightIndex !== 'undefined' && codePointAtRightIndex >= 0x10000) {
        rightIndex++;
        leftIndex++;
      }
      const codePointAtLeftIndex = string.codePointAt(leftIndex - 1);
      if (typeof codePointAtLeftIndex !== 'undefined' && leftIndex > 0 && codePointAtLeftIndex >= 0x10000) {
        leftIndex--;
      }
      return [string.substring(0, leftIndex), string.substring(leftIndex, rightIndex), string.substring(rightIndex)];
    }
  }

  /**
   * @param {!Element} link
   * @param {string} string
   */
  static _appendTextWithoutHashes(link, string) {
    const hashSplit = TextUtils.TextUtils.Utils.splitStringByRegexes(string, [/[a-f0-9]{20,}/g]);
    for (const match of hashSplit) {
      if (match.regexIndex === -1) {
        UI.UIUtils.createTextChild(link, match.value);
      } else {
        UI.UIUtils.createTextChild(link, match.value.substring(0, 7));
        Linkifier._appendHiddenText(link, match.value.substring(7));
      }
    }
  }

  /**
   * @param {!Element} link
   * @param {string} string
   */
  static _appendHiddenText(link, string) {
    const ellipsisNode = UI.UIUtils.createTextChild(link.createChild('span', 'devtools-link-ellipsis'), '…');
    textByAnchor.set(ellipsisNode, string);
  }

  /**
   * @param {!Node} node
   * @return {string}
   */
  static untruncatedNodeText(node) {
    return textByAnchor.get(node) || node.textContent || '';
  }

  /**
   * @param {?Element} link
   * @return {?_LinkInfo}
   */
  static linkInfo(link) {
    return /** @type {?_LinkInfo} */ (link ? infoByAnchor.get(link) || null : null);
  }

  /**
   * @param {!Event} event
   * @return {boolean}
   */
  static _handleClick(event) {
    const link = /** @type {!Element} */ (event.currentTarget);
    if (UI.UIUtils.isBeingEdited(/** @type {!Node} */ (event.target)) || link.hasSelection()) {
      return false;
    }
    const linkInfo = Linkifier.linkInfo(link);
    if (!linkInfo) {
      return false;
    }
    return Linkifier.invokeFirstAction(linkInfo);
  }

  /**
   *
   * @param {!_LinkInfo} linkInfo
   */
  static _handleClickFromNewComponentLand(linkInfo) {
    Linkifier.invokeFirstAction(linkInfo);
  }

  /**
   * @param {!_LinkInfo} linkInfo
   * @return {boolean}
   */
  static invokeFirstAction(linkInfo) {
    const actions = Linkifier._linkActions(linkInfo);
    if (actions.length) {
      actions[0].handler.call(null);
      return true;
    }
    return false;
  }

  /**
   * @return {!Common.Settings.Setting<string>}
   */
  static _linkHandlerSetting() {
    if (!linkHandlerSettingInstance) {
      linkHandlerSettingInstance =
          Common.Settings.Settings.instance().createSetting('openLinkHandler', i18nString(UIStrings.auto));
    }
    return linkHandlerSettingInstance;
  }

  /**
   * @param {string} title
   * @param {!LinkHandler} handler
   */
  static registerLinkHandler(title, handler) {
    linkHandlers.set(title, handler);
    LinkHandlerSettingUI.instance()._update();
  }

  /**
   * @param {string} title
   */
  static unregisterLinkHandler(title) {
    linkHandlers.delete(title);
    LinkHandlerSettingUI.instance()._update();
  }

  /**
   * @param {!Element} link
   * @return {?Workspace.UISourceCode.UILocation}
   */
  static uiLocation(link) {
    const info = Linkifier.linkInfo(link);
    return info ? info.uiLocation : null;
  }

  /**
   * @param {!_LinkInfo} info
   * @return {!Array<{section: string, title: string, handler: function()}>}
   */
  static _linkActions(info) {
    /** @type {!Array<{section: string, title: string, handler: function():*}>} */
    const result = [];

    if (!info) {
      return result;
    }

    let url = '';
    let uiLocation = null;
    if (info.uiLocation) {
      uiLocation = info.uiLocation;
      url = uiLocation.uiSourceCode.contentURL();
    } else if (info.url) {
      url = info.url;
      const uiSourceCode = Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL(url) ||
          Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL(
              Common.ParsedURL.ParsedURL.urlWithoutHash(url));
      uiLocation = uiSourceCode ? uiSourceCode.uiLocation(info.lineNumber || 0, info.columnNumber || 0) : null;
    }
    const resource = url ? Bindings.ResourceUtils.resourceForURL(url) : null;
    const contentProvider = uiLocation ? uiLocation.uiSourceCode : resource;

    const revealable = info.revealable || uiLocation || resource;
    if (revealable) {
      const destination = Common.Revealer.revealDestination(revealable);
      result.push({
        section: 'reveal',
        title: destination ? i18nString(UIStrings.revealInS, {PH1: destination}) : i18nString(UIStrings.reveal),
        handler: () => Common.Revealer.reveal(revealable)
      });
    }
    if (contentProvider) {
      const lineNumber = uiLocation ? uiLocation.lineNumber : info.lineNumber || 0;
      for (const title of linkHandlers.keys()) {
        const handler = linkHandlers.get(title);
        if (!handler) {
          continue;
        }
        const action = {
          section: 'reveal',
          title: i18nString(UIStrings.openUsingS, {PH1: title}),
          handler: handler.bind(null, contentProvider, lineNumber)
        };
        if (title === Linkifier._linkHandlerSetting().get()) {
          result.unshift(action);
        } else {
          result.push(action);
        }
      }
    }
    if (resource || info.url) {
      result.push({
        section: 'reveal',
        title: UI.UIUtils.openLinkExternallyLabel(),
        handler: () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.openInNewTab(url)
      });
      result.push({
        section: 'clipboard',
        title: UI.UIUtils.copyLinkAddressLabel(),
        handler: () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(url)
      });
    }

    if (uiLocation && uiLocation.uiSourceCode) {
      const contentProvider = /** @type {!Workspace.UISourceCode.UISourceCode} */ uiLocation.uiSourceCode;
      result.push({
        section: 'clipboard',
        title: UI.UIUtils.copyFileNameLabel(),
        handler: () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(contentProvider.displayName())
      });
    }

    return result;
  }
}

/**
 * @extends {Common.EventTarget.EventTarget}
 * @interface
 */
export class LinkDecorator extends Common.EventTarget.EventTarget {
  /**
   * @param {!Workspace.UISourceCode.UISourceCode} uiSourceCode
   * @return {?UI.Icon.Icon}
   */
  linkIcon(uiSourceCode) {
    throw new Error('Not implemented yet');
  }
}

LinkDecorator.Events = {
  LinkIconChanged: Symbol('LinkIconChanged')
};

/**
 * @type {LinkContextMenuProvider}
 */
let linkContextMenuProviderInstance;

/**
 * @implements {UI.ContextMenu.Provider}
 */
export class LinkContextMenuProvider {
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!linkContextMenuProviderInstance || forceNew) {
      linkContextMenuProviderInstance = new LinkContextMenuProvider();
    }

    return linkContextMenuProviderInstance;
  }
  /**
   * @override
   * @param {!Event} event
   * @param {!UI.ContextMenu.ContextMenu} contextMenu
   * @param {!Object} target
   */
  appendApplicableItems(event, contextMenu, target) {
    let targetNode = /** @type {?Node} */ (target);
    while (targetNode && !infoByAnchor.get(targetNode)) {
      targetNode = targetNode.parentNodeOrShadowHost();
    }
    const link = /** @type {?Element} */ (targetNode);
    const linkInfo = Linkifier.linkInfo(link);
    if (!linkInfo) {
      return;
    }

    const actions = Linkifier._linkActions(linkInfo);
    for (const action of actions) {
      contextMenu.section(action.section).appendItem(action.title, action.handler);
    }
  }
}

/** @type {!LinkHandlerSettingUI} */
let linkHandlerSettingUIInstance;

/**
 * @implements {UI.SettingsUI.SettingUI}
 */
export class LinkHandlerSettingUI {
  /**
   * @private
   */
  constructor() {
    this._element = document.createElement('select');
    this._element.classList.add('chrome-select');
    this._element.addEventListener('change', this._onChange.bind(this), false);
    this._update();
  }

  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!linkHandlerSettingUIInstance || forceNew) {
      linkHandlerSettingUIInstance = new LinkHandlerSettingUI();
    }

    return linkHandlerSettingUIInstance;
  }

  _update() {
    this._element.removeChildren();
    const names = [...linkHandlers.keys()];
    names.unshift(i18nString(UIStrings.auto));
    for (const name of names) {
      const option = document.createElement('option');
      option.textContent = name;
      option.selected = name === Linkifier._linkHandlerSetting().get();
      this._element.appendChild(option);
    }
    this._element.disabled = names.length <= 1;
  }

  /**
   * @param {!Event} event
   */
  _onChange(event) {
    if (!event.target) {
      return;
    }
    const value = (/** @type {!HTMLSelectElement} */ (event.target)).value;
    Linkifier._linkHandlerSetting().set(value);
  }

  /**
   * @override
   * @return {?Element}
   */
  settingElement() {
    return UI.SettingsUI.createCustomSetting(i18nString(UIStrings.linkHandling), this._element);
  }
}

let listeningToNewEvents = false;
function listenForNewComponentLinkifierEvents() {
  if (listeningToNewEvents) {
    return;
  }

  listeningToNewEvents = true;

  window.addEventListener(
      'linkifier-activated',
      /**
   *
   * @param {!Event} event
   */
      function(event) {
        const unknownEvent = /** @type {?} */ (event);
        const eventWithData = /** @type {!{data: !_LinkInfo}} */ (unknownEvent);
        Linkifier._handleClickFromNewComponentLand(eventWithData.data);
      });
}

listenForNewComponentLinkifierEvents();

/**
 * @type {ContentProviderContextMenuProvider}
 */
let contentProviderContextMenuProviderInstance;

/**
 * @implements {UI.ContextMenu.Provider}
 */
export class ContentProviderContextMenuProvider {
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!contentProviderContextMenuProviderInstance || forceNew) {
      contentProviderContextMenuProviderInstance = new ContentProviderContextMenuProvider();
    }

    return contentProviderContextMenuProviderInstance;
  }

  /**
   * @override
   * @param {!Event} event
   * @param {!UI.ContextMenu.ContextMenu} contextMenu
   * @param {!Object} target
   */
  appendApplicableItems(event, contextMenu, target) {
    const contentProvider = /** @type {!Workspace.UISourceCode.UISourceCode} */ (target);
    const contentUrl = contentProvider.contentURL();
    if (!contentUrl) {
      return;
    }

    contextMenu.revealSection().appendItem(
        UI.UIUtils.openLinkExternallyLabel(),
        () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.openInNewTab(
            contentUrl.endsWith(':formatted') ? contentUrl.slice(0, contentUrl.lastIndexOf(':')) : contentUrl));
    for (const title of linkHandlers.keys()) {
      const handler = linkHandlers.get(title);
      if (!handler) {
        continue;
      }
      contextMenu.revealSection().appendItem(
          i18nString(UIStrings.openUsingS, {PH1: title}), handler.bind(null, contentProvider, 0));
    }
    if (contentProvider instanceof SDK.NetworkRequest.NetworkRequest) {
      return;
    }

    contextMenu.clipboardSection().appendItem(
        UI.UIUtils.copyLinkAddressLabel(),
        () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(contentUrl));

    contextMenu.clipboardSection().appendItem(
        UI.UIUtils.copyFileNameLabel(),
        () => Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(contentProvider.displayName()));
  }
}

/**
 * @typedef {{
 *     icon: ?UI.Icon.Icon,
 *     enableDecorator: boolean,
 *     uiLocation: ?Workspace.UISourceCode.UILocation,
 *     liveLocation: ?Bindings.LiveLocation.LiveLocation,
 *     url: ?string,
 *     lineNumber: ?number,
 *     columnNumber: ?number,
 *     inlineFrameIndex: number,
 *     revealable: ?Object,
 *     fallback: ?Element
 * }}
 */
// @ts-ignore typedef
export let _LinkInfo;

/**
 * @typedef {{
 *     text: (string|undefined),
 *     className: (string|undefined),
 *     lineNumber: (number|undefined),
 *     columnNumber: (number|undefined),
 *     inlineFrameIndex: number,
 *     preventClick: (boolean|undefined),
 *     maxLength: (number|undefined),
 *     tabStop: (boolean|undefined),
 *     bypassURLTrimming: (boolean|undefined)
 * }}
 */
// @ts-ignore typedef
export let LinkifyURLOptions;

/**
 * @typedef {{
 *     className: (string|undefined),
 *     columnNumber: (number|undefined),
 *     inlineFrameIndex: number,
 *     tabStop: (boolean|undefined)
 * }}
 */
// @ts-ignore typedef
export let LinkifyOptions;

/**
 * @typedef {{
 *     maxLength: (number|undefined),
 *     title: (string|undefined),
 *     href: (string|undefined),
 *     preventClick: (boolean|undefined),
 *     tabStop: (boolean|undefined),
 *     bypassURLTrimming: (boolean|undefined)
 * }}
 */
// @ts-ignore typedef
export let _CreateLinkOptions;

/**
 * @typedef {function(!TextUtils.ContentProvider.ContentProvider, number):void}
 */
// @ts-ignore typedef
export let LinkHandler;
