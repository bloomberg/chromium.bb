/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Joseph Pecoraro
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

/**
 * @unrestricted
 */
Elements.ElementsTreeElement = class extends UI.TreeElement {
  /**
   * @param {!SDK.DOMNode} node
   * @param {boolean=} elementCloseTag
   */
  constructor(node, elementCloseTag) {
    // The title will be updated in onattach.
    super();
    this._node = node;

    this._gutterContainer = this.listItemElement.createChild('div', 'gutter-container');
    this._gutterContainer.addEventListener('click', this._showContextMenu.bind(this));
    var gutterMenuIcon = UI.Icon.create('largeicon-menu', 'gutter-menu-icon');
    this._gutterContainer.appendChild(gutterMenuIcon);
    this._decorationsElement = this._gutterContainer.createChild('div', 'hidden');

    this._elementCloseTag = elementCloseTag;

    if (this._node.nodeType() === Node.ELEMENT_NODE && !elementCloseTag)
      this._canAddAttributes = true;
    this._searchQuery = null;
    this._expandedChildrenLimit = Elements.ElementsTreeElement.InitialChildrenLimit;
    this._decorationsThrottler = new Common.Throttler(100);
  }

  /**
   * @param {!Elements.ElementsTreeElement} treeElement
   */
  static animateOnDOMUpdate(treeElement) {
    var tagName = treeElement.listItemElement.querySelector('.webkit-html-tag-name');
    UI.runCSSAnimationOnce(tagName || treeElement.listItemElement, 'dom-update-highlight');
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {!Array<!SDK.DOMNode>}
   */
  static visibleShadowRoots(node) {
    var roots = node.shadowRoots();
    if (roots.length && !Common.moduleSetting('showUAShadowDOM').get())
      roots = roots.filter(filter);

    /**
     * @param {!SDK.DOMNode} root
     */
    function filter(root) {
      return root.shadowRootType() !== SDK.DOMNode.ShadowRootTypes.UserAgent;
    }
    return roots;
  }

  /**
   * @param {!SDK.DOMNode} node
   * @return {boolean}
   */
  static canShowInlineText(node) {
    if (node.importedDocument() || node.templateContent() ||
        Elements.ElementsTreeElement.visibleShadowRoots(node).length || node.hasPseudoElements())
      return false;
    if (node.nodeType() !== Node.ELEMENT_NODE)
      return false;
    if (!node.firstChild || node.firstChild !== node.lastChild || node.firstChild.nodeType() !== Node.TEXT_NODE)
      return false;
    var textChild = node.firstChild;
    var maxInlineTextChildLength = 80;
    if (textChild.nodeValue().length < maxInlineTextChildLength)
      return true;
    return false;
  }

  /**
   * @param {!UI.ContextSubMenuItem} subMenu
   * @param {!SDK.DOMNode} node
   */
  static populateForcedPseudoStateItems(subMenu, node) {
    const pseudoClasses = ['active', 'hover', 'focus', 'visited'];
    var forcedPseudoState = node.domModel().cssModel().pseudoState(node);
    for (var i = 0; i < pseudoClasses.length; ++i) {
      var pseudoClassForced = forcedPseudoState.indexOf(pseudoClasses[i]) >= 0;
      subMenu.appendCheckboxItem(
          ':' + pseudoClasses[i], setPseudoStateCallback.bind(null, pseudoClasses[i], !pseudoClassForced),
          pseudoClassForced, false);
    }

    /**
     * @param {string} pseudoState
     * @param {boolean} enabled
     */
    function setPseudoStateCallback(pseudoState, enabled) {
      node.domModel().cssModel().forcePseudoState(node, pseudoState, enabled);
    }
  }

  /**
   * @return {boolean}
   */
  isClosingTag() {
    return !!this._elementCloseTag;
  }

  /**
   * @return {!SDK.DOMNode}
   */
  node() {
    return this._node;
  }

  /**
   * @return {boolean}
   */
  isEditing() {
    return !!this._editing;
  }

  /**
   * @param {string} searchQuery
   */
  highlightSearchResults(searchQuery) {
    if (this._searchQuery !== searchQuery)
      this._hideSearchHighlight();

    this._searchQuery = searchQuery;
    this._searchHighlightsVisible = true;
    this.updateTitle(null, true);
  }

  hideSearchHighlights() {
    delete this._searchHighlightsVisible;
    this._hideSearchHighlight();
  }

  _hideSearchHighlight() {
    if (!this._highlightResult)
      return;

    function updateEntryHide(entry) {
      switch (entry.type) {
        case 'added':
          entry.node.remove();
          break;
        case 'changed':
          entry.node.textContent = entry.oldText;
          break;
      }
    }

    for (var i = (this._highlightResult.length - 1); i >= 0; --i)
      updateEntryHide(this._highlightResult[i]);

    delete this._highlightResult;
  }

  /**
   * @param {boolean} inClipboard
   */
  setInClipboard(inClipboard) {
    if (this._inClipboard === inClipboard)
      return;
    this._inClipboard = inClipboard;
    this.listItemElement.classList.toggle('in-clipboard', inClipboard);
  }

  get hovered() {
    return this._hovered;
  }

  set hovered(x) {
    if (this._hovered === x)
      return;

    this._hovered = x;

    if (this.listItemElement) {
      if (x) {
        this._createSelection();
        this.listItemElement.classList.add('hovered');
      } else {
        this.listItemElement.classList.remove('hovered');
      }
    }
  }

  /**
   * @return {number}
   */
  expandedChildrenLimit() {
    return this._expandedChildrenLimit;
  }

  /**
   * @param {number} expandedChildrenLimit
   */
  setExpandedChildrenLimit(expandedChildrenLimit) {
    this._expandedChildrenLimit = expandedChildrenLimit;
  }

  _createSelection() {
    var listItemElement = this.listItemElement;
    if (!listItemElement)
      return;

    if (!this.selectionElement) {
      this.selectionElement = createElement('div');
      this.selectionElement.className = 'selection fill';
      this.selectionElement.style.setProperty('margin-left', (-this._computeLeftIndent()) + 'px');
      listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
    }
  }

  _createHint() {
    if (this.listItemElement && !this._hintElement) {
      this._hintElement = this.listItemElement.createChild('span', 'selected-hint');
      this._hintElement.title = Common.UIString('Use $0 in the console to refer to this element.');
    }
  }

  /**
   * @override
   */
  onbind() {
    if (!this._elementCloseTag)
      this._node[this.treeOutline.treeElementSymbol()] = this;
  }

  /**
   * @override
   */
  onunbind() {
    if (this._node[this.treeOutline.treeElementSymbol()] === this)
      this._node[this.treeOutline.treeElementSymbol()] = null;
  }

  /**
   * @override
   */
  onattach() {
    if (this._hovered) {
      this._createSelection();
      this.listItemElement.classList.add('hovered');
    }

    this.updateTitle();
    this.listItemElement.draggable = true;
  }

  /**
   * @override
   */
  onpopulate() {
    this.populated = true;
    this.treeOutline.populateTreeElement(this);
  }

  /**
   * @override
   */
  expandRecursively() {
    this._node.getSubtree(-1).then(UI.TreeElement.prototype.expandRecursively.bind(this, Number.MAX_VALUE));
  }

  /**
   * @override
   */
  onexpand() {
    if (this._elementCloseTag)
      return;

    this.updateTitle();
  }

  /**
   * @override
   */
  oncollapse() {
    if (this._elementCloseTag)
      return;

    this.updateTitle();
  }

  /**
   * @override
   * @param {boolean=} omitFocus
   * @param {boolean=} selectedByUser
   * @return {boolean}
   */
  select(omitFocus, selectedByUser) {
    if (this._editing)
      return false;
    return super.select(omitFocus, selectedByUser);
  }

  /**
   * @override
   * @param {boolean=} selectedByUser
   * @return {boolean}
   */
  onselect(selectedByUser) {
    this.treeOutline.suppressRevealAndSelect = true;
    this.treeOutline.selectDOMNode(this._node, selectedByUser);
    if (selectedByUser) {
      this._node.highlight();
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.ChangeInspectedNodeInElementsPanel);
    }
    this._createSelection();
    this._createHint();
    this.treeOutline.suppressRevealAndSelect = false;
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  ondelete() {
    var startTagTreeElement = this.treeOutline.findTreeElement(this._node);
    startTagTreeElement ? startTagTreeElement.remove() : this.remove();
    return true;
  }

  /**
   * @override
   * @return {boolean}
   */
  onenter() {
    // On Enter or Return start editing the first attribute
    // or create a new attribute on the selected element.
    if (this._editing)
      return false;

    this._startEditing();

    // prevent a newline from being immediately inserted
    return true;
  }

  /**
   * @override
   */
  selectOnMouseDown(event) {
    super.selectOnMouseDown(event);

    if (this._editing)
      return;

    // Prevent selecting the nearest word on double click.
    if (event.detail >= 2)
      event.preventDefault();
  }

  /**
   * @override
   * @return {boolean}
   */
  ondblclick(event) {
    if (this._editing || this._elementCloseTag)
      return false;

    if (this._startEditingTarget(/** @type {!Element} */ (event.target)))
      return false;

    if (this.isExpandable() && !this.expanded)
      this.expand();
    return false;
  }

  /**
   * @return {boolean}
   */
  hasEditableNode() {
    return !this._node.isShadowRoot() && !this._node.ancestorUserAgentShadowRoot();
  }

  _insertInLastAttributePosition(tag, node) {
    if (tag.getElementsByClassName('webkit-html-attribute').length > 0) {
      tag.insertBefore(node, tag.lastChild);
    } else {
      var nodeName = tag.textContent.match(/^<(.*?)>$/)[1];
      tag.textContent = '';
      tag.createTextChild('<' + nodeName);
      tag.appendChild(node);
      tag.createTextChild('>');
    }
  }

  /**
   * @param {!Element} eventTarget
   * @return {boolean}
   */
  _startEditingTarget(eventTarget) {
    if (this.treeOutline.selectedDOMNode() !== this._node)
      return false;

    if (this._node.nodeType() !== Node.ELEMENT_NODE && this._node.nodeType() !== Node.TEXT_NODE)
      return false;

    var textNode = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (textNode)
      return this._startEditingTextNode(textNode);

    var attribute = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    if (attribute)
      return this._startEditingAttribute(attribute, eventTarget);

    var tagName = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-tag-name');
    if (tagName)
      return this._startEditingTagName(tagName);

    var newAttribute = eventTarget.enclosingNodeOrSelfWithClass('add-attribute');
    if (newAttribute)
      return this._addNewAttribute();

    return false;
  }

  /**
   * @param {!Event} event
   */
  _showContextMenu(event) {
    this.treeOutline.showContextMenu(this, event);
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Event} event
   */
  populateTagContextMenu(contextMenu, event) {
    // Add attribute-related actions.
    var treeElement = this._elementCloseTag ? this.treeOutline.findTreeElement(this._node) : this;
    contextMenu.appendItem(
        Common.UIString.capitalize('Add ^attribute'), treeElement._addNewAttribute.bind(treeElement));

    var attribute = event.target.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    var newAttribute = event.target.enclosingNodeOrSelfWithClass('add-attribute');
    if (attribute && !newAttribute) {
      contextMenu.appendItem(
          Common.UIString.capitalize('Edit ^attribute'),
          this._startEditingAttribute.bind(this, attribute, event.target));
    }
    this.populateNodeContextMenu(contextMenu);
    Elements.ElementsTreeElement.populateForcedPseudoStateItems(contextMenu, treeElement.node());
    contextMenu.appendSeparator();
    this.populateScrollIntoView(contextMenu);
  }

  /**
   * @param {!UI.ContextMenu} contextMenu
   */
  populateScrollIntoView(contextMenu) {
    contextMenu.appendItem(Common.UIString.capitalize('Scroll into ^view'), this._scrollIntoView.bind(this));
  }

  populateTextContextMenu(contextMenu, textNode) {
    if (!this._editing)
      contextMenu.appendItem(Common.UIString.capitalize('Edit ^text'), this._startEditingTextNode.bind(this, textNode));
    this.populateNodeContextMenu(contextMenu);
  }

  populateNodeContextMenu(contextMenu) {
    // Add free-form node-related actions.
    var isEditable = this.hasEditableNode();
    if (isEditable && !this._editing)
      contextMenu.appendItem(Common.UIString('Edit as HTML'), this._editAsHTML.bind(this));
    var isShadowRoot = this._node.isShadowRoot();

    // Place it here so that all "Copy"-ing items stick together.
    var copyMenu = contextMenu.appendSubMenuItem(Common.UIString('Copy'));
    var createShortcut = UI.KeyboardShortcut.shortcutToString;
    var modifier = UI.KeyboardShortcut.Modifiers.CtrlOrMeta;
    var treeOutline = this.treeOutline;
    var menuItem;
    if (!isShadowRoot) {
      menuItem = copyMenu.appendItem(
          Common.UIString('Copy outerHTML'), treeOutline.performCopyOrCut.bind(treeOutline, false, this._node));
      menuItem.setShortcut(createShortcut('V', modifier));
    }
    if (this._node.nodeType() === Node.ELEMENT_NODE)
      copyMenu.appendItem(Common.UIString.capitalize('Copy selector'), this._copyCSSPath.bind(this));
    if (!isShadowRoot)
      copyMenu.appendItem(Common.UIString('Copy XPath'), this._copyXPath.bind(this));
    if (!isShadowRoot) {
      menuItem = copyMenu.appendItem(
          Common.UIString('Cut element'), treeOutline.performCopyOrCut.bind(treeOutline, true, this._node),
          !this.hasEditableNode());
      menuItem.setShortcut(createShortcut('X', modifier));
      menuItem = copyMenu.appendItem(
          Common.UIString('Copy element'), treeOutline.performCopyOrCut.bind(treeOutline, false, this._node));
      menuItem.setShortcut(createShortcut('C', modifier));
      menuItem = copyMenu.appendItem(
          Common.UIString('Paste element'), treeOutline.pasteNode.bind(treeOutline, this._node),
          !treeOutline.canPaste(this._node));
      menuItem.setShortcut(createShortcut('V', modifier));
    }

    contextMenu.appendSeparator();
    menuItem = contextMenu.appendCheckboxItem(
        Common.UIString('Hide element'), treeOutline.toggleHideElement.bind(treeOutline, this._node),
        treeOutline.isToggledToHidden(this._node));
    menuItem.setShortcut(UI.shortcutRegistry.shortcutTitleForAction('elements.hide-element'));

    if (isEditable)
      contextMenu.appendItem(Common.UIString('Delete element'), this.remove.bind(this));
    contextMenu.appendSeparator();

    contextMenu.appendItem(Common.UIString('Expand all'), this.expandRecursively.bind(this));
    contextMenu.appendItem(Common.UIString('Collapse all'), this.collapseRecursively.bind(this));
    contextMenu.appendSeparator();
  }

  _startEditing() {
    if (this.treeOutline.selectedDOMNode() !== this._node)
      return;

    var listItem = this.listItemElement;

    if (this._canAddAttributes) {
      var attribute = listItem.getElementsByClassName('webkit-html-attribute')[0];
      if (attribute) {
        return this._startEditingAttribute(
            attribute, attribute.getElementsByClassName('webkit-html-attribute-value')[0]);
      }

      return this._addNewAttribute();
    }

    if (this._node.nodeType() === Node.TEXT_NODE) {
      var textNode = listItem.getElementsByClassName('webkit-html-text-node')[0];
      if (textNode)
        return this._startEditingTextNode(textNode);
      return;
    }
  }

  _addNewAttribute() {
    // Cannot just convert the textual html into an element without
    // a parent node. Use a temporary span container for the HTML.
    var container = createElement('span');
    this._buildAttributeDOM(container, ' ', '', null);
    var attr = container.firstElementChild;
    attr.style.marginLeft = '2px';   // overrides the .editing margin rule
    attr.style.marginRight = '2px';  // overrides the .editing margin rule

    var tag = this.listItemElement.getElementsByClassName('webkit-html-tag')[0];
    this._insertInLastAttributePosition(tag, attr);
    attr.scrollIntoViewIfNeeded(true);
    return this._startEditingAttribute(attr, attr);
  }

  _triggerEditAttribute(attributeName) {
    var attributeElements = this.listItemElement.getElementsByClassName('webkit-html-attribute-name');
    for (var i = 0, len = attributeElements.length; i < len; ++i) {
      if (attributeElements[i].textContent === attributeName) {
        for (var elem = attributeElements[i].nextSibling; elem; elem = elem.nextSibling) {
          if (elem.nodeType !== Node.ELEMENT_NODE)
            continue;

          if (elem.classList.contains('webkit-html-attribute-value'))
            return this._startEditingAttribute(elem.parentNode, elem);
        }
      }
    }
  }

  _startEditingAttribute(attribute, elementForSelection) {
    console.assert(this.listItemElement.isAncestor(attribute));

    if (UI.isBeingEdited(attribute))
      return true;

    var attributeNameElement = attribute.getElementsByClassName('webkit-html-attribute-name')[0];
    if (!attributeNameElement)
      return false;

    var attributeName = attributeNameElement.textContent;
    var attributeValueElement = attribute.getElementsByClassName('webkit-html-attribute-value')[0];

    // Make sure elementForSelection is not a child of attributeValueElement.
    elementForSelection =
        attributeValueElement.isAncestor(elementForSelection) ? attributeValueElement : elementForSelection;

    function removeZeroWidthSpaceRecursive(node) {
      if (node.nodeType === Node.TEXT_NODE) {
        node.nodeValue = node.nodeValue.replace(/\u200B/g, '');
        return;
      }

      if (node.nodeType !== Node.ELEMENT_NODE)
        return;

      for (var child = node.firstChild; child; child = child.nextSibling)
        removeZeroWidthSpaceRecursive(child);
    }

    var attributeValue = attributeName && attributeValueElement ? this._node.getAttribute(attributeName) : undefined;
    if (attributeValue !== undefined) {
      attributeValueElement.setTextContentTruncatedIfNeeded(
          attributeValue, Common.UIString('<value is too large to edit>'));
    }

    // Remove zero-width spaces that were added by nodeTitleInfo.
    removeZeroWidthSpaceRecursive(attribute);

    var config = new UI.InplaceEditor.Config(
        this._attributeEditingCommitted.bind(this), this._editingCancelled.bind(this), attributeName);

    /**
     * @param {!Event} event
     * @return {string}
     */
    function postKeyDownFinishHandler(event) {
      UI.handleElementValueModifications(event, attribute);
      return '';
    }

    if (!attributeValueElement.textContent.asParsedURL())
      config.setPostKeydownFinishHandler(postKeyDownFinishHandler);

    this._editing = UI.InplaceEditor.startEditing(attribute, config);

    this.listItemElement.getComponentSelection().selectAllChildren(elementForSelection);

    return true;
  }

  /**
   * @param {!Element} textNodeElement
   */
  _startEditingTextNode(textNodeElement) {
    if (UI.isBeingEdited(textNodeElement))
      return true;

    var textNode = this._node;
    // We only show text nodes inline in elements if the element only
    // has a single child, and that child is a text node.
    if (textNode.nodeType() === Node.ELEMENT_NODE && textNode.firstChild)
      textNode = textNode.firstChild;

    var container = textNodeElement.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (container)
      container.textContent = textNode.nodeValue();  // Strip the CSS or JS highlighting if present.
    var config = new UI.InplaceEditor.Config(
        this._textNodeEditingCommitted.bind(this, textNode), this._editingCancelled.bind(this));
    this._editing = UI.InplaceEditor.startEditing(textNodeElement, config);
    this.listItemElement.getComponentSelection().selectAllChildren(textNodeElement);

    return true;
  }

  /**
   * @param {!Element=} tagNameElement
   */
  _startEditingTagName(tagNameElement) {
    if (!tagNameElement) {
      tagNameElement = this.listItemElement.getElementsByClassName('webkit-html-tag-name')[0];
      if (!tagNameElement)
        return false;
    }

    var tagName = tagNameElement.textContent;
    if (Elements.ElementsTreeElement.EditTagBlacklist.has(tagName.toLowerCase()))
      return false;

    if (UI.isBeingEdited(tagNameElement))
      return true;

    var closingTagElement = this._distinctClosingTagElement();

    /**
     * @param {!Event} event
     */
    function keyupListener(event) {
      if (closingTagElement)
        closingTagElement.textContent = '</' + tagNameElement.textContent + '>';
    }

    /**
     * @param {!Element} element
     * @param {string} newTagName
     * @this {Elements.ElementsTreeElement}
     */
    function editingComitted(element, newTagName) {
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      this._tagNameEditingCommitted.apply(this, arguments);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function editingCancelled() {
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      this._editingCancelled.apply(this, arguments);
    }

    tagNameElement.addEventListener('keyup', keyupListener, false);

    var config = new UI.InplaceEditor.Config(editingComitted.bind(this), editingCancelled.bind(this), tagName);
    this._editing = UI.InplaceEditor.startEditing(tagNameElement, config);
    this.listItemElement.getComponentSelection().selectAllChildren(tagNameElement);
    return true;
  }

  /**
   * @param {function(string, string)} commitCallback
   * @param {function()} disposeCallback
   * @param {?string} maybeInitialValue
   */
  _startEditingAsHTML(commitCallback, disposeCallback, maybeInitialValue) {
    if (maybeInitialValue === null)
      return;
    var initialValue = maybeInitialValue;  // To suppress a compiler warning.
    if (this._editing)
      return;

    function consume(event) {
      if (event.eventPhase === Event.AT_TARGET)
        event.consume(true);
    }

    initialValue = this._convertWhitespaceToEntities(initialValue).text;

    this._htmlEditElement = createElement('div');
    this._htmlEditElement.className = 'source-code elements-tree-editor';

    // Hide header items.
    var child = this.listItemElement.firstChild;
    while (child) {
      child.style.display = 'none';
      child = child.nextSibling;
    }
    // Hide children item.
    if (this.childrenListElement)
      this.childrenListElement.style.display = 'none';
    // Append editor.
    this.listItemElement.appendChild(this._htmlEditElement);
    this.treeOutline.element.addEventListener('mousedown', consume, false);

    self.runtime.extension(UI.TextEditorFactory).instance().then(gotFactory.bind(this));

    /**
     * @param {!UI.TextEditorFactory} factory
     * @this {Elements.ElementsTreeElement}
     */
    function gotFactory(factory) {
      var editor = factory.createEditor({
        lineNumbers: false,
        lineWrapping: Common.moduleSetting('domWordWrap').get(),
        mimeType: 'text/html',
        autoHeight: false,
        padBottom: false
      });
      this._editing =
          {commit: commit.bind(this), cancel: dispose.bind(this), editor: editor, resize: resize.bind(this)};
      resize.call(this);

      editor.widget().show(this._htmlEditElement);
      editor.setText(initialValue);
      editor.widget().focus();
      editor.widget().element.addEventListener('blur', this._editing.commit, true);
      editor.widget().element.addEventListener('keydown', keydown.bind(this), true);

      this.treeOutline.setMultilineEditing(this._editing);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function resize() {
      this._htmlEditElement.style.width = this.treeOutline.visibleWidth() - this._computeLeftIndent() - 30 + 'px';
      this._editing.editor.onResize();
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function commit() {
      commitCallback(initialValue, this._editing.editor.text());
      dispose.call(this);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function dispose() {
      this._editing.editor.widget().element.removeEventListener('blur', this._editing.commit, true);
      this._editing.editor.widget().detach();
      delete this._editing;

      // Remove editor.
      this.listItemElement.removeChild(this._htmlEditElement);
      delete this._htmlEditElement;
      // Unhide children item.
      if (this.childrenListElement)
        this.childrenListElement.style.removeProperty('display');
      // Unhide header items.
      var child = this.listItemElement.firstChild;
      while (child) {
        child.style.removeProperty('display');
        child = child.nextSibling;
      }

      if (this.treeOutline) {
        this.treeOutline.setMultilineEditing(null);
        this.treeOutline.element.removeEventListener('mousedown', consume, false);
        this.treeOutline.focus();
      }

      disposeCallback();
    }

    /**
     * @param {!Event} event
     * @this {!Elements.ElementsTreeElement}
     */
    function keydown(event) {
      var isMetaOrCtrl = UI.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!KeyboardEvent} */ (event)) &&
          !event.altKey && !event.shiftKey;
      if (isEnterKey(event) && (isMetaOrCtrl || event.isMetaOrCtrlForTest)) {
        event.consume(true);
        this._editing.commit();
      } else if (event.keyCode === UI.KeyboardShortcut.Keys.Esc.code || event.key === 'Escape') {
        event.consume(true);
        this._editing.cancel();
      }
    }
  }

  _attributeEditingCommitted(element, newText, oldText, attributeName, moveDirection) {
    delete this._editing;

    var treeOutline = this.treeOutline;

    /**
     * @param {?Protocol.Error=} error
     * @this {Elements.ElementsTreeElement}
     */
    function moveToNextAttributeIfNeeded(error) {
      if (error)
        this._editingCancelled(element, attributeName);

      if (!moveDirection)
        return;

      treeOutline.runPendingUpdates();
      treeOutline.focus();

      // Search for the attribute's position, and then decide where to move to.
      var attributes = this._node.attributes();
      for (var i = 0; i < attributes.length; ++i) {
        if (attributes[i].name !== attributeName)
          continue;

        if (moveDirection === 'backward') {
          if (i === 0)
            this._startEditingTagName();
          else
            this._triggerEditAttribute(attributes[i - 1].name);
        } else {
          if (i === attributes.length - 1)
            this._addNewAttribute();
          else
            this._triggerEditAttribute(attributes[i + 1].name);
        }
        return;
      }

      // Moving From the "New Attribute" position.
      if (moveDirection === 'backward') {
        if (newText === ' ') {
          // Moving from "New Attribute" that was not edited
          if (attributes.length > 0)
            this._triggerEditAttribute(attributes[attributes.length - 1].name);
        } else {
          // Moving from "New Attribute" that holds new value
          if (attributes.length > 1)
            this._triggerEditAttribute(attributes[attributes.length - 2].name);
        }
      } else if (moveDirection === 'forward') {
        if (!newText.isWhitespace())
          this._addNewAttribute();
        else
          this._startEditingTagName();
      }
    }

    if ((attributeName.trim() || newText.trim()) && oldText !== newText) {
      this._node.setAttribute(attributeName, newText, moveToNextAttributeIfNeeded.bind(this));
      return;
    }

    this.updateTitle();
    moveToNextAttributeIfNeeded.call(this);
  }

  _tagNameEditingCommitted(element, newText, oldText, tagName, moveDirection) {
    delete this._editing;
    var self = this;

    function cancel() {
      var closingTagElement = self._distinctClosingTagElement();
      if (closingTagElement)
        closingTagElement.textContent = '</' + tagName + '>';

      self._editingCancelled(element, tagName);
      moveToNextAttributeIfNeeded.call(self);
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function moveToNextAttributeIfNeeded() {
      if (moveDirection !== 'forward') {
        this._addNewAttribute();
        return;
      }

      var attributes = this._node.attributes();
      if (attributes.length > 0)
        this._triggerEditAttribute(attributes[0].name);
      else
        this._addNewAttribute();
    }

    newText = newText.trim();
    if (newText === oldText) {
      cancel();
      return;
    }

    var treeOutline = this.treeOutline;
    var wasExpanded = this.expanded;

    function changeTagNameCallback(error, nodeId) {
      if (error || !nodeId) {
        cancel();
        return;
      }
      var newTreeItem = treeOutline.selectNodeAfterEdit(wasExpanded, error, nodeId);
      moveToNextAttributeIfNeeded.call(newTreeItem);
    }
    this._node.setNodeName(newText, changeTagNameCallback);
  }

  /**
   * @param {!SDK.DOMNode} textNode
   * @param {!Element} element
   * @param {string} newText
   */
  _textNodeEditingCommitted(textNode, element, newText) {
    delete this._editing;

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function callback() {
      this.updateTitle();
    }
    textNode.setNodeValue(newText, callback.bind(this));
  }

  /**
   * @param {!Element} element
   * @param {*} context
   */
  _editingCancelled(element, context) {
    delete this._editing;

    // Need to restore attributes structure.
    this.updateTitle();
  }

  /**
   * @return {!Element}
   */
  _distinctClosingTagElement() {
    // FIXME: Improve the Tree Element / Outline Abstraction to prevent crawling the DOM

    // For an expanded element, it will be the last element with class "close"
    // in the child element list.
    if (this.expanded) {
      var closers = this.childrenListElement.querySelectorAll('.close');
      return closers[closers.length - 1];
    }

    // Remaining cases are single line non-expanded elements with a closing
    // tag, or HTML elements without a closing tag (such as <br>). Return
    // null in the case where there isn't a closing tag.
    var tags = this.listItemElement.getElementsByClassName('webkit-html-tag');
    return (tags.length === 1 ? null : tags[tags.length - 1]);
  }

  /**
   * @param {?Elements.ElementsTreeOutline.UpdateRecord=} updateRecord
   * @param {boolean=} onlySearchQueryChanged
   */
  updateTitle(updateRecord, onlySearchQueryChanged) {
    // If we are editing, return early to prevent canceling the edit.
    // After editing is committed updateTitle will be called.
    if (this._editing)
      return;

    if (onlySearchQueryChanged) {
      this._hideSearchHighlight();
    } else {
      var nodeInfo = this._nodeTitleInfo(updateRecord || null);
      if (this._node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE && this._node.isInShadowTree() &&
          this._node.shadowRootType()) {
        this.childrenListElement.classList.add('shadow-root');
        var depth = 4;
        for (var node = this._node; depth && node; node = node.parentNode) {
          if (node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE)
            depth--;
        }
        if (!depth)
          this.childrenListElement.classList.add('shadow-root-deep');
        else
          this.childrenListElement.classList.add('shadow-root-depth-' + depth);
      }
      var highlightElement = createElement('span');
      highlightElement.className = 'highlight';
      highlightElement.appendChild(nodeInfo);
      this.title = highlightElement;
      this.updateDecorations();
      this.listItemElement.insertBefore(this._gutterContainer, this.listItemElement.firstChild);
      delete this._highlightResult;
      delete this.selectionElement;
      delete this._hintElement;
      if (this.selected) {
        this._createSelection();
        this._createHint();
      }
    }

    this._highlightSearchResults();
  }

  /**
   * @return {number}
   */
  _computeLeftIndent() {
    var treeElement = this.parent;
    var depth = 0;
    while (treeElement !== null) {
      depth++;
      treeElement = treeElement.parent;
    }

    /** Keep it in sync with elementsTreeOutline.css **/
    return 12 * (depth - 2) + (this.isExpandable() ? 1 : 12);
  }

  updateDecorations() {
    this._gutterContainer.style.left = (-this._computeLeftIndent()) + 'px';

    if (this.isClosingTag())
      return;

    if (this._node.nodeType() !== Node.ELEMENT_NODE)
      return;

    this._decorationsThrottler.schedule(this._updateDecorationsInternal.bind(this));
  }

  /**
   * @return {!Promise}
   */
  _updateDecorationsInternal() {
    if (!this.treeOutline)
      return Promise.resolve();

    var node = this._node;

    if (!this.treeOutline._decoratorExtensions)
      /** @type {!Array.<!Runtime.Extension>} */
      this.treeOutline._decoratorExtensions = runtime.extensions(Components.DOMPresentationUtils.MarkerDecorator);

    var markerToExtension = new Map();
    for (var i = 0; i < this.treeOutline._decoratorExtensions.length; ++i) {
      markerToExtension.set(
          this.treeOutline._decoratorExtensions[i].descriptor()['marker'], this.treeOutline._decoratorExtensions[i]);
    }

    var promises = [];
    var decorations = [];
    var descendantDecorations = [];
    node.traverseMarkers(visitor);

    /**
     * @param {!SDK.DOMNode} n
     * @param {string} marker
     */
    function visitor(n, marker) {
      var extension = markerToExtension.get(marker);
      if (!extension)
        return;
      promises.push(extension.instance().then(collectDecoration.bind(null, n)));
    }

    /**
     * @param {!SDK.DOMNode} n
     * @param {!Components.DOMPresentationUtils.MarkerDecorator} decorator
     */
    function collectDecoration(n, decorator) {
      var decoration = decorator.decorate(n);
      if (!decoration)
        return;
      (n === node ? decorations : descendantDecorations).push(decoration);
    }

    return Promise.all(promises).then(updateDecorationsUI.bind(this));

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function updateDecorationsUI() {
      this._decorationsElement.removeChildren();
      this._decorationsElement.classList.add('hidden');
      this._gutterContainer.classList.toggle('has-decorations', decorations.length || descendantDecorations.length);

      if (!decorations.length && !descendantDecorations.length)
        return;

      var colors = new Set();
      var titles = createElement('div');

      for (var decoration of decorations) {
        var titleElement = titles.createChild('div');
        titleElement.textContent = decoration.title;
        colors.add(decoration.color);
      }
      if (this.expanded && !decorations.length)
        return;

      var descendantColors = new Set();
      if (descendantDecorations.length) {
        var element = titles.createChild('div');
        element.textContent = Common.UIString('Children:');
        for (var decoration of descendantDecorations) {
          element = titles.createChild('div');
          element.style.marginLeft = '15px';
          element.textContent = decoration.title;
          descendantColors.add(decoration.color);
        }
      }

      var offset = 0;
      processColors.call(this, colors, 'elements-gutter-decoration');
      if (!this.expanded)
        processColors.call(this, descendantColors, 'elements-gutter-decoration elements-has-decorated-children');
      UI.Tooltip.install(this._decorationsElement, titles);

      /**
       * @param {!Set<string>} colors
       * @param {string} className
       * @this {Elements.ElementsTreeElement}
       */
      function processColors(colors, className) {
        for (var color of colors) {
          var child = this._decorationsElement.createChild('div', className);
          this._decorationsElement.classList.remove('hidden');
          child.style.backgroundColor = color;
          child.style.borderColor = color;
          if (offset)
            child.style.marginLeft = offset + 'px';
          offset += 3;
        }
      }
    }
  }

  /**
   * @param {!Node} parentElement
   * @param {string} name
   * @param {string} value
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   * @param {boolean=} forceValue
   * @param {!SDK.DOMNode=} node
   */
  _buildAttributeDOM(parentElement, name, value, updateRecord, forceValue, node) {
    var closingPunctuationRegex = /[\/;:\)\]\}]/g;
    var highlightIndex = 0;
    var highlightCount;
    var additionalHighlightOffset = 0;
    var result;

    /**
     * @param {string} match
     * @param {number} replaceOffset
     * @return {string}
     */
    function replacer(match, replaceOffset) {
      while (highlightIndex < highlightCount && result.entityRanges[highlightIndex].offset < replaceOffset) {
        result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
        ++highlightIndex;
      }
      additionalHighlightOffset += 1;
      return match + '\u200B';
    }

    /**
     * @param {!Element} element
     * @param {string} value
     * @this {Elements.ElementsTreeElement}
     */
    function setValueWithEntities(element, value) {
      result = this._convertWhitespaceToEntities(value);
      highlightCount = result.entityRanges.length;
      value = result.text.replace(closingPunctuationRegex, replacer);
      while (highlightIndex < highlightCount) {
        result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
        ++highlightIndex;
      }
      element.setTextContentTruncatedIfNeeded(value);
      UI.highlightRangesWithStyleClass(element, result.entityRanges, 'webkit-html-entity-value');
    }

    var hasText = (forceValue || value.length > 0);
    var attrSpanElement = parentElement.createChild('span', 'webkit-html-attribute');
    var attrNameElement = attrSpanElement.createChild('span', 'webkit-html-attribute-name');
    attrNameElement.textContent = name;

    if (hasText)
      attrSpanElement.createTextChild('=\u200B"');

    var attrValueElement = attrSpanElement.createChild('span', 'webkit-html-attribute-value');

    if (updateRecord && updateRecord.isAttributeModified(name))
      UI.runCSSAnimationOnce(hasText ? attrValueElement : attrNameElement, 'dom-update-highlight');

    /**
     * @this {Elements.ElementsTreeElement}
     * @param {string} value
     * @return {!Element}
     */
    function linkifyValue(value) {
      var rewrittenHref = node.resolveURL(value);
      if (rewrittenHref === null) {
        var span = createElement('span');
        setValueWithEntities.call(this, span, value);
        return span;
      }
      value = value.replace(closingPunctuationRegex, '$&\u200B');
      if (value.startsWith('data:'))
        value = value.trimMiddle(60);
      var link = node.nodeName().toLowerCase() === 'a' ?
          UI.createExternalLink(rewrittenHref, value, '', true) :
          Components.Linkifier.linkifyURL(rewrittenHref, {text: value, preventClick: true});
      link[Elements.ElementsTreeElement.HrefSymbol] = rewrittenHref;
      return link;
    }

    var nodeName = node ? node.nodeName().toLowerCase() : '';
    if (nodeName && (name === 'src' || name === 'href'))
      attrValueElement.appendChild(linkifyValue.call(this, value));
    else if ((nodeName === 'img' || nodeName === 'source') && name === 'srcset')
      attrValueElement.appendChild(linkifySrcset.call(this, value));
    else
      setValueWithEntities.call(this, attrValueElement, value);

    if (hasText)
      attrSpanElement.createTextChild('"');

    /**
     * @param {string} value
     * @return {!DocumentFragment}
     * @this {!Elements.ElementsTreeElement}
     */
    function linkifySrcset(value) {
      // Splitting normally on commas or spaces will break on valid srcsets "foo 1x,bar 2x" and "data:,foo 1x".
      // 1) Let the index of the next space be `indexOfSpace`.
      // 2a) If the character at `indexOfSpace - 1` is a comma, collect the preceding characters up to
      //     `indexOfSpace - 1` as a URL and repeat step 1).
      // 2b) Else, collect the preceding characters as a URL.
      // 3) Collect the characters from `indexOfSpace` up to the next comma as the size descriptor and repeat step 1).
      // https://html.spec.whatwg.org/multipage/embedded-content.html#parse-a-srcset-attribute
      var fragment = createDocumentFragment();
      var i = 0;
      while (value.length) {
        if (i++ > 0)
          fragment.createTextChild(' ');
        value = value.trim();
        // The url and descriptor may end with a separating comma.
        var url = '';
        var descriptor = '';
        var indexOfSpace = value.search(/\s/);
        if (indexOfSpace === -1) {
          url = value;
        } else if (indexOfSpace > 0 && value[indexOfSpace - 1] === ',') {
          url = value.substring(0, indexOfSpace);
        } else {
          url = value.substring(0, indexOfSpace);
          var indexOfComma = value.indexOf(',', indexOfSpace);
          if (indexOfComma !== -1)
            descriptor = value.substring(indexOfSpace, indexOfComma + 1);
          else
            descriptor = value.substring(indexOfSpace);
        }

        if (url) {
          // Up to one trailing comma should be removed from `url`.
          if (url.endsWith(',')) {
            fragment.appendChild(linkifyValue.call(this, url.substring(0, url.length - 1)));
            fragment.createTextChild(',');
          } else {
            fragment.appendChild(linkifyValue.call(this, url));
          }
        }
        if (descriptor)
          fragment.createTextChild(descriptor);
        value = value.substring(url.length + descriptor.length);
      }
      return fragment;
    }
  }

  /**
   * @param {!Node} parentElement
   * @param {string} pseudoElementName
   */
  _buildPseudoElementDOM(parentElement, pseudoElementName) {
    var pseudoElement = parentElement.createChild('span', 'webkit-html-pseudo-element');
    pseudoElement.textContent = '::' + pseudoElementName;
    parentElement.createTextChild('\u200B');
  }

  /**
   * @param {!Node} parentElement
   * @param {string} tagName
   * @param {boolean} isClosingTag
   * @param {boolean} isDistinctTreeElement
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   */
  _buildTagDOM(parentElement, tagName, isClosingTag, isDistinctTreeElement, updateRecord) {
    var node = this._node;
    var classes = ['webkit-html-tag'];
    if (isClosingTag && isDistinctTreeElement)
      classes.push('close');
    var tagElement = parentElement.createChild('span', classes.join(' '));
    tagElement.createTextChild('<');
    var tagNameElement =
        tagElement.createChild('span', isClosingTag ? 'webkit-html-close-tag-name' : 'webkit-html-tag-name');
    tagNameElement.textContent = (isClosingTag ? '/' : '') + tagName;
    if (!isClosingTag) {
      if (node.hasAttributes()) {
        var attributes = node.attributes();
        for (var i = 0; i < attributes.length; ++i) {
          var attr = attributes[i];
          tagElement.createTextChild(' ');
          this._buildAttributeDOM(tagElement, attr.name, attr.value, updateRecord, false, node);
        }
      }
      if (updateRecord) {
        var hasUpdates = updateRecord.hasRemovedAttributes() || updateRecord.hasRemovedChildren();
        hasUpdates |= !this.expanded && updateRecord.hasChangedChildren();
        if (hasUpdates)
          UI.runCSSAnimationOnce(tagNameElement, 'dom-update-highlight');
      }
    }

    tagElement.createTextChild('>');
    parentElement.createTextChild('\u200B');
  }

  /**
   * @param {string} text
   * @return {!{text: string, entityRanges: !Array.<!TextUtils.SourceRange>}}
   */
  _convertWhitespaceToEntities(text) {
    var result = '';
    var lastIndexAfterEntity = 0;
    var entityRanges = [];
    var charToEntity = Elements.ElementsTreeOutline.MappedCharToEntity;
    for (var i = 0, size = text.length; i < size; ++i) {
      var char = text.charAt(i);
      if (charToEntity[char]) {
        result += text.substring(lastIndexAfterEntity, i);
        var entityValue = '&' + charToEntity[char] + ';';
        entityRanges.push({offset: result.length, length: entityValue.length});
        result += entityValue;
        lastIndexAfterEntity = i + 1;
      }
    }
    if (result)
      result += text.substring(lastIndexAfterEntity);
    return {text: result || text, entityRanges: entityRanges};
  }

  /**
   * @param {?Elements.ElementsTreeOutline.UpdateRecord} updateRecord
   * @return {!DocumentFragment} result
   */
  _nodeTitleInfo(updateRecord) {
    var node = this._node;
    var titleDOM = createDocumentFragment();

    switch (node.nodeType()) {
      case Node.ATTRIBUTE_NODE:
        this._buildAttributeDOM(
            titleDOM, /** @type {string} */ (node.name), /** @type {string} */ (node.value), updateRecord, true);
        break;

      case Node.ELEMENT_NODE:
        var pseudoType = node.pseudoType();
        if (pseudoType) {
          this._buildPseudoElementDOM(titleDOM, pseudoType);
          break;
        }

        var tagName = node.nodeNameInCorrectCase();
        if (this._elementCloseTag) {
          this._buildTagDOM(titleDOM, tagName, true, true, updateRecord);
          break;
        }

        this._buildTagDOM(titleDOM, tagName, false, false, updateRecord);

        if (this.isExpandable()) {
          if (!this.expanded) {
            var textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node bogus');
            textNodeElement.textContent = '\u2026';
            titleDOM.createTextChild('\u200B');
            this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          }
          break;
        }

        if (Elements.ElementsTreeElement.canShowInlineText(node)) {
          var textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          var result = this._convertWhitespaceToEntities(node.firstChild.nodeValue());
          textNodeElement.textContent = result.text;
          UI.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          titleDOM.createTextChild('\u200B');
          this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          if (updateRecord && updateRecord.hasChangedChildren())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          if (updateRecord && updateRecord.isCharDataModified())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          break;
        }

        if (this.treeOutline.isXMLMimeType || !Elements.ElementsTreeElement.ForbiddenClosingTagElements.has(tagName))
          this._buildTagDOM(titleDOM, tagName, true, false, updateRecord);
        break;

      case Node.TEXT_NODE:
        if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'script') {
          var newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-js-node');
          var text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          var javascriptSyntaxHighlighter = new UI.SyntaxHighlighter('text/javascript', true);
          javascriptSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight.bind(this));
        } else if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'style') {
          var newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-css-node');
          var text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          var cssSyntaxHighlighter = new UI.SyntaxHighlighter('text/css', true);
          cssSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight.bind(this));
        } else {
          titleDOM.createTextChild('"');
          var textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          var result = this._convertWhitespaceToEntities(node.nodeValue());
          textNodeElement.textContent = result.text;
          UI.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          titleDOM.createTextChild('"');
          if (updateRecord && updateRecord.isCharDataModified())
            UI.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
        }
        break;

      case Node.COMMENT_NODE:
        var commentElement = titleDOM.createChild('span', 'webkit-html-comment');
        commentElement.createTextChild('<!--' + node.nodeValue() + '-->');
        break;

      case Node.DOCUMENT_TYPE_NODE:
        var docTypeElement = titleDOM.createChild('span', 'webkit-html-doctype');
        docTypeElement.createTextChild('<!DOCTYPE ' + node.nodeName());
        if (node.publicId) {
          docTypeElement.createTextChild(' PUBLIC "' + node.publicId + '"');
          if (node.systemId)
            docTypeElement.createTextChild(' "' + node.systemId + '"');
        } else if (node.systemId) {
          docTypeElement.createTextChild(' SYSTEM "' + node.systemId + '"');
        }

        if (node.internalSubset)
          docTypeElement.createTextChild(' [' + node.internalSubset + ']');

        docTypeElement.createTextChild('>');
        break;

      case Node.CDATA_SECTION_NODE:
        var cdataElement = titleDOM.createChild('span', 'webkit-html-text-node');
        cdataElement.createTextChild('<![CDATA[' + node.nodeValue() + ']]>');
        break;

      case Node.DOCUMENT_FRAGMENT_NODE:
        var fragmentElement = titleDOM.createChild('span', 'webkit-html-fragment');
        fragmentElement.textContent = node.nodeNameInCorrectCase().collapseWhitespace();
        break;
      default:
        titleDOM.createTextChild(node.nodeNameInCorrectCase().collapseWhitespace());
    }

    /**
     * @this {Elements.ElementsTreeElement}
     */
    function updateSearchHighlight() {
      delete this._highlightResult;
      this._highlightSearchResults();
    }

    return titleDOM;
  }

  remove() {
    if (this._node.pseudoType())
      return;
    var parentElement = this.parent;
    if (!parentElement)
      return;

    if (!this._node.parentNode || this._node.parentNode.nodeType() === Node.DOCUMENT_NODE)
      return;
    this._node.removeNode();
  }

  /**
   * @param {function(boolean)=} callback
   * @param {boolean=} startEditing
   */
  toggleEditAsHTML(callback, startEditing) {
    if (this._editing && this._htmlEditElement) {
      this._editing.commit();
      return;
    }

    if (startEditing === false)
      return;

    /**
     * @param {?Protocol.Error} error
     */
    function selectNode(error) {
      if (callback)
        callback(!error);
    }

    /**
     * @param {string} initialValue
     * @param {string} value
     */
    function commitChange(initialValue, value) {
      if (initialValue !== value)
        node.setOuterHTML(value, selectNode);
    }

    function disposeCallback() {
      if (callback)
        callback(false);
    }

    var node = this._node;
    node.getOuterHTML().then(this._startEditingAsHTML.bind(this, commitChange, disposeCallback));
  }

  _copyCSSPath() {
    InspectorFrontendHost.copyText(Components.DOMPresentationUtils.cssPath(this._node, true));
  }

  _copyXPath() {
    InspectorFrontendHost.copyText(Components.DOMPresentationUtils.xPath(this._node, true));
  }

  _highlightSearchResults() {
    if (!this._searchQuery || !this._searchHighlightsVisible)
      return;
    this._hideSearchHighlight();

    var text = this.listItemElement.textContent;
    var regexObject = createPlainTextSearchRegex(this._searchQuery, 'gi');

    var match = regexObject.exec(text);
    var matchRanges = [];
    while (match) {
      matchRanges.push(new TextUtils.SourceRange(match.index, match[0].length));
      match = regexObject.exec(text);
    }

    // Fall back for XPath, etc. matches.
    if (!matchRanges.length)
      matchRanges.push(new TextUtils.SourceRange(0, text.length));

    this._highlightResult = [];
    UI.highlightSearchResults(this.listItemElement, matchRanges, this._highlightResult);
  }

  _scrollIntoView() {
    function scrollIntoViewCallback(object) {
      /**
       * @suppressReceiverCheck
       * @this {!Element}
       */
      function scrollIntoView() {
        this.scrollIntoViewIfNeeded(true);
      }

      if (object)
        object.callFunction(scrollIntoView);
    }

    this._node.resolveToObject('', scrollIntoViewCallback);
  }

  _editAsHTML() {
    var promise = Common.Revealer.revealPromise(this.node());
    promise.then(() => UI.actionRegistry.action('elements.edit-as-html').execute());
  }
};

Elements.ElementsTreeElement.HrefSymbol = Symbol('ElementsTreeElement.Href');

Elements.ElementsTreeElement.InitialChildrenLimit = 500;

// A union of HTML4 and HTML5-Draft elements that explicitly
// or implicitly (for HTML5) forbid the closing tag.
Elements.ElementsTreeElement.ForbiddenClosingTagElements = new Set([
  'area', 'base',  'basefont', 'br',   'canvas',   'col',  'command', 'embed',  'frame', 'hr',
  'img',  'input', 'keygen',   'link', 'menuitem', 'meta', 'param',   'source', 'track', 'wbr'
]);

// These tags we do not allow editing their tag name.
Elements.ElementsTreeElement.EditTagBlacklist = new Set(['html', 'head', 'body']);

/** @typedef {{cancel: function(), commit: function(), resize: function(), editor:!UI.TextEditor}} */
Elements.MultilineEditorController;
