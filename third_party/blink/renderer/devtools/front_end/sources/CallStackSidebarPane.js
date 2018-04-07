/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @implements {UI.ContextFlavorListener}
 * @implements {UI.ListDelegate<!Sources.CallStackSidebarPane.Item>}
 * @unrestricted
 */
Sources.CallStackSidebarPane = class extends UI.SimpleView {
  constructor() {
    super(Common.UIString('Call Stack'), true);
    this.registerRequiredCSS('sources/callStackSidebarPane.css');

    this._blackboxedMessageElement = this._createBlackboxedMessageElement();
    this.contentElement.appendChild(this._blackboxedMessageElement);

    this._notPausedMessageElement = this.contentElement.createChild('div', 'gray-info-message');
    this._notPausedMessageElement.textContent = Common.UIString('Not paused');

    /** @type {!UI.ListModel<!Sources.CallStackSidebarPane.Item>} */
    this._items = new UI.ListModel();
    /** @type {!UI.ListControl<!Sources.CallStackSidebarPane.Item>} */
    this._list = new UI.ListControl(this._items, this, UI.ListMode.NonViewport);
    this.contentElement.appendChild(this._list.element);
    this._list.element.addEventListener('contextmenu', this._onContextMenu.bind(this), false);
    this._list.element.addEventListener('click', this._onClick.bind(this), false);

    this._showMoreMessageElement = this._createShowMoreMessageElement();
    this._showMoreMessageElement.classList.add('hidden');
    this.contentElement.appendChild(this._showMoreMessageElement);

    this._showBlackboxed = false;
    Bindings.blackboxManager.addChangeListener(this._update.bind(this));
    this._locationPool = new Bindings.LiveLocationPool();

    this._updateThrottler = new Common.Throttler(100);
    this._maxAsyncStackChainDepth = Sources.CallStackSidebarPane._defaultMaxAsyncStackChainDepth;
    this._update();
  }

  /**
   * @override
   * @param {?Object} object
   */
  flavorChanged(object) {
    this._showBlackboxed = false;
    this._maxAsyncStackChainDepth = Sources.CallStackSidebarPane._defaultMaxAsyncStackChainDepth;
    this._update();
  }

  _update() {
    this._updateThrottler.schedule(() => this._doUpdate());
  }

  /**
   * @return {!Promise<undefined>}
   */
  async _doUpdate() {
    this._locationPool.disposeAll();

    const details = UI.context.flavor(SDK.DebuggerPausedDetails);
    if (!details) {
      this._notPausedMessageElement.classList.remove('hidden');
      this._blackboxedMessageElement.classList.add('hidden');
      this._showMoreMessageElement.classList.add('hidden');
      this._items.replaceAll([]);
      UI.context.setFlavor(SDK.DebuggerModel.CallFrame, null);
      return;
    }

    let debuggerModel = details.debuggerModel;
    this._notPausedMessageElement.classList.add('hidden');

    const showBlackboxed = this._showBlackboxed ||
        details.callFrames.every(frame => Bindings.blackboxManager.isBlackboxedRawLocation(frame.location()));

    let hiddenCallFramesCount = 0;
    let items = details.callFrames.map(frame => ({debuggerCallFrame: frame, debuggerModel: debuggerModel}));
    if (!showBlackboxed) {
      items = items.filter(
          item => !Bindings.blackboxManager.isBlackboxedRawLocation(
              /** @type {!SDK.DebuggerModel.Location} */ (this._itemLocation(item))));
      hiddenCallFramesCount += details.callFrames.length - items.length;
    }

    let asyncStackTrace = details.asyncStackTrace;
    if (!asyncStackTrace && details.asyncStackTraceId) {
      if (details.asyncStackTraceId.debuggerId)
        debuggerModel = SDK.DebuggerModel.modelForDebuggerId(details.asyncStackTraceId.debuggerId);
      asyncStackTrace = debuggerModel ? await debuggerModel.fetchAsyncStackTrace(details.asyncStackTraceId) : null;
    }
    let peviousStackTrace = details.callFrames;
    let maxAsyncStackChainDepth = this._maxAsyncStackChainDepth;
    while (asyncStackTrace && maxAsyncStackChainDepth > 0) {
      let title = '';
      const isAwait = asyncStackTrace.description === 'async function';
      if (isAwait && peviousStackTrace.length && asyncStackTrace.callFrames.length) {
        const lastPreviousFrame = peviousStackTrace[peviousStackTrace.length - 1];
        const lastPreviousFrameName = UI.beautifyFunctionName(lastPreviousFrame.functionName);
        title = UI.asyncStackTraceLabel('await in ' + lastPreviousFrameName);
      } else {
        title = UI.asyncStackTraceLabel(asyncStackTrace.description);
      }

      let asyncItems =
          asyncStackTrace.callFrames.map(frame => ({runtimeCallFrame: frame, debuggerModel: debuggerModel}));
      if (!showBlackboxed) {
        asyncItems = asyncItems.filter(
            item => !Bindings.blackboxManager.isBlackboxedRawLocation(
                /** @type {!SDK.DebuggerModel.Location} */ (this._itemLocation(item))));
        hiddenCallFramesCount += asyncStackTrace.callFrames.length - asyncItems.length;
      }

      if (asyncItems.length) {
        items.push({asyncStackHeader: title});
        items = items.concat(asyncItems);
      }

      --maxAsyncStackChainDepth;
      peviousStackTrace = asyncStackTrace.callFrames;
      if (asyncStackTrace.parent) {
        asyncStackTrace = asyncStackTrace.parent;
      } else if (asyncStackTrace.parentId) {
        if (asyncStackTrace.parentId.debuggerId)
          debuggerModel = SDK.DebuggerModel.modelForDebuggerId(asyncStackTrace.parentId.debuggerId);
        asyncStackTrace = debuggerModel ? await debuggerModel.fetchAsyncStackTrace(asyncStackTrace.parentId) : null;
      } else {
        asyncStackTrace = null;
      }
    }
    if (asyncStackTrace)
      this._showMoreMessageElement.classList.remove('hidden');
    else
      this._showMoreMessageElement.classList.add('hidden');

    if (!hiddenCallFramesCount) {
      this._blackboxedMessageElement.classList.add('hidden');
    } else {
      if (hiddenCallFramesCount === 1) {
        this._blackboxedMessageElement.firstChild.textContent =
            Common.UIString('1 stack frame is hidden (blackboxed).');
      } else {
        this._blackboxedMessageElement.firstChild.textContent =
            Common.UIString('%d stack frames are hidden (blackboxed).', hiddenCallFramesCount);
      }
      this._blackboxedMessageElement.classList.remove('hidden');
    }

    this._items.replaceAll(items);
    if (this._maxAsyncStackChainDepth === Sources.CallStackSidebarPane._defaultMaxAsyncStackChainDepth)
      this._list.selectNextItem(true /* canWrap */, false /* center */);
    this._updatedForTest();
  }

  _updatedForTest() {
  }

  /**
   * @override
   * @param {!Sources.CallStackSidebarPane.Item} item
   * @return {!Element}
   */
  createElementForItem(item) {
    const element = createElementWithClass('div', 'call-frame-item');
    const title = element.createChild('div', 'call-frame-item-title');
    title.createChild('div', 'call-frame-title-text').textContent = this._itemTitle(item);
    if (item.asyncStackHeader)
      element.classList.add('async-header');

    const location = this._itemLocation(item);
    if (location) {
      if (Bindings.blackboxManager.isBlackboxedRawLocation(location))
        element.classList.add('blackboxed-call-frame');

      /**
       * @param {!Bindings.LiveLocation} liveLocation
       */
      function updateLocation(liveLocation) {
        const uiLocation = liveLocation.uiLocation();
        if (!uiLocation)
          return;
        const text = uiLocation.linkText();
        linkElement.textContent = text.trimMiddle(30);
        linkElement.title = text;
      }

      const linkElement = element.createChild('div', 'call-frame-location');
      Bindings.debuggerWorkspaceBinding.createCallFrameLiveLocation(location, updateLocation, this._locationPool);
    }

    element.appendChild(UI.Icon.create('smallicon-thick-right-arrow', 'selected-call-frame-icon'));
    return element;
  }

  /**
   * @override
   * @param {!Sources.CallStackSidebarPane.Item} item
   * @return {number}
   */
  heightForItem(item) {
    console.assert(false);  // Should not be called.
    return 0;
  }

  /**
   * @override
   * @param {!Sources.CallStackSidebarPane.Item} item
   * @return {boolean}
   */
  isItemSelectable(item) {
    return !!item.debuggerCallFrame;
  }

  /**
   * @override
   * @param {?Sources.CallStackSidebarPane.Item} from
   * @param {?Sources.CallStackSidebarPane.Item} to
   * @param {?Element} fromElement
   * @param {?Element} toElement
   */
  selectedItemChanged(from, to, fromElement, toElement) {
    if (fromElement)
      fromElement.classList.remove('selected');
    if (toElement)
      toElement.classList.add('selected');
    if (to)
      this._activateItem(to);
  }

  /**
   * @param {!Sources.CallStackSidebarPane.Item} item
   * @return {string}
   */
  _itemTitle(item) {
    if (item.debuggerCallFrame)
      return UI.beautifyFunctionName(item.debuggerCallFrame.functionName);
    if (item.runtimeCallFrame)
      return UI.beautifyFunctionName(item.runtimeCallFrame.functionName);
    return item.asyncStackHeader || '';
  }

  /**
   * @param {!Sources.CallStackSidebarPane.Item} item
   * @return {?SDK.DebuggerModel.Location}
   */
  _itemLocation(item) {
    if (item.debuggerCallFrame)
      return item.debuggerCallFrame.location();
    if (!item.debuggerModel)
      return null;
    if (item.runtimeCallFrame) {
      const frame = item.runtimeCallFrame;
      return new SDK.DebuggerModel.Location(item.debuggerModel, frame.scriptId, frame.lineNumber, frame.columnNumber);
    }
    return null;
  }

  /**
   * @return {!Element}
   */
  _createBlackboxedMessageElement() {
    const element = createElementWithClass('div', 'blackboxed-message');
    element.createChild('span');
    const showAllLink = element.createChild('span', 'link');
    showAllLink.textContent = Common.UIString('Show');
    showAllLink.addEventListener('click', () => {
      this._showBlackboxed = true;
      this._update();
    }, false);
    return element;
  }

  /**
   * @return {!Element}
   */
  _createShowMoreMessageElement() {
    const element = createElementWithClass('div', 'show-more-message');
    element.createChild('span');
    const showAllLink = element.createChild('span', 'link');
    showAllLink.textContent = Common.UIString('Show more');
    showAllLink.addEventListener('click', () => {
      this._maxAsyncStackChainDepth += Sources.CallStackSidebarPane._defaultMaxAsyncStackChainDepth;
      this._update();
    }, false);
    return element;
  }

  /**
   * @param {!Event} event
   */
  _onContextMenu(event) {
    const item = this._list.itemForNode(/** @type {?Node} */ (event.target));
    if (!item)
      return;
    const contextMenu = new UI.ContextMenu(event);
    if (item.debuggerCallFrame)
      contextMenu.defaultSection().appendItem(Common.UIString('Restart frame'), () => item.debuggerCallFrame.restart());
    contextMenu.defaultSection().appendItem(Common.UIString('Copy stack trace'), this._copyStackTrace.bind(this));
    const location = this._itemLocation(item);
    const uiLocation = location ? Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(location) : null;
    if (uiLocation)
      this.appendBlackboxURLContextMenuItems(contextMenu, uiLocation.uiSourceCode);
    contextMenu.show();
  }

  /**
   * @param {!Event} event
   */
  _onClick(event) {
    const item = this._list.itemForNode(/** @type {?Node} */ (event.target));
    if (item)
      this._activateItem(item);
  }

  /**
   * @param {!Sources.CallStackSidebarPane.Item} item
   */
  _activateItem(item) {
    const location = this._itemLocation(item);
    if (!location)
      return;
    if (item.debuggerCallFrame && UI.context.flavor(SDK.DebuggerModel.CallFrame) !== item.debuggerCallFrame) {
      item.debuggerModel.setSelectedCallFrame(item.debuggerCallFrame);
      UI.context.setFlavor(SDK.DebuggerModel.CallFrame, item.debuggerCallFrame);
    } else {
      Common.Revealer.reveal(Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(location));
    }
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Workspace.UISourceCode} uiSourceCode
   */
  appendBlackboxURLContextMenuItems(contextMenu, uiSourceCode) {
    const binding = Persistence.persistence.binding(uiSourceCode);
    if (binding)
      uiSourceCode = binding.network;
    if (uiSourceCode.project().type() === Workspace.projectTypes.FileSystem)
      return;
    const canBlackbox = Bindings.blackboxManager.canBlackboxUISourceCode(uiSourceCode);
    const isBlackboxed = Bindings.blackboxManager.isBlackboxedUISourceCode(uiSourceCode);
    const isContentScript = uiSourceCode.project().type() === Workspace.projectTypes.ContentScripts;

    const manager = Bindings.blackboxManager;
    if (canBlackbox) {
      if (isBlackboxed) {
        contextMenu.defaultSection().appendItem(
            Common.UIString('Stop blackboxing'), manager.unblackboxUISourceCode.bind(manager, uiSourceCode));
      } else {
        contextMenu.defaultSection().appendItem(
            Common.UIString('Blackbox script'), manager.blackboxUISourceCode.bind(manager, uiSourceCode));
      }
    }
    if (isContentScript) {
      if (isBlackboxed) {
        contextMenu.defaultSection().appendItem(
            Common.UIString('Stop blackboxing all content scripts'), manager.blackboxContentScripts.bind(manager));
      } else {
        contextMenu.defaultSection().appendItem(
            Common.UIString('Blackbox all content scripts'), manager.unblackboxContentScripts.bind(manager));
      }
    }
  }

  /**
   * @return {boolean}
   */
  _selectNextCallFrameOnStack() {
    return this._list.selectNextItem(false /* canWrap */, false /* center */);
  }

  /**
   * @return {boolean}
   */
  _selectPreviousCallFrameOnStack() {
    return this._list.selectPreviousItem(false /* canWrap */, false /* center */);
  }

  _copyStackTrace() {
    const text = [];
    for (const item of this._items) {
      let itemText = this._itemTitle(item);
      const location = this._itemLocation(item);
      const uiLocation = location ? Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(location) : null;
      if (uiLocation)
        itemText += ' (' + uiLocation.linkText(true /* skipTrim */) + ')';
      text.push(itemText);
    }
    InspectorFrontendHost.copyText(text.join('\n'));
  }
};

Sources.CallStackSidebarPane._defaultMaxAsyncStackChainDepth = 32;

/**
 * @typedef {{
 *     debuggerCallFrame: (SDK.DebuggerModel.CallFrame|undefined),
 *     asyncStackHeader: (string|undefined),
 *     runtimeCallFrame: (Protocol.Runtime.CallFrame|undefined),
 *     debuggerModel: (!SDK.DebuggerModel|undefined)
 * }}
 */
Sources.CallStackSidebarPane.Item;

/**
 * @implements {UI.ActionDelegate}
 */
Sources.CallStackSidebarPane.ActionDelegate = class {
  /**
   * @override
   * @param {!UI.Context} context
   * @param {string} actionId
   * @return {boolean}
   */
  handleAction(context, actionId) {
    const callStackSidebarPane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
    switch (actionId) {
      case 'debugger.next-call-frame':
        callStackSidebarPane._selectNextCallFrameOnStack();
        return true;
      case 'debugger.previous-call-frame':
        callStackSidebarPane._selectPreviousCallFrameOnStack();
        return true;
    }
    return false;
  }
};
