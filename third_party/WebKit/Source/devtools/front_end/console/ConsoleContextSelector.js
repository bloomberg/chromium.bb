// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @implements {SDK.SDKModelObserver<!SDK.RuntimeModel>}
 * @implements {UI.ListDelegate<!SDK.ExecutionContext>}
 */
Console.ConsoleContextSelector = class {
  constructor() {
    this._toolbarItem = new UI.ToolbarItem(createElementWithClass('button', 'console-context'));
    var shadowRoot =
        UI.createShadowRootWithCoreStyles(this._toolbarItem.element, 'console/consoleContextSelectorButton.css');
    this._titleElement = shadowRoot.createChild('span', 'title');

    /** @type {!Map<!SDK.ExecutionContext, !ProductRegistry.BadgePool>} */
    this._badgePoolForExecutionContext = new Map();

    this._toolbarItem.element.classList.add('toolbar-has-dropdown');
    this._toolbarItem.element.tabIndex = 0;
    this._glassPane = new UI.GlassPane();
    this._glassPane.setMarginBehavior(UI.GlassPane.MarginBehavior.NoMargin);
    this._glassPane.setAnchorBehavior(UI.GlassPane.AnchorBehavior.PreferBottom);
    this._glassPane.setOutsideClickCallback(this._hide.bind(this));
    this._glassPane.setPointerEventsBehavior(UI.GlassPane.PointerEventsBehavior.BlockedByGlassPane);
    /** @type {!UI.ListModel<!SDK.ExecutionContext>} */
    this._items = new UI.ListModel();
    /** @type {!UI.ListControl<!SDK.ExecutionContext>} */
    this._list = new UI.ListControl(this._items, this, UI.ListMode.EqualHeightItems);
    this._list.element.classList.add('context-list');
    this._rowHeight = 36;
    UI.createShadowRootWithCoreStyles(this._glassPane.contentElement, 'console/consoleContextSelector.css')
        .appendChild(this._list.element);

    this._listWasShowing200msAgo = false;
    this._toolbarItem.element.addEventListener('mousedown', event => {
      if (this._listWasShowing200msAgo)
        this._hide(event);
      else
        this._show(event);
    }, false);
    this._toolbarItem.element.addEventListener('keydown', this._onKeyDown.bind(this), false);
    this._toolbarItem.element.addEventListener('focusout', this._hide.bind(this), false);
    this._list.element.addEventListener('mousedown', event => {
      event.consume(true);
    }, false);
    this._list.element.addEventListener('mouseup', event => {
      if (event.target === this._list.element)
        return;

      if (!this._listWasShowing200msAgo)
        return;
      this._updateSelectedContext();
      this._hide(event);
    }, false);

    var dropdownArrowIcon = UI.Icon.create('smallicon-triangle-down');
    shadowRoot.appendChild(dropdownArrowIcon);

    SDK.targetManager.addModelListener(
        SDK.RuntimeModel, SDK.RuntimeModel.Events.ExecutionContextCreated, this._onExecutionContextCreated, this);
    SDK.targetManager.addModelListener(
        SDK.RuntimeModel, SDK.RuntimeModel.Events.ExecutionContextChanged, this._onExecutionContextChanged, this);
    SDK.targetManager.addModelListener(
        SDK.RuntimeModel, SDK.RuntimeModel.Events.ExecutionContextDestroyed, this._onExecutionContextDestroyed, this);
    SDK.targetManager.addModelListener(
        SDK.ResourceTreeModel, SDK.ResourceTreeModel.Events.FrameNavigated, this._frameNavigated, this);

    UI.context.addFlavorChangeListener(SDK.ExecutionContext, this._executionContextChangedExternally, this);
    UI.context.addFlavorChangeListener(SDK.DebuggerModel.CallFrame, this._callFrameSelectedInUI, this);
    SDK.targetManager.observeModels(SDK.RuntimeModel, this);
    SDK.targetManager.addModelListener(
        SDK.DebuggerModel, SDK.DebuggerModel.Events.CallFrameSelected, this._callFrameSelectedInModel, this);
  }

  /**
   * @return {!UI.ToolbarItem}
   */
  toolbarItem() {
    return this._toolbarItem;
  }

  /**
   * @param {!Event} event
   */
  _show(event) {
    if (this._glassPane.isShowing())
      return;
    this._glassPane.setContentAnchorBox(this._toolbarItem.element.boxInWindow());
    this._glassPane.show(/** @type {!Document} **/ (this._toolbarItem.element.ownerDocument));
    this._updateGlasspaneSize();
    var selectedContext = UI.context.flavor(SDK.ExecutionContext);
    if (selectedContext) {
      this._list.selectItem(selectedContext);
      this._list.scrollItemIntoView(selectedContext, true);
    }
    this._toolbarItem.element.focus();
    event.consume(true);
    setTimeout(() => this._listWasShowing200msAgo = true, 200);
  }

  _updateGlasspaneSize() {
    var maxHeight = this._rowHeight * (Math.min(this._items.length(), 9));
    this._glassPane.setMaxContentSize(new UI.Size(315, maxHeight));
    this._list.viewportResized();
  }

  /**
   * @param {!Event} event
   */
  _hide(event) {
    setTimeout(() => this._listWasShowing200msAgo = false, 200);
    this._glassPane.hide();
    SDK.OverlayModel.hideDOMNodeHighlight();
    event.consume(true);
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    var handled = false;
    switch (event.key) {
      case 'ArrowUp':
        handled = this._list.selectPreviousItem(false, false);
        break;
      case 'ArrowDown':
        handled = this._list.selectNextItem(false, false);
        break;
      case 'ArrowRight':
        var currentExecutionContext = this._list.selectedItem();
        if (!currentExecutionContext)
          break;
        var nextExecutionContext = this._items.itemAtIndex(this._list.selectedIndex() + 1);
        if (nextExecutionContext && this._depthFor(currentExecutionContext) < this._depthFor(nextExecutionContext))
          handled = this._list.selectNextItem(false, false);
        break;
      case 'ArrowLeft':
        var currentExecutionContext = this._list.selectedItem();
        if (!currentExecutionContext)
          break;
        var depth = this._depthFor(currentExecutionContext);
        for (var i = this._list.selectedIndex() - 1; i >= 0; i--) {
          if (this._depthFor(this._items.itemAtIndex(i)) < depth) {
            handled = true;
            this._list.selectItem(this._items.itemAtIndex(i), false);
            break;
          }
        }
        break;
      case 'PageUp':
        handled = this._list.selectItemPreviousPage(false);
        break;
      case 'PageDown':
        handled = this._list.selectItemNextPage(false);
        break;
      case 'Home':
        for (var i = 0; i < this._items.length(); i++) {
          if (this.isItemSelectable(this._items.itemAtIndex(i))) {
            this._list.selectItem(this._items.itemAtIndex(i));
            handled = true;
            break;
          }
        }
        break;
      case 'End':
        for (var i = this._items.length() - 1; i >= 0; i--) {
          if (this.isItemSelectable(this._items.itemAtIndex(i))) {
            this._list.selectItem(this._items.itemAtIndex(i));
            handled = true;
            break;
          }
        }
        break;
      case 'Escape':
        this._hide(event);
        break;
      case 'Tab':
        if (!this._glassPane.isShowing())
          break;
        this._updateSelectedContext();
        this._hide(event);
        break;
      case 'Enter':
        if (!this._glassPane.isShowing()) {
          this._show(event);
          break;
        }
        this._updateSelectedContext();
        this._hide(event);
        break;
      case ' ':
        this._show(event);
        break;
      default:
        if (event.key.length === 1) {
          var selectedIndex = this._list.selectedIndex();
          var letter = event.key.toUpperCase();
          for (var i = 0; i < this._items.length(); i++) {
            var context = this._items.itemAtIndex((selectedIndex + i + 1) % this._items.length());
            if (this._titleFor(context).toUpperCase().startsWith(letter)) {
              this._list.selectItem(context);
              break;
            }
          }
          handled = true;
        }
        break;
    }

    if (handled) {
      event.consume(true);
      this._updateSelectedContext();
    }
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {string}
   */
  _titleFor(executionContext) {
    var target = executionContext.target();
    var label = executionContext.label() ? target.decorateLabel(executionContext.label()) : '';
    if (executionContext.frameId) {
      var resourceTreeModel = target.model(SDK.ResourceTreeModel);
      var frame = resourceTreeModel && resourceTreeModel.frameForId(executionContext.frameId);
      if (frame)
        label = label || frame.displayName();
    }
    label = label || executionContext.origin;

    return label;
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {number}
   */
  _depthFor(executionContext) {
    var target = executionContext.target();
    var depth = 0;
    if (!executionContext.isDefault)
      depth++;
    if (executionContext.frameId) {
      var resourceTreeModel = target.model(SDK.ResourceTreeModel);
      var frame = resourceTreeModel && resourceTreeModel.frameForId(executionContext.frameId);
      while (frame && frame.parentFrame) {
        depth++;
        frame = frame.parentFrame;
      }
    }
    var targetDepth = 0;
    while (target.parentTarget()) {
      if (target.parentTarget().hasJSCapability()) {
        targetDepth++;
      } else {
        // Special casing service workers to be top-level.
        targetDepth = 0;
        break;
      }
      target = target.parentTarget();
    }
    depth += targetDepth;
    return depth;
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {?Element}
   */
  _badgeFor(executionContext) {
    if (!executionContext.frameId || !executionContext.isDefault)
      return null;
    var resourceTreeModel = executionContext.target().model(SDK.ResourceTreeModel);
    var frame = resourceTreeModel && resourceTreeModel.frameForId(executionContext.frameId);
    if (!frame)
      return null;
    var badgePool = new ProductRegistry.BadgePool();
    this._badgePoolForExecutionContext.set(executionContext, badgePool);
    return badgePool.badgeForFrame(frame);
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   */
  _disposeExecutionContextBadge(executionContext) {
    var badgePool = this._badgePoolForExecutionContext.get(executionContext);
    if (!badgePool)
      return;
    badgePool.reset();
    this._badgePoolForExecutionContext.delete(executionContext);
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   */
  _executionContextCreated(executionContext) {
    // FIXME(413886): We never want to show execution context for the main thread of shadow page in service/shared worker frontend.
    // This check could be removed once we do not send this context to frontend.
    if (!executionContext.target().hasJSCapability())
      return;

    this._items.insertItemWithComparator(executionContext, executionContext.runtimeModel.executionContextComparator());

    if (executionContext === UI.context.flavor(SDK.ExecutionContext))
      this._updateSelectionTitle();
  }

  /**
   * @param {!Common.Event} event
   */
  _onExecutionContextCreated(event) {
    var executionContext = /** @type {!SDK.ExecutionContext} */ (event.data);
    this._executionContextCreated(executionContext);
    this._updateSelectionTitle();
    this._updateGlasspaneSize();
  }

  /**
   * @param {!Common.Event} event
   */
  _onExecutionContextChanged(event) {
    var executionContext = /** @type {!SDK.ExecutionContext} */ (event.data);
    if (this._items.indexOfItem(executionContext) === -1)
      return;
    this._executionContextDestroyed(executionContext);
    this._executionContextCreated(executionContext);
    this._updateSelectionTitle();
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   */
  _executionContextDestroyed(executionContext) {
    if (this._items.indexOfItem(executionContext) === -1)
      return;
    this._disposeExecutionContextBadge(executionContext);
    this._items.removeItem(executionContext);
    this._updateGlasspaneSize();
  }

  /**
   * @param {!Common.Event} event
   */
  _onExecutionContextDestroyed(event) {
    var executionContext = /** @type {!SDK.ExecutionContext} */ (event.data);
    this._executionContextDestroyed(executionContext);
    this._updateSelectionTitle();
  }

  /**
   * @param {!Common.Event} event
   */
  _executionContextChangedExternally(event) {
    var executionContext = /** @type {?SDK.ExecutionContext} */ (event.data);
    if (!executionContext || this._items.indexOfItem(executionContext) === -1)
      return;
    this._list.selectItem(executionContext);
    this._updateSelectedContext();
  }

  _updateSelectionTitle() {
    var executionContext = UI.context.flavor(SDK.ExecutionContext);
    if (executionContext)
      this._titleElement.textContent = this._titleFor(executionContext);
    else
      this._titleElement.textContent = '';
    this._toolbarItem.element.classList.toggle(
        'warning', !this._isTopContext(executionContext) && this._hasTopContext());
  }

  /**
   * @param {?SDK.ExecutionContext} executionContext
   * @return {boolean}
   */
  _isTopContext(executionContext) {
    if (!executionContext || !executionContext.isDefault)
      return false;
    var resourceTreeModel = executionContext.target().model(SDK.ResourceTreeModel);
    var frame = executionContext.frameId && resourceTreeModel && resourceTreeModel.frameForId(executionContext.frameId);
    if (!frame)
      return false;
    return frame.isMainFrame();
  }

  /**
   * @return {boolean}
   */
  _hasTopContext() {
    for (var i = 0; i < this._items.length(); i++) {
      if (this._isTopContext(this._items.itemAtIndex(i)))
        return true;
    }
    return false;
  }

  /**
   * @override
   * @param {!SDK.RuntimeModel} runtimeModel
   */
  modelAdded(runtimeModel) {
    runtimeModel.executionContexts().forEach(this._executionContextCreated, this);
  }

  /**
   * @override
   * @param {!SDK.RuntimeModel} runtimeModel
   */
  modelRemoved(runtimeModel) {
    for (var i = 0; i < this._items.length(); i++) {
      if (this._items.itemAtIndex(i).runtimeModel === runtimeModel)
        this._executionContextDestroyed(this._items.itemAtIndex(i));
    }
  }

  /**
   * @override
   * @param {!SDK.ExecutionContext} item
   * @return {!Element}
   */
  createElementForItem(item) {
    var element = createElementWithClass('div', 'context');
    element.style.paddingLeft = (8 + this._depthFor(item) * 15) + 'px';
    // var titleArea = element.createChild('div', 'title-area');
    var title = element.createChild('div', 'title');
    title.createTextChild(this._titleFor(item).trimEnd(100));
    var subTitle = element.createChild('div', 'subtitle');
    var badgeElement = this._badgeFor(item);
    if (badgeElement) {
      badgeElement.classList.add('badge');
      subTitle.appendChild(badgeElement);
    }
    subTitle.createTextChild(this._subtitleFor(item));
    element.addEventListener('mousemove', e => {
      if ((e.movementX || e.movementY) && this.isItemSelectable(item))
        this._list.selectItem(item, false, /* Don't scroll */ true);
    });
    element.classList.toggle('disabled', !this.isItemSelectable(item));
    return element;
  }

  /**
   * @param {!SDK.ExecutionContext} executionContext
   * @return {string}
   */
  _subtitleFor(executionContext) {
    var target = executionContext.target();
    if (executionContext.frameId) {
      var resourceTreeModel = target.model(SDK.ResourceTreeModel);
      var frame = resourceTreeModel && resourceTreeModel.frameForId(executionContext.frameId);
    }
    if (executionContext.origin.startsWith('chrome-extension://'))
      return Common.UIString('Extension');
    if (!frame || !frame.parentFrame || frame.parentFrame.securityOrigin !== executionContext.origin) {
      var url = executionContext.origin.asParsedURL();
      if (url)
        return url.domain();
    }

    if (frame) {
      var callFrame = frame.findCreationCallFrame(callFrame => !!callFrame.url);
      if (callFrame)
        return new Common.ParsedURL(callFrame.url).domain();
      return Common.UIString('IFrame');
    }
    return '';
  }

  /**
   * @override
   * @param {!SDK.ExecutionContext} item
   * @return {number}
   */
  heightForItem(item) {
    return this._rowHeight;
  }

  /**
   * @override
   * @param {!SDK.ExecutionContext} item
   * @return {boolean}
   */
  isItemSelectable(item) {
    var callFrame = item.debuggerModel.selectedCallFrame();
    var callFrameContext = callFrame && callFrame.script.executionContext();
    return !callFrameContext || item === callFrameContext;
  }

  /**
   * @override
   * @param {?SDK.ExecutionContext} from
   * @param {?SDK.ExecutionContext} to
   * @param {?Element} fromElement
   * @param {?Element} toElement
   */
  selectedItemChanged(from, to, fromElement, toElement) {
    if (fromElement)
      fromElement.classList.remove('selected');
    if (toElement)
      toElement.classList.add('selected');
    SDK.OverlayModel.hideDOMNodeHighlight();
    if (to && to.frameId) {
      var resourceTreeModel = to.target().model(SDK.ResourceTreeModel);
      if (resourceTreeModel)
        resourceTreeModel.domModel().overlayModel().highlightFrame(to.frameId);
    }
  }

  _updateSelectedContext() {
    var context = this._list.selectedItem();
    UI.context.setFlavor(SDK.ExecutionContext, context);
    this._updateSelectionTitle();
  }

  _callFrameSelectedInUI() {
    var callFrame = UI.context.flavor(SDK.DebuggerModel.CallFrame);
    var callFrameContext = callFrame && callFrame.script.executionContext();
    if (callFrameContext)
      UI.context.setFlavor(SDK.ExecutionContext, callFrameContext);
  }

  /**
   * @param {!Common.Event} event
   */
  _callFrameSelectedInModel(event) {
    var debuggerModel = /** @type {!SDK.DebuggerModel} */ (event.data);
    for (var i = 0; i < this._items.length(); i++) {
      var executionContext = this._items.itemAtIndex(i);
      if (executionContext.debuggerModel === debuggerModel) {
        this._disposeExecutionContextBadge(executionContext);
        this._list.refreshItem(executionContext);
      }
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _frameNavigated(event) {
    var frameId = event.data.id;
    for (var i = 0; i < this._items.length(); i++) {
      var executionContext = this._items.itemAtIndex(i);
      if (frameId === executionContext.frameId) {
        this._disposeExecutionContextBadge(executionContext);
        this._list.refreshItem(executionContext);
      }
    }
  }
};
