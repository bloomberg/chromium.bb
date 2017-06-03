// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @implements {UI.ContextFlavorListener}
 * @implements {UI.ToolbarItem.ItemsProvider}
 * @unrestricted
 */
Sources.XHRBreakpointsSidebarPane = class extends UI.VBox {
  constructor() {
    super(true);
    this.registerRequiredCSS('sources/xhrBreakpointsSidebarPane.css');

    this._listElement = this.contentElement.createChild('div', 'breakpoint-list hidden');
    this._emptyElement = this.contentElement.createChild('div', 'gray-info-message');
    this._emptyElement.textContent = Common.UIString('No breakpoints');

    /** @type {!Map.<string, !Element>} */
    this._breakpointElements = new Map();

    this._addButton = new UI.ToolbarButton(Common.UIString('Add breakpoint'), 'largeicon-add');
    this._addButton.addEventListener(UI.ToolbarButton.Events.Click, this._addButtonClicked.bind(this));

    this._emptyElement.addEventListener('contextmenu', this._emptyElementContextMenu.bind(this), true);
    this._restoreBreakpoints();
    this._update();
  }

  /**
   * @override
   * @return {!Array<!UI.ToolbarItem>}
   */
  toolbarItems() {
    return [this._addButton];
  }

  _emptyElementContextMenu(event) {
    var contextMenu = new UI.ContextMenu(event);
    contextMenu.appendItem(Common.UIString.capitalize('Add ^breakpoint'), this._addButtonClicked.bind(this));
    contextMenu.show();
  }

  _addButtonClicked() {
    UI.viewManager.showView('sources.xhrBreakpoints');

    var inputElementContainer = createElementWithClass('p', 'breakpoint-condition');
    inputElementContainer.textContent = Common.UIString('Break when URL contains:');

    var inputElement = inputElementContainer.createChild('span', 'breakpoint-condition-input');
    this._addListElement(inputElementContainer, /** @type {?Element} */ (this._listElement.firstChild));

    /**
     * @param {boolean} accept
     * @param {!Element} e
     * @param {string} text
     * @this {Sources.XHRBreakpointsSidebarPane}
     */
    function finishEditing(accept, e, text) {
      this._removeListElement(inputElementContainer);
      if (accept) {
        SDK.domDebuggerManager.addXHRBreakpoint(text, true);
        this._setBreakpoint(text, true);
      }
    }

    var config = new UI.InplaceEditor.Config(finishEditing.bind(this, true), finishEditing.bind(this, false));
    UI.InplaceEditor.startEditing(inputElement, config);
  }

  /**
   * @param {string} url
   * @param {boolean} enabled
   */
  _setBreakpoint(url, enabled) {
    if (this._breakpointElements.has(url)) {
      this._breakpointElements.get(url)._checkboxElement.checked = enabled;
      return;
    }

    var element = createElementWithClass('div', 'breakpoint-entry');
    element._url = url;
    element.addEventListener('contextmenu', this._contextMenu.bind(this, url), true);

    var title = url ? Common.UIString('URL contains "%s"', url) : Common.UIString('Any XHR');
    var label = UI.CheckboxLabel.create(title, enabled);
    element.appendChild(label);
    label.checkboxElement.addEventListener('click', this._checkboxClicked.bind(this, url), false);
    element._checkboxElement = label.checkboxElement;

    label.classList.add('cursor-auto');
    label.textElement.addEventListener('dblclick', this._labelClicked.bind(this, url), false);

    var currentElement = /** @type {?Element} */ (this._listElement.firstChild);
    while (currentElement) {
      if (currentElement._url && currentElement._url < element._url)
        break;
      currentElement = /** @type {?Element} */ (currentElement.nextSibling);
    }
    this._addListElement(element, currentElement);
    this._breakpointElements.set(url, element);
  }

  /**
   * @param {string} url
   */
  _removeBreakpoint(url) {
    var element = this._breakpointElements.get(url);
    if (!element)
      return;

    this._removeListElement(element);
    this._breakpointElements.delete(url);
  }

  /**
   * @param {!Element} element
   * @param {?Node} beforeNode
   */
  _addListElement(element, beforeNode) {
    this._listElement.insertBefore(element, beforeNode);
    this._emptyElement.classList.add('hidden');
    this._listElement.classList.remove('hidden');
  }

  /**
   * @param {!Element} element
   */
  _removeListElement(element) {
    this._listElement.removeChild(element);
    if (!this._listElement.firstChild) {
      this._emptyElement.classList.remove('hidden');
      this._listElement.classList.add('hidden');
    }
  }

  _contextMenu(url, event) {
    var contextMenu = new UI.ContextMenu(event);

    /**
     * @this {Sources.XHRBreakpointsSidebarPane}
     */
    function removeBreakpoint() {
      SDK.domDebuggerManager.removeXHRBreakpoint(url);
      this._removeBreakpoint(url);
    }

    /**
     * @this {Sources.XHRBreakpointsSidebarPane}
     */
    function removeAllBreakpoints() {
      for (var url of this._breakpointElements.keys()) {
        SDK.domDebuggerManager.removeXHRBreakpoint(url);
        this._removeBreakpoint(url);
      }
    }
    var removeAllTitle = Common.UIString.capitalize('Remove ^all ^breakpoints');

    contextMenu.appendItem(Common.UIString.capitalize('Add ^breakpoint'), this._addButtonClicked.bind(this));
    contextMenu.appendItem(Common.UIString.capitalize('Remove ^breakpoint'), removeBreakpoint.bind(this));
    contextMenu.appendItem(removeAllTitle, removeAllBreakpoints.bind(this));
    contextMenu.show();
  }

  _checkboxClicked(url, event) {
    SDK.domDebuggerManager.toggleXHRBreakpoint(url, event.target.checked);
  }

  _labelClicked(url) {
    var element = this._breakpointElements.get(url) || null;
    var inputElement = createElementWithClass('span', 'breakpoint-condition');
    inputElement.textContent = url;
    this._listElement.insertBefore(inputElement, element);
    element.classList.add('hidden');

    /**
     * @param {boolean} accept
     * @param {!Element} e
     * @param {string} text
     * @this {Sources.XHRBreakpointsSidebarPane}
     */
    function finishEditing(accept, e, text) {
      this._removeListElement(inputElement);
      if (accept) {
        SDK.domDebuggerManager.removeXHRBreakpoint(url);
        this._removeBreakpoint(url);
        var enabled = element ? element._checkboxElement.checked : true;
        SDK.domDebuggerManager.addXHRBreakpoint(text, enabled);
        this._setBreakpoint(text, enabled);
      } else {
        element.classList.remove('hidden');
      }
    }

    UI.InplaceEditor.startEditing(
        inputElement, new UI.InplaceEditor.Config(finishEditing.bind(this, true), finishEditing.bind(this, false)));
  }

  /**
   * @override
   * @param {?Object} object
   */
  flavorChanged(object) {
    this._update();
  }

  _update() {
    var details = UI.context.flavor(SDK.DebuggerPausedDetails);
    if (!details || details.reason !== SDK.DebuggerModel.BreakReason.XHR) {
      if (this._highlightedElement) {
        this._highlightedElement.classList.remove('breakpoint-hit');
        delete this._highlightedElement;
      }
      return;
    }
    var url = details.auxData['breakpointURL'];
    var element = this._breakpointElements.get(url);
    if (!element)
      return;
    UI.viewManager.showView('sources.xhrBreakpoints');
    element.classList.add('breakpoint-hit');
    this._highlightedElement = element;
  }

  _restoreBreakpoints() {
    var breakpoints = SDK.domDebuggerManager.xhrBreakpoints();
    for (var url of breakpoints.keys())
      this._setBreakpoint(url, breakpoints.get(url));
  }
};
