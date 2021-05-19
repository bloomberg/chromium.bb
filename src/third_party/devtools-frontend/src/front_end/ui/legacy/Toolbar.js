/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import * as Root from '../../core/root/root.js';

import {Action, Events as ActionEvents} from './ActionRegistration.js';  // eslint-disable-line no-unused-vars
import {ActionRegistry} from './ActionRegistry.js';
import * as ARIAUtils from './ARIAUtils.js';
import {ContextMenu} from './ContextMenu.js';
import {GlassPane, PointerEventsBehavior} from './GlassPane.js';
import {Icon} from './Icon.js';
import {bindCheckbox} from './SettingsUI.js';
import {Suggestions} from './SuggestBox.js';  // eslint-disable-line no-unused-vars
import {Events as TextPromptEvents, TextPrompt} from './TextPrompt.js';
import {Tooltip} from './Tooltip.js';
import {CheckboxLabel, LongClickController} from './UIUtils.js';
import {createShadowRootWithCoreStyles} from './utils/create-shadow-root-with-core-styles.js';

export class Toolbar {
  /**
   * @param {string} className
   * @param {!Element=} parentElement
   */
  constructor(className, parentElement) {
    /** @type {!Array.<!ToolbarItem>} */
    this._items = [];
    /** @type {!HTMLElement} */
    this.element =
        /** @type {!HTMLElement} */ (parentElement ? parentElement.createChild('div') : document.createElement('div'));
    this.element.className = className;
    this.element.classList.add('toolbar');
    this._enabled = true;
    this._shadowRoot = createShadowRootWithCoreStyles(
        this.element, {cssFile: 'ui/legacy/toolbar.css', enableLegacyPatching: true, delegatesFocus: undefined});
    this._contentElement = this._shadowRoot.createChild('div', 'toolbar-shadow');
    this._insertionPoint = this._contentElement.createChild('slot');
  }

  /**
   * @param {!Action} action
   * @param {!Array<!ToolbarButton>} toggledOptions
   * @param {!Array<!ToolbarButton>} untoggledOptions
   * @return {!ToolbarButton}
   */
  static createLongPressActionButton(action, toggledOptions, untoggledOptions) {
    const button = Toolbar.createActionButton(action);
    const mainButtonClone = Toolbar.createActionButton(action);

    /** @type {?LongClickController} */
    let longClickController = null;
    /** @type {?Array<!ToolbarButton>} */
    let longClickButtons = null;
    /** @type {?Element} */
    let longClickGlyph = null;

    action.addEventListener(ActionEvents.Toggled, updateOptions);
    updateOptions();
    return button;

    function updateOptions() {
      const buttons = action.toggled() ? (toggledOptions || null) : (untoggledOptions || null);

      if (buttons && buttons.length) {
        if (!longClickController) {
          longClickController = new LongClickController(button.element, showOptions);
          longClickGlyph = Icon.create('largeicon-longclick-triangle', 'long-click-glyph');
          button.element.appendChild(longClickGlyph);
          longClickButtons = buttons;
        }
      } else {
        if (longClickController) {
          longClickController.dispose();
          longClickController = null;
          if (longClickGlyph) {
            longClickGlyph.remove();
          }
          longClickGlyph = null;
          longClickButtons = null;
        }
      }
    }

    function showOptions() {
      let buttons = longClickButtons ? longClickButtons.slice() : [];
      buttons.push(mainButtonClone);

      const document = button.element.ownerDocument;
      document.documentElement.addEventListener('mouseup', mouseUp, false);

      const optionsGlassPane = new GlassPane();
      optionsGlassPane.setPointerEventsBehavior(PointerEventsBehavior.BlockedByGlassPane);
      optionsGlassPane.show(document);
      const optionsBar = new Toolbar('fill', optionsGlassPane.contentElement);
      optionsBar._contentElement.classList.add('floating');
      const buttonHeight = 26;

      const hostButtonPosition = button.element.boxInWindow().relativeToElement(GlassPane.container(document));

      const topNotBottom = hostButtonPosition.y + buttonHeight * buttons.length < document.documentElement.offsetHeight;

      if (topNotBottom) {
        buttons = buttons.reverse();
      }

      optionsBar.element.style.height = (buttonHeight * buttons.length) + 'px';
      if (topNotBottom) {
        optionsBar.element.style.top = (hostButtonPosition.y - 5) + 'px';
      } else {
        optionsBar.element.style.top = (hostButtonPosition.y - (buttonHeight * (buttons.length - 1)) - 6) + 'px';
      }
      optionsBar.element.style.left = (hostButtonPosition.x - 5) + 'px';

      for (let i = 0; i < buttons.length; ++i) {
        buttons[i].element.addEventListener('mousemove', mouseOver, false);
        buttons[i].element.addEventListener('mouseout', mouseOut, false);
        optionsBar.appendToolbarItem(buttons[i]);
      }
      const hostButtonIndex = topNotBottom ? 0 : buttons.length - 1;
      buttons[hostButtonIndex].element.classList.add('emulate-active');

      /** @param {!Event} e */
      function mouseOver(e) {
        if (/** @type {!MouseEvent} */ (e).which !== 1) {
          return;
        }
        if (e.target instanceof HTMLElement) {
          const buttonElement = e.target.enclosingNodeOrSelfWithClass('toolbar-item');
          buttonElement.classList.add('emulate-active');
        }
      }

      /** @param {!Event} e */
      function mouseOut(e) {
        if (/** @type {!MouseEvent} */ (e).which !== 1) {
          return;
        }
        if (e.target instanceof HTMLElement) {
          const buttonElement = e.target.enclosingNodeOrSelfWithClass('toolbar-item');
          buttonElement.classList.remove('emulate-active');
        }
      }

      /** @param {!Event} e */
      function mouseUp(e) {
        if (/** @type {!MouseEvent} */ (e).which !== 1) {
          return;
        }
        optionsGlassPane.hide();
        document.documentElement.removeEventListener('mouseup', mouseUp, false);

        for (let i = 0; i < buttons.length; ++i) {
          if (buttons[i].element.classList.contains('emulate-active')) {
            buttons[i].element.classList.remove('emulate-active');
            buttons[i]._clicked(e);
            break;
          }
        }
      }
    }
  }

  /**
   * @param {!Action} action
   * @param {!ToolbarButtonOptions=} options
   * @return {!ToolbarButton}
   */
  static createActionButton(action, options = TOOLBAR_BUTTON_DEFAULT_OPTIONS) {
    const button = action.toggleable() ? makeToggle() : makeButton();

    if (options.showLabel) {
      button.setText(action.title());
    }

    /** @param {!{data: *}} event */
    let handler = event => {
      action.execute();
    };
    if (options.userActionCode) {
      const actionCode = options.userActionCode;
      handler = () => {
        Host.userMetrics.actionTaken(actionCode);
        action.execute();
      };
    }
    button.addEventListener(ToolbarButton.Events.Click, handler, action);
    action.addEventListener(ActionEvents.Enabled, enabledChanged);
    button.setEnabled(action.enabled());
    return button;

    /**
     * @return {!ToolbarButton}
     */

    function makeButton() {
      const button = new ToolbarButton(action.title(), action.icon());
      if (action.title()) {
        Tooltip.install(button.element, action.title(), action.id(), {
          anchorTooltipAtElement: true,
        });
      }
      return button;
    }

    /**
     * @return {!ToolbarToggle}
     */
    function makeToggle() {
      const toggleButton = new ToolbarToggle(action.title(), action.icon(), action.toggledIcon());
      toggleButton.setToggleWithRedColor(action.toggleWithRedColor());
      action.addEventListener(ActionEvents.Toggled, toggled);
      toggled();
      return toggleButton;

      function toggled() {
        toggleButton.setToggled(action.toggled());
        if (action.title()) {
          toggleButton.setTitle(action.title());
          Tooltip.install(toggleButton.element, action.title(), action.id(), {
            anchorTooltipAtElement: true,
          });
        }
      }
    }

    /**
     * @param {!Common.EventTarget.EventTargetEvent} event
     */
    function enabledChanged(event) {
      button.setEnabled(/** @type {boolean} */ (event.data));
    }
  }

  /**
   * @param {string} actionId
   * @param {!ToolbarButtonOptions=} options
   * @return {!ToolbarButton}
   */
  static createActionButtonForId(actionId, options = TOOLBAR_BUTTON_DEFAULT_OPTIONS) {
    const action = ActionRegistry.instance().action(actionId);
    return Toolbar.createActionButton(/** @type {!Action} */ (action), options);
  }

  /**
   * @return {!Element}
   */
  gripElementForResize() {
    return this._contentElement;
  }

  /**
   * @param {boolean=} growVertically
   */
  makeWrappable(growVertically) {
    this._contentElement.classList.add('wrappable');
    if (growVertically) {
      this._contentElement.classList.add('toolbar-grow-vertical');
    }
  }

  makeVertical() {
    this._contentElement.classList.add('vertical');
  }

  makeBlueOnHover() {
    this._contentElement.classList.add('toolbar-blue-on-hover');
  }

  makeToggledGray() {
    this._contentElement.classList.add('toolbar-toggled-gray');
  }

  renderAsLinks() {
    this._contentElement.classList.add('toolbar-render-as-links');
  }

  /**
   * @return {boolean}
   */
  empty() {
    return !this._items.length;
  }

  /**
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    this._enabled = enabled;
    for (const item of this._items) {
      item._applyEnabledState(this._enabled && item._enabled);
    }
  }

  /**
   * @param {!ToolbarItem} item
   */
  appendToolbarItem(item) {
    this._items.push(item);
    item.toolbar = this;
    if (!this._enabled) {
      item._applyEnabledState(false);
    }
    this._contentElement.insertBefore(item.element, this._insertionPoint);
    this._hideSeparatorDupes();
  }

  appendSeparator() {
    this.appendToolbarItem(new ToolbarSeparator());
  }

  appendSpacer() {
    this.appendToolbarItem(new ToolbarSeparator(true));
  }

  /**
   * @param {string} text
   */
  appendText(text) {
    this.appendToolbarItem(new ToolbarText(text));
  }

  removeToolbarItems() {
    for (const item of this._items) {
      item.toolbar = null;
    }
    this._items = [];
    this._contentElement.removeChildren();
    this._insertionPoint = this._contentElement.createChild('slot');
  }

  /**
   * @param {string} color
   */
  setColor(color) {
    const style = document.createElement('style');
    style.textContent = '.toolbar-glyph { background-color: ' + color + ' !important }';
    this._shadowRoot.appendChild(style);
  }

  /**
   * @param {string} color
   */
  setToggledColor(color) {
    const style = document.createElement('style');
    style.textContent =
        '.toolbar-button.toolbar-state-on .toolbar-glyph { background-color: ' + color + ' !important }';
    this._shadowRoot.appendChild(style);
  }

  _hideSeparatorDupes() {
    if (!this._items.length) {
      return;
    }
    // Don't hide first and last separators if they were added explicitly.
    let previousIsSeparator = false;
    let lastSeparator;
    let nonSeparatorVisible = false;
    for (let i = 0; i < this._items.length; ++i) {
      if (this._items[i] instanceof ToolbarSeparator) {
        this._items[i].setVisible(!previousIsSeparator);
        previousIsSeparator = true;
        lastSeparator = this._items[i];
        continue;
      }
      if (this._items[i].visible()) {
        previousIsSeparator = false;
        lastSeparator = null;
        nonSeparatorVisible = true;
      }
    }
    if (lastSeparator && lastSeparator !== this._items[this._items.length - 1]) {
      lastSeparator.setVisible(false);
    }

    this.element.classList.toggle(
        'hidden',
        lastSeparator !== null && lastSeparator !== undefined && lastSeparator.visible() && !nonSeparatorVisible);
  }

  /**
   * @param {string} location
   * @return {!Promise<void>}
   */
  async appendItemsAtLocation(location) {
    /** @type {!Array<!ToolbarItemRegistration>} */
    const extensions = getRegisteredToolbarItems();

    extensions.sort((extension1, extension2) => {
      const order1 = extension1.order || 0;
      const order2 = extension2.order || 0;
      return order1 - order2;
    });

    const filtered = extensions.filter(e => e.location === location);
    const items = await Promise.all(filtered.map(extension => {
      const {separator, actionId, showLabel, loadItem} = extension;
      if (separator) {
        return new ToolbarSeparator();
      }
      if (actionId) {
        return Toolbar.createActionButtonForId(actionId, {showLabel: Boolean(showLabel), userActionCode: undefined});
      }
      // TODO(crbug.com/1134103) constratint the case checked with this if using TS type definitions once UI is TS-authored.
      if (!loadItem) {
        throw new Error('Could not load a toolbar item registration with no loadItem function');
      }
      return loadItem().then(p => /** @type {!Provider} */ (p).item());
    }));

    for (const item of items) {
      if (item) {
        this.appendToolbarItem(item);
      }
    }
  }
}

/**
 * @typedef {{
  *    showLabel: boolean,
  *    userActionCode: (!Host.UserMetrics.Action|undefined)
  * }}
  */
// @ts-ignore typedef
export let ToolbarButtonOptions;

/** @type {!ToolbarButtonOptions} */
const TOOLBAR_BUTTON_DEFAULT_OPTIONS = {
  showLabel: false,
  userActionCode: undefined
};

export class ToolbarItem extends Common.ObjectWrapper.ObjectWrapper {
  /**
   * @param {!Element} element
   */
  constructor(element) {
    super();
    /** @type {!HTMLElement} */
    this.element = /** @type {!HTMLElement} */ (element);
    this.element.classList.add('toolbar-item');
    this._visible = true;
    this._enabled = true;

    /**
     * Set by the parent toolbar during appending.
     * @type {?Toolbar}
     */
    this.toolbar = null;
  }

  /**
   * @param {string} title
   * @param {string | undefined} actionId
   */
  setTitle(title, actionId = undefined) {
    if (this._title === title) {
      return;
    }
    /** @type {string|undefined} */
    this._title = title;
    ARIAUtils.setAccessibleName(this.element, title);
    Tooltip.install(this.element, title, actionId, {
      anchorTooltipAtElement: true,
    });
  }

  /**
   * @param {boolean} value
   */
  setEnabled(value) {
    if (this._enabled === value) {
      return;
    }
    this._enabled = value;
    this._applyEnabledState(this._enabled && (!this.toolbar || this.toolbar._enabled));
  }

  /**
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    // @ts-ignore: Ignoring in favor of an `instanceof` check for all the different
    //             kind of HTMLElement classes that have a disabled attribute.
    this.element.disabled = !enabled;
  }

  /**
   * @return {boolean} x
   */
  visible() {
    return this._visible;
  }

  /**
   * @param {boolean} x
   */
  setVisible(x) {
    if (this._visible === x) {
      return;
    }
    this.element.classList.toggle('hidden', !x);
    this._visible = x;
    if (this.toolbar && !(this instanceof ToolbarSeparator)) {
      this.toolbar._hideSeparatorDupes();
    }
  }

  /** @param {boolean} alignRight */
  setRightAligned(alignRight) {
    this.element.classList.toggle('toolbar-item-right-aligned', alignRight);
  }
}

export class ToolbarText extends ToolbarItem {
  /**
   * @param {string=} text
   */
  constructor(text) {
    const element = document.createElement('div');
    element.classList.add('toolbar-text');
    super(element);
    this.element.classList.add('toolbar-text');
    this.setText(text || '');
  }

  /**
   * @return {string}
   */
  text() {
    return this.element.textContent || '';
  }

  /**
   * @param {string} text
   */
  setText(text) {
    this.element.textContent = text;
  }
}

export class ToolbarButton extends ToolbarItem {
  /**
   * TODO(crbug.com/1126026): remove glyph parameter in favor of icon.
   * @param {string} title
   * @param {(string|HTMLElement)=} glyphOrIcon
   * @param {string=} text
   */
  constructor(title, glyphOrIcon, text) {
    const element = document.createElement('button');
    element.classList.add('toolbar-button');
    super(element);
    this.element.addEventListener('click', this._clicked.bind(this), false);
    this.element.addEventListener('mousedown', this._mouseDown.bind(this), false);

    this._glyphElement = Icon.create('', 'toolbar-glyph hidden');
    this.element.appendChild(this._glyphElement);
    this._textElement = this.element.createChild('div', 'toolbar-text hidden');

    this.setTitle(title);
    if (glyphOrIcon instanceof HTMLElement) {
      glyphOrIcon.classList.add('toolbar-icon');
      this.element.append(glyphOrIcon);
    } else if (glyphOrIcon) {
      this.setGlyph(glyphOrIcon);
    }
    this.setText(text || '');
    this._title = '';
  }

  focus() {
    this.element.focus();
  }

  /**
   * @param {string} text
   */
  setText(text) {
    if (this._text === text) {
      return;
    }
    this._textElement.textContent = text;
    this._textElement.classList.toggle('hidden', !text);
    /** @type {string|undefined} */
    this._text = text;
  }

  /**
   * @param {string} glyph
   */
  setGlyph(glyph) {
    if (this._glyph === glyph) {
      return;
    }
    this._glyphElement.setIconType(glyph);
    this._glyphElement.classList.toggle('hidden', !glyph);
    this.element.classList.toggle('toolbar-has-glyph', Boolean(glyph));
    /** @type {string|undefined} */
    this._glyph = glyph;
  }

  /**
   * @param {string} iconURL
   */
  setBackgroundImage(iconURL) {
    this.element.style.backgroundImage = 'url(' + iconURL + ')';
  }

  setSecondary() {
    this.element.classList.add('toolbar-button-secondary');
  }

  setDarkText() {
    this.element.classList.add('dark-text');
  }

  /**
   * @param {boolean=} shrinkable
   */
  turnIntoSelect(shrinkable = false) {
    this.element.classList.add('toolbar-has-dropdown');
    if (shrinkable) {
      this.element.classList.add('toolbar-has-dropdown-shrinkable');
    }
    const dropdownArrowIcon = Icon.create('smallicon-triangle-down', 'toolbar-dropdown-arrow');
    this.element.appendChild(dropdownArrowIcon);
  }

  /**
   * @param {!Event} event
   */
  _clicked(event) {
    if (!this._enabled) {
      return;
    }
    this.dispatchEventToListeners(ToolbarButton.Events.Click, event);
    event.consume();
  }

  /**
   * @param {!Event} event
   */
  _mouseDown(event) {
    if (!this._enabled) {
      return;
    }
    this.dispatchEventToListeners(ToolbarButton.Events.MouseDown, event);
  }
}

ToolbarButton.Events = {
  Click: Symbol('Click'),
  MouseDown: Symbol('MouseDown')
};

export class ToolbarInput extends ToolbarItem {
  /**
   * @param {string} placeholder
   * @param {string=} accessiblePlaceholder
   * @param {number=} growFactor
   * @param {number=} shrinkFactor
   * @param {string=} tooltip
   * @param {(function(string, string, boolean=):!Promise<!Suggestions>)=} completions
   * @param {boolean=} dynamicCompletions
   */
  constructor(placeholder, accessiblePlaceholder, growFactor, shrinkFactor, tooltip, completions, dynamicCompletions) {
    const element = document.createElement('div');
    element.classList.add('toolbar-input');
    super(element);

    const internalPromptElement = this.element.createChild('div', 'toolbar-input-prompt');
    ARIAUtils.setAccessibleName(internalPromptElement, placeholder);
    internalPromptElement.addEventListener('focus', () => this.element.classList.add('focused'));
    internalPromptElement.addEventListener('blur', () => this.element.classList.remove('focused'));

    this._prompt = new TextPrompt();
    this._proxyElement = this._prompt.attach(internalPromptElement);
    this._proxyElement.classList.add('toolbar-prompt-proxy');
    this._proxyElement.addEventListener(
        'keydown', /** @param {!Event} event */ event => this._onKeydownCallback(event));
    this._prompt.initialize(completions || (() => Promise.resolve([])), ' ', dynamicCompletions);
    if (tooltip) {
      this._prompt.setTitle(tooltip);
    }
    this._prompt.setPlaceholder(placeholder, accessiblePlaceholder);
    this._prompt.addEventListener(TextPromptEvents.TextChanged, this._onChangeCallback.bind(this));

    if (growFactor) {
      this.element.style.flexGrow = String(growFactor);
    }
    if (shrinkFactor) {
      this.element.style.flexShrink = String(shrinkFactor);
    }

    const clearButton = this.element.createChild('div', 'toolbar-input-clear-button');
    clearButton.appendChild(Icon.create('mediumicon-gray-cross-hover', 'search-cancel-button'));
    clearButton.addEventListener('click', () => {
      this.setValue('', true);
      this._prompt.focus();
    });

    this._updateEmptyStyles();
  }

  /**
   * @override
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    this._prompt.setEnabled(enabled);
  }

  /**
   * @param {string} value
   * @param {boolean=} notify
   */
  setValue(value, notify) {
    this._prompt.setText(value);
    if (notify) {
      this._onChangeCallback();
    }
    this._updateEmptyStyles();
  }

  /**
   * @return {string}
   */
  value() {
    return this._prompt.textWithCurrentSuggestion();
  }

  /**
   * @param {!Event} event
   */
  _onKeydownCallback(event) {
    if (!isEscKey(event) || !this._prompt.text()) {
      return;
    }
    this.setValue('', true);
    event.consume(true);
  }

  _onChangeCallback() {
    this._updateEmptyStyles();
    this.dispatchEventToListeners(ToolbarInput.Event.TextChanged, this._prompt.text());
  }

  _updateEmptyStyles() {
    this.element.classList.toggle('toolbar-input-empty', !this._prompt.text());
  }
}

ToolbarInput.Event = {
  TextChanged: Symbol('TextChanged')
};

export class ToolbarToggle extends ToolbarButton {
  /**
   * @param {string} title
   * @param {string=} glyph
   * @param {string=} toggledGlyph
   */
  constructor(title, glyph, toggledGlyph) {
    super(title, glyph, '');
    this._toggled = false;
    this._untoggledGlyph = glyph;
    this._toggledGlyph = toggledGlyph;
    this.element.classList.add('toolbar-state-off');
    ARIAUtils.setPressed(this.element, false);
  }

  /**
   * @return {boolean}
   */
  toggled() {
    return this._toggled;
  }

  /**
   * @param {boolean} toggled
   */
  setToggled(toggled) {
    if (this._toggled === toggled) {
      return;
    }
    this._toggled = toggled;
    this.element.classList.toggle('toolbar-state-on', toggled);
    this.element.classList.toggle('toolbar-state-off', !toggled);
    ARIAUtils.setPressed(this.element, toggled);
    if (this._toggledGlyph && this._untoggledGlyph) {
      this.setGlyph(toggled ? this._toggledGlyph : this._untoggledGlyph);
    }
  }

  /**
   * @param {boolean} withRedColor
   */
  setDefaultWithRedColor(withRedColor) {
    this.element.classList.toggle('toolbar-default-with-red-color', withRedColor);
  }

  /**
   * @param {boolean} toggleWithRedColor
   */
  setToggleWithRedColor(toggleWithRedColor) {
    this.element.classList.toggle('toolbar-toggle-with-red-color', toggleWithRedColor);
  }
}


export class ToolbarMenuButton extends ToolbarButton {
  /**
   * @param {function(!ContextMenu):void} contextMenuHandler
   * @param {boolean=} useSoftMenu
   */
  constructor(contextMenuHandler, useSoftMenu) {
    super('', 'largeicon-menu');
    this._contextMenuHandler = contextMenuHandler;
    this._useSoftMenu = Boolean(useSoftMenu);
    ARIAUtils.markAsMenuButton(this.element);
  }

  /**
   * @override
   * @param {!Event} event
   */
  _mouseDown(event) {
    if (/** @type {!MouseEvent} */ (event).buttons !== 1) {
      super._mouseDown(event);
      return;
    }

    if (!this._triggerTimeout) {
      this._triggerTimeout = setTimeout(this._trigger.bind(this, event), 200);
    }
  }

  /**
   * @param {!Event} event
   */
  _trigger(event) {
    delete this._triggerTimeout;

    // Throttling avoids entering a bad state on Macs when rapidly triggering context menus just
    // after the window gains focus. See crbug.com/655556
    if (this._lastTriggerTime && Date.now() - this._lastTriggerTime < 300) {
      return;
    }
    const contextMenu = new ContextMenu(
        event, this._useSoftMenu, this.element.totalOffsetLeft(),
        this.element.totalOffsetTop() + this.element.offsetHeight);
    this._contextMenuHandler(contextMenu);
    contextMenu.show();
    this._lastTriggerTime = Date.now();
  }

  /**
   * @override
   * @param {!Event} event
   */
  _clicked(event) {
    if (this._triggerTimeout) {
      clearTimeout(this._triggerTimeout);
    }
    this._trigger(event);
  }
}

export class ToolbarSettingToggle extends ToolbarToggle {
  /**
   * @param {!Common.Settings.Setting<boolean>} setting
   * @param {string} glyph
   * @param {string} title
   */
  constructor(setting, glyph, title) {
    super(title, glyph);
    this._defaultTitle = title;
    this._setting = setting;
    this._settingChanged();
    this._setting.addChangeListener(this._settingChanged, this);
  }

  _settingChanged() {
    const toggled = this._setting.get();
    this.setToggled(toggled);
    this.setTitle(this._defaultTitle);
  }

  /**
   * @override
   * @param {!Event} event
   */
  _clicked(event) {
    this._setting.set(!this.toggled());
    super._clicked(event);
  }
}

export class ToolbarSeparator extends ToolbarItem {
  /**
   * @param {boolean=} spacer
   */
  constructor(spacer) {
    const element = document.createElement('div');
    element.classList.add(spacer ? 'toolbar-spacer' : 'toolbar-divider');
    super(element);
  }
}

/**
 * @interface
 */
export class Provider {
  /**
   * @return {?ToolbarItem}
   */
  item() {
    throw new Error('not implemented');
  }
}

/**
 * @interface
 */
export class ItemsProvider {
  /**
   * @return {!Array<!ToolbarItem>}
   */
  toolbarItems() {
    throw new Error('not implemented');
  }
}

export class ToolbarComboBox extends ToolbarItem {
  /**
   * @param {?function(!Event):void} changeHandler
   * @param {string} title
   * @param {string=} className
   */
  constructor(changeHandler, title, className) {
    const element = document.createElement('span');
    element.classList.add('toolbar-select-container');
    super(element);

    /** @type {!HTMLSelectElement} */
    this._selectElement = /** @type {!HTMLSelectElement} */ (this.element.createChild('select', 'toolbar-item'));
    const dropdownArrowIcon = Icon.create('smallicon-triangle-down', 'toolbar-dropdown-arrow');
    this.element.appendChild(dropdownArrowIcon);
    if (changeHandler) {
      this._selectElement.addEventListener('change', changeHandler, false);
    }
    ARIAUtils.setAccessibleName(this._selectElement, title);
    super.setTitle(title);
    if (className) {
      this._selectElement.classList.add(className);
    }
  }

  /**
   * @return {!HTMLSelectElement}
   */
  selectElement() {
    return this._selectElement;
  }

  /**
   * @return {number}
   */
  size() {
    return this._selectElement.childElementCount;
  }

  /**
   * @return {!Array.<!HTMLOptionElement>}
   */
  options() {
    return Array.prototype.slice.call(this._selectElement.children, 0);
  }

  /**
   * @param {!Element} option
   */
  addOption(option) {
    this._selectElement.appendChild(option);
  }

  /**
   * @param {string} label
   * @param {string=} value
   * @return {!Element}
   */
  createOption(label, value) {
    /** @type {!HTMLOptionElement} */
    const option = /** @type {!HTMLOptionElement} */ (this._selectElement.createChild('option'));
    option.text = label;
    if (typeof value !== 'undefined') {
      option.value = value;
    }
    return option;
  }

  /**
   * @override
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    super._applyEnabledState(enabled);
    this._selectElement.disabled = !enabled;
  }

  /**
   * @param {!Element} option
   */
  removeOption(option) {
    this._selectElement.removeChild(option);
  }

  removeOptions() {
    this._selectElement.removeChildren();
  }

  /**
   * @return {?HTMLOptionElement}
   */
  selectedOption() {
    if (this._selectElement.selectedIndex >= 0) {
      return /** @type {!HTMLOptionElement} */ (this._selectElement[this._selectElement.selectedIndex]);
    }
    return null;
  }

  /**
   * @param {!Element} option
   */
  select(option) {
    this._selectElement.selectedIndex = Array.prototype.indexOf.call(/** @type {?} */ (this._selectElement), option);
  }

  /**
   * @param {number} index
   */
  setSelectedIndex(index) {
    this._selectElement.selectedIndex = index;
  }

  /**
   * @return {number}
   */
  selectedIndex() {
    return this._selectElement.selectedIndex;
  }

  /**
   * @param {number} width
   */
  setMaxWidth(width) {
    this._selectElement.style.maxWidth = width + 'px';
  }

  /**
   * @param {number} width
   */
  setMinWidth(width) {
    this._selectElement.style.minWidth = width + 'px';
  }
}

export class ToolbarSettingComboBox extends ToolbarComboBox {
  /**
   * @param {!Array<!{value: string, label: string}>} options
   * @param {!Common.Settings.Setting<*>} setting
   * @param {string} accessibleName
   */
  constructor(options, setting, accessibleName) {
    super(null, accessibleName);
    this._options = options;
    this._setting = setting;
    this._selectElement.addEventListener('change', this._valueChanged.bind(this), false);
    this.setOptions(options);
    setting.addChangeListener(this._settingChanged, this);
  }

  /**
   * @param {!Array<!{value: string, label: string}>} options
   */
  setOptions(options) {
    this._options = options;
    this._selectElement.removeChildren();
    for (let i = 0; i < options.length; ++i) {
      const dataOption = options[i];
      const option = this.createOption(dataOption.label, dataOption.value);
      this._selectElement.appendChild(option);
      if (this._setting.get() === dataOption.value) {
        this.setSelectedIndex(i);
      }
    }
  }

  /**
   * @return {string}
   */
  value() {
    return this._options[this.selectedIndex()].value;
  }

  _settingChanged() {
    if (this._muteSettingListener) {
      return;
    }

    const value = this._setting.get();
    for (let i = 0; i < this._options.length; ++i) {
      if (value === this._options[i].value) {
        this.setSelectedIndex(i);
        break;
      }
    }
  }

  /**
   * @param {!Event} event
   */
  _valueChanged(event) {
    const option = this._options[this.selectedIndex()];
    this._muteSettingListener = true;
    this._setting.set(option.value);
    this._muteSettingListener = false;
  }
}

export class ToolbarCheckbox extends ToolbarItem {
  /**
   * @param {string} text
   * @param {string=} tooltip
   * @param {function(MouseEvent):void=} listener
   */
  constructor(text, tooltip, listener) {
    super(CheckboxLabel.create(text));
    this.element.classList.add('checkbox');
    this.inputElement = /** @type {!CheckboxLabel} */ (this.element).checkboxElement;
    if (tooltip) {
      // install on the checkbox
      Tooltip.install(this.inputElement, tooltip, undefined, {
        anchorTooltipAtElement: true,
      });
      // install on the checkbox label
      Tooltip.install(/** @type {!CheckboxLabel} */ (this.element).textElement, tooltip, undefined, {
        anchorTooltipAtElement: true,
      });
    }
    if (listener) {
      this.inputElement.addEventListener('click', listener, false);
    }
  }

  /**
   * @return {boolean}
   */
  checked() {
    return this.inputElement.checked;
  }

  /**
   * @param {boolean} value
   */
  setChecked(value) {
    this.inputElement.checked = value;
  }

  /**
   * @override
   * @param {boolean} enabled
   */
  _applyEnabledState(enabled) {
    super._applyEnabledState(enabled);
    this.inputElement.disabled = !enabled;
  }
}

export class ToolbarSettingCheckbox extends ToolbarCheckbox {
  /**
   * @param {!Common.Settings.Setting<boolean>} setting
   * @param {string=} tooltip
   * @param {string=} alternateTitle
   */
  constructor(setting, tooltip, alternateTitle) {
    super(alternateTitle || setting.title() || '', tooltip);
    bindCheckbox(this.inputElement, setting);
  }
}

/** @type {!Array<!ToolbarItemRegistration>} */
const registeredToolbarItems = [];

/**
 * @param {!ToolbarItemRegistration} registration
 */
export function registerToolbarItem(registration) {
  registeredToolbarItems.push(registration);
}

/**
 * @return {!Array<ToolbarItemRegistration>}
 */
function getRegisteredToolbarItems() {
  return registeredToolbarItems.filter(
      item => Root.Runtime.Runtime.isDescriptorEnabled({experiment: undefined, condition: item.condition}));
}

/**
 * @typedef {{
  *  order: (number|undefined),
  *  location: !ToolbarItemLocation,
  *  separator: (boolean|undefined),
  *  showLabel: (boolean|undefined),
  *  actionId: (string|undefined),
  *  condition: (!Root.Runtime.ConditionName|undefined),
  *  loadItem: (undefined|function(): !Promise<!Provider>)
  * }} */
// @ts-ignore typedef
export let ToolbarItemRegistration;


/** @enum {string} */
export const ToolbarItemLocation = {
  FILES_NAVIGATION_TOOLBAR: 'files-navigator-toolbar',
  MAIN_TOOLBAR_RIGHT: 'main-toolbar-right',
  MAIN_TOOLBAR_LEFT: 'main-toolbar-left',
  STYLES_SIDEBARPANE_TOOLBAR: 'styles-sidebarpane-toolbar',
};
