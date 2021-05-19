// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

import * as Components from '../../components/components.js';
import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as InlineEditor from '../../inline_editor/inline_editor.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as TextUtils from '../../models/text_utils/text_utils.js';
import * as WebComponents from '../../ui/components/components.js';
import * as UI from '../../ui/legacy/legacy.js';

import {FontEditorSectionManager} from './ColorSwatchPopoverIcon.js';
import {ComputedStyleModel} from './ComputedStyleModel.js';
import {findIcon} from './CSSPropertyIconResolver.js';
import {linkifyDeferredNodeReference} from './DOMLinkifier.js';
import {ElementsSidebarPane} from './ElementsSidebarPane.js';
import {FlexboxEditorWidget} from './FlexboxEditorWidget.js';
import {ImagePreviewPopover} from './ImagePreviewPopover.js';
import {StylePropertyHighlighter} from './StylePropertyHighlighter.js';
import {Context, StylePropertyTreeElement} from './StylePropertyTreeElement.js';  // eslint-disable-line no-unused-vars

const UIStrings = {
  /**
  *@description No matches element text content in Styles Sidebar Pane of the Elements panel
  */
  noMatchingSelectorOrStyle: 'No matching selector or style',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  invalidPropertyValue: 'Invalid property value',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  unknownPropertyName: 'Unknown property name',
  /**
  *@description Text to filter result items
  */
  filter: 'Filter',
  /**
  *@description ARIA accessible name in Styles Sidebar Pane of the Elements panel
  */
  filterStyles: 'Filter Styles',
  /**
  *@description Separator element text content in Styles Sidebar Pane of the Elements panel
  *@example {scrollbar-corner} PH1
  */
  pseudoSElement: 'Pseudo ::{PH1} element',
  /**
  *@description Text of a DOM element in Styles Sidebar Pane of the Elements panel
  */
  inheritedFroms: 'Inherited from ',
  /**
  *@description Tooltip text that appears when hovering over the largeicon add button in the Styles Sidebar Pane of the Elements panel
  */
  insertStyleRuleBelow: 'Insert Style Rule Below',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  constructedStylesheet: 'constructed stylesheet',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  userAgentStylesheet: 'user agent stylesheet',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  injectedStylesheet: 'injected stylesheet',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  viaInspector: 'via inspector',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  */
  styleAttribute: '`style` attribute',
  /**
  *@description Text in Styles Sidebar Pane of the Elements panel
  *@example {html} PH1
  */
  sattributesStyle: '{PH1}[Attributes Style]',
  /**
  *@description Show all button text content in Styles Sidebar Pane of the Elements panel
  *@example {3} PH1
  */
  showAllPropertiesSMore: 'Show All Properties ({PH1} more)',
  /**
  *@description Text in Elements Tree Element of the Elements panel, copy should be used as a verb
  */
  copySelector: 'Copy `selector`',
  /**
  *@description A context menu item in Styles panel to copy CSS rule
  */
  copyRule: 'Copy rule',
  /**
  *@description A context menu item in Styles panel to copy all CSS declarations
  */
  copyAllDeclarations: 'Copy all declarations',
  /**
  *@description Title of  in styles sidebar pane of the elements panel
  *@example {Ctrl} PH1
  */
  incrementdecrementWithMousewheelOne:
      'Increment/decrement with mousewheel or up/down keys. {PH1}: R ±1, Shift: G ±1, Alt: B ±1',
  /**
  *@description Title of  in styles sidebar pane of the elements panel
  *@example {Ctrl} PH1
  */
  incrementdecrementWithMousewheelHundred:
      'Increment/decrement with mousewheel or up/down keys. {PH1}: ±100, Shift: ±10, Alt: ±0.1',
  /**
  *@description Announcement string for invalid properties.
  *@example {Invalid property value} PH1
  *@example {font-size} PH2
  *@example {invalidValue} PH3
  */
  invalidString: '{PH1}, property name: {PH2}, property value: {PH3}',
  /**
  *@description Tooltip text that appears when hovering over the largeicon add button in the Styles Sidebar Pane of the Elements panel
  */
  newStyleRule: 'New Style Rule',
};
const str_ = i18n.i18n.registerUIStrings('panels/elements/StylesSidebarPane.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
// Highlightable properties are those that can be hovered in the sidebar to trigger a specific
// highlighting mode on the current element.
const HIGHLIGHTABLE_PROPERTIES = [
  {mode: 'padding', properties: ['padding']},
  {mode: 'border', properties: ['border']},
  {mode: 'margin', properties: ['margin']},
  {mode: 'gap', properties: ['gap', 'grid-gap']},
  {mode: 'column-gap', properties: ['column-gap', 'grid-column-gap']},
  {mode: 'row-gap', properties: ['row-gap', 'grid-row-gap']},
  {mode: 'grid-template-columns', properties: ['grid-template-columns']},
  {mode: 'grid-template-rows', properties: ['grid-template-rows']},
  {mode: 'grid-template-areas', properties: ['grid-areas']},
  {mode: 'justify-content', properties: ['justify-content']},
  {mode: 'align-content', properties: ['align-content']},
  {mode: 'align-items', properties: ['align-items']},
  {mode: 'flexibility', properties: ['flex', 'flex-basis', 'flex-grow', 'flex-shrink']},
];

/** @type {!StylesSidebarPane} */
let _stylesSidebarPaneInstance;

export class StylesSidebarPane extends ElementsSidebarPane {
  /**
   * @return {!StylesSidebarPane}
   */
  static instance() {
    if (!_stylesSidebarPaneInstance) {
      _stylesSidebarPaneInstance = new StylesSidebarPane();
    }
    return _stylesSidebarPaneInstance;
  }

  /**
   * @private
   */
  constructor() {
    super(true /* delegatesFocus */);
    this.setMinimumSize(96, 26);
    this.registerRequiredCSS('panels/elements/stylesSidebarPane.css', {enableLegacyPatching: false});

    Common.Settings.Settings.instance().moduleSetting('colorFormat').addChangeListener(this.update.bind(this));
    Common.Settings.Settings.instance().moduleSetting('textEditorIndent').addChangeListener(this.update.bind(this));

    /** @type {?UI.Widget.Widget} */
    this._currentToolbarPane = null;
    /** @type {?UI.Widget.Widget} */
    this._animatedToolbarPane = null;
    /** @type {?UI.Widget.Widget} */
    this._pendingWidget = null;
    /** @type {?UI.Toolbar.ToolbarToggle} */
    this._pendingWidgetToggle = null;
    /** @type {?UI.Toolbar.Toolbar} */
    this._toolbar = null;
    this._toolbarPaneElement = this._createStylesSidebarToolbar();
    this._computedStyleModel = new ComputedStyleModel();

    this._noMatchesElement = this.contentElement.createChild('div', 'gray-info-message hidden');
    this._noMatchesElement.textContent = i18nString(UIStrings.noMatchingSelectorOrStyle);

    this._sectionsContainer = this.contentElement.createChild('div');
    UI.ARIAUtils.markAsTree(this._sectionsContainer);
    this._sectionsContainer.addEventListener('keydown', this._sectionsContainerKeyDown.bind(this), false);
    this._sectionsContainer.addEventListener('focusin', this._sectionsContainerFocusChanged.bind(this), false);
    this._sectionsContainer.addEventListener('focusout', this._sectionsContainerFocusChanged.bind(this), false);
    /** @type {!WeakMap<!Node, !StylePropertiesSection>} */
    this.sectionByElement = new WeakMap();

    this._swatchPopoverHelper = new InlineEditor.SwatchPopoverHelper.SwatchPopoverHelper();
    this._swatchPopoverHelper.addEventListener(
        InlineEditor.SwatchPopoverHelper.Events.WillShowPopover, this.hideAllPopovers, this);
    this._linkifier = new Components.Linkifier.Linkifier(_maxLinkLength, /* useLinkDecorator */ true);
    /** @type {!StylePropertyHighlighter} */
    this._decorator = new StylePropertyHighlighter(this);
    /** @type {?SDK.CSSProperty.CSSProperty} */
    this._lastRevealedProperty = null;
    this._userOperation = false;
    this._isEditingStyle = false;
    /** @type {?RegExp} */
    this._filterRegex = null;
    this._isActivePropertyHighlighted = false;
    this._initialUpdateCompleted = false;
    this.hasMatchedStyles = false;

    this.contentElement.classList.add('styles-pane');

    /** @type {!Array<!SectionBlock>} */
    this._sectionBlocks = [];
    /** @type {?IdleCallbackManager} */
    this._idleCallbackManager = null;
    this._needsForceUpdate = false;
    _stylesSidebarPaneInstance = this;
    UI.Context.Context.instance().addFlavorChangeListener(SDK.DOMModel.DOMNode, this.forceUpdate, this);
    this.contentElement.addEventListener('copy', this._clipboardCopy.bind(this));
    this._resizeThrottler = new Common.Throttler.Throttler(100);

    this._imagePreviewPopover = new ImagePreviewPopover(this.contentElement, event => {
      const link = event.composedPath()[0];
      if (link instanceof Element) {
        return link;
      }
      return null;
    }, () => this.node());

    /** @type {?InlineEditor.CSSAngle.CSSAngle} */
    this.activeCSSAngle = null;
  }

  /**
   * @return {!InlineEditor.SwatchPopoverHelper.SwatchPopoverHelper}
   */
  swatchPopoverHelper() {
    return this._swatchPopoverHelper;
  }

  /**
   * @param {boolean} userOperation
   */
  setUserOperation(userOperation) {
    this._userOperation = userOperation;
  }

  /**
   * @param {!SDK.CSSProperty.CSSProperty} property
   * @param {?string} title
   * @return {!Element}
   */
  static createExclamationMark(property, title) {
    const exclamationElement =
        /** @type {!UI.UIUtils.DevToolsIconLabel} */ (document.createElement('span', {is: 'dt-icon-label'}));
    exclamationElement.className = 'exclamation-mark';
    if (!StylesSidebarPane.ignoreErrorsForProperty(property)) {
      exclamationElement.type = 'smallicon-warning';
    }
    /** @type {string | Common.UIString.LocalizedString} */
    let invalidMessage;
    if (title) {
      UI.Tooltip.Tooltip.install(exclamationElement, title);
      invalidMessage = title;
    } else {
      invalidMessage = SDK.CSSMetadata.cssMetadata().isCSSPropertyName(property.name) ?
          i18nString(UIStrings.invalidPropertyValue) :
          i18nString(UIStrings.unknownPropertyName);
      UI.Tooltip.Tooltip.install(exclamationElement, invalidMessage);
    }
    const invalidString =
        i18nString(UIStrings.invalidString, {PH1: invalidMessage, PH2: property.name, PH3: property.value});

    // Storing the invalidString for future screen reader support when editing the property
    property.setDisplayedStringForInvalidProperty(invalidString);

    return exclamationElement;
  }

  /**
   * @param {!SDK.CSSProperty.CSSProperty} property
   * @return {boolean}
   */
  static ignoreErrorsForProperty(property) {
    /**
     * @param {string} string
     */
    function hasUnknownVendorPrefix(string) {
      return !string.startsWith('-webkit-') && /^[-_][\w\d]+-\w/.test(string);
    }

    const name = property.name.toLowerCase();

    // IE hack.
    if (name.charAt(0) === '_') {
      return true;
    }

    // IE has a different format for this.
    if (name === 'filter') {
      return true;
    }

    // Common IE-specific property prefix.
    if (name.startsWith('scrollbar-')) {
      return true;
    }
    if (hasUnknownVendorPrefix(name)) {
      return true;
    }

    const value = property.value.toLowerCase();

    // IE hack.
    if (value.endsWith('\\9')) {
      return true;
    }
    if (hasUnknownVendorPrefix(value)) {
      return true;
    }

    return false;
  }

  /**
   * @param {string} placeholder
   * @param {!Element} container
   * @param {function(?RegExp):void} filterCallback
   * @return {!Element}
   */
  static createPropertyFilterElement(placeholder, container, filterCallback) {
    const input = document.createElement('input');
    input.type = 'search';
    input.classList.add('custom-search-input');
    input.placeholder = placeholder;

    function searchHandler() {
      const regex = input.value ? new RegExp(Platform.StringUtilities.escapeForRegExp(input.value), 'i') : null;
      filterCallback(regex);
    }
    input.addEventListener('input', searchHandler, false);

    /**
     * @param {!Event} event
     */
    function keydownHandler(event) {
      const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
      if (keyboardEvent.key !== 'Escape' || !input.value) {
        return;
      }
      keyboardEvent.consume(true);
      input.value = '';
      searchHandler();
    }
    input.addEventListener('keydown', keydownHandler, false);
    return input;
  }

  /**
   * @param {!StylePropertiesSection} section
   * @return {{allDeclarationText: string, ruleText: string}}
   */
  static formatLeadingProperties(section) {
    const selectorText = section._headerText();
    const indent = Common.Settings.Settings.instance().moduleSetting('textEditorIndent').get();

    const style = section._style;
    /** @type {!Array<string>} */
    const lines = [];

    // Invalid property should also be copied.
    // For example: *display: inline.
    for (const property of style.leadingProperties()) {
      if (property.disabled) {
        lines.push(`${indent}/* ${property.name}: ${property.value}; */`);
      } else {
        lines.push(`${indent}${property.name}: ${property.value};`);
      }
    }

    /** @type {string} */
    const allDeclarationText = lines.join('\n');
    /** @type {string} */
    const ruleText = `${selectorText} {\n${allDeclarationText}\n}`;

    return {
      allDeclarationText,
      ruleText,
    };
  }

  /**
   * @param {!SDK.CSSProperty.CSSProperty} cssProperty
   */
  revealProperty(cssProperty) {
    this._decorator.highlightProperty(cssProperty);
    this._lastRevealedProperty = cssProperty;
    this.update();
  }

  /**
   * @param {string} propertyName
   */
  jumpToProperty(propertyName) {
    this._decorator.findAndHighlightPropertyName(propertyName);
  }

  forceUpdate() {
    this._needsForceUpdate = true;
    this._swatchPopoverHelper.hide();
    this._resetCache();
    this.update();
  }

  /**
   * @param {!Event} event
   */
  _sectionsContainerKeyDown(event) {
    const activeElement = this._sectionsContainer.ownerDocument.deepActiveElement();
    if (!activeElement) {
      return;
    }
    const section = this.sectionByElement.get(activeElement);
    if (!section) {
      return;
    }
    let sectionToFocus = null;
    let willIterateForward = false;
    switch (/** @type {!KeyboardEvent} */ (event).key) {
      case 'ArrowUp':
      case 'ArrowLeft': {
        sectionToFocus = section.previousSibling() || section.lastSibling();
        willIterateForward = false;
        break;
      }
      case 'ArrowDown':
      case 'ArrowRight': {
        sectionToFocus = section.nextSibling() || section.firstSibling();
        willIterateForward = true;
        break;
      }
      case 'Home': {
        sectionToFocus = section.firstSibling();
        willIterateForward = true;
        break;
      }
      case 'End': {
        sectionToFocus = section.lastSibling();
        willIterateForward = false;
        break;
      }
    }

    if (sectionToFocus && this._filterRegex) {
      sectionToFocus = sectionToFocus.findCurrentOrNextVisible(/* willIterateForward= */ willIterateForward);
    }
    if (sectionToFocus) {
      sectionToFocus.element.focus();
      event.consume(true);
    }
  }

  _sectionsContainerFocusChanged() {
    this.resetFocus();
  }

  resetFocus() {
    // When a styles section is focused, shift+tab should leave the section.
    // Leaving tabIndex = 0 on the first element would cause it to be focused instead.
    if (!this._noMatchesElement.classList.contains('hidden')) {
      return;
    }
    if (this._sectionBlocks[0] && this._sectionBlocks[0].sections[0]) {
      const firstVisibleSection =
          this._sectionBlocks[0].sections[0].findCurrentOrNextVisible(/* willIterateForward= */ true);
      if (firstVisibleSection) {
        firstVisibleSection.element.tabIndex = this._sectionsContainer.hasFocus() ? -1 : 0;
      }
    }
  }

  /**
   * @param {!Event} event
   */
  _onAddButtonLongClick(event) {
    const cssModel = this.cssModel();
    if (!cssModel) {
      return;
    }
    const headers = cssModel.styleSheetHeaders().filter(styleSheetResourceHeader);

    /** @type {!Array.<{text: string, handler: function():Promise<void>}>} */
    const contextMenuDescriptors = [];
    for (let i = 0; i < headers.length; ++i) {
      const header = headers[i];
      const handler = this._createNewRuleInStyleSheet.bind(this, header);
      contextMenuDescriptors.push({text: Bindings.ResourceUtils.displayNameForURL(header.resourceURL()), handler});
    }

    contextMenuDescriptors.sort(compareDescriptors);

    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    for (let i = 0; i < contextMenuDescriptors.length; ++i) {
      const descriptor = contextMenuDescriptors[i];
      contextMenu.defaultSection().appendItem(descriptor.text, descriptor.handler);
    }
    contextMenu.footerSection().appendItem(
        'inspector-stylesheet', this._createNewRuleInViaInspectorStyleSheet.bind(this));
    contextMenu.show();

    /**
     * @param {!{text: string, handler: function():Promise<void>}} descriptor1
     * @param {!{text: string, handler: function():Promise<void>}} descriptor2
     * @return {number}
     */
    function compareDescriptors(descriptor1, descriptor2) {
      return Platform.StringUtilities.naturalOrderComparator(descriptor1.text, descriptor2.text);
    }

    /**
     * @param {!SDK.CSSStyleSheetHeader.CSSStyleSheetHeader} header
     * @return {boolean}
     */
    function styleSheetResourceHeader(header) {
      return !header.isViaInspector() && !header.isInline && Boolean(header.resourceURL());
    }
  }

  /**
   * @param {?RegExp} regex
   */
  _onFilterChanged(regex) {
    this._filterRegex = regex;
    this._updateFilter();
    this.resetFocus();
  }

  /**
   * @param {!StylePropertiesSection} editedSection
   * @param {!StylePropertyTreeElement=} editedTreeElement
   */
  _refreshUpdate(editedSection, editedTreeElement) {
    if (editedTreeElement) {
      for (const section of this.allSections()) {
        if (section instanceof BlankStylePropertiesSection && section.isBlank) {
          continue;
        }
        section._updateVarFunctions(editedTreeElement);
      }
    }

    if (this._isEditingStyle) {
      return;
    }
    const node = this.node();
    if (!node) {
      return;
    }

    for (const section of this.allSections()) {
      if (section instanceof BlankStylePropertiesSection && section.isBlank) {
        continue;
      }
      section.update(section === editedSection);
    }

    if (this._filterRegex) {
      this._updateFilter();
    }
    this._nodeStylesUpdatedForTest(node, false);
  }

  /**
   * @override
   * @return {!Promise.<?>}
   */
  async doUpdate() {
    if (!this._initialUpdateCompleted) {
      setTimeout(() => {
        if (!this._initialUpdateCompleted) {
          // the spinner will get automatically removed when _innerRebuildUpdate is called
          this._sectionsContainer.createChild('span', 'spinner');
        }
      }, 200 /* only spin for loading time > 200ms to avoid unpleasant render flashes */);
    }

    const matchedStyles = await this._fetchMatchedCascade();
    await this._innerRebuildUpdate(matchedStyles);
    if (!this._initialUpdateCompleted) {
      this._initialUpdateCompleted = true;
      this.dispatchEventToListeners(Events.InitialUpdateCompleted);
    }

    this.dispatchEventToListeners(Events.StylesUpdateCompleted, {hasMatchedStyles: this.hasMatchedStyles});
  }

  /**
   * @override
   */
  onResize() {
    this._resizeThrottler.schedule(this._innerResize.bind(this));
  }

  /**
   * @return {!Promise<void>}
   */
  _innerResize() {
    const width = this.contentElement.getBoundingClientRect().width + 'px';
    this.allSections().forEach(section => {
      section.propertiesTreeOutline.element.style.width = width;
    });
    return Promise.resolve();
  }

  _resetCache() {
    const cssModel = this.cssModel();
    if (cssModel) {
      cssModel.discardCachedMatchedCascade();
    }
  }

  /**
   * @return {!Promise.<?SDK.CSSMatchedStyles.CSSMatchedStyles>}
   */
  _fetchMatchedCascade() {
    const node = this.node();
    if (!node || !this.cssModel()) {
      return Promise.resolve(/** @type {?SDK.CSSMatchedStyles.CSSMatchedStyles} */ (null));
    }

    const cssModel = this.cssModel();
    if (!cssModel) {
      return Promise.resolve(null);
    }
    return cssModel.cachedMatchedCascadeForNode(node).then(validateStyles.bind(this));

    /**
     * @param {?SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
     * @return {?SDK.CSSMatchedStyles.CSSMatchedStyles}
     * @this {StylesSidebarPane}
     */
    function validateStyles(matchedStyles) {
      return matchedStyles && matchedStyles.node() === this.node() ? matchedStyles : null;
    }
  }

  /**
   * @param {boolean} editing
   * @param {!StylePropertyTreeElement=} treeElement
   */
  setEditingStyle(editing, treeElement) {
    if (this._isEditingStyle === editing) {
      return;
    }
    this.contentElement.classList.toggle('is-editing-style', editing);
    this._isEditingStyle = editing;
    this._setActiveProperty(null);
  }

  /**
   * @param {?StylePropertyTreeElement} treeElement
   */
  _setActiveProperty(treeElement) {
    if (this._isActivePropertyHighlighted) {
      SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
    }
    this._isActivePropertyHighlighted = false;

    if (!this.node()) {
      return;
    }

    if (!treeElement || treeElement.overloaded() || treeElement.inherited()) {
      return;
    }

    const rule = treeElement.property.ownerStyle.parentRule;
    const selectorList = (rule instanceof SDK.CSSRule.CSSStyleRule) ? rule.selectorText() : undefined;
    for (const {properties, mode} of HIGHLIGHTABLE_PROPERTIES) {
      if (!properties.includes(treeElement.name)) {
        continue;
      }
      const node = this.node();
      if (!node) {
        continue;
      }
      node.domModel().overlayModel().highlightInOverlay(
          {node: /** @type {!SDK.DOMModel.DOMNode} */ (this.node()), selectorList}, mode);
      this._isActivePropertyHighlighted = true;
      break;
    }
  }

  /**
   * @override
   * @param {!Common.EventTarget.EventTargetEvent=} event
   */
  onCSSModelChanged(event) {
    const edit = event && event.data ? /** @type {?SDK.CSSModel.Edit} */ (event.data.edit) : null;
    if (edit) {
      for (const section of this.allSections()) {
        section._styleSheetEdited(edit);
      }
      return;
    }

    if (this._userOperation || this._isEditingStyle) {
      return;
    }

    this._resetCache();
    this.update();
  }

  /**
   * @return {number}
   */
  focusedSectionIndex() {
    let index = 0;
    for (const block of this._sectionBlocks) {
      for (const section of block.sections) {
        if (section.element.hasFocus()) {
          return index;
        }
        index++;
      }
    }
    return -1;
  }

  /**
   * @param {number} sectionIndex
   * @param {number} propertyIndex
   */
  continueEditingElement(sectionIndex, propertyIndex) {
    const section = this.allSections()[sectionIndex];
    if (section) {
      const element = /** @type {?StylePropertyTreeElement} */ (section.closestPropertyForEditing(propertyIndex));
      if (!element) {
        section.element.focus();
        return;
      }
      element.startEditing();
    }
  }

  /**
   * @param {?SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @return {!Promise<void>}
   */
  async _innerRebuildUpdate(matchedStyles) {
    // ElementsSidebarPane's throttler schedules this method. Usually,
    // rebuild is suppressed while editing (see onCSSModelChanged()), but we need a
    // 'force' flag since the currently running throttler process cannot be canceled.
    if (this._needsForceUpdate) {
      this._needsForceUpdate = false;
    } else if (this._isEditingStyle || this._userOperation) {
      return;
    }

    const focusedIndex = this.focusedSectionIndex();

    this._linkifier.reset();
    const prevSections = this._sectionBlocks.map(block => block.sections).flat();
    this._sectionBlocks = [];

    const node = this.node();
    this.hasMatchedStyles = matchedStyles !== null && node !== null;
    if (!this.hasMatchedStyles) {
      this._sectionsContainer.removeChildren();
      this._noMatchesElement.classList.remove('hidden');
      return;
    }

    this._sectionBlocks = await this._rebuildSectionsForMatchedStyleRules(
        /** @type {!SDK.CSSMatchedStyles.CSSMatchedStyles} */ (matchedStyles));

    // Style sections maybe re-created when flexbox editor is activated.
    // With the following code we re-bind the flexbox editor to the new
    // section with the same index as the previous section had.
    const newSections = this._sectionBlocks.map(block => block.sections).flat();
    const flexEditorWidget = FlexboxEditorWidget.instance();
    const boundSection = flexEditorWidget.getSection();
    if (boundSection) {
      flexEditorWidget.unbindContext();
      for (const [index, prevSection] of prevSections.entries()) {
        if (boundSection === prevSection && index < newSections.length) {
          flexEditorWidget.bindContext(this, newSections[index]);
        }
      }
    }

    this._sectionsContainer.removeChildren();
    const fragment = document.createDocumentFragment();

    let index = 0;
    let elementToFocus = null;
    for (const block of this._sectionBlocks) {
      const titleElement = block.titleElement();
      if (titleElement) {
        fragment.appendChild(titleElement);
      }
      for (const section of block.sections) {
        fragment.appendChild(section.element);
        if (index === focusedIndex) {
          elementToFocus = section.element;
        }
        index++;
      }
    }

    this._sectionsContainer.appendChild(fragment);

    if (elementToFocus) {
      elementToFocus.focus();
    }

    if (focusedIndex >= index) {
      this._sectionBlocks[0].sections[0].element.focus();
    }

    this._sectionsContainerFocusChanged();

    if (this._filterRegex) {
      this._updateFilter();
    } else {
      this._noMatchesElement.classList.toggle('hidden', this._sectionBlocks.length > 0);
    }

    this._nodeStylesUpdatedForTest(/** @type {!SDK.DOMModel.DOMNode} */ (node), true);
    if (this._lastRevealedProperty) {
      this._decorator.highlightProperty(this._lastRevealedProperty);
      this._lastRevealedProperty = null;
    }

    // Record the elements tool load time after the sidepane has loaded.
    Host.userMetrics.panelLoaded('elements', 'DevTools.Launch.Elements');

    this.dispatchEventToListeners(Events.StylesUpdateCompleted, {hasStyle: true});
  }

  /**
   * @param {!SDK.DOMModel.DOMNode} node
   * @param {boolean} rebuild
   */
  _nodeStylesUpdatedForTest(node, rebuild) {
    // For sniffing in tests.
  }

  /**
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @return {!Promise<!Array.<!SectionBlock>>}
   */
  async _rebuildSectionsForMatchedStyleRules(matchedStyles) {
    if (this._idleCallbackManager) {
      this._idleCallbackManager.discard();
    }

    this._idleCallbackManager = new IdleCallbackManager();

    const blocks = [new SectionBlock(null)];
    let lastParentNode = null;
    for (const style of matchedStyles.nodeStyles()) {
      const parentNode = matchedStyles.isInherited(style) ? matchedStyles.nodeForStyle(style) : null;
      if (parentNode && parentNode !== lastParentNode) {
        lastParentNode = parentNode;
        const block = await SectionBlock._createInheritedNodeBlock(lastParentNode);
        blocks.push(block);
      }

      const lastBlock = blocks[blocks.length - 1];
      if (lastBlock) {
        this._idleCallbackManager.schedule(() => {
          const section = new StylePropertiesSection(this, matchedStyles, style);
          lastBlock.sections.push(section);
        });
      }
    }

    let pseudoTypes = [];
    const keys = matchedStyles.pseudoTypes();
    if (keys.delete(Protocol.DOM.PseudoType.Before)) {
      pseudoTypes.push(Protocol.DOM.PseudoType.Before);
    }
    pseudoTypes = pseudoTypes.concat([...keys].sort());
    for (const pseudoType of pseudoTypes) {
      const block = SectionBlock.createPseudoTypeBlock(pseudoType);
      for (const style of matchedStyles.pseudoStyles(pseudoType)) {
        this._idleCallbackManager.schedule(() => {
          const section = new StylePropertiesSection(this, matchedStyles, style);
          block.sections.push(section);
        });
      }
      blocks.push(block);
    }

    for (const keyframesRule of matchedStyles.keyframes()) {
      const block = SectionBlock.createKeyframesBlock(keyframesRule.name().text);
      for (const keyframe of keyframesRule.keyframes()) {
        this._idleCallbackManager.schedule(() => {
          block.sections.push(new KeyframePropertiesSection(this, matchedStyles, keyframe.style));
        });
      }
      blocks.push(block);
    }

    await this._idleCallbackManager.awaitDone();

    return blocks;
  }

  async _createNewRuleInViaInspectorStyleSheet() {
    const cssModel = this.cssModel();
    const node = this.node();
    if (!cssModel || !node) {
      return;
    }
    this.setUserOperation(true);

    const styleSheetHeader = await cssModel.requestViaInspectorStylesheet(/** @type {!SDK.DOMModel.DOMNode} */ (node));

    this.setUserOperation(false);
    await this._createNewRuleInStyleSheet(styleSheetHeader);
  }

  /**
   * @param {?SDK.CSSStyleSheetHeader.CSSStyleSheetHeader} styleSheetHeader
   */
  async _createNewRuleInStyleSheet(styleSheetHeader) {
    if (!styleSheetHeader) {
      return;
    }

    const text = (await styleSheetHeader.requestContent()).content || '';
    const lines = text.split('\n');
    const range = TextUtils.TextRange.TextRange.createFromLocation(lines.length - 1, lines[lines.length - 1].length);

    if (this._sectionBlocks && this._sectionBlocks.length > 0) {
      this._addBlankSection(this._sectionBlocks[0].sections[0], styleSheetHeader.id, range);
    }
  }

  /**
   * @param {!StylePropertiesSection} insertAfterSection
   * @param {string} styleSheetId
   * @param {!TextUtils.TextRange.TextRange} ruleLocation
   */
  _addBlankSection(insertAfterSection, styleSheetId, ruleLocation) {
    const node = this.node();
    const blankSection = new BlankStylePropertiesSection(
        this, insertAfterSection._matchedStyles, node ? node.simpleSelector() : '', styleSheetId, ruleLocation,
        insertAfterSection._style);

    this._sectionsContainer.insertBefore(blankSection.element, insertAfterSection.element.nextSibling);

    for (const block of this._sectionBlocks) {
      const index = block.sections.indexOf(insertAfterSection);
      if (index === -1) {
        continue;
      }
      block.sections.splice(index + 1, 0, blankSection);
      blankSection.startEditingSelector();
    }
  }

  /**
   * @param {!StylePropertiesSection} section
   */
  removeSection(section) {
    for (const block of this._sectionBlocks) {
      const index = block.sections.indexOf(section);
      if (index === -1) {
        continue;
      }
      block.sections.splice(index, 1);
      section.element.remove();
    }
  }

  /**
   * @return {?RegExp}
   */
  filterRegex() {
    return this._filterRegex;
  }

  _updateFilter() {
    let hasAnyVisibleBlock = false;
    for (const block of this._sectionBlocks) {
      hasAnyVisibleBlock = block.updateFilter() || hasAnyVisibleBlock;
    }
    this._noMatchesElement.classList.toggle('hidden', Boolean(hasAnyVisibleBlock));
  }

  /**
   * @override
   */
  willHide() {
    this.hideAllPopovers();
    super.willHide();
  }

  hideAllPopovers() {
    this._swatchPopoverHelper.hide();
    this._imagePreviewPopover.hide();
    if (this.activeCSSAngle) {
      this.activeCSSAngle.minify();
      this.activeCSSAngle = null;
    }
  }

  /**
   * @return {!Array<!StylePropertiesSection>}
   */
  allSections() {
    /** @type {!Array<!StylePropertiesSection>} */
    let sections = [];
    for (const block of this._sectionBlocks) {
      sections = sections.concat(block.sections);
    }
    return sections;
  }

  /**
   * @param {!Event} event
   */
  _clipboardCopy(event) {
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.StyleRuleCopied);
  }

  /**
   * @return {!HTMLElement}
   */
  _createStylesSidebarToolbar() {
    const container = this.contentElement.createChild('div', 'styles-sidebar-pane-toolbar-container');
    const hbox = container.createChild('div', 'hbox styles-sidebar-pane-toolbar');
    const filterContainerElement = hbox.createChild('div', 'styles-sidebar-pane-filter-box');
    const filterInput = StylesSidebarPane.createPropertyFilterElement(
        i18nString(UIStrings.filter), hbox, this._onFilterChanged.bind(this));
    UI.ARIAUtils.setAccessibleName(filterInput, i18nString(UIStrings.filterStyles));
    filterContainerElement.appendChild(filterInput);
    const toolbar = new UI.Toolbar.Toolbar('styles-pane-toolbar', hbox);
    toolbar.makeToggledGray();
    toolbar.appendItemsAtLocation('styles-sidebarpane-toolbar');
    this._toolbar = toolbar;
    const toolbarPaneContainer = container.createChild('div', 'styles-sidebar-toolbar-pane-container');
    const toolbarPaneContent =
        /** @type {!HTMLElement} */ (toolbarPaneContainer.createChild('div', 'styles-sidebar-toolbar-pane'));

    return toolbarPaneContent;
  }

  /**
   * @param {?UI.Widget.Widget} widget
   * @param {?UI.Toolbar.ToolbarToggle} toggle
   */
  showToolbarPane(widget, toggle) {
    if (this._pendingWidgetToggle) {
      this._pendingWidgetToggle.setToggled(false);
    }
    this._pendingWidgetToggle = toggle;

    if (this._animatedToolbarPane) {
      this._pendingWidget = widget;
    } else {
      this._startToolbarPaneAnimation(widget);
    }

    if (widget && toggle) {
      toggle.setToggled(true);
    }
  }

  /**
   * @param {!UI.Toolbar.ToolbarItem} item
   */
  appendToolbarItem(item) {
    if (this._toolbar) {
      this._toolbar.appendToolbarItem(item);
    }
  }

  /**
   * @param {?UI.Widget.Widget} widget
   */
  _startToolbarPaneAnimation(widget) {
    if (widget === this._currentToolbarPane) {
      return;
    }

    if (widget && this._currentToolbarPane) {
      this._currentToolbarPane.detach();
      widget.show(this._toolbarPaneElement);
      this._currentToolbarPane = widget;
      this._currentToolbarPane.focus();
      return;
    }

    this._animatedToolbarPane = widget;

    if (this._currentToolbarPane) {
      this._toolbarPaneElement.style.animationName = 'styles-element-state-pane-slideout';
    } else if (widget) {
      this._toolbarPaneElement.style.animationName = 'styles-element-state-pane-slidein';
    }

    if (widget) {
      widget.show(this._toolbarPaneElement);
    }

    const listener = onAnimationEnd.bind(this);
    this._toolbarPaneElement.addEventListener('animationend', listener, false);

    /**
     * @this {!StylesSidebarPane}
     */
    function onAnimationEnd() {
      this._toolbarPaneElement.style.removeProperty('animation-name');
      this._toolbarPaneElement.removeEventListener('animationend', listener, false);

      if (this._currentToolbarPane) {
        this._currentToolbarPane.detach();
      }

      this._currentToolbarPane = this._animatedToolbarPane;
      if (this._currentToolbarPane) {
        this._currentToolbarPane.focus();
      }
      this._animatedToolbarPane = null;

      if (this._pendingWidget) {
        this._startToolbarPaneAnimation(this._pendingWidget);
        this._pendingWidget = null;
      }
    }
  }
}

/** @enum {symbol} */
export const Events = {
  InitialUpdateCompleted: Symbol('InitialUpdateCompleted'),
  StylesUpdateCompleted: Symbol('StylesUpdateCompleted'),
};

export const _maxLinkLength = 23;

export class SectionBlock {
  /**
   * @param {?Element} titleElement
   */
  constructor(titleElement) {
    this._titleElement = titleElement;
    /** @type {!Array<!StylePropertiesSection>} */
    this.sections = [];
  }

  /**
   * @param {!Protocol.DOM.PseudoType} pseudoType
   * @return {!SectionBlock}
   */
  static createPseudoTypeBlock(pseudoType) {
    const separatorElement = document.createElement('div');
    separatorElement.className = 'sidebar-separator';
    separatorElement.textContent = i18nString(UIStrings.pseudoSElement, {PH1: pseudoType});
    return new SectionBlock(separatorElement);
  }

  /**
   * @param {string} keyframesName
   * @return {!SectionBlock}
   */
  static createKeyframesBlock(keyframesName) {
    const separatorElement = document.createElement('div');
    separatorElement.className = 'sidebar-separator';
    separatorElement.textContent = `@keyframes ${keyframesName}`;
    return new SectionBlock(separatorElement);
  }

  /**
   * @param {!SDK.DOMModel.DOMNode} node
   * @return {!Promise<!SectionBlock>}
   */
  static async _createInheritedNodeBlock(node) {
    const separatorElement = document.createElement('div');
    separatorElement.className = 'sidebar-separator';
    UI.UIUtils.createTextChild(separatorElement, i18nString(UIStrings.inheritedFroms));
    const link = await Common.Linkifier.Linkifier.linkify(node, {
      preventKeyboardFocus: true,
      tooltip: undefined,
    });
    separatorElement.appendChild(link);
    return new SectionBlock(separatorElement);
  }

  /**
   * @return {boolean}
   */
  updateFilter() {
    let hasAnyVisibleSection = false;
    for (const section of this.sections) {
      hasAnyVisibleSection = section._updateFilter() || hasAnyVisibleSection;
    }
    if (this._titleElement) {
      this._titleElement.classList.toggle('hidden', !hasAnyVisibleSection);
    }
    return Boolean(hasAnyVisibleSection);
  }

  /**
   * @return {?Element}
   */
  titleElement() {
    return this._titleElement;
  }
}

export class IdleCallbackManager {
  constructor() {
    this._discarded = false;
    /** @type {!Array<!Promise<void>>} */
    this._promises = [];
  }

  discard() {
    this._discarded = true;
  }

  /**
   * @param {function():void} fn
   * @param {number} timeout
   */
  schedule(fn, timeout = 100) {
    if (this._discarded) {
      return;
    }
    this._promises.push(new Promise((resolve, reject) => {
      const run = () => {
        try {
          fn();
          resolve();
        } catch (err) {
          reject(err);
        }
      };
      window.requestIdleCallback(() => {
        if (this._discarded) {
          return resolve();
        }
        run();
      }, {timeout});
    }));
  }

  awaitDone() {
    return Promise.all(this._promises);
  }
}

export class StylePropertiesSection {
  /**
   * @param {!StylesSidebarPane} parentPane
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @param {!SDK.CSSStyleDeclaration.CSSStyleDeclaration} style
   */
  constructor(parentPane, matchedStyles, style) {
    this._parentPane = parentPane;
    this._style = style;
    this._matchedStyles = matchedStyles;
    this.editable = Boolean(style.styleSheetId && style.range);
    /** @type {?number} */
    this._hoverTimer = null;
    this._willCauseCancelEditing = false;
    this._forceShowAll = false;
    this._originalPropertiesCount = style.leadingProperties().length;

    const rule = style.parentRule;
    this.element = document.createElement('div');
    this.element.classList.add('styles-section');
    this.element.classList.add('matched-styles');
    this.element.classList.add('monospace');
    UI.ARIAUtils.setAccessibleName(this.element, `${this._headerText()}, css selector`);
    this.element.tabIndex = -1;
    UI.ARIAUtils.markAsTreeitem(this.element);
    this.element.addEventListener('keydown', this._onKeyDown.bind(this), false);
    parentPane.sectionByElement.set(this.element, this);
    this._innerElement = this.element.createChild('div');

    this._titleElement =
        this._innerElement.createChild('div', 'styles-section-title ' + (rule ? 'styles-selector' : ''));

    this.propertiesTreeOutline = new UI.TreeOutline.TreeOutlineInShadow();
    this.propertiesTreeOutline.setFocusable(false);
    this.propertiesTreeOutline.registerRequiredCSS(
        'panels/elements/stylesSectionTree.css', {enableLegacyPatching: false});
    this.propertiesTreeOutline.element.classList.add('style-properties', 'matched-styles', 'monospace');
    // @ts-ignore TODO: fix ad hoc section property in a separate CL to be safe
    this.propertiesTreeOutline.section = this;
    this._innerElement.appendChild(this.propertiesTreeOutline.element);

    this._showAllButton = UI.UIUtils.createTextButton('', this._showAllItems.bind(this), 'styles-show-all');
    this._innerElement.appendChild(this._showAllButton);

    const selectorContainer = document.createElement('div');
    this._selectorElement = document.createElement('span');
    this._selectorElement.classList.add('selector');
    this._selectorElement.textContent = this._headerText();
    selectorContainer.appendChild(this._selectorElement);
    this._selectorElement.addEventListener('mouseenter', this._onMouseEnterSelector.bind(this), false);
    this._selectorElement.addEventListener('mousemove', event => event.consume(), false);
    this._selectorElement.addEventListener('mouseleave', this._onMouseOutSelector.bind(this), false);

    const openBrace = selectorContainer.createChild('span', 'sidebar-pane-open-brace');
    openBrace.textContent = ' {';
    selectorContainer.addEventListener('mousedown', this._handleEmptySpaceMouseDown.bind(this), false);
    selectorContainer.addEventListener('click', this._handleSelectorContainerClick.bind(this), false);

    const closeBrace = this._innerElement.createChild('div', 'sidebar-pane-closing-brace');
    closeBrace.textContent = '}';

    if (this._style.parentRule) {
      const newRuleButton = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.insertStyleRuleBelow), 'largeicon-add');
      newRuleButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this._onNewRuleClick, this);
      newRuleButton.element.tabIndex = -1;
      if (!this._newStyleRuleToolbar) {
        this._newStyleRuleToolbar =
            new UI.Toolbar.Toolbar('sidebar-pane-section-toolbar new-rule-toolbar', this._innerElement);
      }
      this._newStyleRuleToolbar.appendToolbarItem(newRuleButton);
      UI.ARIAUtils.markAsHidden(this._newStyleRuleToolbar.element);
    }

    if (Root.Runtime.experiments.isEnabled('fontEditor') && this.editable) {
      this._fontEditorToolbar = new UI.Toolbar.Toolbar('sidebar-pane-section-toolbar', this._innerElement);
      this._fontEditorSectionManager = new FontEditorSectionManager(this._parentPane.swatchPopoverHelper(), this);
      this._fontEditorButton = new UI.Toolbar.ToolbarButton('Font Editor', 'largeicon-font-editor');
      this._fontEditorButton.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, () => {
        this._onFontEditorButtonClicked();
      }, this);
      this._fontEditorButton.element.addEventListener('keydown', event => {
        if (isEnterOrSpaceKey(event)) {
          event.consume(true);
          this._onFontEditorButtonClicked();
        }
      }, false);
      this._fontEditorToolbar.appendToolbarItem(this._fontEditorButton);

      if (this._style.type === SDK.CSSStyleDeclaration.Type.Inline) {
        if (this._newStyleRuleToolbar) {
          this._newStyleRuleToolbar.element.classList.add('shifted-toolbar');
        }
      } else {
        this._fontEditorToolbar.element.classList.add('font-toolbar-hidden');
      }
    }

    this._selectorElement.addEventListener('click', this._handleSelectorClick.bind(this), false);
    this.element.addEventListener('contextmenu', this._handleContextMenuEvent.bind(this), false);
    this.element.addEventListener('mousedown', this._handleEmptySpaceMouseDown.bind(this), false);
    this.element.addEventListener('click', this._handleEmptySpaceClick.bind(this), false);
    this.element.addEventListener('mousemove', this._onMouseMove.bind(this), false);
    this.element.addEventListener('mouseleave', this._onMouseLeave.bind(this), false);
    this._selectedSinceMouseDown = false;

    /** @type {!WeakMap<!Element, number>} */
    this._elementToSelectorIndex = new WeakMap();

    if (rule) {
      // Prevent editing the user agent and user rules.
      if (rule.isUserAgent() || rule.isInjected()) {
        this.editable = false;
      } else {
        // Check this is a real CSSRule, not a bogus object coming from BlankStylePropertiesSection.
        if (rule.styleSheetId) {
          const header = rule.cssModel().styleSheetHeaderForId(rule.styleSheetId);
          this.navigable = header && !header.isAnonymousInlineStyleSheet();
        }
      }
    }

    this._mediaListElement = this._titleElement.createChild('div', 'media-list media-matches');
    this._selectorRefElement = this._titleElement.createChild('div', 'styles-section-subtitle');
    this._updateMediaList();
    this._updateRuleOrigin();
    this._titleElement.appendChild(selectorContainer);
    this._selectorContainer = selectorContainer;

    if (this.navigable) {
      this.element.classList.add('navigable');
    }

    if (!this.editable) {
      this.element.classList.add('read-only');
      this.propertiesTreeOutline.element.classList.add('read-only');
    }
    /** @type {?FontEditorSectionManager} */
    this._fontPopoverIcon = null;
    this._hoverableSelectorsMode = false;
    this._isHidden = false;
    this._markSelectorMatches();
    this.onpopulate();
  }

  /**
   * @param {!StylePropertyTreeElement} treeElement
   */
  registerFontProperty(treeElement) {
    if (this._fontEditorSectionManager) {
      this._fontEditorSectionManager.registerFontProperty(treeElement);
    }
    if (this._fontEditorToolbar) {
      this._fontEditorToolbar.element.classList.remove('font-toolbar-hidden');
      if (this._newStyleRuleToolbar) {
        this._newStyleRuleToolbar.element.classList.add('shifted-toolbar');
      }
    }
  }

  resetToolbars() {
    if (this._parentPane.swatchPopoverHelper().isShowing() ||
        this._style.type === SDK.CSSStyleDeclaration.Type.Inline) {
      return;
    }
    if (this._fontEditorToolbar) {
      this._fontEditorToolbar.element.classList.add('font-toolbar-hidden');
    }
    if (this._newStyleRuleToolbar) {
      this._newStyleRuleToolbar.element.classList.remove('shifted-toolbar');
    }
  }

  /**
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @param {!Components.Linkifier.Linkifier} linkifier
   * @param {?SDK.CSSRule.CSSRule} rule
   * @return {!Node}
   */
  static createRuleOriginNode(matchedStyles, linkifier, rule) {
    if (!rule) {
      return document.createTextNode('');
    }

    const ruleLocation = this._getRuleLocationFromCSSRule(rule);

    const header = rule.styleSheetId ? matchedStyles.cssModel().styleSheetHeaderForId(rule.styleSheetId) : null;

    /**
     * @return {?Node}
     */
    function linkifyRuleLocation() {
      if (!rule) {
        return null;
      }
      if (ruleLocation && rule.styleSheetId && header && !header.isAnonymousInlineStyleSheet()) {
        return StylePropertiesSection._linkifyRuleLocation(
            matchedStyles.cssModel(), linkifier, rule.styleSheetId, ruleLocation);
      }
      return null;
    }

    /**
     * @param {string} label
     * @return {?Node}
     */
    function linkifyNode(label) {
      if (header?.ownerNode) {
        const link = linkifyDeferredNodeReference(header.ownerNode, {
          preventKeyboardFocus: false,
          tooltip: undefined,
        });
        link.textContent = label;
        return link;
      }
      return null;
    }

    if (header?.isMutable && !header.isViaInspector()) {
      const location = !header.isConstructed ? linkifyRuleLocation() : null;
      if (location) {
        return location;
      }
      const label = header.isConstructed ? i18nString(UIStrings.constructedStylesheet) : '<style>';
      const node = linkifyNode(label);
      if (node) {
        return node;
      }
      return document.createTextNode(label);
    }

    const location = linkifyRuleLocation();
    if (location) {
      return location;
    }

    if (rule.isUserAgent()) {
      return document.createTextNode(i18nString(UIStrings.userAgentStylesheet));
    }
    if (rule.isInjected()) {
      return document.createTextNode(i18nString(UIStrings.injectedStylesheet));
    }
    if (rule.isViaInspector()) {
      return document.createTextNode(i18nString(UIStrings.viaInspector));
    }

    const node = linkifyNode('<style>');
    if (node) {
      return node;
    }

    return document.createTextNode('');
  }

  /**
   * @param {!SDK.CSSRule.CSSRule} rule
   * @return {?TextUtils.TextRange.TextRange|undefined}
   */
  static _getRuleLocationFromCSSRule(rule) {
    let ruleLocation;
    if (rule instanceof SDK.CSSRule.CSSStyleRule) {
      ruleLocation = rule.style.range;
    } else if (rule instanceof SDK.CSSRule.CSSKeyframeRule) {
      ruleLocation = rule.key().range;
    }
    return ruleLocation;
  }

  /**
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @param {?SDK.CSSRule.CSSRule} rule
   */
  static tryNavigateToRuleLocation(matchedStyles, rule) {
    if (!rule) {
      return;
    }

    const ruleLocation = this._getRuleLocationFromCSSRule(rule);
    const header = rule.styleSheetId ? matchedStyles.cssModel().styleSheetHeaderForId(rule.styleSheetId) : null;

    if (ruleLocation && rule.styleSheetId && header && !header.isAnonymousInlineStyleSheet()) {
      const matchingSelectorLocation =
          this._getCSSSelectorLocation(matchedStyles.cssModel(), rule.styleSheetId, ruleLocation);
      this._revealSelectorSource(matchingSelectorLocation, true);
    }
  }

  /**
   * @param {!SDK.CSSModel.CSSModel} cssModel
   * @param {!Components.Linkifier.Linkifier} linkifier
   * @param {string} styleSheetId
   * @param {!TextUtils.TextRange.TextRange} ruleLocation
   * @return {!Node}
   */
  static _linkifyRuleLocation(cssModel, linkifier, styleSheetId, ruleLocation) {
    const matchingSelectorLocation = this._getCSSSelectorLocation(cssModel, styleSheetId, ruleLocation);
    return linkifier.linkifyCSSLocation(matchingSelectorLocation);
  }

  /**
   * @param {!SDK.CSSModel.CSSModel} cssModel
   * @param {string} styleSheetId
   * @param {!TextUtils.TextRange.TextRange} ruleLocation
   * @return {!SDK.CSSModel.CSSLocation}
   */
  static _getCSSSelectorLocation(cssModel, styleSheetId, ruleLocation) {
    const styleSheetHeader =
        /** @type {!SDK.CSSStyleSheetHeader.CSSStyleSheetHeader} */ (cssModel.styleSheetHeaderForId(styleSheetId));
    const lineNumber = styleSheetHeader.lineNumberInSource(ruleLocation.startLine);
    const columnNumber = styleSheetHeader.columnNumberInSource(ruleLocation.startLine, ruleLocation.startColumn);
    return new SDK.CSSModel.CSSLocation(styleSheetHeader, lineNumber, columnNumber);
  }

  /**
   * @returns {HTMLElement|null}
   */
  _getFocused() {
    return /** @type {HTMLElement} */ (this.propertiesTreeOutline._shadowRoot.activeElement) || null;
  }

  /**
   * @param {HTMLElement} element
   */
  _focusNext(element) {
    // Clear remembered focused item (if any).
    const focused = this._getFocused();
    if (focused) {
      focused.tabIndex = -1;
    }

    // Focus the next item and remember it (if in our subtree).
    element.focus();
    if (this.propertiesTreeOutline._shadowRoot.contains(element)) {
      element.tabIndex = 0;
    }
  }

  /**
   * @param {KeyboardEvent} keyboardEvent
   */
  _ruleNavigation(keyboardEvent) {
    if (keyboardEvent.altKey || keyboardEvent.ctrlKey || keyboardEvent.metaKey || keyboardEvent.shiftKey) {
      return;
    }

    const focused = this._getFocused();

    /** @type {?HTMLElement} */
    let focusNext = null;
    const focusable = Array.from(
        /** @type {NodeListOf<HTMLElement>} */ (this.propertiesTreeOutline._shadowRoot.querySelectorAll('[tabindex]')));

    if (focusable.length === 0) {
      return;
    }

    const focusedIndex = focused ? focusable.indexOf(focused) : -1;

    if (keyboardEvent.key === 'ArrowLeft') {
      focusNext = focusable[focusedIndex - 1] || this.element;
    } else if (keyboardEvent.key === 'ArrowRight') {
      focusNext = focusable[focusedIndex + 1] || this.element;
    } else if (keyboardEvent.key === 'ArrowUp' || keyboardEvent.key === 'ArrowDown') {
      this._focusNext(this.element);
      return;
    }

    if (focusNext) {
      this._focusNext(focusNext);
      keyboardEvent.consume(true);
    }
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
    if (UI.UIUtils.isEditing() || !this.editable || keyboardEvent.altKey || keyboardEvent.ctrlKey ||
        keyboardEvent.metaKey) {
      return;
    }
    switch (keyboardEvent.key) {
      case 'Enter':
      case ' ':
        this._startEditingAtFirstPosition();
        keyboardEvent.consume(true);
        break;
      case 'ArrowLeft':
      case 'ArrowRight':
      case 'ArrowUp':
      case 'ArrowDown':
        this._ruleNavigation(keyboardEvent);
        break;
      default:
        // Filter out non-printable key strokes.
        if (keyboardEvent.key.length === 1) {
          this.addNewBlankProperty(0).startEditing();
        }
        break;
    }
  }

  /**
   * @param {boolean} isHovered
   */
  _setSectionHovered(isHovered) {
    this.element.classList.toggle('styles-panel-hovered', isHovered);
    this.propertiesTreeOutline.element.classList.toggle('styles-panel-hovered', isHovered);
    if (this._hoverableSelectorsMode !== isHovered) {
      this._hoverableSelectorsMode = isHovered;
      this._markSelectorMatches();
    }
  }

  /**
   * @param {!Event} event
   */
  _onMouseLeave(event) {
    this._setSectionHovered(false);
    this._parentPane._setActiveProperty(null);
  }

  /**
   * @param {!MouseEvent} event
   */
  _onMouseMove(event) {
    const hasCtrlOrMeta = UI.KeyboardShortcut.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!MouseEvent} */ (event));
    this._setSectionHovered(hasCtrlOrMeta);

    const treeElement = this.propertiesTreeOutline.treeElementFromEvent(event);
    if (treeElement instanceof StylePropertyTreeElement) {
      this._parentPane._setActiveProperty(/** @type {!StylePropertyTreeElement} */ (treeElement));
    } else {
      this._parentPane._setActiveProperty(null);
    }
    const selection = this.element.getComponentSelection();
    if (!this._selectedSinceMouseDown && selection && selection.toString()) {
      this._selectedSinceMouseDown = true;
    }
  }

  _onFontEditorButtonClicked() {
    if (this._fontEditorSectionManager && this._fontEditorButton) {
      Host.userMetrics.cssEditorOpened('fontEditor');
      this._fontEditorSectionManager.showPopover(this._fontEditorButton.element, this._parentPane);
    }
  }

  /**
   * @return {!SDK.CSSStyleDeclaration.CSSStyleDeclaration}
   */
  style() {
    return this._style;
  }

  /**
   * @return {string}
   */
  _headerText() {
    const node = this._matchedStyles.nodeForStyle(this._style);
    if (this._style.type === SDK.CSSStyleDeclaration.Type.Inline) {
      return this._matchedStyles.isInherited(this._style) ? i18nString(UIStrings.styleAttribute) : 'element.style';
    }
    if (node && this._style.type === SDK.CSSStyleDeclaration.Type.Attributes) {
      return i18nString(UIStrings.sattributesStyle, {PH1: node.nodeNameInCorrectCase()});
    }
    if (this._style.parentRule instanceof SDK.CSSRule.CSSStyleRule) {
      return this._style.parentRule.selectorText();
    }
    return '';
  }

  _onMouseOutSelector() {
    if (this._hoverTimer) {
      clearTimeout(this._hoverTimer);
    }
    SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
  }

  _onMouseEnterSelector() {
    if (this._hoverTimer) {
      clearTimeout(this._hoverTimer);
    }
    this._hoverTimer = setTimeout(this._highlight.bind(this), 300);
  }

  /**
   * @param {string=} mode
   */
  _highlight(mode = 'all') {
    SDK.OverlayModel.OverlayModel.hideDOMNodeHighlight();
    const node = this._parentPane.node();
    if (!node) {
      return;
    }
    const selectorList = this._style.parentRule && this._style.parentRule instanceof SDK.CSSRule.CSSStyleRule ?
        this._style.parentRule.selectorText() :
        undefined;
    node.domModel().overlayModel().highlightInOverlay({node, selectorList}, mode);
  }

  /**
   * @return {?StylePropertiesSection}
   */
  firstSibling() {
    const parent = this.element.parentElement;
    if (!parent) {
      return null;
    }

    let childElement = parent.firstChild;
    while (childElement) {
      const childSection = this._parentPane.sectionByElement.get(childElement);
      if (childSection) {
        return childSection;
      }
      childElement = childElement.nextSibling;
    }

    return null;
  }

  /**
   * @param {!boolean} willIterateForward
   * @param {!StylePropertiesSection=} originalSection
   * @return {?StylePropertiesSection}
   */
  findCurrentOrNextVisible(willIterateForward, originalSection) {
    if (!this.isHidden()) {
      return this;
    }
    if (this === originalSection) {
      return null;
    }
    if (!originalSection) {
      originalSection = this;
    }
    let visibleSibling = null;
    const nextSibling = willIterateForward ? this.nextSibling() : this.previousSibling();
    if (nextSibling) {
      visibleSibling = nextSibling.findCurrentOrNextVisible(willIterateForward, originalSection);
    } else {
      const loopSibling = willIterateForward ? this.firstSibling() : this.lastSibling();
      if (loopSibling) {
        visibleSibling = loopSibling.findCurrentOrNextVisible(willIterateForward, originalSection);
      }
    }

    return visibleSibling;
  }

  /**
   * @return {?StylePropertiesSection}
   */
  lastSibling() {
    const parent = this.element.parentElement;
    if (!parent) {
      return null;
    }

    let childElement = parent.lastChild;
    while (childElement) {
      const childSection = this._parentPane.sectionByElement.get(childElement);
      if (childSection) {
        return childSection;
      }
      childElement = childElement.previousSibling;
    }

    return null;
  }

  /**
   * @return {!StylePropertiesSection|undefined}
   */
  nextSibling() {
    /** @type {?Node} */
    let curElement = this.element;
    do {
      curElement = curElement.nextSibling;
    } while (curElement && !this._parentPane.sectionByElement.has(curElement));

    if (curElement) {
      return this._parentPane.sectionByElement.get(curElement);
    }
    return;
  }

  /**
   * @return {!StylePropertiesSection|undefined}
   */
  previousSibling() {
    /** @type {?Node} */
    let curElement = this.element;
    do {
      curElement = curElement.previousSibling;
    } while (curElement && !this._parentPane.sectionByElement.has(curElement));

    if (curElement) {
      return this._parentPane.sectionByElement.get(curElement);
    }
    return;
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _onNewRuleClick(event) {
    event.data.consume();
    const rule = this._style.parentRule;
    if (!rule || !rule.style.range) {
      return;
    }
    const range =
        TextUtils.TextRange.TextRange.createFromLocation(rule.style.range.endLine, rule.style.range.endColumn + 1);
    this._parentPane._addBlankSection(this, /** @type {string} */ (rule.styleSheetId), range);
  }

  /**
   * @param {!SDK.CSSModel.Edit} edit
   */
  _styleSheetEdited(edit) {
    const rule = this._style.parentRule;
    if (rule) {
      rule.rebase(edit);
    } else {
      this._style.rebase(edit);
    }

    this._updateMediaList();
    this._updateRuleOrigin();
  }

  /**
   * @param {!Array.<!SDK.CSSMedia.CSSMedia>} mediaRules
   */
  _createMediaList(mediaRules) {
    for (let i = mediaRules.length - 1; i >= 0; --i) {
      const media = mediaRules[i];
      // Don't display trivial non-print media types.
      if (!media.text || !media.text.includes('(') && media.text !== 'print') {
        continue;
      }
      const mediaDataElement = this._mediaListElement.createChild('div', 'media');
      const mediaContainerElement = mediaDataElement.createChild('span');
      const mediaTextElement = mediaContainerElement.createChild('span', 'media-text');
      switch (media.source) {
        case SDK.CSSMedia.Source.LINKED_SHEET:
        case SDK.CSSMedia.Source.INLINE_SHEET: {
          mediaTextElement.textContent = `media="${media.text}"`;
          break;
        }
        case SDK.CSSMedia.Source.MEDIA_RULE: {
          const decoration = mediaContainerElement.createChild('span');
          mediaContainerElement.insertBefore(decoration, mediaTextElement);
          decoration.textContent = '@media ';
          mediaTextElement.textContent = media.text;
          if (media.styleSheetId) {
            mediaDataElement.classList.add('editable-media');
            mediaTextElement.addEventListener(
                'click', this._handleMediaRuleClick.bind(this, media, mediaTextElement), false);
          }
          break;
        }
        case SDK.CSSMedia.Source.IMPORT_RULE: {
          mediaTextElement.textContent = `@import ${media.text}`;
          break;
        }
      }
    }
  }

  _updateMediaList() {
    this._mediaListElement.removeChildren();
    if (this._style.parentRule && this._style.parentRule instanceof SDK.CSSRule.CSSStyleRule) {
      this._createMediaList(this._style.parentRule.media);
    }
  }

  /**
   * @param {string} propertyName
   * @return {boolean}
   */
  isPropertyInherited(propertyName) {
    if (this._matchedStyles.isInherited(this._style)) {
      // While rendering inherited stylesheet, reverse meaning of this property.
      // Render truly inherited properties with black, i.e. return them as non-inherited.
      return !SDK.CSSMetadata.cssMetadata().isPropertyInherited(propertyName);
    }
    return false;
  }

  /**
   * @return {?StylePropertiesSection}
   */
  nextEditableSibling() {
    /** @type {?StylePropertiesSection|undefined} */
    let curSection = this;
    do {
      curSection = curSection.nextSibling();
    } while (curSection && !curSection.editable);

    if (!curSection) {
      curSection = this.firstSibling();
      while (curSection && !curSection.editable) {
        curSection = curSection.nextSibling();
      }
    }

    return (curSection && curSection.editable) ? curSection : null;
  }

  /**
   * @return {?StylePropertiesSection}
   */
  previousEditableSibling() {
    /** @type {?StylePropertiesSection|undefined} */
    let curSection = this;
    do {
      curSection = curSection.previousSibling();
    } while (curSection && !curSection.editable);

    if (!curSection) {
      curSection = this.lastSibling();
      while (curSection && !curSection.editable) {
        curSection = curSection.previousSibling();
      }
    }

    return (curSection && curSection.editable) ? curSection : null;
  }

  /**
   * @param {!StylePropertyTreeElement} editedTreeElement
   */
  refreshUpdate(editedTreeElement) {
    this._parentPane._refreshUpdate(this, editedTreeElement);
  }

  /**
   * @param {!StylePropertyTreeElement} editedTreeElement
   */
  _updateVarFunctions(editedTreeElement) {
    let child = this.propertiesTreeOutline.firstChild();
    while (child) {
      if (child !== editedTreeElement && child instanceof StylePropertyTreeElement) {
        child.updateTitleIfComputedValueChanged();
      }
      child = child.traverseNextTreeElement(false /* skipUnrevealed */, null /* stayWithin */, true /* dontPopulate */);
    }
  }

  /**
   * @param {boolean} full
   */
  update(full) {
    this._selectorElement.textContent = this._headerText();
    this._markSelectorMatches();
    if (full) {
      this.onpopulate();
    } else {
      let child = this.propertiesTreeOutline.firstChild();
      while (child && child instanceof StylePropertyTreeElement) {
        child.setOverloaded(this._isPropertyOverloaded(child.property));
        child =
            child.traverseNextTreeElement(false /* skipUnrevealed */, null /* stayWithin */, true /* dontPopulate */);
      }
    }
  }

  /**
   * @param {!Event=} event
   */
  _showAllItems(event) {
    if (event) {
      event.consume();
    }
    if (this._forceShowAll) {
      return;
    }
    this._forceShowAll = true;
    this.onpopulate();
  }

  onpopulate() {
    this._parentPane._setActiveProperty(null);
    this.propertiesTreeOutline.removeChildren();
    const style = this._style;
    let count = 0;
    const properties = style.leadingProperties();
    const maxProperties = StylePropertiesSection.MaxProperties + properties.length - this._originalPropertiesCount;

    for (const property of properties) {
      if (!this._forceShowAll && count >= maxProperties) {
        break;
      }
      count++;
      const isShorthand = Boolean(style.longhandProperties(property.name).length);
      const inherited = this.isPropertyInherited(property.name);
      const overloaded = this._isPropertyOverloaded(property);
      if (style.parentRule && style.parentRule.isUserAgent() && inherited) {
        continue;
      }
      const item = new StylePropertyTreeElement(
          this._parentPane, this._matchedStyles, property, isShorthand, inherited, overloaded, false);
      this.propertiesTreeOutline.appendChild(item);
    }

    if (count < properties.length) {
      this._showAllButton.classList.remove('hidden');
      this._showAllButton.textContent = i18nString(UIStrings.showAllPropertiesSMore, {PH1: properties.length - count});
    } else {
      this._showAllButton.classList.add('hidden');
    }
  }

  /**
   * @param {!SDK.CSSProperty.CSSProperty} property
   * @return {boolean}
   */
  _isPropertyOverloaded(property) {
    return this._matchedStyles.propertyState(property) === SDK.CSSMatchedStyles.PropertyState.Overloaded;
  }

  /**
   * @return {boolean}
   */
  _updateFilter() {
    let hasMatchingChild = false;
    this._showAllItems();
    for (const child of this.propertiesTreeOutline.rootElement().children()) {
      if (child instanceof StylePropertyTreeElement) {
        const childHasMatches = child.updateFilter();
        hasMatchingChild = hasMatchingChild || childHasMatches;
      }
    }

    const regex = this._parentPane.filterRegex();
    const hideRule = !hasMatchingChild && regex !== null && !regex.test(this.element.deepTextContent());
    this._isHidden = hideRule;
    this.element.classList.toggle('hidden', hideRule);
    if (!hideRule && this._style.parentRule) {
      this._markSelectorHighlights();
    }
    return !hideRule;
  }

  /**
   * @returns {boolean}
   */
  isHidden() {
    return this._isHidden;
  }

  _markSelectorMatches() {
    const rule = this._style.parentRule;
    if (!rule || !(rule instanceof SDK.CSSRule.CSSStyleRule)) {
      return;
    }

    this._mediaListElement.classList.toggle('media-matches', this._matchedStyles.mediaMatches(this._style));

    const selectorTexts = rule.selectors.map(selector => selector.text);
    const matchingSelectorIndexes = this._matchedStyles.matchingSelectors(rule);
    const matchingSelectors = /** @type {!Array<boolean>} */ (new Array(selectorTexts.length).fill(false));
    for (const matchingIndex of matchingSelectorIndexes) {
      matchingSelectors[matchingIndex] = true;
    }

    if (this._parentPane._isEditingStyle) {
      return;
    }

    const fragment = this._hoverableSelectorsMode ? this._renderHoverableSelectors(selectorTexts, matchingSelectors) :
                                                    this._renderSimplifiedSelectors(selectorTexts, matchingSelectors);
    this._selectorElement.removeChildren();
    this._selectorElement.appendChild(fragment);
    this._markSelectorHighlights();
  }

  /**
   * @param {!Array<string>} selectors
   * @param {!Array<boolean>} matchingSelectors
   * @return {!DocumentFragment}
   */
  _renderHoverableSelectors(selectors, matchingSelectors) {
    const fragment = document.createDocumentFragment();
    for (let i = 0; i < selectors.length; ++i) {
      if (i) {
        UI.UIUtils.createTextChild(fragment, ', ');
      }
      fragment.appendChild(this._createSelectorElement(selectors[i], matchingSelectors[i], i));
    }
    return fragment;
  }

  /**
   * @param {string} text
   * @param {boolean} isMatching
   * @param {number=} navigationIndex
   * @return {!Element}
   */
  _createSelectorElement(text, isMatching, navigationIndex) {
    const element = document.createElement('span');
    element.classList.add('simple-selector');
    element.classList.toggle('selector-matches', isMatching);
    if (typeof navigationIndex === 'number') {
      this._elementToSelectorIndex.set(element, navigationIndex);
    }
    element.textContent = text;
    return element;
  }

  /**
   * @param {!Array<string>} selectors
   * @param {!Array<boolean>} matchingSelectors
   * @return {!DocumentFragment}
   */
  _renderSimplifiedSelectors(selectors, matchingSelectors) {
    const fragment = document.createDocumentFragment();
    let currentMatching = false;
    let text = '';
    for (let i = 0; i < selectors.length; ++i) {
      if (currentMatching !== matchingSelectors[i] && text) {
        fragment.appendChild(this._createSelectorElement(text, currentMatching));
        text = '';
      }
      currentMatching = matchingSelectors[i];
      text += selectors[i] + (i === selectors.length - 1 ? '' : ', ');
    }
    if (text) {
      fragment.appendChild(this._createSelectorElement(text, currentMatching));
    }
    return fragment;
  }

  _markSelectorHighlights() {
    const selectors = this._selectorElement.getElementsByClassName('simple-selector');
    const regex = this._parentPane.filterRegex();
    for (let i = 0; i < selectors.length; ++i) {
      const selectorMatchesFilter = regex !== null && regex.test(selectors[i].textContent || '');
      selectors[i].classList.toggle('filter-match', selectorMatchesFilter);
    }
  }

  /**
   * @return {boolean}
   */
  _checkWillCancelEditing() {
    const willCauseCancelEditing = this._willCauseCancelEditing;
    this._willCauseCancelEditing = false;
    return willCauseCancelEditing;
  }

  /**
   * @param {!Event} event
   */
  _handleSelectorContainerClick(event) {
    if (this._checkWillCancelEditing() || !this.editable) {
      return;
    }
    if (event.target === this._selectorContainer) {
      this.addNewBlankProperty(0).startEditing();
      event.consume(true);
    }
  }

  /**
   * @param {number=} index
   * @return {!StylePropertyTreeElement}
   */
  addNewBlankProperty(index = this.propertiesTreeOutline.rootElement().childCount()) {
    const property = this._style.newBlankProperty(index);
    const item =
        new StylePropertyTreeElement(this._parentPane, this._matchedStyles, property, false, false, false, true);
    this.propertiesTreeOutline.insertChild(item, property.index);
    return item;
  }

  _handleEmptySpaceMouseDown() {
    this._willCauseCancelEditing = this._parentPane._isEditingStyle;
    this._selectedSinceMouseDown = false;
  }

  /**
   * @param {!Event} event
   */
  _handleEmptySpaceClick(event) {
    if (!this.editable || this.element.hasSelection() || this._checkWillCancelEditing() ||
        this._selectedSinceMouseDown) {
      return;
    }

    const target = /** @type {!Element} */ (event.target);

    if (target.classList.contains('header') || this.element.classList.contains('read-only') ||
        target.enclosingNodeOrSelfWithClass('media')) {
      event.consume();
      return;
    }
    const deepTarget = UI.UIUtils.deepElementFromEvent(event);
    const treeElement = deepTarget && UI.TreeOutline.TreeElement.getTreeElementBylistItemNode(deepTarget);
    if (treeElement && treeElement instanceof StylePropertyTreeElement) {
      this.addNewBlankProperty(treeElement.property.index + 1).startEditing();
    } else {
      this.addNewBlankProperty().startEditing();
    }
    event.consume(true);
  }

  /**
   * @param {!SDK.CSSMedia.CSSMedia} media
   * @param {!Element} element
   * @param {!Event} event
   */
  _handleMediaRuleClick(media, element, event) {
    if (UI.UIUtils.isBeingEdited(element)) {
      return;
    }

    if (UI.KeyboardShortcut.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!MouseEvent} */ (event)) && this.navigable) {
      const location = media.rawLocation();
      if (!location) {
        event.consume(true);
        return;
      }
      const uiLocation = Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance().rawLocationToUILocation(location);
      if (uiLocation) {
        Common.Revealer.reveal(uiLocation);
      }
      event.consume(true);
      return;
    }

    if (!this.editable) {
      return;
    }

    const config = new UI.InplaceEditor.Config(
        this._editingMediaCommitted.bind(this, media), this._editingMediaCancelled.bind(this, element), undefined,
        this._editingMediaBlurHandler.bind(this));
    UI.InplaceEditor.InplaceEditor.startEditing(element, /** @type {!UI.InplaceEditor.Config<?>} */ (config));

    const selection = element.getComponentSelection();
    if (selection) {
      selection.selectAllChildren(element);
    }
    this._parentPane.setEditingStyle(true);
    const parentMediaElement = element.enclosingNodeOrSelfWithClass('media');
    parentMediaElement.classList.add('editing-media');

    event.consume(true);
  }

  /**
   * @param {!Element} element
   */
  _editingMediaFinished(element) {
    this._parentPane.setEditingStyle(false);
    const parentMediaElement = element.enclosingNodeOrSelfWithClass('media');
    parentMediaElement.classList.remove('editing-media');
  }

  /**
   * @param {!Element} element
   */
  _editingMediaCancelled(element) {
    this._editingMediaFinished(element);
    // Mark the selectors in group if necessary.
    // This is overridden by BlankStylePropertiesSection.
    this._markSelectorMatches();
    const selection = element.getComponentSelection();
    if (selection) {
      selection.collapse(element, 0);
    }
  }

  /**
   * @return {boolean}
   */
  _editingMediaBlurHandler() {
    return true;
  }

  /**
   * @param {!SDK.CSSMedia.CSSMedia} media
   * @param {!Element} element
   * @param {string} newContent
   * @param {string} oldContent
   * @param {(!Context|undefined)} context
   * @param {string} moveDirection
   */
  _editingMediaCommitted(media, element, newContent, oldContent, context, moveDirection) {
    this._parentPane.setEditingStyle(false);
    this._editingMediaFinished(element);

    if (newContent) {
      newContent = newContent.trim();
    }

    /**
     * @param {boolean} success
     * @this {StylePropertiesSection}
     */
    function userCallback(success) {
      if (success) {
        this._matchedStyles.resetActiveProperties();
        this._parentPane._refreshUpdate(this);
      }
      this._parentPane.setUserOperation(false);
      this._editingMediaTextCommittedForTest();
    }

    // This gets deleted in finishOperation(), which is called both on success and failure.
    this._parentPane.setUserOperation(true);
    const cssModel = this._parentPane.cssModel();
    if (cssModel && media.styleSheetId) {
      cssModel.setMediaText(media.styleSheetId, /** @type {!TextUtils.TextRange.TextRange} */ (media.range), newContent)
          .then(userCallback.bind(this));
    }
  }

  _editingMediaTextCommittedForTest() {
  }

  /**
   * @param {!Event} event
   */
  _handleSelectorClick(event) {
    const target = /** @type {?Element} */ (event.target);
    if (!target) {
      return;
    }
    if (UI.KeyboardShortcut.KeyboardShortcut.eventHasCtrlOrMeta(/** @type {!MouseEvent} */ (event)) && this.navigable &&
        target.classList.contains('simple-selector')) {
      const selectorIndex = this._elementToSelectorIndex.get(target);
      if (selectorIndex) {
        this._navigateToSelectorSource(selectorIndex, true);
      }
      event.consume(true);
      return;
    }
    if (this.element.hasSelection()) {
      return;
    }
    this._startEditingAtFirstPosition();
    event.consume(true);
  }

  /**
   * @param {!Event} event
   */
  _handleContextMenuEvent(event) {
    const target = /** @type {?Element} */ (event.target);
    if (!target) {
      return;
    }


    const contextMenu = new UI.ContextMenu.ContextMenu(event);
    contextMenu.clipboardSection().appendItem(i18nString(UIStrings.copySelector), () => {
      const selectorText = this._headerText();
      Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(selectorText);
    });

    contextMenu.clipboardSection().appendItem(i18nString(UIStrings.copyRule), () => {
      const ruleText = StylesSidebarPane.formatLeadingProperties(this).ruleText;
      Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(ruleText);
    });

    contextMenu.clipboardSection().appendItem(i18nString(UIStrings.copyAllDeclarations), () => {
      const allDeclarationText = StylesSidebarPane.formatLeadingProperties(this).allDeclarationText;
      Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(allDeclarationText);
    });

    contextMenu.show();
  }

  /**
   * @param {number} index
   * @param {boolean} focus
   */
  _navigateToSelectorSource(index, focus) {
    const cssModel = this._parentPane.cssModel();
    if (!cssModel) {
      return;
    }
    const rule = /** @type {?SDK.CSSRule.CSSStyleRule} */ (this._style.parentRule);
    if (!rule) {
      return;
    }
    const header = cssModel.styleSheetHeaderForId(/** @type {string} */ (rule.styleSheetId));
    if (!header) {
      return;
    }
    const rawLocation =
        new SDK.CSSModel.CSSLocation(header, rule.lineNumberInSource(index), rule.columnNumberInSource(index));
    StylePropertiesSection._revealSelectorSource(rawLocation, focus);
  }

  /**
   * @param {!SDK.CSSModel.CSSLocation} rawLocation
   * @param {boolean} focus
   */
  static _revealSelectorSource(rawLocation, focus) {
    const uiLocation = Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance().rawLocationToUILocation(rawLocation);
    if (uiLocation) {
      Common.Revealer.reveal(uiLocation, !focus);
    }
  }

  _startEditingAtFirstPosition() {
    if (!this.editable) {
      return;
    }

    if (!this._style.parentRule) {
      this.moveEditorFromSelector('forward');
      return;
    }

    this.startEditingSelector();
  }

  startEditingSelector() {
    const element = this._selectorElement;
    if (UI.UIUtils.isBeingEdited(element)) {
      return;
    }

    element.scrollIntoViewIfNeeded(false);
    // Reset selector marks in group, and normalize whitespace.
    const textContent = element.textContent;
    if (textContent !== null) {
      element.textContent = textContent.replace(/\s+/g, ' ').trim();
    }

    const config =
        new UI.InplaceEditor.Config(this.editingSelectorCommitted.bind(this), this.editingSelectorCancelled.bind(this));
    UI.InplaceEditor.InplaceEditor.startEditing(
        this._selectorElement, /** @type {!UI.InplaceEditor.Config<?>} */ (config));

    const selection = element.getComponentSelection();
    if (selection) {
      selection.selectAllChildren(element);
    }
    this._parentPane.setEditingStyle(true);
    if (element.classList.contains('simple-selector')) {
      this._navigateToSelectorSource(0, false);
    }
  }

  /**
   * @param {string} moveDirection
   */
  moveEditorFromSelector(moveDirection) {
    this._markSelectorMatches();

    if (!moveDirection) {
      return;
    }

    if (moveDirection === 'forward') {
      const firstChild = /** @type {!StylePropertyTreeElement} */ (this.propertiesTreeOutline.firstChild());
      /** @type {?StylePropertyTreeElement} */
      let currentChild = firstChild;
      while (currentChild && currentChild.inherited()) {
        /** @type {?UI.TreeOutline.TreeElement} */
        const sibling = currentChild.nextSibling;
        currentChild = sibling instanceof StylePropertyTreeElement ? sibling : null;
      }
      if (!currentChild) {
        this.addNewBlankProperty().startEditing();
      } else {
        currentChild.startEditing(currentChild.nameElement);
      }
    } else {
      const previousSection = this.previousEditableSibling();
      if (!previousSection) {
        return;
      }

      previousSection.addNewBlankProperty().startEditing();
    }
  }

  /**
   * @param {!Element} element
   * @param {string} newContent
   * @param {string} oldContent
   * @param {(!Context|undefined)} context
   * @param {string} moveDirection
   */
  editingSelectorCommitted(element, newContent, oldContent, context, moveDirection) {
    this._editingSelectorEnded();
    if (newContent) {
      newContent = newContent.trim();
    }
    if (newContent === oldContent) {
      // Revert to a trimmed version of the selector if need be.
      this._selectorElement.textContent = newContent;
      this.moveEditorFromSelector(moveDirection);
      return;
    }
    const rule = this._style.parentRule;
    if (!rule) {
      return;
    }

    /**
     * @this {StylePropertiesSection}
     */
    function headerTextCommitted() {
      this._parentPane.setUserOperation(false);
      this.moveEditorFromSelector(moveDirection);
      this._editingSelectorCommittedForTest();
    }

    // This gets deleted in finishOperationAndMoveEditor(), which is called both on success and failure.
    this._parentPane.setUserOperation(true);
    this._setHeaderText(rule, newContent).then(headerTextCommitted.bind(this));
  }

  /**
   * @param {!SDK.CSSRule.CSSRule} rule
   * @param {string} newContent
   * @return {!Promise<void>}
   */
  _setHeaderText(rule, newContent) {
    /**
     * @param {!SDK.CSSRule.CSSStyleRule} rule
     * @param {boolean} success
     * @return {!Promise<void>}
     * @this {StylePropertiesSection}
     */
    function onSelectorsUpdated(rule, success) {
      if (!success) {
        return Promise.resolve();
      }
      return this._matchedStyles.recomputeMatchingSelectors(rule).then(updateSourceRanges.bind(this, rule));
    }

    /**
     * @param {!SDK.CSSRule.CSSStyleRule} rule
     * @this {StylePropertiesSection}
     */
    function updateSourceRanges(rule) {
      const doesAffectSelectedNode = this._matchedStyles.matchingSelectors(rule).length > 0;
      this.propertiesTreeOutline.element.classList.toggle('no-affect', !doesAffectSelectedNode);
      this._matchedStyles.resetActiveProperties();
      this._parentPane._refreshUpdate(this);
    }

    if (!(rule instanceof SDK.CSSRule.CSSStyleRule)) {
      return Promise.resolve();
    }
    const oldSelectorRange = rule.selectorRange();
    if (!oldSelectorRange) {
      return Promise.resolve();
    }
    return rule.setSelectorText(newContent).then(onSelectorsUpdated.bind(this, rule, Boolean(oldSelectorRange)));
  }

  _editingSelectorCommittedForTest() {
  }

  _updateRuleOrigin() {
    this._selectorRefElement.removeChildren();
    this._selectorRefElement.appendChild(StylePropertiesSection.createRuleOriginNode(
        this._matchedStyles, this._parentPane._linkifier, this._style.parentRule));
  }

  _editingSelectorEnded() {
    this._parentPane.setEditingStyle(false);
  }

  editingSelectorCancelled() {
    this._editingSelectorEnded();

    // Mark the selectors in group if necessary.
    // This is overridden by BlankStylePropertiesSection.
    this._markSelectorMatches();
  }

  /**
   * A property at or near an index and suitable for subsequent editing.
   * Either the last property, if index out-of-upper-bound,
   * or property at index, if such a property exists,
   * or otherwise, null.
   * @param {number} propertyIndex
   * @returns {?UI.TreeOutline.TreeElement}
   */
  closestPropertyForEditing(propertyIndex) {
    const rootElement = this.propertiesTreeOutline.rootElement();
    if (propertyIndex >= rootElement.childCount()) {
      return rootElement.lastChild();
    }
    return rootElement.childAt(propertyIndex);
  }
}

StylePropertiesSection.MaxProperties = 50;

export class BlankStylePropertiesSection extends StylePropertiesSection {
  /**
   * @param {!StylesSidebarPane} stylesPane
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @param {string} defaultSelectorText
   * @param {string} styleSheetId
   * @param {!TextUtils.TextRange.TextRange} ruleLocation
   * @param {!SDK.CSSStyleDeclaration.CSSStyleDeclaration} insertAfterStyle
   */
  constructor(stylesPane, matchedStyles, defaultSelectorText, styleSheetId, ruleLocation, insertAfterStyle) {
    const cssModel = /** @type {!SDK.CSSModel.CSSModel} */ (stylesPane.cssModel());
    const rule = SDK.CSSRule.CSSStyleRule.createDummyRule(cssModel, defaultSelectorText);
    super(stylesPane, matchedStyles, rule.style);
    this._normal = false;
    this._ruleLocation = ruleLocation;
    this._styleSheetId = styleSheetId;
    this._selectorRefElement.removeChildren();
    this._selectorRefElement.appendChild(StylePropertiesSection._linkifyRuleLocation(
        cssModel, this._parentPane._linkifier, styleSheetId, this._actualRuleLocation()));
    if (insertAfterStyle && insertAfterStyle.parentRule &&
        insertAfterStyle.parentRule instanceof SDK.CSSRule.CSSStyleRule) {
      this._createMediaList(insertAfterStyle.parentRule.media);
    }
    this.element.classList.add('blank-section');
  }

  /**
   * @return {!TextUtils.TextRange.TextRange}
   */
  _actualRuleLocation() {
    const prefix = this._rulePrefix();
    const lines = prefix.split('\n');
    const lastLine = lines[lines.length - 1];
    const editRange = new TextUtils.TextRange.TextRange(0, 0, lines.length - 1, lastLine ? lastLine.length : 0);
    return this._ruleLocation.rebaseAfterTextEdit(TextUtils.TextRange.TextRange.createFromLocation(0, 0), editRange);
  }

  /**
   * @return {string}
   */
  _rulePrefix() {
    return this._ruleLocation.startLine === 0 && this._ruleLocation.startColumn === 0 ? '' : '\n\n';
  }

  /**
   * @return {boolean}
   */
  get isBlank() {
    return !this._normal;
  }

  /**
   * @override
   * @param {!Element} element
   * @param {string} newContent
   * @param {string} oldContent
   * @param {!Context|undefined} context
   * @param {string} moveDirection
   */
  editingSelectorCommitted(element, newContent, oldContent, context, moveDirection) {
    if (!this.isBlank) {
      super.editingSelectorCommitted(element, newContent, oldContent, context, moveDirection);
      return;
    }

    /**
     * @param {?SDK.CSSRule.CSSStyleRule} newRule
     * @return {!Promise<void>}
     * @this {BlankStylePropertiesSection}
     */
    function onRuleAdded(newRule) {
      if (!newRule) {
        this.editingSelectorCancelled();
        this._editingSelectorCommittedForTest();
        return Promise.resolve();
      }
      return this._matchedStyles.addNewRule(newRule, this._matchedStyles.node())
          .then(onAddedToCascade.bind(this, newRule));
    }

    /**
     * @param {!SDK.CSSRule.CSSStyleRule} newRule
     * @this {BlankStylePropertiesSection}
     */
    function onAddedToCascade(newRule) {
      const doesSelectorAffectSelectedNode = this._matchedStyles.matchingSelectors(newRule).length > 0;
      this._makeNormal(newRule);

      if (!doesSelectorAffectSelectedNode) {
        this.propertiesTreeOutline.element.classList.add('no-affect');
      }

      this._updateRuleOrigin();

      this._parentPane.setUserOperation(false);
      this._editingSelectorEnded();
      if (this.element.parentElement)  // Might have been detached already.
      {
        this.moveEditorFromSelector(moveDirection);
      }
      this._markSelectorMatches();

      this._editingSelectorCommittedForTest();
    }

    if (newContent) {
      newContent = newContent.trim();
    }
    this._parentPane.setUserOperation(true);

    const cssModel = this._parentPane.cssModel();
    const ruleText = this._rulePrefix() + newContent + ' {}';
    if (cssModel) {
      cssModel.addRule(this._styleSheetId, ruleText, this._ruleLocation).then(onRuleAdded.bind(this));
    }
  }

  /**
   * @override
   */
  editingSelectorCancelled() {
    this._parentPane.setUserOperation(false);
    if (!this.isBlank) {
      super.editingSelectorCancelled();
      return;
    }

    this._editingSelectorEnded();
    this._parentPane.removeSection(this);
  }

  /**
   * @param {!SDK.CSSRule.CSSRule} newRule
   */
  _makeNormal(newRule) {
    this.element.classList.remove('blank-section');
    this._style = newRule.style;
    // FIXME: replace this instance by a normal StylePropertiesSection.
    this._normal = true;
  }
}

export class KeyframePropertiesSection extends StylePropertiesSection {
  /**
   * @param {!StylesSidebarPane} stylesPane
   * @param {!SDK.CSSMatchedStyles.CSSMatchedStyles} matchedStyles
   * @param {!SDK.CSSStyleDeclaration.CSSStyleDeclaration} style
   */
  constructor(stylesPane, matchedStyles, style) {
    super(stylesPane, matchedStyles, style);
    this._selectorElement.className = 'keyframe-key';
  }

  /**
   * @override
   * @return {string}
   */
  _headerText() {
    if (this._style.parentRule instanceof SDK.CSSRule.CSSKeyframeRule) {
      return this._style.parentRule.key().text;
    }
    return '';
  }

  /**
   * @override
   * @param {!SDK.CSSRule.CSSRule} rule
   * @param {string} newContent
   * @return {!Promise<void>}
   */
  _setHeaderText(rule, newContent) {
    /**
     * @param {boolean} success
     * @this {KeyframePropertiesSection}
     */
    function updateSourceRanges(success) {
      if (!success) {
        return;
      }
      this._parentPane._refreshUpdate(this);
    }

    if (!(rule instanceof SDK.CSSRule.CSSKeyframeRule)) {
      return Promise.resolve();
    }
    const oldRange = rule.key().range;
    if (!oldRange) {
      return Promise.resolve();
    }
    return rule.setKeyText(newContent).then(updateSourceRanges.bind(this));
  }

  /**
   * @override
   * @param {string} propertyName
   * @return {boolean}
   */
  isPropertyInherited(propertyName) {
    return false;
  }

  /**
   * @override
   * @param {!SDK.CSSProperty.CSSProperty} property
   * @return {boolean}
   */
  _isPropertyOverloaded(property) {
    return false;
  }

  /**
   * @override
   */
  _markSelectorHighlights() {
  }

  /**
   * @override
   */
  _markSelectorMatches() {
    if (this._style.parentRule instanceof SDK.CSSRule.CSSKeyframeRule) {
      this._selectorElement.textContent = this._style.parentRule.key().text;
    }
  }

  /**
   * @override
   */
  _highlight() {
  }
}

/**
 * @param {string} familyName
 * @return {string}
 */
export function quoteFamilyName(familyName) {
  return `'${familyName.replaceAll('\'', '\\\'')}'`;
}

export class CSSPropertyPrompt extends UI.TextPrompt.TextPrompt {
  /**
   * @param {!StylePropertyTreeElement} treeElement
   * @param {boolean} isEditingName
   */
  constructor(treeElement, isEditingName) {
    // Use the same callback both for applyItemCallback and acceptItemCallback.
    super();
    this.initialize(this._buildPropertyCompletions.bind(this), UI.UIUtils.StyleValueDelimiters);
    const cssMetadata = SDK.CSSMetadata.cssMetadata();
    this._isColorAware = SDK.CSSMetadata.cssMetadata().isColorAwareProperty(treeElement.property.name);
    /** @type {!Array<string>} */
    this._cssCompletions = [];
    const node = treeElement.node();
    if (isEditingName) {
      this._cssCompletions = cssMetadata.allProperties();
      if (node && !node.isSVGNode()) {
        this._cssCompletions = this._cssCompletions.filter(property => !cssMetadata.isSVGProperty(property));
      }
    } else {
      this._cssCompletions = cssMetadata.propertyValues(treeElement.property.name);
      if (node && cssMetadata.isFontFamilyProperty(treeElement.property.name)) {
        const fontFamilies = node.domModel().cssModel().fontFaces().map(font => quoteFamilyName(font.getFontFamily()));
        this._cssCompletions.unshift(...fontFamilies);
      }
    }

    /**
     * Computed styles cache populated for flexbox features.
     * @type {?Map<string, string>}
     */
    this._selectedNodeComputedStyles = null;
    /**
     * Computed styles cache populated for flexbox features.
     * @type {?Map<string, string>}
     */
    this._parentNodeComputedStyles = null;
    this._treeElement = treeElement;
    this._isEditingName = isEditingName;
    this._cssVariables = treeElement.matchedStyles().availableCSSVariables(treeElement.property.ownerStyle);
    if (this._cssVariables.length < 1000) {
      this._cssVariables.sort(Platform.StringUtilities.naturalOrderComparator);
    } else {
      this._cssVariables.sort();
    }

    if (!isEditingName) {
      this.disableDefaultSuggestionForEmptyInput();

      // If a CSS value is being edited that has a numeric or hex substring, hint that precision modifier shortcuts are available.
      if (treeElement && treeElement.valueElement) {
        const cssValueText = treeElement.valueElement.textContent;
        const cmdOrCtrl = Host.Platform.isMac() ? 'Cmd' : 'Ctrl';
        if (cssValueText !== null) {
          if (cssValueText.match(/#[\da-f]{3,6}$/i)) {
            this.setTitle(i18nString(UIStrings.incrementdecrementWithMousewheelOne, {PH1: cmdOrCtrl}));
          } else if (cssValueText.match(/\d+/)) {
            this.setTitle(i18nString(UIStrings.incrementdecrementWithMousewheelHundred, {PH1: cmdOrCtrl}));
          }
        }
      }
    }
  }

  /**
   * @override
   * @param {!Event} event
   */
  onKeyDown(event) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
    switch (keyboardEvent.key) {
      case 'ArrowUp':
      case 'ArrowDown':
      case 'PageUp':
      case 'PageDown':
        if (!this.isSuggestBoxVisible() && this._handleNameOrValueUpDown(keyboardEvent)) {
          keyboardEvent.preventDefault();
          return;
        }
        break;
      case 'Enter':
        if (keyboardEvent.shiftKey) {
          return;
        }
        // Accept any available autocompletions and advance to the next field.
        this.tabKeyPressed();
        keyboardEvent.preventDefault();
        return;
    }

    super.onKeyDown(keyboardEvent);
  }

  /**
   * @override
   * @param {!Event} event
   */
  onMouseWheel(event) {
    if (this._handleNameOrValueUpDown(event)) {
      event.consume(true);
      return;
    }
    super.onMouseWheel(event);
  }

  /**
   * @override
   * @return {boolean}
   */
  tabKeyPressed() {
    this.acceptAutoComplete();

    // Always tab to the next field.
    return false;
  }

  /**
   * @param {!Event} event
   * @return {boolean}
   */
  _handleNameOrValueUpDown(event) {
    /**
     * @param {string} originalValue
     * @param {string} replacementString
     * @this {CSSPropertyPrompt}
     */
    function finishHandler(originalValue, replacementString) {
      // Synthesize property text disregarding any comments, custom whitespace etc.
      if (this._treeElement.nameElement && this._treeElement.valueElement) {
        this._treeElement.applyStyleText(
            this._treeElement.nameElement.textContent + ': ' + this._treeElement.valueElement.textContent, false);
      }
    }

    /**
     * @param {string} prefix
     * @param {number} number
     * @param {string} suffix
     * @return {string}
     * @this {CSSPropertyPrompt}
     */
    function customNumberHandler(prefix, number, suffix) {
      if (number !== 0 && !suffix.length &&
          SDK.CSSMetadata.cssMetadata().isLengthProperty(this._treeElement.property.name) &&
          !this._treeElement.property.value.toLowerCase().startsWith('calc(')) {
        suffix = 'px';
      }
      return prefix + number + suffix;
    }

    // Handle numeric value increment/decrement only at this point.
    if (!this._isEditingName && this._treeElement.valueElement &&
        UI.UIUtils.handleElementValueModifications(
            event, this._treeElement.valueElement, finishHandler.bind(this), this._isValueSuggestion.bind(this),
            customNumberHandler.bind(this))) {
      return true;
    }

    return false;
  }

  /**
   * @param {string} word
   * @return {boolean}
   */
  _isValueSuggestion(word) {
    if (!word) {
      return false;
    }
    word = word.toLowerCase();
    return this._cssCompletions.indexOf(word) !== -1 || word.startsWith('--');
  }

  /**
   * @param {string} expression
   * @param {string} query
   * @param {boolean=} force
   * @return {!Promise<!UI.SuggestBox.Suggestions>}
   */
  async _buildPropertyCompletions(expression, query, force) {
    const lowerQuery = query.toLowerCase();
    const editingVariable = !this._isEditingName && expression.trim().endsWith('var(');
    if (!query && !force && !editingVariable && (this._isEditingName || expression)) {
      return Promise.resolve([]);
    }

    /** @type {!UI.SuggestBox.Suggestions} */
    const prefixResults = [];
    /** @type {!UI.SuggestBox.Suggestions} */
    const anywhereResults = [];
    if (!editingVariable) {
      this._cssCompletions.forEach(completion => filterCompletions.call(this, completion, false /* variable */));
    }
    const node = this._treeElement.node();
    if (this._isEditingName && node) {
      const nameValuePresets = SDK.CSSMetadata.cssMetadata().nameValuePresets(node.isSVGNode());
      nameValuePresets.forEach(
          preset => filterCompletions.call(this, preset, false /* variable */, true /* nameValue */));
    }
    if (this._isEditingName || editingVariable) {
      this._cssVariables.forEach(variable => filterCompletions.call(this, variable, true /* variable */));
    }

    const results = prefixResults.concat(anywhereResults);
    if (!this._isEditingName && !results.length && query.length > 1 && '!important'.startsWith(lowerQuery)) {
      results.push({
        text: '!important',
        title: undefined,
        subtitle: undefined,
        iconType: undefined,
        priority: undefined,
        isSecondary: undefined,
        subtitleRenderer: undefined,
        selectionRange: undefined,
        hideGhostText: undefined,
        iconElement: undefined,
      });
    }
    const userEnteredText = query.replace('-', '');
    if (userEnteredText && (userEnteredText === userEnteredText.toUpperCase())) {
      for (let i = 0; i < results.length; ++i) {
        if (!results[i].text.startsWith('--')) {
          results[i].text = results[i].text.toUpperCase();
        }
      }
    }

    for (const result of results) {
      if (editingVariable) {
        result.title = result.text;
        result.text += ')';
        continue;
      }
      const valuePreset = SDK.CSSMetadata.cssMetadata().getValuePreset(this._treeElement.name, result.text);
      if (!this._isEditingName && valuePreset) {
        result.title = result.text;
        result.text = valuePreset.text;
        result.selectionRange = {startColumn: valuePreset.startColumn, endColumn: valuePreset.endColumn};
      }
    }

    const ensureComputedStyles = async () => {
      if (!node || this._selectedNodeComputedStyles) {
        return;
      }
      this._selectedNodeComputedStyles = await node.domModel().cssModel().computedStylePromise(node.id);
      const parentNode = node.parentNode;
      if (parentNode) {
        this._parentNodeComputedStyles = await parentNode.domModel().cssModel().computedStylePromise(node.id);
      }
    };

    for (const result of results) {
      await ensureComputedStyles();
      // Using parent node's computed styles does not work in all cases. For example:
      //
      // <div id="container" style="display:flex">
      //  <div id="useless" style="display:contents">
      //    <div id="item">item</div>
      //  </div>
      // </div>
      // TODO(crbug/1139945): Find a better way to get the flex container styles.
      const iconInfo = findIcon(
          this._isEditingName ? result.text : `${this._treeElement.property.name}: ${result.text}`,
          this._selectedNodeComputedStyles, this._parentNodeComputedStyles);
      if (!iconInfo) {
        continue;
      }
      const icon = new WebComponents.Icon.Icon();
      const width = '12.5px';
      const height = '12.5px';
      icon.data = {
        iconName: iconInfo.iconName,
        width,
        height,
        color: 'black',
      };
      icon.style.transform = `rotate(${iconInfo.rotate}deg) scale(${iconInfo.scaleX * 1.1}, ${iconInfo.scaleY * 1.1})`;
      icon.style.maxHeight = height;
      icon.style.maxWidth = width;
      result.iconElement = icon;
    }

    if (this._isColorAware && !this._isEditingName) {
      results.sort((a, b) => {
        if (Boolean(a.subtitleRenderer) === Boolean(b.subtitleRenderer)) {
          return 0;
        }
        return a.subtitleRenderer ? -1 : 1;
      });
    }
    return Promise.resolve(results);

    /**
     * @param {string} completion
     * @param {boolean} variable
     * @param {boolean=} nameValue
     * @this {CSSPropertyPrompt}
     */
    function filterCompletions(completion, variable, nameValue) {
      const index = completion.toLowerCase().indexOf(lowerQuery);
      /** @type {!UI.SuggestBox.Suggestion} */
      const result = {
        text: completion,
        title: undefined,
        subtitle: undefined,
        iconType: undefined,
        priority: undefined,
        isSecondary: undefined,
        subtitleRenderer: undefined,
        selectionRange: undefined,
        hideGhostText: undefined,
        iconElement: undefined,
      };
      if (variable) {
        const computedValue =
            this._treeElement.matchedStyles().computeCSSVariable(this._treeElement.property.ownerStyle, completion);
        if (computedValue) {
          const color = Common.Color.Color.parse(computedValue);
          if (color) {
            result.subtitleRenderer = swatchRenderer.bind(null, color);
          }
        }
      }
      if (nameValue) {
        result.hideGhostText = true;
      }
      if (index === 0) {
        result.priority = this._isEditingName ? SDK.CSSMetadata.cssMetadata().propertyUsageWeight(completion) : 1;
        prefixResults.push(result);
      } else if (index > -1) {
        anywhereResults.push(result);
      }
    }

    /**
     * @param {!Common.Color.Color} color
     * @return {!Element}
     */
    function swatchRenderer(color) {
      const swatch = new InlineEditor.ColorSwatch.ColorSwatch();
      swatch.renderColor(color);
      swatch.style.pointerEvents = 'none';
      return swatch;
    }
  }
}

/**
 * @param {string} input
 * @return {string}
 */
export function unescapeCssString(input) {
  // https://drafts.csswg.org/css-syntax/#consume-escaped-code-point
  const reCssEscapeSequence = /(?<!\\)\\(?:([a-fA-F0-9]{1,6})|(.))[\n\t\x20]?/gs;
  return input.replace(reCssEscapeSequence, (_, $1, $2) => {
    if ($2) {  // Handle the single-character escape sequence.
      return $2;
    }
    // Otherwise, handle the code point escape sequence.
    const codePoint = parseInt($1, 16);
    const isSurrogate = 0xD800 <= codePoint && codePoint <= 0xDFFF;
    if (isSurrogate || codePoint === 0x0000 || codePoint > 0x10FFFF) {
      return '\uFFFD';
    }
    return String.fromCodePoint(codePoint);
  });
}

export class StylesSidebarPropertyRenderer {
  /**
   * @param {?SDK.CSSRule.CSSRule} rule
   * @param {?SDK.DOMModel.DOMNode} node
   * @param {string} name
   * @param {string} value
   */
  constructor(rule, node, name, value) {
    this._rule = rule;
    this._node = node;
    this._propertyName = name;
    this._propertyValue = value;
    /** @type {?function(string):!Node} */
    this._colorHandler = null;
    /** @type {?function(string):!Node} */
    this._bezierHandler = null;
    /** @type {?function(string):!Node} */
    this._fontHandler = null;
    /** @type {?function(string, string):!Node} */
    this._shadowHandler = null;
    /** @type {?function(string, string):!Node} */
    this._gridHandler = null;
    /** @type {?function(string):!Node} */
    this._varHandler = document.createTextNode.bind(document);
    /** @type {?function(string):!Node} */
    this._angleHandler = null;
  }

  /**
   * @param {function(string):!Node} handler
   */
  setColorHandler(handler) {
    this._colorHandler = handler;
  }

  /**
   * @param {function(string):!Node} handler
   */
  setBezierHandler(handler) {
    this._bezierHandler = handler;
  }

  /**
   * @param {function(string):!Node} handler
   */
  setFontHandler(handler) {
    this._fontHandler = handler;
  }

  /**
   * @param {function(string, string):!Node} handler
   */
  setShadowHandler(handler) {
    this._shadowHandler = handler;
  }

  /**
   * @param {function(string, string):!Node} handler
   */
  setGridHandler(handler) {
    this._gridHandler = handler;
  }

  /**
   * @param {function(string):!Node} handler
   */
  setVarHandler(handler) {
    this._varHandler = handler;
  }

  /**
   * @param {function(string):!Node} handler
   */
  setAngleHandler(handler) {
    this._angleHandler = handler;
  }

  /**
   * @return {!Element}
   */
  renderName() {
    const nameElement = document.createElement('span');
    nameElement.className = 'webkit-css-property';
    nameElement.textContent = this._propertyName;
    nameElement.normalize();
    return nameElement;
  }

  /**
   * @return {!Element}
   */
  renderValue() {
    const valueElement = document.createElement('span');
    valueElement.className = 'value';
    if (!this._propertyValue) {
      return valueElement;
    }

    const metadata = SDK.CSSMetadata.cssMetadata();

    if (this._shadowHandler && metadata.isShadowProperty(this._propertyName) &&
        !SDK.CSSMetadata.VariableRegex.test(this._propertyValue)) {
      valueElement.appendChild(this._shadowHandler(this._propertyValue, this._propertyName));
      valueElement.normalize();
      return valueElement;
    }

    if (this._gridHandler && metadata.isGridAreaDefiningProperty(this._propertyName)) {
      valueElement.appendChild(this._gridHandler(this._propertyValue, this._propertyName));
      valueElement.normalize();
      return valueElement;
    }

    if (metadata.isStringProperty(this._propertyName)) {
      UI.Tooltip.Tooltip.install(valueElement, unescapeCssString(this._propertyValue));
    }

    const regexes = [SDK.CSSMetadata.VariableRegex, SDK.CSSMetadata.URLRegex];
    const processors = [this._varHandler, this._processURL.bind(this)];
    if (this._bezierHandler && metadata.isBezierAwareProperty(this._propertyName)) {
      regexes.push(UI.Geometry.CubicBezier.Regex);
      processors.push(this._bezierHandler);
    }
    if (this._colorHandler && metadata.isColorAwareProperty(this._propertyName)) {
      regexes.push(Common.Color.Regex);
      processors.push(this._colorHandler);
    }
    if (this._angleHandler && metadata.isAngleAwareProperty(this._propertyName)) {
      // TODO(changhaohan): crbug.com/1138628 refactor this to handle unitless 0 cases
      regexes.push(InlineEditor.CSSAngleUtils.CSSAngleRegex);
      processors.push(this._angleHandler);
    }
    if (this._fontHandler && metadata.isFontAwareProperty(this._propertyName)) {
      if (this._propertyName === 'font-family') {
        regexes.push(InlineEditor.FontEditorUtils.FontFamilyRegex);
      } else {
        regexes.push(InlineEditor.FontEditorUtils.FontPropertiesRegex);
      }
      processors.push(this._fontHandler);
    }
    const results = TextUtils.TextUtils.Utils.splitStringByRegexes(this._propertyValue, regexes);
    for (let i = 0; i < results.length; i++) {
      const result = results[i];
      const processor =
          result.regexIndex === -1 ? document.createTextNode.bind(document) : processors[result.regexIndex];
      if (processor) {
        valueElement.appendChild(processor(result.value));
      }
    }
    valueElement.normalize();
    return valueElement;
  }

  /**
   * @param {string} text
   * @return {!Node}
   */
  _processURL(text) {
    // Strip "url(" and ")" along with whitespace.
    let url = text.substring(4, text.length - 1).trim();
    const isQuoted = /^'.*'$/s.test(url) || /^".*"$/s.test(url);
    if (isQuoted) {
      url = url.substring(1, url.length - 1);
    }
    const container = document.createDocumentFragment();
    UI.UIUtils.createTextChild(container, 'url(');
    let hrefUrl = null;
    if (this._rule && this._rule.resourceURL()) {
      hrefUrl = Common.ParsedURL.ParsedURL.completeURL(this._rule.resourceURL(), url);
    } else if (this._node) {
      hrefUrl = this._node.resolveURL(url);
    }
    const link = ImagePreviewPopover.setImageUrl(
        Components.Linkifier.Linkifier.linkifyURL(hrefUrl || url, {
          text: url,
          preventClick: false,
          // crbug.com/1027168
          // We rely on CSS text-overflow: ellipsis to hide long URLs in the Style panel,
          // so that we don't have to keep two versions (original vs. trimmed) of URL
          // at the same time, which complicates both StylesSidebarPane and StylePropertyTreeElement.
          bypassURLTrimming: true,
          className: undefined,
          lineNumber: undefined,
          columnNumber: undefined,
          inlineFrameIndex: 0,
          maxLength: undefined,
          tabStop: undefined,
        }),
        hrefUrl || url);
    container.appendChild(link);
    UI.UIUtils.createTextChild(container, ')');
    return container;
  }
}

/** @type {!ButtonProvider} */
let buttonProviderInstance;

/**
 * @implements {UI.Toolbar.Provider}
 */
export class ButtonProvider {
  /** @private */
  constructor() {
    this._button = new UI.Toolbar.ToolbarButton(i18nString(UIStrings.newStyleRule), 'largeicon-add');
    this._button.addEventListener(UI.Toolbar.ToolbarButton.Events.Click, this._clicked, this);
    const longclickTriangle = UI.Icon.Icon.create('largeicon-longclick-triangle', 'long-click-glyph');
    this._button.element.appendChild(longclickTriangle);

    new UI.UIUtils.LongClickController(this._button.element, this._longClicked.bind(this));
    UI.Context.Context.instance().addFlavorChangeListener(SDK.DOMModel.DOMNode, onNodeChanged.bind(this));
    onNodeChanged.call(this);

    /**
     * @this {ButtonProvider}
     */
    function onNodeChanged() {
      let node = UI.Context.Context.instance().flavor(SDK.DOMModel.DOMNode);
      node = node ? node.enclosingElementOrSelf() : null;
      this._button.setEnabled(Boolean(node));
    }
  }

  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!buttonProviderInstance || forceNew) {
      buttonProviderInstance = new ButtonProvider();
    }

    return buttonProviderInstance;
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _clicked(event) {
    StylesSidebarPane.instance()._createNewRuleInViaInspectorStyleSheet();
  }

  /**
   * @param {!Event} event
   */
  _longClicked(event) {
    StylesSidebarPane.instance()._onAddButtonLongClick(event);
  }

  /**
   * @override
   * @return {!UI.Toolbar.ToolbarItem}
   */
  item() {
    return this._button;
  }
}
