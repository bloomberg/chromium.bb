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

import * as Common from '../../../../core/common/common.js';
import * as Host from '../../../../core/host/host.js';
import * as i18n from '../../../../core/i18n/i18n.js';
import * as SDK from '../../../../core/sdk/sdk.js';
import * as Bindings from '../../../../models/bindings/bindings.js';
import * as TextUtils from '../../../../models/text_utils/text_utils.js';
import * as Workspace from '../../../../models/workspace/workspace.js';
import type * as Protocol from '../../../../generated/protocol.js';
import * as UI from '../../legacy.js';

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
const str_ = i18n.i18n.registerUIStrings('ui/legacy/components/utils/Linkifier.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
const instances = new Set<Linkifier>();

let decorator: LinkDecorator|null = null;

const anchorsByUISourceCode = new WeakMap<Workspace.UISourceCode.UISourceCode, Set<Element>>();

const infoByAnchor = new WeakMap<Node, _LinkInfo>();

const textByAnchor = new WeakMap<Node, string>();

const linkHandlers = new Map<string, LinkHandler>();

let linkHandlerSettingInstance: Common.Settings.Setting<string>;

export class Linkifier implements SDK.TargetManager.Observer {
  private readonly maxLength: number;
  private readonly anchorsByTarget: Map<SDK.Target.Target, Element[]>;
  private readonly locationPoolByTarget: Map<SDK.Target.Target, Bindings.LiveLocation.LiveLocationPool>;
  private onLiveLocationUpdate: (() => void)|undefined;
  private useLinkDecorator: boolean;

  constructor(
      maxLengthForDisplayedURLs?: number, useLinkDecorator?: boolean,
      onLiveLocationUpdate: (() => void) = (): void => {}) {
    this.maxLength = maxLengthForDisplayedURLs || UI.UIUtils.MaxLengthForDisplayedURLs;
    this.anchorsByTarget = new Map();
    this.locationPoolByTarget = new Map();
    this.onLiveLocationUpdate = onLiveLocationUpdate;
    this.useLinkDecorator = Boolean(useLinkDecorator);
    instances.add(this);
    SDK.TargetManager.TargetManager.instance().observeTargets(this);
  }

  static setLinkDecorator(linkDecorator: LinkDecorator): void {
    console.assert(!decorator, 'Cannot re-register link decorator.');
    decorator = linkDecorator;
    linkDecorator.addEventListener(LinkDecorator.Events.LinkIconChanged, onLinkIconChanged);
    for (const linkifier of instances) {
      linkifier.updateAllAnchorDecorations();
    }

    function onLinkIconChanged(event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>): void {
      const uiSourceCode = event.data;
      const links = anchorsByUISourceCode.get(uiSourceCode) || [];
      for (const link of links) {
        Linkifier.updateLinkDecorations(link);
      }
    }
  }

  private updateAllAnchorDecorations(): void {
    for (const anchors of this.anchorsByTarget.values()) {
      for (const anchor of anchors) {
        Linkifier.updateLinkDecorations(anchor);
      }
    }
  }

  private static bindUILocation(anchor: Element, uiLocation: Workspace.UISourceCode.UILocation): void {
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

  private static unbindUILocation(anchor: Element): void {
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

  targetAdded(target: SDK.Target.Target): void {
    this.anchorsByTarget.set(target, []);
    this.locationPoolByTarget.set(target, new Bindings.LiveLocation.LiveLocationPool());
  }

  targetRemoved(target: SDK.Target.Target): void {
    const locationPool = this.locationPoolByTarget.get(target);
    this.locationPoolByTarget.delete(target);
    if (!locationPool) {
      return;
    }
    locationPool.disposeAll();
    const anchors = (this.anchorsByTarget.get(target) as HTMLElement[] | null);
    if (!anchors) {
      return;
    }
    this.anchorsByTarget.delete(target);
    for (const anchor of anchors) {
      const info = Linkifier.linkInfo(anchor);
      if (!info) {
        continue;
      }
      info.liveLocation = null;
      Linkifier.unbindUILocation(anchor);
      const fallback = info.fallback;
      if (fallback) {
        anchor.replaceWith(fallback);
      }
    }
  }

  maybeLinkifyScriptLocation(
      target: SDK.Target.Target|null, scriptId: Protocol.Runtime.ScriptId|null, sourceURL: string,
      lineNumber: number|undefined, options?: LinkifyOptions): HTMLElement|null {
    let fallbackAnchor: HTMLElement|null = null;
    const linkifyURLOptions: LinkifyURLOptions = {
      lineNumber,
      maxLength: this.maxLength,
      columnNumber: options?.columnNumber,
      showColumnNumber: Boolean(options?.showColumnNumber),
      className: options?.className,
      tabStop: options?.tabStop,
      inlineFrameIndex: options?.inlineFrameIndex ?? 0,
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

    // Prefer createRawLocationByScriptId() here, since it will always produce a correct
    // link, since the script ID is unique. Only fall back to createRawLocationByURL()
    // when all we have is an URL, which is not guaranteed to be unique.
    const rawLocation = scriptId ? debuggerModel.createRawLocationByScriptId(
                                       scriptId, lineNumber || 0, columnNumber, linkifyURLOptions.inlineFrameIndex) :
                                   debuggerModel.createRawLocationByURL(
                                       sourceURL, lineNumber || 0, columnNumber, linkifyURLOptions.inlineFrameIndex);
    if (!rawLocation) {
      return fallbackAnchor;
    }

    const createLinkOptions: _CreateLinkOptions = {
      tabStop: options?.tabStop,
    };
    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const {link, linkInfo} = Linkifier.createLink(
        fallbackAnchor && fallbackAnchor.textContent ? fallbackAnchor.textContent : '\u200b', className,
        createLinkOptions);
    linkInfo.enableDecorator = this.useLinkDecorator;
    linkInfo.fallback = fallbackAnchor;

    const pool = this.locationPoolByTarget.get(rawLocation.debuggerModel.target());
    if (!pool) {
      return fallbackAnchor;
    }

    const linkDisplayOptions = {showColumnNumber: linkifyURLOptions.showColumnNumber};

    const currentOnLiveLocationUpdate = this.onLiveLocationUpdate;
    void Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance()
        .createLiveLocation(rawLocation, this.updateAnchor.bind(this, link, linkDisplayOptions), pool)
        .then(liveLocation => {
          if (liveLocation) {
            linkInfo.liveLocation = liveLocation;
            // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
            // @ts-expect-error
            currentOnLiveLocationUpdate();
          }
        });

    const anchors = (this.anchorsByTarget.get(rawLocation.debuggerModel.target()) as Element[]);
    anchors.push(link);
    return link;
  }

  linkifyScriptLocation(
      target: SDK.Target.Target|null, scriptId: Protocol.Runtime.ScriptId|null, sourceURL: string,
      lineNumber: number|undefined, options?: LinkifyOptions): HTMLElement {
    const scriptLink = this.maybeLinkifyScriptLocation(target, scriptId, sourceURL, lineNumber, options);
    const linkifyURLOptions: LinkifyURLOptions = {
      lineNumber,
      maxLength: this.maxLength,
      className: options?.className,
      columnNumber: options?.columnNumber,
      showColumnNumber: Boolean(options?.showColumnNumber),
      inlineFrameIndex: options?.inlineFrameIndex ?? 0,
      tabStop: options?.tabStop,
    };

    return scriptLink || Linkifier.linkifyURL(sourceURL, linkifyURLOptions);
  }

  linkifyRawLocation(rawLocation: SDK.DebuggerModel.Location, fallbackUrl: string, className?: string): Element {
    return this.linkifyScriptLocation(
        rawLocation.debuggerModel.target(), rawLocation.scriptId, fallbackUrl, rawLocation.lineNumber, {
          columnNumber: rawLocation.columnNumber,
          className,
          inlineFrameIndex: rawLocation.inlineFrameIndex,
        });
  }

  maybeLinkifyConsoleCallFrame(
      target: SDK.Target.Target|null, callFrame: Protocol.Runtime.CallFrame, options?: LinkifyOptions): HTMLElement
      |null {
    const linkifyOptions: LinkifyOptions = {
      columnNumber: callFrame.columnNumber,
      showColumnNumber: Boolean(options?.showColumnNumber),
      inlineFrameIndex: options?.inlineFrameIndex ?? 0,
      tabStop: options?.tabStop,
      className: options?.className,
    };
    return this.maybeLinkifyScriptLocation(
        target, callFrame.scriptId, callFrame.url, callFrame.lineNumber, linkifyOptions);
  }

  linkifyStackTraceTopFrame(target: SDK.Target.Target, stackTrace: Protocol.Runtime.StackTrace, className?: string):
      HTMLElement {
    console.assert(stackTrace.callFrames.length > 0);

    const {url, lineNumber, columnNumber} = stackTrace.callFrames[0];
    const fallbackAnchor = Linkifier.linkifyURL(url, {
      className,
      lineNumber,
      columnNumber,
      showColumnNumber: false,
      inlineFrameIndex: 0,
      maxLength: this.maxLength,
      preventClick: true,
    });

    // The contract is that disposed targets don't have a LiveLocationPool
    // associated, whereas all active targets have one such pool. This ensures
    // that the fallbackAnchor is only ever used when the target was disposed.
    const pool = this.locationPoolByTarget.get(target);
    if (!pool) {
      console.assert(target.isDisposed());
      return fallbackAnchor;
    }
    console.assert(!target.isDisposed());

    // All targets that can report stack traces also have a debugger model.
    const debuggerModel = target.model(SDK.DebuggerModel.DebuggerModel) as SDK.DebuggerModel.DebuggerModel;

    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const {link, linkInfo} = Linkifier.createLink('\u200b', className ?? '');
    linkInfo.enableDecorator = this.useLinkDecorator;
    linkInfo.fallback = fallbackAnchor;

    const linkDisplayOptions = {showColumnNumber: false};

    const currentOnLiveLocationUpdate = this.onLiveLocationUpdate;
    void Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance()
        .createStackTraceTopFrameLiveLocation(
            debuggerModel.createRawLocationsByStackTrace(stackTrace),
            this.updateAnchor.bind(this, link, linkDisplayOptions), pool)
        .then(liveLocation => {
          linkInfo.liveLocation = liveLocation;
          // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
          // @ts-expect-error
          currentOnLiveLocationUpdate();
        });

    const anchors = (this.anchorsByTarget.get(target) as Element[]);
    anchors.push(link);
    return link;
  }

  linkifyCSSLocation(rawLocation: SDK.CSSModel.CSSLocation, classes?: string): Element {
    const createLinkOptions: _CreateLinkOptions = {
      tabStop: true,
    };
    // Not initialising the anchor element with 'zero width space' (\u200b) causes a crash
    // in the layout engine.
    // TODO(szuend): Remove comment and workaround once the crash is fixed.
    const {link, linkInfo} = Linkifier.createLink('\u200b', classes || '', createLinkOptions);
    linkInfo.enableDecorator = this.useLinkDecorator;

    const pool = this.locationPoolByTarget.get(rawLocation.cssModel().target());
    if (!pool) {
      return link;
    }

    const linkDisplayOptions = {showColumnNumber: false};

    const currentOnLiveLocationUpdate = this.onLiveLocationUpdate;
    void Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance()
        .createLiveLocation(rawLocation, this.updateAnchor.bind(this, link, linkDisplayOptions), pool)
        .then(liveLocation => {
          linkInfo.liveLocation = liveLocation;
          // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
          // @ts-expect-error
          currentOnLiveLocationUpdate();
        });

    const anchors = (this.anchorsByTarget.get(rawLocation.cssModel().target()) as Element[]);
    anchors.push(link);
    return link;
  }

  reset(): void {
    // Create a copy of {keys} so {targetRemoved} can safely modify the map.
    for (const target of [...this.anchorsByTarget.keys()]) {
      this.targetRemoved(target);
      this.targetAdded(target);
    }
  }

  dispose(): void {
    // Create a copy of {keys} so {targetRemoved} can safely modify the map.
    for (const target of [...this.anchorsByTarget.keys()]) {
      this.targetRemoved(target);
    }
    SDK.TargetManager.TargetManager.instance().unobserveTargets(this);
    instances.delete(this);
  }

  private async updateAnchor(
      anchor: HTMLElement, options: LinkDisplayOptions,
      liveLocation: Bindings.LiveLocation.LiveLocation): Promise<void> {
    Linkifier.unbindUILocation(anchor);
    const uiLocation = await liveLocation.uiLocation();
    if (!uiLocation) {
      if (liveLocation instanceof Bindings.CSSWorkspaceBinding.LiveLocation) {
        const header = (liveLocation as Bindings.CSSWorkspaceBinding.LiveLocation).header();
        if (header && header.ownerNode) {
          anchor.addEventListener('click', event => {
            event.consume(true);
            void Common.Revealer.reveal(header.ownerNode || null);
          }, false);
          // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
          // This workaround is needed to make stylelint happy
          Linkifier.setTrimmedText(
              anchor,
              '<' +
                  'style>');
        }
      }
      return;
    }

    Linkifier.bindUILocation(anchor, uiLocation);
    const text = uiLocation.linkText(true /* skipTrim */, options.showColumnNumber);
    Linkifier.setTrimmedText(anchor, text, this.maxLength);

    let titleText: string = uiLocation.uiSourceCode.url();
    if (uiLocation.uiSourceCode.mimeType() === 'application/wasm') {
      // For WebAssembly locations, we follow the conventions described in
      // github.com/WebAssembly/design/blob/master/Web.md#developer-facing-display-conventions
      if (typeof uiLocation.columnNumber === 'number') {
        titleText += `:0x${uiLocation.columnNumber.toString(16)}`;
      }
    } else {
      titleText += ':' + (uiLocation.lineNumber + 1);
      if (options.showColumnNumber && typeof uiLocation.columnNumber === 'number') {
        titleText += ':' + (uiLocation.columnNumber + 1);
      }
    }
    UI.Tooltip.Tooltip.install(anchor, titleText);
    anchor.classList.toggle('ignore-list-link', await liveLocation.isIgnoreListed());
    Linkifier.updateLinkDecorations(anchor);
  }

  setLiveLocationUpdateCallback(callback: () => void): void {
    this.onLiveLocationUpdate = callback;
  }

  private static updateLinkDecorations(anchor: Element): void {
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

  static linkifyURL(url: string, options?: LinkifyURLOptions): HTMLElement {
    options = options || {
      showColumnNumber: false,
      inlineFrameIndex: 0,
    };
    const text = options.text;
    const className = options.className || '';
    const lineNumber = options.lineNumber;
    const columnNumber = options.columnNumber;
    const showColumnNumber = options.showColumnNumber;
    const preventClick = options.preventClick;
    const maxLength = options.maxLength || UI.UIUtils.MaxLengthForDisplayedURLs;
    const bypassURLTrimming = options.bypassURLTrimming;
    if (!url || url.trim().toLowerCase().startsWith('javascript:')) {
      const element = document.createElement('span');
      if (className) {
        element.className = className;
      }
      element.textContent = text || url || i18nString(UIStrings.unknown);
      return element;
    }

    let linkText = text || Bindings.ResourceUtils.displayNameForURL(url);
    if (typeof lineNumber === 'number' && !text) {
      linkText += ':' + (lineNumber + 1);
      if (showColumnNumber && typeof columnNumber === 'number') {
        linkText += ':' + (columnNumber + 1);
      }
    }
    const title = linkText !== url ? url : '';
    const linkOptions = {maxLength, title, href: url, preventClick, tabStop: options.tabStop, bypassURLTrimming};
    const {link, linkInfo} = Linkifier.createLink(linkText, className, linkOptions);
    if (lineNumber) {
      linkInfo.lineNumber = lineNumber;
    }
    if (columnNumber) {
      linkInfo.columnNumber = columnNumber;
    }
    return link;
  }

  static linkifyRevealable(
      revealable: Object, text: string|HTMLElement, fallbackHref?: string, title?: string,
      className?: string): HTMLElement {
    const createLinkOptions: _CreateLinkOptions = {
      maxLength: UI.UIUtils.MaxLengthForDisplayedURLs,
      href: fallbackHref,
      title,
    };
    const {link, linkInfo} = Linkifier.createLink(text, className || '', createLinkOptions);
    linkInfo.revealable = revealable;
    return link;
  }

  private static createLink(text: string|HTMLElement, className: string, options: _CreateLinkOptions = {}):
      {link: HTMLElement, linkInfo: _LinkInfo} {
    const {maxLength, title, href, preventClick, tabStop, bypassURLTrimming} = options;
    const link = document.createElement('span');
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
        Linkifier.appendTextWithoutHashes(link, text);
      } else {
        Linkifier.setTrimmedText(link, text, maxLength);
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
      fallback: null,
    };
    infoByAnchor.set(link, linkInfo);
    if (!preventClick) {
      link.addEventListener('click', event => {
        if (Linkifier.handleClick(event)) {
          event.consume(true);
        }
      }, false);
      link.addEventListener('keydown', event => {
        if (event.key === 'Enter' && Linkifier.handleClick(event)) {
          event.consume(true);
        }
      }, false);
    } else {
      link.classList.add('devtools-link-prevent-click');
    }
    UI.ARIAUtils.markAsLink(link);
    link.tabIndex = tabStop ? 0 : -1;
    return {link, linkInfo};
  }

  private static setTrimmedText(link: Element, text: string, maxLength?: number): void {
    link.removeChildren();
    if (maxLength && text.length > maxLength) {
      const middleSplit = splitMiddle(text, maxLength);
      Linkifier.appendTextWithoutHashes(link, middleSplit[0]);
      Linkifier.appendHiddenText(link, middleSplit[1]);
      Linkifier.appendTextWithoutHashes(link, middleSplit[2]);
    } else {
      Linkifier.appendTextWithoutHashes(link, text);
    }

    function splitMiddle(string: string, maxLength: number): string[] {
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

  private static appendTextWithoutHashes(link: Element, string: string): void {
    const hashSplit = TextUtils.TextUtils.Utils.splitStringByRegexes(string, [/[a-f0-9]{20,}/g]);
    for (const match of hashSplit) {
      if (match.regexIndex === -1) {
        UI.UIUtils.createTextChild(link, match.value);
      } else {
        UI.UIUtils.createTextChild(link, match.value.substring(0, 7));
        Linkifier.appendHiddenText(link, match.value.substring(7));
      }
    }
  }

  private static appendHiddenText(link: Element, string: string): void {
    const ellipsisNode = UI.UIUtils.createTextChild(link.createChild('span', 'devtools-link-ellipsis'), '…');
    textByAnchor.set(ellipsisNode, string);
  }

  static untruncatedNodeText(node: Node): string {
    return textByAnchor.get(node) || node.textContent || '';
  }

  static linkInfo(link: Element|null): _LinkInfo|null {
    return link ? infoByAnchor.get(link) || null : null as _LinkInfo | null;
  }

  private static handleClick(event: Event): boolean {
    const link = (event.currentTarget as Element);
    if (UI.UIUtils.isBeingEdited((event.target as Node)) || link.hasSelection()) {
      return false;
    }
    const linkInfo = Linkifier.linkInfo(link);
    if (!linkInfo) {
      return false;
    }
    return Linkifier.invokeFirstAction(linkInfo);
  }

  static handleClickFromNewComponentLand(linkInfo: _LinkInfo): void {
    Linkifier.invokeFirstAction(linkInfo);
  }

  static invokeFirstAction(linkInfo: _LinkInfo): boolean {
    const actions = Linkifier.linkActions(linkInfo);
    if (actions.length) {
      void actions[0].handler.call(null);
      return true;
    }
    return false;
  }

  static linkHandlerSetting(): Common.Settings.Setting<string> {
    if (!linkHandlerSettingInstance) {
      linkHandlerSettingInstance =
          Common.Settings.Settings.instance().createSetting('openLinkHandler', i18nString(UIStrings.auto));
    }
    return linkHandlerSettingInstance;
  }

  static registerLinkHandler(title: string, handler: LinkHandler): void {
    linkHandlers.set(title, handler);
    LinkHandlerSettingUI.instance().update();
  }

  static unregisterLinkHandler(title: string): void {
    linkHandlers.delete(title);
    LinkHandlerSettingUI.instance().update();
  }

  static uiLocation(link: Element): Workspace.UISourceCode.UILocation|null {
    const info = Linkifier.linkInfo(link);
    return info ? info.uiLocation : null;
  }

  static linkActions(info: _LinkInfo): {
    section: string,
    title: string,
    handler: () => Promise<void>| void,
  }[] {
    const result: {
      section: string,
      title: string,
      handler: () => Promise<void>| void,
    }[] = [];

    if (!info) {
      return result;
    }

    let url = '';
    let uiLocation: Workspace.UISourceCode.UILocation|(Workspace.UISourceCode.UILocation | null)|null = null;
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
        handler: (): Promise<void> => Common.Revealer.reveal(revealable),
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
          handler: handler.bind(null, contentProvider, lineNumber),
        };
        if (title === Linkifier.linkHandlerSetting().get()) {
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
        handler: (): void => Host.InspectorFrontendHost.InspectorFrontendHostInstance.openInNewTab(url),
      });
      result.push({
        section: 'clipboard',
        title: UI.UIUtils.copyLinkAddressLabel(),
        handler: (): void => Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(url),
      });
    }

    if (uiLocation && uiLocation.uiSourceCode) {
      const contentProvider = uiLocation.uiSourceCode;
      result.push({
        section: 'clipboard',
        title: UI.UIUtils.copyFileNameLabel(),
        handler: (): void =>
            Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(contentProvider.displayName()),
      });
    }

    return result;
  }
}

export interface LinkDecorator extends Common.EventTarget.EventTarget<LinkDecorator.EventTypes> {
  linkIcon(uiSourceCode: Workspace.UISourceCode.UISourceCode): UI.Icon.Icon|null;
}

export namespace LinkDecorator {
  // TODO(crbug.com/1167717): Make this a const enum again
  // eslint-disable-next-line rulesdir/const_enum
  export enum Events {
    LinkIconChanged = 'LinkIconChanged',
  }

  export type EventTypes = {
    [Events.LinkIconChanged]: Workspace.UISourceCode.UISourceCode,
  };
}

let linkContextMenuProviderInstance: LinkContextMenuProvider;

export class LinkContextMenuProvider implements UI.ContextMenu.Provider {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): LinkContextMenuProvider {
    const {forceNew} = opts;
    if (!linkContextMenuProviderInstance || forceNew) {
      linkContextMenuProviderInstance = new LinkContextMenuProvider();
    }

    return linkContextMenuProviderInstance;
  }
  appendApplicableItems(event: Event, contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    let targetNode: (Node|null) = (target as Node | null);
    while (targetNode && !infoByAnchor.get(targetNode)) {
      targetNode = targetNode.parentNodeOrShadowHost();
    }
    const link = (targetNode as Element | null);
    const linkInfo = Linkifier.linkInfo(link);
    if (!linkInfo) {
      return;
    }

    const actions = Linkifier.linkActions(linkInfo);
    for (const action of actions) {
      contextMenu.section(action.section).appendItem(action.title, action.handler);
    }
  }
}

let linkHandlerSettingUIInstance: LinkHandlerSettingUI;

export class LinkHandlerSettingUI implements UI.SettingsUI.SettingUI {
  private element: HTMLSelectElement;

  private constructor() {
    this.element = document.createElement('select');
    this.element.classList.add('chrome-select');
    this.element.addEventListener('change', this.onChange.bind(this), false);
    this.update();
  }

  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): LinkHandlerSettingUI {
    const {forceNew} = opts;
    if (!linkHandlerSettingUIInstance || forceNew) {
      linkHandlerSettingUIInstance = new LinkHandlerSettingUI();
    }

    return linkHandlerSettingUIInstance;
  }

  update(): void {
    this.element.removeChildren();
    const names = [...linkHandlers.keys()];
    names.unshift(i18nString(UIStrings.auto));
    for (const name of names) {
      const option = document.createElement('option');
      option.textContent = name;
      option.selected = name === Linkifier.linkHandlerSetting().get();
      this.element.appendChild(option);
    }
    this.element.disabled = names.length <= 1;
  }

  private onChange(event: Event): void {
    if (!event.target) {
      return;
    }
    const value = (event.target as HTMLSelectElement).value;
    Linkifier.linkHandlerSetting().set(value);
  }

  settingElement(): Element|null {
    return UI.SettingsUI.createCustomSetting(i18nString(UIStrings.linkHandling), this.element);
  }
}

let listeningToNewEvents = false;
function listenForNewComponentLinkifierEvents(): void {
  if (listeningToNewEvents) {
    return;
  }

  listeningToNewEvents = true;

  window.addEventListener('linkifieractivated', function(event: Event) {
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const unknownEvent = (event as any);
    const eventWithData = (unknownEvent as {
      data: _LinkInfo,
    });
    Linkifier.handleClickFromNewComponentLand(eventWithData.data);
  });
}

listenForNewComponentLinkifierEvents();

let contentProviderContextMenuProviderInstance: ContentProviderContextMenuProvider;

export class ContentProviderContextMenuProvider implements UI.ContextMenu.Provider {
  static instance(opts: {
    forceNew: boolean|null,
  } = {forceNew: null}): ContentProviderContextMenuProvider {
    const {forceNew} = opts;
    if (!contentProviderContextMenuProviderInstance || forceNew) {
      contentProviderContextMenuProviderInstance = new ContentProviderContextMenuProvider();
    }

    return contentProviderContextMenuProviderInstance;
  }

  appendApplicableItems(event: Event, contextMenu: UI.ContextMenu.ContextMenu, target: Object): void {
    const contentProvider = (target as Workspace.UISourceCode.UISourceCode);
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

// TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
// eslint-disable-next-line @typescript-eslint/naming-convention
export interface _LinkInfo {
  icon: UI.Icon.Icon|null;
  enableDecorator: boolean;
  uiLocation: Workspace.UISourceCode.UILocation|null;
  liveLocation: Bindings.LiveLocation.LiveLocation|null;
  url: string|null;
  lineNumber: number|null;
  columnNumber: number|null;
  inlineFrameIndex: number;
  revealable: Object|null;
  fallback: Element|null;
}

export interface LinkifyURLOptions {
  text?: string;
  className?: string;
  lineNumber?: number;
  columnNumber?: number;
  showColumnNumber: boolean;
  inlineFrameIndex: number;
  preventClick?: boolean;
  maxLength?: number;
  tabStop?: boolean;
  bypassURLTrimming?: boolean;
}

export interface LinkifyOptions {
  className?: string;
  columnNumber?: number;
  showColumnNumber?: boolean;
  inlineFrameIndex: number;
  tabStop?: boolean;
}

// TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
// eslint-disable-next-line @typescript-eslint/naming-convention
export interface _CreateLinkOptions {
  maxLength?: number;
  title?: string;
  href?: string;
  preventClick?: boolean;
  tabStop?: boolean;
  bypassURLTrimming?: boolean;
}

interface LinkDisplayOptions {
  showColumnNumber: boolean;
}

export type LinkHandler = (arg0: TextUtils.ContentProvider.ContentProvider, arg1: number) => void;
