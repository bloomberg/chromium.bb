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
goog.provide('i18n.input.chrome.inputview.Css');


/**
 * The css used for input view keyboard.
 *
 * @enum {string}
 */
i18n.input.chrome.inputview.Css = {
  A11Y: goog.getCssName('inputview-a11y'),
  ALTDATA_COVER: goog.getCssName('inputview-altdata-cover'),
  ALTDATA_KEY: goog.getCssName('inputview-altdata-key'),
  ALTDATA_SEPARATOR: goog.getCssName('inputview-altdata-separator'),
  ALTDATA_VIEW: goog.getCssName('inputview-altdata-view'),
  ALTGR_CONTENT: goog.getCssName('inputview-ac'),
  ARROW_KEY: goog.getCssName('inputview-arrow-key'),
  BACKSPACE_ICON: goog.getCssName('inputview-backspace-icon'),
  CANDIDATE: goog.getCssName('inputview-candidate'),
  CANDIDATES_LINE: goog.getCssName('inputview-candidates-line'),
  CANDIDATES_TOP_LINE: goog.getCssName('inputview-candidates-top-line'),
  CANDIDATE_AUTOCORRECT: goog.getCssName('inputview-candidate-autocorrect'),
  CANDIDATE_BUTTON: goog.getCssName('inputview-candidate-button'),
  CANDIDATE_DEFAULT: goog.getCssName('inputview-candidate-default'),
  CANDIDATE_HIGHLIGHT: goog.getCssName('inputview-candidate-highlight'),
  CANDIDATE_INTER_CONTAINER: goog.getCssName('inputview-candiate-ic'),
  CANDIDATE_SEPARATOR: goog.getCssName('inputview-candidate-separator'),
  CANDIDATE_VIEW: goog.getCssName('inputview-candidate-view'),
  CANVAS: goog.getCssName('inputview-canvas'),
  CANVAS_LEFT_COLUMN: goog.getCssName('inputview-canvas-left-column'),
  CANVAS_RIGHT_COLUMN: goog.getCssName('inputview-canvas-right-column'),
  CANVAS_VIEW: goog.getCssName('inputview-canvasview'),
  CAPSLOCK_DOT: goog.getCssName('inputview-capslock-dot'),
  CAPSLOCK_DOT_HIGHLIGHT: goog.getCssName('inputview-capslock-dot-highlight'),
  CHARACTER: goog.getCssName('inputview-character'),
  CHARACTER_HIGHLIGHT: goog.getCssName('inputview-ch'),
  CHECKED_MENU_LIST: goog.getCssName('inputview-checked-menu-list'),
  COMPACT_KEY: goog.getCssName('inputview-compact-key'),
  COMPACT_SWITCHER: goog.getCssName('inputview-compact-switcher'),
  CONTAINER: goog.getCssName('inputview-container'),
  DEFAULT_CONTENT: goog.getCssName('inputview-dc'),
  DIGIT_CHARACTER: goog.getCssName('inputview-digit-character'),
  DOWN_KEY: goog.getCssName('inputview-down-key'),
  ELEMENT_HIGHLIGHT: goog.getCssName('inputview-element-highlight'),
  EMOJI: goog.getCssName('inputview-emoji'),
  EMOJI_BACK: goog.getCssName('inputview-emoji-back'),
  EMOJI_FONT: goog.getCssName('inputview-emoji-font'),
  EMOJI_KEY: goog.getCssName('inputview-emoji-key'),
  EMOJI_KEY_HIGHLIGHT: goog.getCssName('inputview-emoji-key-highlight'),
  EMOJI_SWITCH: goog.getCssName('inputview-emoji-switch'),
  EMOJI_SWITCH_CAR:
      goog.getCssName('inputview-emoji-switch-car'),
  EMOJI_SWITCH_EMOJI:
      goog.getCssName('inputview-emoji-switch-emoji'),
  EMOJI_SWITCH_EMOTICON:
      goog.getCssName('inputview-emoji-switch-emoticon'),
  EMOJI_SWITCH_FAVORITS:
      goog.getCssName('inputview-emoji-switch-favorits'),
  EMOJI_SWITCH_FLOWER:
      goog.getCssName('inputview-emoji-switch-flower'),
  EMOJI_SWITCH_HIGHLIGHT:
      goog.getCssName('inputview-emoji-switch-highlight'),
  EMOJI_SWITCH_RECENT:
      goog.getCssName('inputview-emoji-switch-recent'),
  EMOJI_SWITCH_SPECIAL:
      goog.getCssName('inputview-emoji-switch-special'),
  EMOJI_SWITCH_SYMBOL:
      goog.getCssName('inputview-emoji-switch-symbol'),
  EMOJI_SWITCH_TRIANGLE:
      goog.getCssName('inputview-emoji-switch-triangle'),
  EMOJI_TABBAR_KEY: goog.getCssName('inputview-emoji-tabbar-key'),
  EMOJI_TABBAR_KEY_HIGHLIGHT:
      goog.getCssName('inputview-emoji-tabbar-key-highlight'),
  EMOJI_TABBAR_SK: goog.getCssName('inputview-emoji-tabbar-sk'),
  EMOJI_TEXT: goog.getCssName('inputview-emoji-text'),
  ENTER_ICON: goog.getCssName('inputview-enter-icon'),
  EN_SWITCHER_DEFAULT: goog.getCssName('inputview-en-switcher-default'),
  EN_SWITCHER_ENGLISH: goog.getCssName('inputview-en-switcher-english'),
  EXPAND_CANDIDATES_ICON: goog.getCssName('inputview-expand-candidates-icon'),
  EXTENDED_LAYOUT_TRANSITION: goog.getCssName('inputview-extended-transition'),
  FONT: goog.getCssName('inputview-font'),
  GLOBE_ICON: goog.getCssName('inputview-globe-icon'),
  HANDWRITING: goog.getCssName('inputview-handwriting'),
  HANDWRITING_BACK: goog.getCssName('inputview-handwriting-back'),
  HANDWRITING_LAYOUT: goog.getCssName('inputview-handwriting-layout'),
  HANDWRITING_NETWORK_ERROR:
      goog.getCssName('inputview-handwriting-network-error'),
  HANDWRITING_SWITCHER: goog.getCssName('inputview-handwriting-switcher'),
  HANDWRITING_PRIVACY_COVER:
      goog.getCssName('inputview-handwriting-privacy-cover'),
  HANDWRITING_PRIVACY_INFO:
      goog.getCssName('inputview-handwriting-privacy-info'),
  HANDWRITING_PRIVACY_INFO_HIDDEN:
      goog.getCssName('inputview-handwriting-privacy-info-hidden'),
  HIDE_KEYBOARD_ICON: goog.getCssName('inputview-hide-keyboard-icon'),
  HINT_TEXT: goog.getCssName('inputview-hint-text'),
  HOLD: goog.getCssName('inputview-hold'),
  IME_LIST_CONTAINER: goog.getCssName('inputview-ime-list-container'),
  INDICATOR: goog.getCssName('inputview-indicator'),
  INDICATOR_BACKGROUND: goog.getCssName('inputview-indicator-background'),
  INLINE_DIV: goog.getCssName('inputview-inline-div'),
  JP_IME_SWITCH: goog.getCssName('inputview-jp-ime-switch'),
  KEY_HOLD: goog.getCssName('inputview-key-hold'),
  LAYOUT_VIEW: goog.getCssName('inputview-layoutview'),
  LEFT_KEY: goog.getCssName('inputview-left-key'),
  LINEAR_LAYOUT: goog.getCssName('inputview-linear'),
  LINEAR_LAYOUT_BORDER: goog.getCssName('inputview-linear-border'),
  MENU_LIST_CHECK_MARK: goog.getCssName('inputview-menu-list-check-mark'),
  MENU_FOOTER: goog.getCssName('inputview-menu-footer'),
  MENU_FOOTER_EMOJI_BUTTON:
      goog.getCssName('inputview-menu-footer-emoji-button'),
  MENU_FOOTER_HANDWRITING_BUTTON:
      goog.getCssName('inputview-menu-footer-handwriting-button'),
  MENU_FOOTER_ITEM: goog.getCssName('inputview-menu-footer-item'),
  MENU_FOOTER_SETTING_BUTTON:
      goog.getCssName('inputview-menu-footer-setting-button'),
  MENU_ICON: goog.getCssName('inputview-menu-icon'),
  MENU_LIST_INDICATOR: goog.getCssName('inputview-menu-list-indicator'),
  MENU_LIST_INDICATOR_NAME:
      goog.getCssName('inputview-menu-list-indicator-name'),
  MENU_LIST_ITEM: goog.getCssName('inputview-menu-list-item'),
  MENU_LIST_NAME: goog.getCssName('inputview-menu-list-name'),
  MENU_VIEW: goog.getCssName('inputview-menu-view'),
  MODIFIER: goog.getCssName('inputview-modifier'),
  MODIFIER_ON: goog.getCssName('inputview-modifier-on'),
  MODIFIER_STATE_ICON: goog.getCssName('inputview-modifier-state-icon'),
  PAGE_DOWN_ICON: goog.getCssName('inputview-page-down-icon'),
  PAGE_UP_ICON: goog.getCssName('inputview-page-up-icon'),
  PINYIN: goog.getCssName('inputview-pinyin'),
  REGULAR_SWITCHER: goog.getCssName('inputview-regular-switcher'),
  RIGHT_KEY: goog.getCssName('inputview-right-key'),
  SHIFT_ICON: goog.getCssName('inputview-shift-icon'),
  SHRINK_CANDIDATES_ICON: goog.getCssName('inputview-shrink-candidates-icon'),
  SOFT_KEY: goog.getCssName('inputview-sk'),
  SOFT_KEY_VIEW: goog.getCssName('inputview-skv'),
  SPACE_ICON: goog.getCssName('inputview-space-icon'),
  SPECIAL_KEY_BG: goog.getCssName('inputview-special-key-bg'),
  SPECIAL_KEY_HIGHLIGHT: goog.getCssName('inputview-special-key-highlight'),
  SPECIAL_KEY_NAME: goog.getCssName('inputview-special-key-name'),
  SWITCHER_CHINESE: goog.getCssName('inputview-switcher-chinese'),
  SWITCHER_ENGLISH: goog.getCssName('inputview-switcher-english'),
  TABLE_CELL: goog.getCssName('inputview-table-cell'),
  TAB_ICON: goog.getCssName('inputview-tab-icon'),
  THREE_CANDIDATES: goog.getCssName('inputview-three-candidates'),
  TITLE: goog.getCssName('inputview-title'),
  TITLE_BAR: goog.getCssName('inputview-title-bar'),
  UP_KEY: goog.getCssName('inputview-up-key'),
  VERTICAL_LAYOUT: goog.getCssName('inputview-vertical'),
  VIEW: goog.getCssName('inputview-view'),
  WRAPPER: goog.getCssName('inputview-wrapper')
};

