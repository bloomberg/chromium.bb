// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as TextUtils from '../../models/text_utils/text_utils.js';
import * as Adorners from '../../ui/components/adorners/adorners.js';
import type * as TextEditor from '../../ui/components/text_editor/text_editor.js';
import * as TextEditorLegacy from '../../ui/legacy/components/text_editor/text_editor.js';
import * as Components from '../../ui/legacy/components/utils/utils.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Emulation from '../emulation/emulation.js';

import * as ElementsComponents from './components/components.js';
import {canGetJSPath, cssPath, jsPath, xPath} from './DOMPath.js';
import {ElementsPanel} from './ElementsPanel.js';

import type {ElementsTreeOutline, UpdateRecord} from './ElementsTreeOutline.js';
import {MappedCharToEntity} from './ElementsTreeOutline.js';
import {ImagePreviewPopover} from './ImagePreviewPopover.js';
import type {MarkerDecorator, MarkerDecoratorRegistration} from './MarkerDecorator.js';
import {getRegisteredDecorators} from './MarkerDecorator.js';

const UIStrings = {
  /**
  *@description Title for Ad adorner. This iframe is marked as advertisement frame.
  */
  thisFrameWasIdentifiedAsAnAd: 'This frame was identified as an ad frame',
  /**
  *@description A context menu item in the Elements panel. Force is used as a verb, indicating intention to make the state change.
  */
  forceState: 'Force state',
  /**
  *@description Hint element title in Elements Tree Element of the Elements panel
  *@example {0} PH1
  */
  useSInTheConsoleToReferToThis: 'Use {PH1} in the console to refer to this element.',
  /**
  *@description A context menu item in the Elements Tree Element of the Elements panel
  */
  addAttribute: 'Add attribute',
  /**
  *@description Text to modify the attribute of an item
  */
  editAttribute: 'Edit attribute',
  /**
  *@description Text to focus on something
  */
  focus: 'Focus',
  /**
  *@description Text to scroll the displayed content into view
  */
  scrollIntoView: 'Scroll into view',
  /**
  *@description Text to enter Isolation Mode, a mode with focus on a single element and interactive resizing
  */
  enterIsolationMode: 'Enter Isolation Mode',
  /**
  *@description Text to exit Isolation Mode, a mode with focus on a single element and interactive resizing
  */
  exitIsolationMode: 'Exit Isolation Mode',
  /**
  *@description A context menu item in the Elements Tree Element of the Elements panel
  */
  editText: 'Edit text',
  /**
  *@description A context menu item in the Elements Tree Element of the Elements panel
  */
  editAsHtml: 'Edit as HTML',
  /**
  *@description Text to cut an element, cut should be used as a verb
  */
  cut: 'Cut',
  /**
  *@description Text for copying, copy should be used as a verb
  */
  copy: 'Copy',
  /**
  *@description Text to paste an element, paste should be used as a verb
  */
  paste: 'Paste',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copyOuterhtml: 'Copy outerHTML',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copySelector: 'Copy `selector`',
  /**
  *@description Text in Elements Tree Element of the Elements panel
  */
  copyJsPath: 'Copy JS path',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copyStyles: 'Copy styles',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copyXpath: 'Copy XPath',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copyFullXpath: 'Copy full XPath',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copyElement: 'Copy element',
  /**
  *@description A context menu item in the Elements Tree Element of the Elements panel
  */
  duplicateElement: 'Duplicate element',
  /**
  *@description Text to hide an element
  */
  hideElement: 'Hide element',
  /**
  *@description A context menu item in the Elements Tree Element of the Elements panel
  */
  deleteElement: 'Delete element',
  /**
  *@description Text to expand something recursively
  */
  expandRecursively: 'Expand recursively',
  /**
  *@description Text to collapse children of a parent group
  */
  collapseChildren: 'Collapse children',
  /**
  *@description Title of an action in the emulation tool to capture node screenshot
  */
  captureNodeScreenshot: 'Capture node screenshot',
  /**
  *@description Title of a context menu item. When clicked DevTools goes to the Application panel and shows this specific iframe's details
  */
  showFrameDetails: 'Show `iframe` details',
  /**
  *@description Text in Elements Tree Element of the Elements panel
  */
  valueIsTooLargeToEdit: '<value is too large to edit>',
  /**
  *@description Element text content in Elements Tree Element of the Elements panel
  */
  children: 'Children:',
  /**
  *@description ARIA label for Elements Tree adorners
  */
  enableGridMode: 'Enable grid mode',
  /**
  *@description ARIA label for Elements Tree adorners
  */
  disableGridMode: 'Disable grid mode',
  /**
  *@description Label of the adorner for flex elements in the Elements panel
  */
  enableFlexMode: 'Enable flex mode',
  /**
  *@description Label of the adorner for flex elements in the Elements panel
  */
  disableFlexMode: 'Disable flex mode',
  /**
  *@description Label of an adorner in the Elements panel. When clicked, it enables
  * the overlay showing CSS scroll snapping for the current element.
  */
  enableScrollSnap: 'Enable scroll-snap overlay',
  /**
  *@description Label of an adorner in the Elements panel. When clicked, it disables
  * the overlay showing CSS scroll snapping for the current element.
  */
  disableScrollSnap: 'Disable scroll-snap overlay',
};
const str_ = i18n.i18n.registerUIStrings('panels/elements/ElementsTreeElement.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export class ElementsTreeElement extends UI.TreeOutline.TreeElement {
  nodeInternal: SDK.DOMModel.DOMNode;
  treeOutline: ElementsTreeOutline|null;
  private gutterContainer: HTMLElement;
  private readonly decorationsElement: HTMLElement;
  private isClosingTagInternal: boolean|undefined;
  private readonly canAddAttributes: boolean|undefined;
  private searchQuery: string|null;
  private expandedChildrenLimitInternal: number;
  private readonly decorationsThrottler: Common.Throttler.Throttler;
  private inClipboard: boolean;
  private hoveredInternal: boolean;
  private editing: EditorHandles|null;
  private highlightResult: UI.UIUtils.HighlightChange[];
  private readonly adornerContainer: HTMLElement|undefined;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // @ts-expect-error
  private adorners: Adorners.Adorner.Adorner[];
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // @ts-expect-error
  private styleAdorners: Adorners.Adorner.Adorner[];
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // @ts-expect-error
  private readonly adornersThrottler: Common.Throttler.Throttler;
  private htmlEditElement!: HTMLElement|undefined;
  expandAllButtonElement: UI.TreeOutline.TreeElement|null;
  private searchHighlightsVisible?: boolean;
  selectionElement?: HTMLDivElement;
  private hintElement?: HTMLElement;

  constructor(node: SDK.DOMModel.DOMNode, isClosingTag?: boolean) {
    // The title will be updated in onattach.
    super();
    this.nodeInternal = node;
    this.treeOutline = null;

    this.gutterContainer = this.listItemElement.createChild('div', 'gutter-container');
    this.gutterContainer.addEventListener('click', this.showContextMenu.bind(this));
    const gutterMenuIcon = UI.Icon.Icon.create('largeicon-menu', 'gutter-menu-icon');
    this.gutterContainer.appendChild(gutterMenuIcon);
    this.decorationsElement = this.gutterContainer.createChild('div', 'hidden');

    this.isClosingTagInternal = isClosingTag;

    if (this.nodeInternal.nodeType() === Node.ELEMENT_NODE && !isClosingTag) {
      this.canAddAttributes = true;
    }
    this.searchQuery = null;
    this.expandedChildrenLimitInternal = InitialChildrenLimit;
    this.decorationsThrottler = new Common.Throttler.Throttler(100);

    this.inClipboard = false;
    this.hoveredInternal = false;

    this.editing = null;

    this.highlightResult = [];

    if (!isClosingTag) {
      this.adornerContainer = this.listItemElement.createChild('div', 'adorner-container hidden');
      this.adorners = [];
      this.styleAdorners = [];
      this.adornersThrottler = new Common.Throttler.Throttler(100);

      this.updateStyleAdorners();

      if (node.isAdFrameNode()) {
        const config = ElementsComponents.AdornerManager.getRegisteredAdorner(
            ElementsComponents.AdornerManager.RegisteredAdorners.AD);
        const adorner = this.adorn(config);
        UI.Tooltip.Tooltip.install(adorner, i18nString(UIStrings.thisFrameWasIdentifiedAsAnAd));
      }
    }

    this.expandAllButtonElement = null;
  }

  static animateOnDOMUpdate(treeElement: ElementsTreeElement): void {
    const tagName = treeElement.listItemElement.querySelector('.webkit-html-tag-name');
    UI.UIUtils.runCSSAnimationOnce(tagName || treeElement.listItemElement, 'dom-update-highlight');
  }

  static visibleShadowRoots(node: SDK.DOMModel.DOMNode): SDK.DOMModel.DOMNode[] {
    let roots = node.shadowRoots();
    if (roots.length && !Common.Settings.Settings.instance().moduleSetting('showUAShadowDOM').get()) {
      roots = roots.filter(filter);
    }

    function filter(root: SDK.DOMModel.DOMNode): boolean {
      return root.shadowRootType() !== SDK.DOMModel.DOMNode.ShadowRootTypes.UserAgent;
    }
    return roots;
  }

  static canShowInlineText(node: SDK.DOMModel.DOMNode): boolean {
    if (node.contentDocument() || node.templateContent() || ElementsTreeElement.visibleShadowRoots(node).length ||
        node.hasPseudoElements()) {
      return false;
    }
    if (node.nodeType() !== Node.ELEMENT_NODE) {
      return false;
    }
    if (!node.firstChild || node.firstChild !== node.lastChild || node.firstChild.nodeType() !== Node.TEXT_NODE) {
      return false;
    }
    const textChild = node.firstChild;
    const maxInlineTextChildLength = 80;
    if (textChild.nodeValue().length < maxInlineTextChildLength) {
      return true;
    }
    return false;
  }

  static populateForcedPseudoStateItems(contextMenu: UI.ContextMenu.ContextMenu, node: SDK.DOMModel.DOMNode): void {
    const pseudoClasses = ['active', 'hover', 'focus', 'visited', 'focus-within', 'focus-visible'];
    const forcedPseudoState = node.domModel().cssModel().pseudoState(node);
    const stateMenu = contextMenu.debugSection().appendSubMenuItem(i18nString(UIStrings.forceState));
    for (const pseudoClass of pseudoClasses) {
      const pseudoClassForced = forcedPseudoState ? forcedPseudoState.indexOf(pseudoClass) >= 0 : false;
      stateMenu.defaultSection().appendCheckboxItem(
          ':' + pseudoClass, setPseudoStateCallback.bind(null, pseudoClass, !pseudoClassForced), pseudoClassForced,
          false);
    }

    function setPseudoStateCallback(pseudoState: string, enabled: boolean): void {
      node.domModel().cssModel().forcePseudoState(node, pseudoState, enabled);
    }
  }

  isClosingTag(): boolean {
    return Boolean(this.isClosingTagInternal);
  }

  node(): SDK.DOMModel.DOMNode {
    return this.nodeInternal;
  }

  isEditing(): boolean {
    return Boolean(this.editing);
  }

  highlightSearchResults(searchQuery: string): void {
    if (this.searchQuery !== searchQuery) {
      this.hideSearchHighlight();
    }

    this.searchQuery = searchQuery;
    this.searchHighlightsVisible = true;
    this.updateTitle(null, true);
  }

  hideSearchHighlights(): void {
    delete this.searchHighlightsVisible;
    this.hideSearchHighlight();
  }

  private hideSearchHighlight(): void {
    if (this.highlightResult.length === 0) {
      return;
    }

    for (let i = (this.highlightResult.length - 1); i >= 0; --i) {
      const entry = this.highlightResult[i];
      switch (entry.type) {
        case 'added':
          entry.node.remove();
          break;
        case 'changed':
          entry.node.textContent = entry.oldText || null;
          break;
      }
    }

    this.highlightResult = [];
  }

  setInClipboard(inClipboard: boolean): void {
    if (this.inClipboard === inClipboard) {
      return;
    }
    this.inClipboard = inClipboard;
    this.listItemElement.classList.toggle('in-clipboard', inClipboard);
  }

  get hovered(): boolean {
    return this.hoveredInternal;
  }

  set hovered(isHovered: boolean) {
    if (this.hoveredInternal === isHovered) {
      return;
    }

    this.hoveredInternal = isHovered;

    if (this.listItemElement) {
      if (isHovered) {
        this.createSelection();
        this.listItemElement.classList.add('hovered');
      } else {
        this.listItemElement.classList.remove('hovered');
      }
    }
  }

  expandedChildrenLimit(): number {
    return this.expandedChildrenLimitInternal;
  }

  setExpandedChildrenLimit(expandedChildrenLimit: number): void {
    this.expandedChildrenLimitInternal = expandedChildrenLimit;
  }

  private createSelection(): void {
    const listItemElement = this.listItemElement;
    if (!listItemElement) {
      return;
    }

    if (!this.selectionElement) {
      this.selectionElement = document.createElement('div');
      this.selectionElement.className = 'selection fill';
      this.selectionElement.style.setProperty('margin-left', (-this.computeLeftIndent()) + 'px');
      listItemElement.insertBefore(this.selectionElement, listItemElement.firstChild);
    }
  }

  private createHint(): void {
    if (this.listItemElement && !this.hintElement) {
      this.hintElement = this.listItemElement.createChild('span', 'selected-hint');
      const selectedElementCommand = '$0';
      UI.Tooltip.Tooltip.install(
          this.hintElement, i18nString(UIStrings.useSInTheConsoleToReferToThis, {PH1: selectedElementCommand}));
      UI.ARIAUtils.markAsHidden(this.hintElement);
    }
  }

  onbind(): void {
    if (this.treeOutline && !this.isClosingTagInternal) {
      this.treeOutline.treeElementByNode.set(this.nodeInternal, this);
    }
  }

  onunbind(): void {
    if (this.editing) {
      this.editing.cancel();
    }
    if (this.treeOutline && this.treeOutline.treeElementByNode.get(this.nodeInternal) === this) {
      this.treeOutline.treeElementByNode.delete(this.nodeInternal);
    }
  }

  onattach(): void {
    if (this.hoveredInternal) {
      this.createSelection();
      this.listItemElement.classList.add('hovered');
    }

    this.updateTitle();
    this.listItemElement.draggable = true;
  }

  async onpopulate(): Promise<void> {
    if (this.treeOutline) {
      return this.treeOutline.populateTreeElement(this);
    }
  }

  async expandRecursively(): Promise<void> {
    await this.nodeInternal.getSubtree(-1, true);
    await super.expandRecursively(Number.MAX_VALUE);
  }

  onexpand(): void {
    if (this.isClosingTagInternal) {
      return;
    }

    this.updateTitle();
  }

  oncollapse(): void {
    if (this.isClosingTagInternal) {
      return;
    }

    this.updateTitle();
  }

  select(omitFocus?: boolean, selectedByUser?: boolean): boolean {
    if (this.editing) {
      return false;
    }
    return super.select(omitFocus, selectedByUser);
  }

  onselect(selectedByUser?: boolean): boolean {
    if (!this.treeOutline) {
      return false;
    }
    this.treeOutline.suppressRevealAndSelect = true;
    this.treeOutline.selectDOMNode(this.nodeInternal, selectedByUser);
    if (selectedByUser) {
      this.nodeInternal.highlight();
      Host.userMetrics.actionTaken(Host.UserMetrics.Action.ChangeInspectedNodeInElementsPanel);
    }
    this.createSelection();
    this.createHint();
    this.treeOutline.suppressRevealAndSelect = false;
    return true;
  }

  ondelete(): boolean {
    if (!this.treeOutline) {
      return false;
    }
    const startTagTreeElement = this.treeOutline.findTreeElement(this.nodeInternal);
    startTagTreeElement ? startTagTreeElement.remove() : this.remove();
    return true;
  }

  onenter(): boolean {
    // On Enter or Return start editing the first attribute
    // or create a new attribute on the selected element.
    if (this.editing) {
      return false;
    }

    this.startEditing();

    // prevent a newline from being immediately inserted
    return true;
  }

  selectOnMouseDown(event: MouseEvent): void {
    super.selectOnMouseDown(event);

    if (this.editing) {
      return;
    }

    // Prevent selecting the nearest word on double click.
    if (event.detail >= 2) {
      event.preventDefault();
    }
  }

  ondblclick(event: Event): boolean {
    if (this.editing || this.isClosingTagInternal) {
      return false;
    }
    if (this.startEditingTarget((event.target as Element))) {
      return false;
    }

    if (this.isExpandable() && !this.expanded) {
      this.expand();
    }
    return false;
  }

  hasEditableNode(): boolean {
    return !this.nodeInternal.isShadowRoot() && !this.nodeInternal.ancestorUserAgentShadowRoot();
  }

  private insertInLastAttributePosition(tag: Element, node: Element): void {
    if (tag.getElementsByClassName('webkit-html-attribute').length > 0) {
      tag.insertBefore(node, tag.lastChild);
    } else if (tag.textContent !== null) {
      const matchResult = tag.textContent.match(/^<(.*?)>$/);
      if (!matchResult) {
        return;
      }
      const nodeName = matchResult[1];
      tag.textContent = '';
      UI.UIUtils.createTextChild(tag, '<' + nodeName);
      tag.appendChild(node);
      UI.UIUtils.createTextChild(tag, '>');
    }
  }

  private startEditingTarget(eventTarget: Element): boolean {
    if (!this.treeOutline || this.treeOutline.selectedDOMNode() !== this.nodeInternal) {
      return false;
    }

    if (this.nodeInternal.nodeType() !== Node.ELEMENT_NODE && this.nodeInternal.nodeType() !== Node.TEXT_NODE) {
      return false;
    }

    const textNode = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (textNode) {
      return this.startEditingTextNode(textNode);
    }

    const attribute = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    if (attribute) {
      return this.startEditingAttribute(attribute, eventTarget);
    }

    const tagName = eventTarget.enclosingNodeOrSelfWithClass('webkit-html-tag-name');
    if (tagName) {
      return this.startEditingTagName(tagName);
    }

    const newAttribute = eventTarget.enclosingNodeOrSelfWithClass('add-attribute');
    if (newAttribute) {
      return this.addNewAttribute();
    }

    return false;
  }

  private showContextMenu(event: Event): void {
    this.treeOutline && this.treeOutline.showContextMenu(this, event);
  }

  populateTagContextMenu(contextMenu: UI.ContextMenu.ContextMenu, event: Event): void {
    // Add attribute-related actions.
    const treeElement =
        this.isClosingTagInternal && this.treeOutline ? this.treeOutline.findTreeElement(this.nodeInternal) : this;
    if (!treeElement) {
      return;
    }
    contextMenu.editSection().appendItem(
        i18nString(UIStrings.addAttribute), treeElement.addNewAttribute.bind(treeElement));

    const target = (event.target as Element);
    const attribute = target.enclosingNodeOrSelfWithClass('webkit-html-attribute');
    const newAttribute = target.enclosingNodeOrSelfWithClass('add-attribute');
    if (attribute && !newAttribute) {
      contextMenu.editSection().appendItem(
          i18nString(UIStrings.editAttribute), this.startEditingAttribute.bind(this, attribute, target));
    }
    this.populateNodeContextMenu(contextMenu);
    ElementsTreeElement.populateForcedPseudoStateItems(contextMenu, treeElement.node());
    this.populateScrollIntoView(contextMenu);
    contextMenu.viewSection().appendItem(i18nString(UIStrings.focus), async () => {
      await this.nodeInternal.focus();
    });

    const overlayModel = this.nodeInternal.domModel().overlayModel();
    if (overlayModel.isHighlightedIsolatedElementInPersistentOverlay(this.nodeInternal.id)) {
      contextMenu.viewSection().appendItem(i18nString(UIStrings.exitIsolationMode), () => {
        overlayModel.hideIsolatedElementInPersistentOverlay(this.nodeInternal.id);
      });
    } else {
      contextMenu.viewSection().appendItem(i18nString(UIStrings.enterIsolationMode), () => {
        overlayModel.highlightIsolatedElementInPersistentOverlay(this.nodeInternal.id);
      });
    }
  }

  populateScrollIntoView(contextMenu: UI.ContextMenu.ContextMenu): void {
    contextMenu.viewSection().appendItem(
        i18nString(UIStrings.scrollIntoView), () => this.nodeInternal.scrollIntoView());
  }

  populateTextContextMenu(contextMenu: UI.ContextMenu.ContextMenu, textNode: Element): void {
    if (!this.editing) {
      contextMenu.editSection().appendItem(
          i18nString(UIStrings.editText), this.startEditingTextNode.bind(this, textNode));
    }
    this.populateNodeContextMenu(contextMenu);
  }

  populateNodeContextMenu(contextMenu: UI.ContextMenu.ContextMenu): void {
    // Add free-form node-related actions.
    const isEditable = this.hasEditableNode();
    // clang-format off
    if (isEditable && !this.editing) {
      // Eagerly load CodeMirror to avoid a delay when opening the "Edit as HTML" editor when the user actually clicks on it
      import('../../ui/components/text_editor/text_editor.js');
      contextMenu.editSection().appendItem(i18nString(UIStrings.editAsHtml), this.editAsHTML.bind(this));
    }
    // clang-format on
    const isShadowRoot = this.nodeInternal.isShadowRoot();

    const createShortcut = UI.KeyboardShortcut.KeyboardShortcut.shortcutToString.bind(null);
    const modifier = UI.KeyboardShortcut.Modifiers.CtrlOrMeta;
    const treeOutline = this.treeOutline;
    if (!treeOutline) {
      return;
    }
    let menuItem;

    menuItem = contextMenu.clipboardSection().appendItem(
        i18nString(UIStrings.cut), treeOutline.performCopyOrCut.bind(treeOutline, true, this.nodeInternal),
        !this.hasEditableNode());
    menuItem.setShortcut(createShortcut('X', modifier));

    // Place it here so that all "Copy"-ing items stick together.
    const copyMenu = contextMenu.clipboardSection().appendSubMenuItem(i18nString(UIStrings.copy));
    const section = copyMenu.section();
    if (!isShadowRoot) {
      menuItem = section.appendItem(
          i18nString(UIStrings.copyOuterhtml),
          treeOutline.performCopyOrCut.bind(treeOutline, false, this.nodeInternal));
      menuItem.setShortcut(createShortcut('V', modifier));
    }
    if (this.nodeInternal.nodeType() === Node.ELEMENT_NODE) {
      section.appendItem(i18nString(UIStrings.copySelector), this.copyCSSPath.bind(this));
      section.appendItem(
          i18nString(UIStrings.copyJsPath), this.copyJSPath.bind(this), !canGetJSPath(this.nodeInternal));
      section.appendItem(i18nString(UIStrings.copyStyles), this.copyStyles.bind(this));
    }
    if (!isShadowRoot) {
      section.appendItem(i18nString(UIStrings.copyXpath), this.copyXPath.bind(this));
      section.appendItem(i18nString(UIStrings.copyFullXpath), this.copyFullXPath.bind(this));
    }

    if (!isShadowRoot) {
      menuItem = copyMenu.clipboardSection().appendItem(
          i18nString(UIStrings.copyElement), treeOutline.performCopyOrCut.bind(treeOutline, false, this.nodeInternal));
      menuItem.setShortcut(createShortcut('C', modifier));

      // Duplicate element, disabled on root element and ShadowDOM.
      const isRootElement = !this.nodeInternal.parentNode || this.nodeInternal.parentNode.nodeName() === '#document';
      menuItem = contextMenu.editSection().appendItem(
          i18nString(UIStrings.duplicateElement), treeOutline.duplicateNode.bind(treeOutline, this.nodeInternal),
          (this.nodeInternal.isInShadowTree() || isRootElement));
    }

    menuItem = contextMenu.clipboardSection().appendItem(
        i18nString(UIStrings.paste), treeOutline.pasteNode.bind(treeOutline, this.nodeInternal),
        !treeOutline.canPaste(this.nodeInternal));
    menuItem.setShortcut(createShortcut('V', modifier));

    menuItem = contextMenu.debugSection().appendCheckboxItem(
        i18nString(UIStrings.hideElement), treeOutline.toggleHideElement.bind(treeOutline, this.nodeInternal),
        treeOutline.isToggledToHidden(this.nodeInternal));
    menuItem.setShortcut(
        UI.ShortcutRegistry.ShortcutRegistry.instance().shortcutTitleForAction('elements.hide-element') || '');

    if (isEditable) {
      contextMenu.editSection().appendItem(i18nString(UIStrings.deleteElement), this.remove.bind(this));
    }

    contextMenu.viewSection().appendItem(i18nString(UIStrings.expandRecursively), this.expandRecursively.bind(this));
    contextMenu.viewSection().appendItem(i18nString(UIStrings.collapseChildren), this.collapseChildren.bind(this));
    const deviceModeWrapperAction = new Emulation.DeviceModeWrapper.ActionDelegate();
    contextMenu.viewSection().appendItem(
        i18nString(UIStrings.captureNodeScreenshot),
        deviceModeWrapperAction.handleAction.bind(
            null, UI.Context.Context.instance(), 'emulation.capture-node-screenshot'));
    if (this.nodeInternal.frameOwnerFrameId()) {
      contextMenu.viewSection().appendItem(i18nString(UIStrings.showFrameDetails), () => {
        const frameOwnerFrameId = this.nodeInternal.frameOwnerFrameId();
        if (frameOwnerFrameId) {
          const frame = SDK.FrameManager.FrameManager.instance().getFrame(frameOwnerFrameId);
          Common.Revealer.reveal(frame);
        }
      });
    }
  }

  private startEditing(): boolean|undefined {
    if (!this.treeOutline || this.treeOutline.selectedDOMNode() !== this.nodeInternal) {
      return;
    }

    const listItem = this.listItemElement;

    if (this.canAddAttributes) {
      const attribute = listItem.getElementsByClassName('webkit-html-attribute')[0];
      if (attribute) {
        return this.startEditingAttribute(
            attribute, attribute.getElementsByClassName('webkit-html-attribute-value')[0]);
      }

      return this.addNewAttribute();
    }

    if (this.nodeInternal.nodeType() === Node.TEXT_NODE) {
      const textNode = listItem.getElementsByClassName('webkit-html-text-node')[0];
      if (textNode) {
        return this.startEditingTextNode(textNode);
      }
    }

    return;
  }

  private addNewAttribute(): boolean {
    // Cannot just convert the textual html into an element without
    // a parent node. Use a temporary span container for the HTML.
    const container = document.createElement('span');
    const attr = this.buildAttributeDOM(container, ' ', '', null);
    attr.style.marginLeft = '2px';   // overrides the .editing margin rule
    attr.style.marginRight = '2px';  // overrides the .editing margin rule

    const tag = this.listItemElement.getElementsByClassName('webkit-html-tag')[0];
    this.insertInLastAttributePosition(tag, attr);
    attr.scrollIntoViewIfNeeded(true);
    return this.startEditingAttribute(attr, attr);
  }

  private triggerEditAttribute(attributeName: string): boolean|undefined {
    const attributeElements = this.listItemElement.getElementsByClassName('webkit-html-attribute-name');
    for (let i = 0, len = attributeElements.length; i < len; ++i) {
      if (attributeElements[i].textContent === attributeName) {
        for (let elem: (ChildNode|null) = attributeElements[i].nextSibling; elem; elem = elem.nextSibling) {
          if (elem.nodeType !== Node.ELEMENT_NODE) {
            continue;
          }
          if ((elem as Element).classList.contains('webkit-html-attribute-value')) {
            return this.startEditingAttribute((elem.parentElement as HTMLElement), (elem as Element));
          }
        }
      }
    }

    return;
  }

  private startEditingAttribute(attribute: Element, elementForSelection: Element): boolean {
    console.assert(this.listItemElement.isAncestor(attribute));

    if (UI.UIUtils.isBeingEdited(attribute)) {
      return true;
    }

    const attributeNameElement = attribute.getElementsByClassName('webkit-html-attribute-name')[0];
    if (!attributeNameElement) {
      return false;
    }

    const attributeName = attributeNameElement.textContent;
    const attributeValueElement = attribute.getElementsByClassName('webkit-html-attribute-value')[0];

    // Make sure elementForSelection is not a child of attributeValueElement.
    elementForSelection =
        attributeValueElement.isAncestor(elementForSelection) ? attributeValueElement : elementForSelection;

    function removeZeroWidthSpaceRecursive(node: Node): void {
      if (node.nodeType === Node.TEXT_NODE) {
        node.nodeValue = node.nodeValue ? node.nodeValue.replace(/\u200B/g, '') : '';
        return;
      }

      if (node.nodeType !== Node.ELEMENT_NODE) {
        return;
      }

      for (let child: (ChildNode|null) = node.firstChild; child; child = child.nextSibling) {
        removeZeroWidthSpaceRecursive(child);
      }
    }

    const attributeValue =
        attributeName && attributeValueElement ? this.nodeInternal.getAttribute(attributeName) : undefined;
    if (attributeValue !== undefined) {
      attributeValueElement.setTextContentTruncatedIfNeeded(
          attributeValue, i18nString(UIStrings.valueIsTooLargeToEdit));
    }

    // Remove zero-width spaces that were added by nodeTitleInfo.
    removeZeroWidthSpaceRecursive(attribute);

    const config = new UI.InplaceEditor.Config(
        this.attributeEditingCommitted.bind(this), this.editingCancelled.bind(this), attributeName || undefined);

    function postKeyDownFinishHandler(event: Event): string {
      UI.UIUtils.handleElementValueModifications(event, attribute);
      return '';
    }

    if (!Common.ParsedURL.ParsedURL.fromString(attributeValueElement.textContent || '')) {
      config.setPostKeydownFinishHandler(postKeyDownFinishHandler);
    }

    this.updateEditorHandles(attribute, config);

    const componentSelection = this.listItemElement.getComponentSelection();
    componentSelection && componentSelection.selectAllChildren(elementForSelection);

    return true;
  }

  private startEditingTextNode(textNodeElement: Element): boolean {
    if (UI.UIUtils.isBeingEdited(textNodeElement)) {
      return true;
    }

    let textNode: SDK.DOMModel.DOMNode = this.nodeInternal;
    // We only show text nodes inline in elements if the element only
    // has a single child, and that child is a text node.
    if (textNode.nodeType() === Node.ELEMENT_NODE && textNode.firstChild) {
      textNode = textNode.firstChild;
    }

    const container = textNodeElement.enclosingNodeOrSelfWithClass('webkit-html-text-node');
    if (container) {
      container.textContent = textNode.nodeValue();
    }  // Strip the CSS or JS highlighting if present.
    const config = new UI.InplaceEditor.Config(
        this.textNodeEditingCommitted.bind(this, textNode), this.editingCancelled.bind(this));
    this.updateEditorHandles(textNodeElement, config);
    const componentSelection = this.listItemElement.getComponentSelection();
    componentSelection && componentSelection.selectAllChildren(textNodeElement);

    return true;
  }

  private startEditingTagName(tagNameElement?: Element): boolean {
    if (!tagNameElement) {
      tagNameElement = this.listItemElement.getElementsByClassName('webkit-html-tag-name')[0];
      if (!tagNameElement) {
        return false;
      }
    }

    const tagName = tagNameElement.textContent;
    if (tagName !== null && EditTagBlocklist.has(tagName.toLowerCase())) {
      return false;
    }

    if (UI.UIUtils.isBeingEdited(tagNameElement)) {
      return true;
    }

    const closingTagElement = this.distinctClosingTagElement();

    function keyupListener(): void {
      if (closingTagElement && tagNameElement) {
        closingTagElement.textContent = '</' + tagNameElement.textContent + '>';
      }
    }

    const keydownListener = (event: Event): void => {
      if ((event as KeyboardEvent).key !== ' ') {
        return;
      }
      this.editing && this.editing.commit();
      event.consume(true);
    };

    function editingCommitted(
        this: ElementsTreeElement, element: Element, newTagName: string, oldText: string, tagName: string|null,
        moveDirection: string): void {
      if (!tagNameElement) {
        return;
      }
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      tagNameElement.removeEventListener('keydown', keydownListener, false);
      this.tagNameEditingCommitted(element, newTagName, oldText, tagName, moveDirection);
    }

    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    function editingCancelled(this: ElementsTreeElement, element: Element, context: any): void {
      if (!tagNameElement) {
        return;
      }
      tagNameElement.removeEventListener('keyup', keyupListener, false);
      tagNameElement.removeEventListener('keydown', keydownListener, false);
      this.editingCancelled(element, context);
    }

    tagNameElement.addEventListener('keyup', keyupListener, false);
    tagNameElement.addEventListener('keydown', keydownListener, false);

    const config =
        new UI.InplaceEditor.Config<string|null>(editingCommitted.bind(this), editingCancelled.bind(this), tagName);
    this.updateEditorHandles(tagNameElement, config);
    const componentSelection = this.listItemElement.getComponentSelection();
    componentSelection && componentSelection.selectAllChildren(tagNameElement);
    return true;
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private updateEditorHandles(element: Element, config?: UI.InplaceEditor.Config<any>): void {
    const editorHandles = UI.InplaceEditor.InplaceEditor.startEditing(element, config);
    if (!editorHandles) {
      this.editing = null;
    } else {
      this.editing = {
        commit: editorHandles.commit,
        cancel: editorHandles.cancel,
        editor: undefined,
        resize: (): void => {},
      };
    }
  }

  private async startEditingAsHTML(
      commitCallback: (arg0: string, arg1: string) => void, disposeCallback: () => void,
      maybeInitialValue: string|null): Promise<void> {
    if (maybeInitialValue === null) {
      return;
    }
    if (this.editing) {
      return;
    }

    const initialValue = this.convertWhitespaceToEntities(maybeInitialValue).text;
    this.htmlEditElement = document.createElement('div');
    this.htmlEditElement.className = 'source-code elements-tree-editor';

    // Hide header items.
    let child: (ChildNode|null) = this.listItemElement.firstChild;
    while (child) {
      (child as HTMLElement).style.display = 'none';
      child = child.nextSibling;
    }
    // Hide children item.
    if (this.childrenListElement) {
      this.childrenListElement.style.display = 'none';
    }
    // Append editor.
    this.listItemElement.appendChild(this.htmlEditElement);
    this.htmlEditElement.addEventListener('keydown', event => {
      if (event.key === 'Escape') {
        event.consume(true);
      }
    });

    const TextEditor = await import('../../ui/components/text_editor/text_editor.js');
    const CodeMirror = await import('../../third_party/codemirror.next/codemirror.next.js');
    const {html} = await CodeMirror.html();
    const editor = new TextEditor.TextEditor.TextEditor(CodeMirror.EditorState.create({
      doc: initialValue,
      extensions: [
        CodeMirror.keymap.of([
          {
            key: 'Mod-Enter',
            run: (): boolean => {
              this.editing?.commit();
              return true;
            },
          },
          {
            key: 'Escape',
            run: (): boolean => {
              this.editing?.cancel();
              return true;
            },
          },
        ]),
        TextEditor.Config.baseConfiguration(initialValue),
        html(),
        TextEditor.Config.domWordWrap,
        CodeMirror.EditorView.theme({
          '.cm-editor': {maxHeight: '300px'},
          '.cm-scroller': {overflowY: 'auto'},
        }),
        CodeMirror.EditorView.domEventHandlers({
          focusout: event => {
            // The relatedTarget is null when no element gains focus, e.g. switching windows.
            const relatedTarget = (event.relatedTarget as Node | null);
            if (relatedTarget && !relatedTarget.isSelfOrDescendant(editor)) {
              this.editing && this.editing.commit();
            }
          },
        }),
      ],
    }));
    this.editing = {commit: commit.bind(this), cancel: dispose.bind(this), editor, resize: resize.bind(this)};
    resize.call(this);
    this.htmlEditElement.appendChild(editor);
    editor.editor.focus();

    this.treeOutline && this.treeOutline.setMultilineEditing(this.editing);

    function resize(this: ElementsTreeElement): void {
      if (this.treeOutline && this.htmlEditElement) {
        this.htmlEditElement.style.width = this.treeOutline.visibleWidth() - this.computeLeftIndent() - 30 + 'px';
      }
    }

    function commit(this: ElementsTreeElement): void {
      if (this.editing && this.editing.editor) {
        commitCallback(initialValue, this.editing.editor.state.doc.toString());
      }
      dispose.call(this);
    }

    function dispose(this: ElementsTreeElement): void {
      if (!this.editing || !this.editing.editor) {
        return;
      }
      this.editing = null;

      // Remove editor.
      if (this.htmlEditElement) {
        this.listItemElement.removeChild(this.htmlEditElement);
      }
      this.htmlEditElement = undefined;
      // Unhide children item.
      if (this.childrenListElement) {
        this.childrenListElement.style.removeProperty('display');
      }
      // Unhide header items.
      let child: (ChildNode|null) = this.listItemElement.firstChild;
      while (child) {
        (child as HTMLElement).style.removeProperty('display');
        child = child.nextSibling;
      }

      if (this.treeOutline) {
        this.treeOutline.setMultilineEditing(null);
        this.treeOutline.focus();
      }

      disposeCallback();
    }
  }

  private attributeEditingCommitted(
      element: Element, newText: string, oldText: string, attributeName: string, moveDirection: string): void {
    this.editing = null;

    const treeOutline = this.treeOutline;

    function moveToNextAttributeIfNeeded(this: ElementsTreeElement, error?: string|null): void {
      if (error) {
        this.editingCancelled(element, attributeName);
      }

      if (!moveDirection) {
        return;
      }

      if (treeOutline) {
        treeOutline.runPendingUpdates();
        treeOutline.focus();
      }

      // Search for the attribute's position, and then decide where to move to.
      const attributes = this.nodeInternal.attributes();
      for (let i = 0; i < attributes.length; ++i) {
        if (attributes[i].name !== attributeName) {
          continue;
        }

        if (moveDirection === 'backward') {
          if (i === 0) {
            this.startEditingTagName();
          } else {
            this.triggerEditAttribute(attributes[i - 1].name);
          }
        } else {
          if (i === attributes.length - 1) {
            this.addNewAttribute();
          } else {
            this.triggerEditAttribute(attributes[i + 1].name);
          }
        }
        return;
      }

      // Moving From the "New Attribute" position.
      if (moveDirection === 'backward') {
        if (newText === ' ') {
          // Moving from "New Attribute" that was not edited
          if (attributes.length > 0) {
            this.triggerEditAttribute(attributes[attributes.length - 1].name);
          }
        } else {
          // Moving from "New Attribute" that holds new value
          if (attributes.length > 1) {
            this.triggerEditAttribute(attributes[attributes.length - 2].name);
          }
        }
      } else if (moveDirection === 'forward') {
        if (!Platform.StringUtilities.isWhitespace(newText)) {
          this.addNewAttribute();
        } else {
          this.startEditingTagName();
        }
      }
    }

    if ((attributeName.trim() || newText.trim()) && oldText !== newText) {
      this.nodeInternal.setAttribute(attributeName, newText, moveToNextAttributeIfNeeded.bind(this));
      return;
    }

    this.updateTitle();
    moveToNextAttributeIfNeeded.call(this);
  }

  private tagNameEditingCommitted(
      element: Element, newText: string, oldText: string, tagName: string|null, moveDirection: string): void {
    this.editing = null;
    const self = this;

    function cancel(): void {
      const closingTagElement = self.distinctClosingTagElement();
      if (closingTagElement) {
        closingTagElement.textContent = '</' + tagName + '>';
      }

      self.editingCancelled(element, tagName);
      moveToNextAttributeIfNeeded.call(self);
    }

    function moveToNextAttributeIfNeeded(this: ElementsTreeElement): void {
      if (moveDirection !== 'forward') {
        this.addNewAttribute();
        return;
      }

      const attributes = this.nodeInternal.attributes();
      if (attributes.length > 0) {
        this.triggerEditAttribute(attributes[0].name);
      } else {
        this.addNewAttribute();
      }
    }

    newText = newText.trim();
    if (newText === oldText) {
      cancel();
      return;
    }

    const treeOutline = this.treeOutline;
    const wasExpanded = this.expanded;

    this.nodeInternal.setNodeName(newText, (error, newNode) => {
      if (error || !newNode) {
        cancel();
        return;
      }
      if (!treeOutline) {
        return;
      }
      const newTreeItem = treeOutline.selectNodeAfterEdit(wasExpanded, error, newNode);
      // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
      // @ts-expect-error
      moveToNextAttributeIfNeeded.call(newTreeItem);
    });
  }

  private textNodeEditingCommitted(textNode: SDK.DOMModel.DOMNode, element: Element, newText: string): void {
    this.editing = null;

    function callback(this: ElementsTreeElement): void {
      this.updateTitle();
    }
    textNode.setNodeValue(newText, callback.bind(this));
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private editingCancelled(_element: Element, _context: any): void {
    this.editing = null;

    // Need to restore attributes structure.
    this.updateTitle();
  }

  private distinctClosingTagElement(): Element|null {
    // FIXME: Improve the Tree Element / Outline Abstraction to prevent crawling the DOM

    // For an expanded element, it will be the last element with class "close"
    // in the child element list.
    if (this.expanded) {
      const closers = this.childrenListElement.querySelectorAll('.close');
      return closers[closers.length - 1];
    }

    // Remaining cases are single line non-expanded elements with a closing
    // tag, or HTML elements without a closing tag (such as <br>). Return
    // null in the case where there isn't a closing tag.
    const tags = this.listItemElement.getElementsByClassName('webkit-html-tag');
    return tags.length === 1 ? null : tags[tags.length - 1];
  }

  updateTitle(updateRecord?: UpdateRecord|null, onlySearchQueryChanged?: boolean): void {
    // If we are editing, return early to prevent canceling the edit.
    // After editing is committed updateTitle will be called.
    if (this.editing) {
      return;
    }

    if (onlySearchQueryChanged) {
      this.hideSearchHighlight();
    } else {
      const nodeInfo = this.nodeTitleInfo(updateRecord || null);
      if (this.nodeInternal.nodeType() === Node.DOCUMENT_FRAGMENT_NODE && this.nodeInternal.isInShadowTree() &&
          this.nodeInternal.shadowRootType()) {
        this.childrenListElement.classList.add('shadow-root');
        let depth = 4;
        for (let node: (SDK.DOMModel.DOMNode|null) = (this.nodeInternal as SDK.DOMModel.DOMNode | null); depth && node;
             node = node.parentNode) {
          if (node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE) {
            depth--;
          }
        }
        if (!depth) {
          this.childrenListElement.classList.add('shadow-root-deep');
        } else {
          this.childrenListElement.classList.add('shadow-root-depth-' + depth);
        }
      }
      const highlightElement = document.createElement('span');
      highlightElement.className = 'highlight';
      highlightElement.appendChild(nodeInfo);
      // fixme: make it clear that `this.title = x` is a setter with significant side effects
      this.title = highlightElement;
      this.updateDecorations();
      this.listItemElement.insertBefore(this.gutterContainer, this.listItemElement.firstChild);
      if (!this.isClosingTagInternal && this.adornerContainer) {
        this.listItemElement.appendChild(this.adornerContainer);
      }
      this.highlightResult = [];
      delete this.selectionElement;
      delete this.hintElement;
      if (this.selected) {
        this.createSelection();
        this.createHint();
      }
    }

    this.highlightSearchResultsInternal();
  }

  private computeLeftIndent(): number {
    let treeElement: (UI.TreeOutline.TreeElement|null) = this.parent;
    let depth = 0;
    while (treeElement !== null) {
      depth++;
      treeElement = treeElement.parent;
    }

    /** Keep it in sync with elementsTreeOutline.css **/
    return 12 * (depth - 2) + (this.isExpandable() ? 1 : 12);
  }

  updateDecorations(): void {
    this.gutterContainer.style.left = (-this.computeLeftIndent()) + 'px';

    if (this.isClosingTag()) {
      return;
    }

    if (this.nodeInternal.nodeType() !== Node.ELEMENT_NODE) {
      return;
    }

    this.decorationsThrottler.schedule(this.updateDecorationsInternal.bind(this));
  }

  private updateDecorationsInternal(): Promise<void> {
    if (!this.treeOutline) {
      return Promise.resolve();
    }

    const node = this.nodeInternal;

    if (!this.treeOutline.decoratorExtensions) {
      this.treeOutline.decoratorExtensions = getRegisteredDecorators();
    }

    const markerToExtension = new Map<string, MarkerDecoratorRegistration>();
    for (const decoratorExtension of this.treeOutline.decoratorExtensions) {
      markerToExtension.set(decoratorExtension.marker, decoratorExtension);
    }

    const promises: Promise<void>[] = [];
    const decorations: {
      title: string,
      color: string,
    }[] = [];
    const descendantDecorations: {
      title: string,
      color: string,
    }[] = [];
    node.traverseMarkers(visitor);

    function visitor(n: SDK.DOMModel.DOMNode, marker: string): void {
      const extension = markerToExtension.get(marker);
      if (!extension) {
        return;
      }
      promises.push(Promise.resolve(extension.decorator()).then(collectDecoration.bind(null, n)));
    }

    function collectDecoration(n: SDK.DOMModel.DOMNode, decorator: MarkerDecorator): void {
      const decoration = decorator.decorate(n);
      if (!decoration) {
        return;
      }
      (n === node ? decorations : descendantDecorations).push(decoration);
    }

    return Promise.all(promises).then(updateDecorationsUI.bind(this));

    function updateDecorationsUI(this: ElementsTreeElement): void {
      this.decorationsElement.removeChildren();
      this.decorationsElement.classList.add('hidden');
      this.gutterContainer.classList.toggle(
          'has-decorations', Boolean(decorations.length || descendantDecorations.length));
      UI.ARIAUtils.setAccessibleName(this.decorationsElement, '');

      if (!decorations.length && !descendantDecorations.length) {
        return;
      }

      const colors = new Set<string>();
      const titles = document.createElement('div');

      for (const decoration of decorations) {
        const titleElement = titles.createChild('div');
        titleElement.textContent = decoration.title;
        colors.add(decoration.color);
      }
      if (this.expanded && !decorations.length) {
        return;
      }

      const descendantColors = new Set<string>();
      if (descendantDecorations.length) {
        let element = titles.createChild('div');
        element.textContent = i18nString(UIStrings.children);
        for (const decoration of descendantDecorations) {
          element = titles.createChild('div');
          element.style.marginLeft = '15px';
          element.textContent = decoration.title;
          descendantColors.add(decoration.color);
        }
      }

      let offset = 0;
      processColors.call(this, colors, 'elements-gutter-decoration');
      if (!this.expanded) {
        processColors.call(this, descendantColors, 'elements-gutter-decoration elements-has-decorated-children');
      }
      UI.Tooltip.Tooltip.install(this.decorationsElement, titles.textContent);
      UI.ARIAUtils.setAccessibleName(this.decorationsElement, titles.textContent || '');

      function processColors(this: ElementsTreeElement, colors: Set<string>, className: string): void {
        for (const color of colors) {
          const child = this.decorationsElement.createChild('div', className);
          this.decorationsElement.classList.remove('hidden');
          child.style.backgroundColor = color;
          child.style.borderColor = color;
          if (offset) {
            child.style.marginLeft = offset + 'px';
          }
          offset += 3;
        }
      }
    }
  }

  private buildAttributeDOM(
      parentElement: Element|DocumentFragment, name: string, value: string, updateRecord: UpdateRecord|null,
      forceValue?: boolean, node?: SDK.DOMModel.DOMNode): HTMLElement {
    const closingPunctuationRegex = /[\/;:\)\]\}]/g;
    let highlightIndex = 0;
    let highlightCount = 0;
    let additionalHighlightOffset = 0;

    function setValueWithEntities(this: ElementsTreeElement, element: Element, value: string): void {
      const result = this.convertWhitespaceToEntities(value);
      highlightCount = result.entityRanges.length;
      value = result.text.replace(closingPunctuationRegex, (match, replaceOffset) => {
        while (highlightIndex < highlightCount && result.entityRanges[highlightIndex].offset < replaceOffset) {
          result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
          ++highlightIndex;
        }
        additionalHighlightOffset += 1;
        return match + '\u200B';
      });

      while (highlightIndex < highlightCount) {
        result.entityRanges[highlightIndex].offset += additionalHighlightOffset;
        ++highlightIndex;
      }
      element.setTextContentTruncatedIfNeeded(value);
      UI.UIUtils.highlightRangesWithStyleClass(element, result.entityRanges, 'webkit-html-entity-value');
    }

    const hasText = (forceValue || value.length > 0);
    const attrSpanElement = (parentElement.createChild('span', 'webkit-html-attribute') as HTMLElement);
    const attrNameElement = attrSpanElement.createChild('span', 'webkit-html-attribute-name');
    attrNameElement.textContent = name;

    if (hasText) {
      UI.UIUtils.createTextChild(attrSpanElement, '=\u200B"');
    }

    const attrValueElement = attrSpanElement.createChild('span', 'webkit-html-attribute-value');

    if (updateRecord && updateRecord.isAttributeModified(name)) {
      UI.UIUtils.runCSSAnimationOnce(hasText ? attrValueElement : attrNameElement, 'dom-update-highlight');
    }

    function linkifyValue(this: ElementsTreeElement, value: string): Element {
      const rewrittenHref = node ? node.resolveURL(value) : null;
      if (rewrittenHref === null) {
        const span = document.createElement('span');
        setValueWithEntities.call(this, span, value);
        return span;
      }
      value = value.replace(closingPunctuationRegex, '$&\u200B');
      if (value.startsWith('data:')) {
        value = Platform.StringUtilities.trimMiddle(value, 60);
      }
      const link = node && node.nodeName().toLowerCase() === 'a' ?
          UI.XLink.XLink.create(rewrittenHref, value, '', true /* preventClick */) :
          Components.Linkifier.Linkifier.linkifyURL(rewrittenHref, {
            text: value,
            preventClick: true,
            className: undefined,
            lineNumber: undefined,
            columnNumber: undefined,
            inlineFrameIndex: 0,
            maxLength: undefined,
            tabStop: undefined,
            bypassURLTrimming: undefined,
          });
      return ImagePreviewPopover.setImageUrl(link, rewrittenHref);
    }

    const nodeName = node ? node.nodeName().toLowerCase() : '';
    if (nodeName && (name === 'src' || name === 'href')) {
      attrValueElement.appendChild(linkifyValue.call(this, value));
    } else if ((nodeName === 'img' || nodeName === 'source') && name === 'srcset') {
      attrValueElement.appendChild(linkifySrcset.call(this, value));
    } else if (nodeName === 'image' && (name === 'xlink:href' || name === 'href')) {
      attrValueElement.appendChild(linkifySrcset.call(this, value));
    } else {
      setValueWithEntities.call(this, attrValueElement, value);
    }

    if (hasText) {
      UI.UIUtils.createTextChild(attrSpanElement, '"');
    }

    function linkifySrcset(this: ElementsTreeElement, value: string): DocumentFragment {
      // Splitting normally on commas or spaces will break on valid srcsets "foo 1x,bar 2x" and "data:,foo 1x".
      // 1) Let the index of the next space be `indexOfSpace`.
      // 2a) If the character at `indexOfSpace - 1` is a comma, collect the preceding characters up to
      //     `indexOfSpace - 1` as a URL and repeat step 1).
      // 2b) Else, collect the preceding characters as a URL.
      // 3) Collect the characters from `indexOfSpace` up to the next comma as the size descriptor and repeat step 1).
      // https://html.spec.whatwg.org/C/#parse-a-srcset-attribute
      const fragment = document.createDocumentFragment();
      let i = 0;
      while (value.length) {
        if (i++ > 0) {
          UI.UIUtils.createTextChild(fragment, ' ');
        }
        value = value.trim();
        // The url and descriptor may end with a separating comma.
        let url = '';
        let descriptor = '';
        const indexOfSpace = value.search(/\s/);
        if (indexOfSpace === -1) {
          url = value;
        } else if (indexOfSpace > 0 && value[indexOfSpace - 1] === ',') {
          url = value.substring(0, indexOfSpace);
        } else {
          url = value.substring(0, indexOfSpace);
          const indexOfComma = value.indexOf(',', indexOfSpace);
          if (indexOfComma !== -1) {
            descriptor = value.substring(indexOfSpace, indexOfComma + 1);
          } else {
            descriptor = value.substring(indexOfSpace);
          }
        }

        if (url) {
          // Up to one trailing comma should be removed from `url`.
          if (url.endsWith(',')) {
            fragment.appendChild(linkifyValue.call(this, url.substring(0, url.length - 1)));
            UI.UIUtils.createTextChild(fragment, ',');
          } else {
            fragment.appendChild(linkifyValue.call(this, url));
          }
        }
        if (descriptor) {
          UI.UIUtils.createTextChild(fragment, descriptor);
        }
        value = value.substring(url.length + descriptor.length);
      }
      return fragment;
    }

    return attrSpanElement;
  }

  private buildPseudoElementDOM(parentElement: DocumentFragment, pseudoElementName: string): void {
    const pseudoElement = parentElement.createChild('span', 'webkit-html-pseudo-element');
    pseudoElement.textContent = '::' + pseudoElementName;
    UI.UIUtils.createTextChild(parentElement, '\u200B');
  }

  private buildTagDOM(
      parentElement: DocumentFragment, tagName: string, isClosingTag: boolean, isDistinctTreeElement: boolean,
      updateRecord: UpdateRecord|null): void {
    const node = this.nodeInternal;
    const classes = ['webkit-html-tag'];
    if (isClosingTag && isDistinctTreeElement) {
      classes.push('close');
    }
    const tagElement = parentElement.createChild('span', classes.join(' '));
    UI.UIUtils.createTextChild(tagElement, '<');
    const tagNameElement =
        tagElement.createChild('span', isClosingTag ? 'webkit-html-close-tag-name' : 'webkit-html-tag-name');
    tagNameElement.textContent = (isClosingTag ? '/' : '') + tagName;
    if (!isClosingTag) {
      if (node.hasAttributes()) {
        const attributes = node.attributes();
        for (let i = 0; i < attributes.length; ++i) {
          const attr = attributes[i];
          UI.UIUtils.createTextChild(tagElement, ' ');
          this.buildAttributeDOM(tagElement, attr.name, attr.value, updateRecord, false, node);
        }
      }
      if (updateRecord) {
        let hasUpdates: boolean = updateRecord.hasRemovedAttributes() || updateRecord.hasRemovedChildren();
        hasUpdates = hasUpdates || (!this.expanded && updateRecord.hasChangedChildren());
        if (hasUpdates) {
          UI.UIUtils.runCSSAnimationOnce(tagNameElement, 'dom-update-highlight');
        }
      }
    }

    UI.UIUtils.createTextChild(tagElement, '>');
    UI.UIUtils.createTextChild(parentElement, '\u200B');
    if (tagElement.textContent) {
      UI.ARIAUtils.setAccessibleName(tagElement, tagElement.textContent);
    }
  }

  private convertWhitespaceToEntities(text: string): {
    text: string,
    entityRanges: Array<TextUtils.TextRange.SourceRange>,
  } {
    let result = '';
    let lastIndexAfterEntity = 0;
    const entityRanges = [];
    const charToEntity = MappedCharToEntity;
    for (let i = 0, size = text.length; i < size; ++i) {
      const char = text.charAt(i);
      if (charToEntity.has(char)) {
        result += text.substring(lastIndexAfterEntity, i);
        const entityValue = '&' + charToEntity.get(char) + ';';
        entityRanges.push({offset: result.length, length: entityValue.length});
        result += entityValue;
        lastIndexAfterEntity = i + 1;
      }
    }
    if (result) {
      result += text.substring(lastIndexAfterEntity);
    }
    return {text: result || text, entityRanges: entityRanges};
  }

  private nodeTitleInfo(updateRecord: UpdateRecord|null): DocumentFragment {
    const node = this.nodeInternal;
    const titleDOM = document.createDocumentFragment();
    const updateSearchHighlight = (): void => {
      this.highlightResult = [];
      this.highlightSearchResultsInternal();
    };

    switch (node.nodeType()) {
      case Node.ATTRIBUTE_NODE:
        this.buildAttributeDOM(titleDOM, (node.name as string), (node.value as string), updateRecord, true);
        break;

      case Node.ELEMENT_NODE: {
        const pseudoType = node.pseudoType();
        if (pseudoType) {
          this.buildPseudoElementDOM(titleDOM, pseudoType);
          break;
        }

        const tagName = node.nodeNameInCorrectCase();
        if (this.isClosingTagInternal) {
          this.buildTagDOM(titleDOM, tagName, true, true, updateRecord);
          break;
        }

        this.buildTagDOM(titleDOM, tagName, false, false, updateRecord);

        if (this.isExpandable()) {
          if (!this.expanded) {
            const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node bogus');
            textNodeElement.textContent = '…';
            UI.UIUtils.createTextChild(titleDOM, '\u200B');
            this.buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          }
          break;
        }

        if (ElementsTreeElement.canShowInlineText(node)) {
          const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          const firstChild = node.firstChild;
          if (!firstChild) {
            throw new Error('ElementsTreeElement._nodeTitleInfo expects node.firstChild to be defined.');
          }
          const result = this.convertWhitespaceToEntities(firstChild.nodeValue());
          textNodeElement.textContent = Platform.StringUtilities.collapseWhitespace(result.text);
          UI.UIUtils.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          UI.UIUtils.createTextChild(titleDOM, '\u200B');
          this.buildTagDOM(titleDOM, tagName, true, false, updateRecord);
          if (updateRecord && updateRecord.hasChangedChildren()) {
            UI.UIUtils.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          }
          if (updateRecord && updateRecord.isCharDataModified()) {
            UI.UIUtils.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          }
          break;
        }

        if (this.treeOutline && this.treeOutline.isXMLMimeType || !ForbiddenClosingTagElements.has(tagName)) {
          this.buildTagDOM(titleDOM, tagName, true, false, updateRecord);
        }
        break;
      }

      case Node.TEXT_NODE:
        if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'script') {
          const newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-js-node');
          const text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          const javascriptSyntaxHighlighter =
              new TextEditorLegacy.SyntaxHighlighter.SyntaxHighlighter('text/javascript', true);
          javascriptSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight);
        } else if (node.parentNode && node.parentNode.nodeName().toLowerCase() === 'style') {
          const newNode = titleDOM.createChild('span', 'webkit-html-text-node webkit-html-css-node');
          const text = node.nodeValue();
          newNode.textContent = text.startsWith('\n') ? text.substring(1) : text;

          const cssSyntaxHighlighter = new TextEditorLegacy.SyntaxHighlighter.SyntaxHighlighter('text/css', true);
          cssSyntaxHighlighter.syntaxHighlightNode(newNode).then(updateSearchHighlight);
        } else {
          UI.UIUtils.createTextChild(titleDOM, '"');
          const textNodeElement = titleDOM.createChild('span', 'webkit-html-text-node');
          const result = this.convertWhitespaceToEntities(node.nodeValue());
          textNodeElement.textContent = Platform.StringUtilities.collapseWhitespace(result.text);
          UI.UIUtils.highlightRangesWithStyleClass(textNodeElement, result.entityRanges, 'webkit-html-entity-value');
          UI.UIUtils.createTextChild(titleDOM, '"');
          if (updateRecord && updateRecord.isCharDataModified()) {
            UI.UIUtils.runCSSAnimationOnce(textNodeElement, 'dom-update-highlight');
          }
        }
        break;

      case Node.COMMENT_NODE: {
        const commentElement = titleDOM.createChild('span', 'webkit-html-comment');
        UI.UIUtils.createTextChild(commentElement, '<!--' + node.nodeValue() + '-->');
        break;
      }

      case Node.DOCUMENT_TYPE_NODE: {
        const docTypeElement = titleDOM.createChild('span', 'webkit-html-doctype');
        UI.UIUtils.createTextChild(docTypeElement, '<!DOCTYPE ' + node.nodeName());
        if (node.publicId) {
          UI.UIUtils.createTextChild(docTypeElement, ' PUBLIC "' + node.publicId + '"');
          if (node.systemId) {
            UI.UIUtils.createTextChild(docTypeElement, ' "' + node.systemId + '"');
          }
        } else if (node.systemId) {
          UI.UIUtils.createTextChild(docTypeElement, ' SYSTEM "' + node.systemId + '"');
        }

        if (node.internalSubset) {
          UI.UIUtils.createTextChild(docTypeElement, ' [' + node.internalSubset + ']');
        }

        UI.UIUtils.createTextChild(docTypeElement, '>');
        break;
      }

      case Node.CDATA_SECTION_NODE: {
        const cdataElement = titleDOM.createChild('span', 'webkit-html-text-node');
        UI.UIUtils.createTextChild(cdataElement, '<![CDATA[' + node.nodeValue() + ']]>');
        break;
      }

      case Node.DOCUMENT_FRAGMENT_NODE: {
        const fragmentElement = titleDOM.createChild('span', 'webkit-html-fragment');
        fragmentElement.textContent = Platform.StringUtilities.collapseWhitespace(node.nodeNameInCorrectCase());
        break;
      }

      default: {
        const nameWithSpaceCollapsed = Platform.StringUtilities.collapseWhitespace(node.nodeNameInCorrectCase());
        UI.UIUtils.createTextChild(titleDOM, nameWithSpaceCollapsed);
      }
    }

    return titleDOM;
  }

  remove(): void {
    if (this.nodeInternal.pseudoType()) {
      return;
    }
    const parentElement = this.parent;
    if (!parentElement) {
      return;
    }

    if (!this.nodeInternal.parentNode || this.nodeInternal.parentNode.nodeType() === Node.DOCUMENT_NODE) {
      return;
    }
    this.nodeInternal.removeNode();
  }

  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  toggleEditAsHTML(callback?: ((arg0: boolean) => any), startEditing?: boolean): void {
    if (this.editing && this.htmlEditElement) {
      this.editing.commit();
      return;
    }

    if (startEditing === false) {
      return;
    }

    function selectNode(error: string|null): void {
      if (callback) {
        callback(!error);
      }
    }

    function commitChange(initialValue: string, value: string): void {
      if (initialValue !== value) {
        node.setOuterHTML(value, selectNode);
      }
    }

    function disposeCallback(): void {
      if (callback) {
        callback(false);
      }
    }

    const node = this.nodeInternal;
    node.getOuterHTML().then(this.startEditingAsHTML.bind(this, commitChange, disposeCallback));
  }

  private copyCSSPath(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(cssPath(this.nodeInternal, true));
  }

  private copyJSPath(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(jsPath(this.nodeInternal, true));
  }

  private copyXPath(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(xPath(this.nodeInternal, true));
  }

  private copyFullXPath(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(xPath(this.nodeInternal, false));
  }

  async copyStyles(): Promise<void> {
    const node = this.nodeInternal;
    const cssModel = node.domModel().cssModel();
    const cascade = await cssModel.cachedMatchedCascadeForNode(node);
    if (!cascade) {
      return;
    }

    const indent = Common.Settings.Settings.instance().moduleSetting('textEditorIndent').get();
    const lines: string[] = [];
    for (const style of cascade.nodeStyles().reverse()) {
      for (const property of style.leadingProperties()) {
        if (!property.parsedOk || property.disabled || !property.activeInStyle() || property.implicit) {
          continue;
        }
        if (cascade.isInherited(style) && !SDK.CSSMetadata.cssMetadata().isPropertyInherited(property.name)) {
          continue;
        }
        if (style.parentRule && style.parentRule.isUserAgent()) {
          continue;
        }
        if (cascade.propertyState(property) !== SDK.CSSMatchedStyles.PropertyState.Active) {
          continue;
        }
        lines.push(`${indent}${property.name}: ${property.value};`);
      }
    }

    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(lines.join('\n'));
  }

  private highlightSearchResultsInternal(): void {
    if (!this.searchQuery || !this.searchHighlightsVisible) {
      return;
    }
    this.hideSearchHighlight();

    const text = this.listItemElement.textContent || '';
    const regexObject = createPlainTextSearchRegex(this.searchQuery, 'gi');

    let match = regexObject.exec(text);
    const matchRanges = [];
    while (match) {
      matchRanges.push(new TextUtils.TextRange.SourceRange(match.index, match[0].length));
      match = regexObject.exec(text);
    }

    // Fall back for XPath, etc. matches.
    if (!matchRanges.length) {
      matchRanges.push(new TextUtils.TextRange.SourceRange(0, text.length));
    }

    this.highlightResult = [];
    UI.UIUtils.highlightSearchResults(this.listItemElement, matchRanges, this.highlightResult);
  }

  private editAsHTML(): void {
    const promise = Common.Revealer.reveal(this.node());
    promise.then(() => {
      const action = UI.ActionRegistry.ActionRegistry.instance().action('elements.edit-as-html');
      if (!action) {
        return;
      }
      return action.execute();
    });
  }

  // TODO: add unit tests for adorner-related methods after component and TypeScript works are done
  adorn({name}: {name: string}): Adorners.Adorner.Adorner {
    const adornerContent = document.createElement('span');
    adornerContent.textContent = name;
    const adorner = new Adorners.Adorner.Adorner();
    adorner.data = {
      name,
      content: adornerContent,
    };
    this.adorners.push(adorner);
    ElementsPanel.instance().registerAdorner(adorner);
    this.updateAdorners();
    return adorner;
  }

  removeAdorner(adornerToRemove: Adorners.Adorner.Adorner): void {
    const adorners = this.adorners;
    ElementsPanel.instance().deregisterAdorner(adornerToRemove);
    adornerToRemove.remove();
    for (let i = 0; i < adorners.length; ++i) {
      if (adorners[i] === adornerToRemove) {
        adorners.splice(i, 1);
        this.updateAdorners();
        return;
      }
    }
  }

  removeAllAdorners(): void {
    for (const adorner of this.adorners) {
      ElementsPanel.instance().deregisterAdorner(adorner);
      adorner.remove();
    }

    this.adorners = [];
    this.updateAdorners();
  }

  private updateAdorners(): void {
    this.adornersThrottler.schedule(this.updateAdornersInternal.bind(this));
  }

  private updateAdornersInternal(): Promise<void> {
    const adornerContainer = this.adornerContainer;
    if (!adornerContainer) {
      return Promise.resolve();
    }
    const adorners = this.adorners;
    if (adorners.length === 0) {
      adornerContainer.classList.add('hidden');
      return Promise.resolve();
    }

    adorners.sort(adornerComparator);

    adornerContainer.removeChildren();
    for (const adorner of adorners) {
      adornerContainer.appendChild(adorner);
    }
    adornerContainer.classList.remove('hidden');
    return Promise.resolve();
  }

  async updateStyleAdorners(): Promise<void> {
    if (this.isClosingTagInternal) {
      return;
    }

    const node = this.node();
    const nodeId = node.id;
    if (node.nodeType() === Node.COMMENT_NODE || node.nodeType() === Node.DOCUMENT_FRAGMENT_NODE ||
        node.nodeType() === Node.TEXT_NODE || nodeId === undefined) {
      return;
    }

    const styles = await node.domModel().cssModel().computedStylePromise(nodeId);
    for (const styleAdorner of this.styleAdorners) {
      this.removeAdorner(styleAdorner);
    }
    this.styleAdorners = [];
    if (!styles) {
      return;
    }

    const display = styles.get('display');
    const isGrid = display === 'grid' || display === 'inline-grid';
    const isFlex = display === 'flex' || display === 'inline-flex';

    const containerType = styles.get('container-type');
    const contain = styles.get('contain');
    const isContainer =
        SDK.CSSContainerQuery.getQueryAxis(`${containerType} ${contain}`) !== SDK.CSSContainerQuery.QueryAxis.None;

    const appendAdorner = (adorner?: Adorners.Adorner.Adorner|null): void => {
      if (adorner) {
        this.styleAdorners.push(adorner);
      }
    };
    if (isGrid) {
      appendAdorner(this.createGridAdorner());
    }
    if (isFlex) {
      appendAdorner(this.createFlexAdorner());
    }
    if (styles.get('scroll-snap-type') && styles.get('scroll-snap-type') !== 'none') {
      appendAdorner(this.createScrollSnapAdorner());
    }
    if (isContainer) {
      appendAdorner(this.createContainerAdorner());
    }
  }

  createGridAdorner(): Adorners.Adorner.Adorner|null {
    const node = this.node();
    const nodeId = node.id;
    if (!nodeId) {
      return null;
    }

    const config = ElementsComponents.AdornerManager.getRegisteredAdorner(
        ElementsComponents.AdornerManager.RegisteredAdorners.GRID);
    const adorner = this.adorn(config);
    adorner.classList.add('grid');

    const onClick = (((): void => {
                       if (adorner.isActive()) {
                         node.domModel().overlayModel().highlightGridInPersistentOverlay(nodeId);
                       } else {
                         node.domModel().overlayModel().hideGridInPersistentOverlay(nodeId);
                       }
                     }) as EventListener);
    adorner.addInteraction(onClick, {
      isToggle: true,
      shouldPropagateOnKeydown: false,
      ariaLabelDefault: i18nString(UIStrings.enableGridMode),
      ariaLabelActive: i18nString(UIStrings.disableGridMode),
    });

    node.domModel().overlayModel().addEventListener(
        SDK.OverlayModel.Events.PersistentGridOverlayStateChanged, event => {
          const {nodeId: eventNodeId, enabled} = event.data;
          if (eventNodeId !== nodeId) {
            return;
          }
          adorner.toggle(enabled);
        });

    return adorner;
  }

  createScrollSnapAdorner(): Adorners.Adorner.Adorner|null {
    const node = this.node();
    const nodeId = node.id;
    if (!nodeId) {
      return null;
    }
    const config = ElementsComponents.AdornerManager.getRegisteredAdorner(
        ElementsComponents.AdornerManager.RegisteredAdorners.SCROLL_SNAP);
    const adorner = this.adorn(config);
    adorner.classList.add('scroll-snap');

    const onClick = (((): void => {
                       const model = node.domModel().overlayModel();
                       if (adorner.isActive()) {
                         model.highlightScrollSnapInPersistentOverlay(nodeId);
                       } else {
                         model.hideScrollSnapInPersistentOverlay(nodeId);
                       }
                     }) as EventListener);

    adorner.addInteraction(onClick, {
      isToggle: true,
      shouldPropagateOnKeydown: false,
      ariaLabelDefault: i18nString(UIStrings.enableScrollSnap),
      ariaLabelActive: i18nString(UIStrings.disableScrollSnap),
    });

    node.domModel().overlayModel().addEventListener(
        SDK.OverlayModel.Events.PersistentScrollSnapOverlayStateChanged, event => {
          const {nodeId: eventNodeId, enabled} = event.data;
          if (eventNodeId !== nodeId) {
            return;
          }
          adorner.toggle(enabled);
        });

    return adorner;
  }

  createFlexAdorner(): Adorners.Adorner.Adorner|null {
    const node = this.node();
    const nodeId = node.id;
    if (!nodeId) {
      return null;
    }
    const config = ElementsComponents.AdornerManager.getRegisteredAdorner(
        ElementsComponents.AdornerManager.RegisteredAdorners.FLEX);
    const adorner = this.adorn(config);
    adorner.classList.add('flex');

    const onClick = (((): void => {
                       const model = node.domModel().overlayModel();
                       if (adorner.isActive()) {
                         model.highlightFlexContainerInPersistentOverlay(nodeId);
                       } else {
                         model.hideFlexContainerInPersistentOverlay(nodeId);
                       }
                     }) as EventListener);

    adorner.addInteraction(onClick, {
      isToggle: true,
      shouldPropagateOnKeydown: false,
      ariaLabelDefault: i18nString(UIStrings.enableFlexMode),
      ariaLabelActive: i18nString(UIStrings.disableFlexMode),
    });

    node.domModel().overlayModel().addEventListener(
        SDK.OverlayModel.Events.PersistentFlexContainerOverlayStateChanged, event => {
          const {nodeId: eventNodeId, enabled} = event.data;
          if (eventNodeId !== nodeId) {
            return;
          }
          adorner.toggle(enabled);
        });

    return adorner;
  }

  createContainerAdorner(): Adorners.Adorner.Adorner|null {
    const node = this.node();
    const nodeId = node.id;
    if (!nodeId) {
      return null;
    }
    const config = ElementsComponents.AdornerManager.getRegisteredAdorner(
        ElementsComponents.AdornerManager.RegisteredAdorners.CONTAINER);
    const adorner = this.adorn(config);
    adorner.classList.add('container');

    const onClick = (((): void => {
                       const model = node.domModel().overlayModel();
                       if (adorner.isActive()) {
                         model.highlightContainerQueryInPersistentOverlay(nodeId);
                       } else {
                         model.hideContainerQueryInPersistentOverlay(nodeId);
                       }
                     }) as EventListener);

    adorner.addInteraction(onClick, {
      isToggle: true,
      shouldPropagateOnKeydown: false,
      ariaLabelDefault: i18nString(UIStrings.enableScrollSnap),
      ariaLabelActive: i18nString(UIStrings.disableScrollSnap),
    });

    node.domModel().overlayModel().addEventListener(
        SDK.OverlayModel.Events.PersistentContainerQueryOverlayStateChanged, event => {
          const {nodeId: eventNodeId, enabled} = event.data;
          if (eventNodeId !== nodeId) {
            return;
          }
          adorner.toggle(enabled);
        });

    return adorner;
  }
}

export const InitialChildrenLimit = 500;

// A union of HTML4 and HTML5-Draft elements that explicitly
// or implicitly (for HTML5) forbid the closing tag.
export const ForbiddenClosingTagElements = new Set<string>([
  'area', 'base',  'basefont', 'br',   'canvas',   'col',  'command', 'embed',  'frame', 'hr',
  'img',  'input', 'keygen',   'link', 'menuitem', 'meta', 'param',   'source', 'track', 'wbr',
]);

// These tags we do not allow editing their tag name.
export const EditTagBlocklist = new Set<string>(['html', 'head', 'body']);

export function adornerComparator(adornerA: Adorners.Adorner.Adorner, adornerB: Adorners.Adorner.Adorner): number {
  const compareCategories =
      ElementsComponents.AdornerManager.compareAdornerNamesByCategory(adornerB.name, adornerB.name);
  if (compareCategories === 0) {
    return adornerA.name.localeCompare(adornerB.name);
  }
  return compareCategories;
}
export interface EditorHandles {
  commit: () => void;
  cancel: () => void;
  editor?: TextEditor.TextEditor.TextEditor;
  // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  resize: () => any;
}
