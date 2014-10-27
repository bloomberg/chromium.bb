// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.elements.content.MenuView');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.content.MenuItem');
goog.require('i18n.input.chrome.inputview.util');



goog.scope(function() {
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var MenuItem = i18n.input.chrome.inputview.elements.content.MenuItem;
var Css = i18n.input.chrome.inputview.Css;


/**
 * The view for IME switcher, layout switcher and settings menu popup..
 *
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
// TODO: MenuView could extend from AltDataView after some refactor.
i18n.input.chrome.inputview.elements.content.MenuView = function(
    opt_eventTarget) {
  goog.base(this, '', ElementType.MENU_VIEW, opt_eventTarget);

  this.pointerConfig.stopEventPropagation = false;
  this.pointerConfig.preventDefault = false;
};
goog.inherits(i18n.input.chrome.inputview.elements.content.MenuView,
    i18n.input.chrome.inputview.elements.Element);
var MenuView = i18n.input.chrome.inputview.elements.content.MenuView;


/**
 * The supported command.
 *
 * @enum {number}
 */
MenuView.Command = {
  SWITCH_IME: 0,
  SWITCH_KEYSET: 1,
  OPEN_EMOJI: 2,
  OPEN_HANDWRITING: 3,
  OPEN_SETTING: 4
};


/**
 * The maximal number of visible input methods in the view. If more than 3 input
 * methods are enabled, only 3 of them will show and others can be scrolled into
 * view. The reason we have this limiation is because menu view can not be
 * higher than the keyboard view.
 *
 * @type {number}
 * @private
 */
MenuView.MAXIMAL_VISIBLE_IMES_ = 3;


/**
 * The width of the popup menu.
 *
 * @type {number}
 * @private
 */
MenuView.WIDTH_ = 300;


/**
 * The height of the popup menu item.
 *
 * @type {number}
 * @private
 */
MenuView.LIST_ITEM_HEIGHT_ = 50;


/**
 * The cover element.
 * Note: The reason we use a separate cover element instead of the view is
 * because of the opacity. We can not reassign the opacity in child element.
 *
 * @type {!Element}
 * @private
 */
MenuView.prototype.coverElement_;


/** @override */
MenuView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.dom.classlist.add(elem, Css.MENU_VIEW);
  this.coverElement_ = dom.createDom(goog.dom.TagName.DIV, Css.ALTDATA_COVER);
  dom.appendChild(document.body, this.coverElement_);
  goog.style.setElementShown(this.coverElement_, false);

  this.coverElement_['view'] = this;
};


/** @override */
MenuView.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');

  goog.style.setElementShown(this.getElement(), false);
};


/**
 * Shows the menu view.
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key The key
 *     triggerred this altdata view.
 * @param {!string} currentKeysetId The keyset ID that this menu key belongs to.
 * @param {boolean} isCompact True if the keyset that owns the menu key is a
 *     compact layout.
 * @param {boolean} enableCompactLayout True if the keyset that owns the menu
 *     key enabled compact layout.
 * @param {!string} currentInputMethod The current input method ID.
 * @param {?Array.<Object>} inputMethods The list of activated input methods.
 * @param {boolean} hasHwt Whether to add handwriting button.
 * @param {boolean} enableSettings Whether to add a link to settings page.
 * @param {boolean} hasEmoji Whether to enable emoji.
 */
MenuView.prototype.show = function(key, currentKeysetId, isCompact,
    enableCompactLayout, currentInputMethod, inputMethods, hasHwt,
    enableSettings, hasEmoji) {
  var ElementType = i18n.input.chrome.inputview.elements.ElementType;
  var dom = this.getDomHelper();
  if (key.type != ElementType.MENU_KEY) {
    console.error('Only menu key should trigger menu view to show');
    return;
  }
  this.triggeredBy = key;
  var coordinate = goog.style.getClientPosition(key.getElement());
  var x = coordinate.x;
  // y is the maximual height that menu view can have.
  var y = coordinate.y;

  goog.style.setElementShown(this.getElement(), true);
  // may not need to remove child.
  dom.removeChildren(this.getElement());

  var totalHeight = 0;
  totalHeight += this.addInputMethodItems_(currentInputMethod, inputMethods);
  totalHeight += this.addLayoutSwitcherItem_(key, currentKeysetId, isCompact,
      enableCompactLayout);
  if (hasHwt || enableSettings || hasEmoji) {
    totalHeight += this.addFooterItems_(hasHwt, enableSettings, hasEmoji);
  }


  var left = x;
  //TODO: take care of elemTop < 0. A scrollable view is probably needed.
  var elemTop = y - totalHeight;

  goog.style.setPosition(this.getElement(), left, elemTop);
  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy.setHighlighted(true);
};


/**
 * Hides the menu view.
 */
MenuView.prototype.hide = function() {
  goog.style.setElementShown(this.getElement(), false);
  goog.style.setElementShown(this.coverElement_, false);
  if (this.triggeredBy) {
    this.triggeredBy.setHighlighted(false);
  }
};


/**
 * Adds the list of activated input methods.
 *
 * @param {!string} currentInputMethod The current input method ID.
 * @param {?Array.<Object>} inputMethods The list of activated input methods.
 * @return {number} The height of the ime list container.
 * @private
 */
MenuView.prototype.addInputMethodItems_ = function(currentInputMethod,
    inputMethods) {
  var dom = this.getDomHelper();
  var container = dom.createDom(goog.dom.TagName.DIV,
      Css.IME_LIST_CONTAINER);

  for (var i = 0; i < inputMethods.length; i++) {
    var inputMethod = inputMethods[i];
    var listItem = {};
    listItem['indicator'] = inputMethod['indicator'];
    listItem['name'] = inputMethod['name'];
    listItem['command'] =
        [MenuView.Command.SWITCH_IME, inputMethod['id']];
    var imeItem = new MenuItem(String(i), listItem, MenuItem.Type.LIST_ITEM);
    imeItem.render(container);
    if (currentInputMethod == inputMethod['id']) {
      imeItem.check();
    }
    goog.style.setSize(imeItem.getElement(), MenuView.WIDTH_,
        MenuView.LIST_ITEM_HEIGHT_);
  }

  var containerHeight = inputMethods.length > MenuView.MAXIMAL_VISIBLE_IMES_ ?
      MenuView.LIST_ITEM_HEIGHT_ * MenuView.MAXIMAL_VISIBLE_IMES_ :
      MenuView.LIST_ITEM_HEIGHT_ * inputMethods.length;
  goog.style.setSize(container, MenuView.WIDTH_, containerHeight);

  dom.appendChild(this.getElement(), container);
  return containerHeight;
};


/**
 * Add the full/compact layout switcher item. If a full layout does not have or
 * disabled compact layout, this is a noop.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key The key
 *     triggerred this altdata view.
 * @param {!string} currentKeysetId The keyset ID that this menu key belongs to.
 * @param {boolean} isCompact True if the keyset that owns the menu key is a
 *     compact layout.
 * @param {boolean} enableCompactLayout True if the keyset that owns the menu
 *     key enabled compact layout.
 * @return {number} The height of the layout switcher item.
 * @private
 */
MenuView.prototype.addLayoutSwitcherItem_ = function(key, currentKeysetId,
    isCompact, enableCompactLayout) {
  // Some full layouts either disabled compact layout (CJK) or do not have one.
  // Do not add a layout switcher item in this case.
  if (!isCompact && !enableCompactLayout) {
    return 0;
  }
  var dom = this.getDomHelper();
  // Adds layout switcher.
  var layoutSwitcher = {};
  if (isCompact) {
    layoutSwitcher['iconURL'] = 'images/regular_size.png';
    layoutSwitcher['name'] = chrome.i18n.getMessage('SWITCH_TO_FULL_LAYOUT');
    var fullLayoutId = currentKeysetId.split('.')[0];
    layoutSwitcher['command'] =
        [MenuView.Command.SWITCH_KEYSET, fullLayoutId];
  } else {
    layoutSwitcher['iconURL'] = 'images/compact.png';
    layoutSwitcher['name'] = chrome.i18n.getMessage('SWITCH_TO_COMPACT_LAYOUT');
    if (goog.array.contains(i18n.input.chrome.inputview.util.KEYSETS_USE_US,
        currentKeysetId)) {
      key.toKeyset = currentKeysetId + '.compact.qwerty';
    }
    layoutSwitcher['command'] =
        [MenuView.Command.SWITCH_KEYSET, key.toKeyset];

  }
  var layoutSwitcherItem = new MenuItem('MenuLayoutSwitcher', layoutSwitcher,
      MenuItem.Type.LIST_ITEM);
  layoutSwitcherItem.render(this.getElement());
  goog.style.setSize(layoutSwitcherItem.getElement(), MenuView.WIDTH_,
      MenuView.LIST_ITEM_HEIGHT_);

  return MenuView.LIST_ITEM_HEIGHT_;
};


/**
 * Adds a few items into the menu view. We only support handwriting and settings
 * now.
 *
 * @param {boolean} hasHwt Whether to add handwriting button.
 * @param {boolean} enableSettings Whether to add settings button.
 * @param {boolean} hasEmoji Whether to add emoji button.
 * @return {number} The height of the footer.
 * @private
 */
MenuView.prototype.addFooterItems_ = function(hasHwt, enableSettings,
    hasEmoji) {
  var dom = this.getDomHelper();
  var footer = dom.createDom(goog.dom.TagName.DIV, Css.MENU_FOOTER);
  if (hasEmoji) {
    var emoji = {};
    emoji['iconCssClass'] = Css.MENU_FOOTER_EMOJI_BUTTON;
    emoji['command'] = [MenuView.Command.OPEN_EMOJI];
    var emojiFooter = new MenuItem('emoji', emoji,
        MenuItem.Type.FOOTER_ITEM);
    emojiFooter.render(footer);
  }

  if (hasHwt) {
    var handWriting = {};
    handWriting['iconCssClass'] = Css.MENU_FOOTER_HANDWRITING_BUTTON;
    handWriting['command'] = [MenuView.Command.OPEN_HANDWRITING];
    var handWritingFooter = new MenuItem('handwriting', handWriting,
        MenuItem.Type.FOOTER_ITEM);
    handWritingFooter.render(footer);
  }

  if (enableSettings) {
    var setting = {};
    setting['iconCssClass'] = Css.MENU_FOOTER_SETTING_BUTTON;
    setting['command'] = [MenuView.Command.OPEN_SETTING];
    var settingFooter = new MenuItem('setting', setting,
        MenuItem.Type.FOOTER_ITEM);
    settingFooter.render(footer);
  }

  // Sets footer itmes' width.
  var elems = dom.getChildren(footer);
  var len = elems.length;
  var subWidth = Math.ceil(MenuView.WIDTH_ / len);
  var i = 0;
  for (; i < len - 1; i++) {
    elems[i].style.width = subWidth + 'px';
  }
  elems[i].style.width = (MenuView.WIDTH_ - subWidth * (len - 1)) + 'px';

  dom.appendChild(this.getElement(), footer);
  goog.style.setSize(footer, MenuView.WIDTH_, MenuView.LIST_ITEM_HEIGHT_);

  return MenuView.LIST_ITEM_HEIGHT_;
};


/**
 * Gets the cover element.
 *
 * @return {!Element} The cover element.
 */
MenuView.prototype.getCoverElement = function() {
  return this.coverElement_;
};


/** @override */
MenuView.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);

  goog.style.setSize(this.coverElement_, width, height);
  // Hides the menu view directly after resize.
  this.hide();
};


/** @override */
MenuView.prototype.disposeInternal = function() {
  this.getElement()['view'] = null;

  goog.base(this, 'disposeInternal');
};
});  // goog.scope

