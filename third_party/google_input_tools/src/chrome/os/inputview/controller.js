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
goog.provide('i18n.input.chrome.inputview.Controller');

goog.require('goog.Disposable');
goog.require('goog.array');
goog.require('goog.async.Delay');
goog.require('goog.dom');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventType');
goog.require('goog.i18n.bidi');
goog.require('goog.object');
goog.require('i18n.input.chrome.DataSource');
goog.require('i18n.input.chrome.inputview.Adapter');
goog.require('i18n.input.chrome.inputview.CandidatesInfo');
goog.require('i18n.input.chrome.inputview.ConditionName');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.KeyboardContainer');
goog.require('i18n.input.chrome.inputview.M17nModel');
goog.require('i18n.input.chrome.inputview.Model');
goog.require('i18n.input.chrome.inputview.PerfTracker');
goog.require('i18n.input.chrome.inputview.ReadyState');
goog.require('i18n.input.chrome.inputview.Settings');
goog.require('i18n.input.chrome.inputview.SizeSpec');
goog.require('i18n.input.chrome.inputview.SoundController');
goog.require('i18n.input.chrome.inputview.SpecNodeName');
goog.require('i18n.input.chrome.inputview.StateType');
goog.require('i18n.input.chrome.inputview.Statistics');
goog.require('i18n.input.chrome.inputview.SwipeDirection');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.content.Candidate');
goog.require('i18n.input.chrome.inputview.elements.content.CandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.ExpandedCandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.MenuView');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.events.KeyCodes');
goog.require('i18n.input.chrome.inputview.handler.PointerHandler');
goog.require('i18n.input.chrome.inputview.util');
goog.require('i18n.input.chrome.message.ContextType');
goog.require('i18n.input.chrome.message.Name');
goog.require('i18n.input.chrome.message.Type');
goog.require('i18n.input.lang.InputToolCode');



goog.scope(function() {
var CandidateType = i18n.input.chrome.inputview.elements.content.Candidate.Type;
var Candidate = i18n.input.chrome.inputview.elements.content.Candidate;
var CandidateView = i18n.input.chrome.inputview.elements.content.CandidateView;
var ConditionName = i18n.input.chrome.inputview.ConditionName;
var ContextType = i18n.input.chrome.message.ContextType;
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var EventType = i18n.input.chrome.inputview.events.EventType;
var ExpandedCandidateView = i18n.input.chrome.inputview.elements.content.
    ExpandedCandidateView;
var InputToolCode = i18n.input.lang.InputToolCode;
var KeyCodes = i18n.input.chrome.inputview.events.KeyCodes;
var MenuView = i18n.input.chrome.inputview.elements.content.MenuView;
var Name = i18n.input.chrome.message.Name;
var PerfTracker = i18n.input.chrome.inputview.PerfTracker;
var SizeSpec = i18n.input.chrome.inputview.SizeSpec;
var SpecNodeName = i18n.input.chrome.inputview.SpecNodeName;
var StateType = i18n.input.chrome.inputview.StateType;
var content = i18n.input.chrome.inputview.elements.content;
var SoundController = i18n.input.chrome.inputview.SoundController;
var Sounds = i18n.input.chrome.inputview.Sounds;
var util = i18n.input.chrome.inputview.util;



/**
 * The controller of the input view keyboard.
 *
 * @param {string} keyset The keyboard keyset.
 * @param {string} languageCode The language code for this keyboard.
 * @param {string} passwordLayout The layout for password box.
 * @param {string} name The input tool name.
 * @constructor
 * @extends {goog.Disposable}
 */
i18n.input.chrome.inputview.Controller = function(keyset, languageCode,
    passwordLayout, name) {
  /**
   * The model.
   *
   * @type {!i18n.input.chrome.inputview.Model}
   * @private
   */
  this.model_ = new i18n.input.chrome.inputview.Model();

  /** @private {!i18n.input.chrome.inputview.PerfTracker} */
  this.perfTracker_ = new i18n.input.chrome.inputview.PerfTracker();

  /**
   * The layout map.
   *
   * @type {!Object.<string, !Object>}
   * @private
   */
  this.layoutDataMap_ = {};

  /**
   * The keyset data map.
   *
   * @type {!Object.<string, !Object>}
   * @private
   */
  this.keysetDataMap_ = {};

  /**
   * The event handler.
   *
   * @type {!goog.events.EventHandler}
   * @private
   */
  this.handler_ = new goog.events.EventHandler(this);

  /**
   * The m17n model.
   *
   * @type {!i18n.input.chrome.inputview.M17nModel}
   * @private
   */
  this.m17nModel_ = new i18n.input.chrome.inputview.M17nModel();

  /**
   * The pointer handler.
   *
   * @type {!i18n.input.chrome.inputview.handler.PointerHandler}
   * @private
   */
  this.pointerHandler_ = new i18n.input.chrome.inputview.handler.
      PointerHandler();

  /**
   * The statistics object for recording metrics values.
   *
   * @type {!i18n.input.chrome.inputview.Statistics}
   * @private
   */
  this.statistics_ = i18n.input.chrome.inputview.Statistics.getInstance();

  /** @private {!i18n.input.chrome.inputview.ReadyState} */
  this.readyState_ = new i18n.input.chrome.inputview.ReadyState();

  /** @private {!i18n.input.chrome.inputview.Adapter} */
  this.adapter_ = new i18n.input.chrome.inputview.Adapter(this.readyState_);

  /** @private {!i18n.input.chrome.inputview.KeyboardContainer} */
  this.container_ = new i18n.input.chrome.inputview.KeyboardContainer(
      this.adapter_);
  this.container_.render();

  /** @private {!i18n.input.chrome.inputview.SoundController} */
  this.soundController_ = new SoundController(false);

  /**
   * The context type and keyset mapping group by input method id.
   * key: input method id.
   * value: Object
   *    key: context type string.
   *    value: keyset string.
   *
   * @private {!Object.<string, !Object.<string, string>>}
   */
  this.contextTypeToKeysetMap_ = {};

  this.initialize(keyset, languageCode, passwordLayout, name);
  /**
   * The suggestions.
   * Note: sets a default empty result to avoid null check.
   *
   * @private {!i18n.input.chrome.inputview.CandidatesInfo}
   */
  this.candidatesInfo_ = i18n.input.chrome.inputview.CandidatesInfo.getEmpty();

  this.registerEventHandler_();
};
goog.inherits(i18n.input.chrome.inputview.Controller,
    goog.Disposable);
var Controller = i18n.input.chrome.inputview.Controller;


/**
 * @define {boolean} Flag to disable handwriting.
 */
Controller.DISABLE_HWT = false;


/**
 * A flag to indicate whether the shift is for auto capital.
 *
 * @private {boolean}
 */
Controller.prototype.shiftForAutoCapital_ = false;


/**
 * @define {boolean} Flag to indicate whether it is debugging.
 */
Controller.DEV = false;


/**
 * The handwriting view code, use the code can switch handwriting panel view.
 *
 * @const {string}
 * @private
 */
Controller.HANDWRITING_VIEW_CODE_ = 'hwt';


/**
 * The emoji view code, use the code can switch to emoji.
 *
 * @const {string}
 * @private
 */
Controller.EMOJI_VIEW_CODE_ = 'emoji';


/**
 * The limitation for backspace repeat time to avoid unexpected
 * problem that backspace is held all the time.
 *
 * @private {number}
 */
Controller.BACKSPACE_REPEAT_LIMIT_ = 255;


/**
 * The repeated times of the backspace.
 *
 * @private {number}
 */
Controller.prototype.backspaceRepeated_ = 0;


/**
 * The handwriting input tool code suffix.
 *
 * @const {string}
 * @private
 */
Controller.HANDWRITING_CODE_SUFFIX_ = '-t-i0-handwrit';


/**
 * True if the settings is loaded.
 *
 * @type {boolean}
 */
Controller.prototype.isSettingReady = false;


/**
 * True if the keyboard is set up.
 * Note: This flag is only used for automation testing.
 *
 * @type {boolean}
 */
Controller.prototype.isKeyboardReady = false;


/**
 * The auto repeat timer for backspace hold.
 *
 * @type {goog.async.Delay}
 * @private
 */
Controller.prototype.backspaceAutoRepeat_;


/**
 * The active keyboard code.
 *
 * @type {string}
 * @private
 */
Controller.prototype.currentKeyset_ = '';


/**
 * The current input method id.
 *
 * @private {string}
 */
Controller.prototype.currentInputmethod_ = '';


/**
 * The operations on candidates.
 *
 * @enum {number}
 */
Controller.CandidatesOperation = {
  NONE: 0,
  EXPAND: 1,
  SHRINK: 2
};


/**
 * The active language code.
 *
 * @type {string}
 * @private
 */
Controller.prototype.lang_;


/**
 * The password keyset.
 *
 * @private {string}
 */
Controller.prototype.passwordKeyset_ = '';


/**
 * The soft key map, because key configuration is loaded before layout,
 * controller needs this varaible to save it and hook into keyboard view.
 *
 * @type {!Array.<!content.SoftKey>}
 * @private
 */
Controller.prototype.softKeyList_;


/**
 * The mapping from soft key id to soft key view id.
 *
 * @type {!Object.<string, string>}
 * @private
 */
Controller.prototype.mapping_;


/**
 * The dead key.
 *
 * @type {string}
 * @private
 */
Controller.prototype.deadKey_ = '';


/**
 * The input tool name.
 *
 * @type {string}
 * @private
 */
Controller.prototype.title_;


/**
 * Registers event handlers.
 * @private
 */
Controller.prototype.registerEventHandler_ = function() {
  this.handler_.
      listen(this.model_,
          EventType.LAYOUT_LOADED,
          this.onLayoutLoaded_).
      listen(this.model_,
          EventType.CONFIG_LOADED,
          this.onConfigLoaded_).
      listen(this.m17nModel_,
          EventType.CONFIG_LOADED,
          this.onConfigLoaded_).
      listen(this.pointerHandler_, [
            EventType.LONG_PRESS,
            EventType.CLICK,
            EventType.DOUBLE_CLICK,
            EventType.DOUBLE_CLICK_END,
            EventType.POINTER_UP,
            EventType.POINTER_DOWN,
            EventType.POINTER_OVER,
            EventType.POINTER_OUT,
            EventType.SWIPE
          ], this.onPointerEvent_).
      listen(window, goog.events.EventType.RESIZE, this.resize).
      listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType.
              SURROUNDING_TEXT_CHANGED,
          this.onSurroundingTextChanged_).
      listen(this.adapter_,
          i18n.input.chrome.DataSource.EventType.CANDIDATES_BACK,
          this.onCandidatesBack_).
      listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType.CONTEXT_FOCUS,
          this.onContextFocus_).
      listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType.CONTEXT_BLUR,
          this.onContextBlur_).
      listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType.VISIBILITY_CHANGE,
          this.onVisibilityChange_).
      listen(this.adapter_,
          i18n.input.chrome.inputview.events.EventType.SETTINGS_READY,
          this.onSettingsReady_).
      listen(this.adapter_,
          i18n.input.chrome.message.Type.UPDATE_SETTINGS,
          this.onUpdateSettings_);
};


/**
 * Callback for updating settings.
 *
 * @param {!i18n.input.chrome.message.Event} e .
 * @private
 */
Controller.prototype.onUpdateSettings_ = function(e) {
  var settings = this.model_.settings;
  if (goog.isDef(e.msg['autoSpace'])) {
    settings.autoSpace = e.msg['autoSpace'];
  }
  if (goog.isDef(e.msg['autoCapital'])) {
    settings.autoCapital = e.msg['autoCapital'];
  }
  if (goog.isDef(e.msg['candidatesNavigation'])) {
    settings.candidatesNavigation = e.msg['candidatesNavigation'];
  }
  if (goog.isDef(e.msg['supportCompact'])) {
    settings.supportCompact = e.msg['supportCompact'];
  }
  if (goog.isDef(e.msg[Name.KEYSET])) {
    this.contextTypeToKeysetMap_[this.currentInputMethod_][
        ContextType.DEFAULT] = e.msg[Name.KEYSET];
  }
  if (goog.isDef(e.msg['enableLongPress'])) {
    settings.enableLongPress = e.msg['enableLongPress'];
  }
  if (goog.isDef(e.msg['doubleSpacePeriod'])) {
    settings.doubleSpacePeriod = e.msg['doubleSpacePeriod'];
  }
  if (goog.isDef(e.msg['soundOnKeypress'])) {
    settings.soundOnKeypress = e.msg['soundOnKeypress'];
    this.soundController_.setEnabled(settings.soundOnKeypress);
  }
  this.perfTracker_.tick(PerfTracker.TickName.BACKGROUND_SETTINGS_FETCHED);
  var isPassword = this.adapter_.isPasswordBox();
  this.switchToKeySet(this.getActiveKeyset_());
};


/**
 * Callback for setting ready.
 *
 * @private
 */
Controller.prototype.onSettingsReady_ = function() {
  if (this.isSettingReady) {
    return;
  }

  this.isSettingReady = true;
  var keysetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  if (this.adapter_.isA11yMode) {
    keysetMap[ContextType.PASSWORD] = keysetMap[ContextType.DEFAULT] =
        util.getConfigName(keysetMap[ContextType.DEFAULT]);
  } else {
    var preferredKeyset = /** @type {string} */ (this.model_.settings.
        getPreference(util.getConfigName(
            keysetMap[ContextType.DEFAULT])));
    if (preferredKeyset) {
      keysetMap[ContextType.PASSWORD] = keysetMap[ContextType.DEFAULT] =
          preferredKeyset;
    }
  }
  this.maybeCreateViews_();
  this.switchToKeySet(this.getActiveKeyset_());
};


/**
 * Gets the data for spatial module.
 *
 * @param {!content.SoftKey} key .
 * @param {number} x The x-offset of the touch point.
 * @param {number} y The y-offset of the touch point.
 * @return {!Object} .
 * @private
 */
Controller.prototype.getSpatialData_ = function(key, x, y) {
  var items = [];
  items.push([this.getKeyContent_(key), key.estimator.estimateInLogSpace(x, y)
      ]);
  for (var i = 0; i < key.nearbyKeys.length; i++) {
    var nearByKey = key.nearbyKeys[i];
    var content = this.getKeyContent_(nearByKey);
    if (content && util.REGEX_LANGUAGE_MODEL_CHARACTERS.test(content)) {
      items.push([content, nearByKey.estimator.estimateInLogSpace(x, y)]);
    }
  }
  goog.array.sort(items, function(item1, item2) {
    return item1[1] - item2[1];
  });
  var sources = items.map(function(item) {
    return item[0].toLowerCase();
  });
  var possibilities = items.map(function(item) {
    return item[1];
  });
  return {
    'sources': sources,
    'possibilities': possibilities
  };
};


/**
 * Gets the key content.
 *
 * @param {!content.SoftKey} key .
 * @return {string} .
 * @private
 */
Controller.prototype.getKeyContent_ = function(key) {
  if (key.type == i18n.input.chrome.inputview.elements.ElementType.
      CHARACTER_KEY) {
    key = /** @type {!content.CharacterKey} */ (key);
    return key.getActiveCharacter();
  }
  if (key.type == i18n.input.chrome.inputview.elements.ElementType.
      COMPACT_KEY) {
    key = /** @type {!content.FunctionalKey} */ (key);
    return key.text;
  }
  return '';
};


/**
 * Callback for pointer event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.onPointerEvent_ = function(e) {
  if ((this.adapter_.isChromeVoxOn || !this.model_.settings.enableLongPress) &&
      e.type == EventType.LONG_PRESS) {
    return;
  }

  if (e.view) {
    this.handlePointerAction_(e.view, e);
  } else {
    var tabbableKeysets = [
      Controller.HANDWRITING_VIEW_CODE_,
      Controller.EMOJI_VIEW_CODE_];
    if (goog.array.contains(tabbableKeysets, this.currentKeyset_)) {
      this.resetAll_();
      this.switchToKeySet(this.container_.currentKeysetView.fromKeyset);
    }
  }
};


/**
 * Handles the swipe action.
 *
 * @param {!i18n.input.chrome.inputview.elements.Element} view The view, for
 *     swipe event, the view would be the soft key which starts the swipe.
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
Controller.prototype.handleSwipeAction_ = function(view, e) {
  var direction = e.direction;
  if (this.container_.altDataView.isVisible()) {
    this.container_.altDataView.highlightItem(e.x, e.y);
    return;
  }

  if (view.type == ElementType.CHARACTER_KEY) {
    view = /** @type {!content.CharacterKey} */ (view);
    if (direction & i18n.input.chrome.inputview.SwipeDirection.UP ||
        direction & i18n.input.chrome.inputview.SwipeDirection.DOWN) {
      var ch = view.getCharacterByGesture(!!(direction &
          i18n.input.chrome.inputview.SwipeDirection.UP));
      if (ch) {
        view.flickerredCharacter = ch;
      }
    }
  }

  if (view.type == ElementType.COMPACT_KEY) {
    view = /** @type {!content.CompactKey} */ (view);
    if ((direction & i18n.input.chrome.inputview.SwipeDirection.UP) &&
        view.hintText) {
      view.flickerredCharacter = view.hintText;
    }
  }
};


/**
 * Execute a command.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.MenuView.Command}
 *     command The command that about to be executed.
 * @param {string=} opt_arg The optional command argument.
 * @private
 */
Controller.prototype.executeCommand_ = function(command, opt_arg) {
  var CommandEnum = MenuView.Command;
  switch (command) {
    case CommandEnum.SWITCH_IME:
      var inputMethodId = opt_arg;
      if (inputMethodId) {
        this.adapter_.switchToInputMethod(inputMethodId);
      }
      break;

    case CommandEnum.SWITCH_KEYSET:
      var keyset = opt_arg;
      if (keyset) {
        this.statistics_.recordSwitch(keyset);
        this.switchToKeySet(keyset);
      }
      break;
    case CommandEnum.OPEN_EMOJI:
      this.switchToKeySet(Controller.EMOJI_VIEW_CODE_);
      break;

    case CommandEnum.OPEN_HANDWRITING:
      // TODO: remember handwriting keyset.
      this.switchToKeySet(Controller.HANDWRITING_VIEW_CODE_);
      break;

    case CommandEnum.OPEN_SETTING:
      if (window.inputview) {
        inputview.openSettings();
      }
      break;
  }
};


/**
 * Handles the pointer action.
 *
 * @param {!i18n.input.chrome.inputview.elements.Element} view The view.
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.handlePointerAction_ = function(view, e) {
  if (e.type == i18n.input.chrome.inputview.events.EventType.SWIPE) {
    e = /** @type {!i18n.input.chrome.inputview.events.SwipeEvent} */ (e);
    this.handleSwipeAction_(view, e);
  }
  switch (view.type) {
    case ElementType.BACK_BUTTON:
      if (e.type == EventType.POINTER_UP) {
        this.switchToKeySet(this.container_.currentKeysetView.fromKeyset);
      }
      return;
    case ElementType.EXPAND_CANDIDATES:
      if (e.type == EventType.POINTER_UP) {
        this.showCandidates_(this.candidatesInfo_.source,
            this.candidatesInfo_.candidates,
            Controller.CandidatesOperation.EXPAND);
      }
      return;
    case ElementType.SHRINK_CANDIDATES:
      if (e.type == EventType.POINTER_UP) {
        this.showCandidates_(this.candidatesInfo_.source,
            this.candidatesInfo_.candidates,
            Controller.CandidatesOperation.SHRINK);
      }
      return;
    case ElementType.CANDIDATE:
      view = /** @type {!Candidate} */ (view);
      if (e.type == EventType.POINTER_UP) {
        if (view.candidateType == CandidateType.CANDIDATE) {
          this.adapter_.selectCandidate(view.candidate);
        } else if (view.candidateType == CandidateType.NUMBER) {
          this.adapter_.commitText(view.candidate[Name.CANDIDATE]);
        }
        this.container_.cleanStroke();
      }
      if (e.type == EventType.POINTER_OUT || e.type == EventType.POINTER_UP) {
        view.setHighlighted(false);
      } else if (e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER) {
        view.setHighlighted(true);
      }
      return;

    case ElementType.ALTDATA_VIEW:
      view = /** @type {!content.AltDataView} */ (view);
      if (e.type == EventType.POINTER_DOWN &&
          e.target == view.getCoverElement()) {
        view.hide();
      } else if (e.type == EventType.POINTER_UP) {
        var ch = view.getHighlightedCharacter();
        this.adapter_.sendKeyDownAndUpEvent(ch, view.triggeredBy.id,
            view.triggeredBy.keyCode,
            {'sources': [ch.toLowerCase()], 'possibilities': [1]});
        view.hide();
        this.clearUnstickyState_();
      }
      return;

    case ElementType.MENU_ITEM:
      view = /** @type {!content.MenuItem} */ (view);
      if (e.type == EventType.CLICK) {
        this.resetAll_();
        this.executeCommand_.apply(this, view.getCommand());
        this.container_.menuView.hide();
      }
      view.setHighlighted(e.type == EventType.POINTER_DOWN ||
          e.type == EventType.POINTER_OVER);
      // TODO: Add chrome vox support.
      return;

    case ElementType.MENU_VIEW:
      view = /** @type {!MenuView} */ (view);

      if (e.type == EventType.POINTER_DOWN &&
          e.target == view.getCoverElement()) {
        view.hide();
      }
      return;

    case ElementType.EMOJI_KEY:
      if (e.type == EventType.POINTER_UP) {
        if (!this.container_.currentKeysetView.isDragging && view.text != '') {
          this.adapter_.commitText(view.text);
        }
      }
      return;

    case ElementType.HWT_PRIVACY_GOT_IT:
      this.adapter_.sendHwtPrivacyConfirmMessage();
      return;

    case ElementType.SOFT_KEY_VIEW:
      // Delegates the events on the soft key view to its soft key.
      view = /** @type {!i18n.input.chrome.inputview.elements.layout.
          SoftKeyView} */ (view);
      if (!view.softKey) {
        return;
      }
      view = view.softKey;
  }

  if (view.type != ElementType.MODIFIER_KEY &&
      !this.container_.altDataView.isVisible() &&
      !this.container_.menuView.isVisible()) {
    // The highlight of the modifier key is depending on the state instead
    // of the key down or up.
    if (e.type == EventType.POINTER_OVER || e.type == EventType.POINTER_DOWN ||
        e.type == EventType.DOUBLE_CLICK) {
      view.setHighlighted(true);
    } else if (e.type == EventType.POINTER_OUT ||
        e.type == EventType.POINTER_UP ||
        e.type == EventType.DOUBLE_CLICK_END) {
      view.setHighlighted(false);
    }
  }
  this.handlePointerEventForSoftKey_(
      /** @type {!content.SoftKey} */ (view), e);
  this.updateContextModifierState_();
};


/**
 * Handles softkey of the pointer action.
 *
 * @param {!content.SoftKey} softKey .
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
Controller.prototype.handlePointerEventForSoftKey_ = function(softKey, e) {
  var key;
  switch (softKey.type) {
    case ElementType.CANDIDATES_PAGE_UP:
      if (e.type == EventType.POINTER_UP) {
        this.container_.expandedCandidateView.pageUp();
      }
      break;
    case ElementType.CANDIDATES_PAGE_DOWN:
      if (e.type == EventType.POINTER_UP) {
        this.container_.expandedCandidateView.pageDown();
      }
      break;
    case ElementType.CHARACTER_KEY:
      key = /** @type {!content.CharacterKey} */ (softKey);
      if (e.type == EventType.LONG_PRESS) {
        this.container_.altDataView.show(
            key, goog.i18n.bidi.isRtlLanguage(this.languageCode_));
      } else if (e.type == EventType.POINTER_UP) {
        this.model_.stateManager.triggerChording();
        var ch = key.getActiveCharacter();
        this.adapter_.sendKeyDownAndUpEvent(ch, key.id, key.keyCode,
            this.getSpatialData_(key, e.x, e.y));
        this.clearUnstickyState_();
        key.flickerredCharacter = '';
      }
      break;

    case ElementType.MODIFIER_KEY:
      key = /** @type {!content.ModifierKey} */ (softKey);
      var isStateEnabled = this.model_.stateManager.hasState(key.toState);
      var isChording = this.model_.stateManager.isChording(key.toState);
      if (e.type == EventType.POINTER_DOWN) {
        this.changeState_(key.toState, !isStateEnabled, true);
        this.model_.stateManager.setKeyDown(key.toState, true);
      } else if (e.type == EventType.POINTER_UP || e.type == EventType.
          POINTER_OUT) {
        if (isChording) {
          this.changeState_(key.toState, false, false);
        } else if (key.toState != StateType.CAPSLOCK &&
            this.model_.stateManager.isKeyDown(key.toState)) {
          this.changeState_(key.toState, isStateEnabled, false);
        }
        this.model_.stateManager.setKeyDown(key.toState, false);
      } else if (e.type == EventType.DOUBLE_CLICK) {
        this.changeState_(key.toState, isStateEnabled, true);
      } else if (e.type == EventType.LONG_PRESS) {
        if (!isChording) {
          this.changeState_(key.toState, true, true);
          this.model_.stateManager.setKeyDown(key.toState, false);
        }
      }
      break;

    case ElementType.BACKSPACE_KEY:
      key = /** @type {!content.FunctionalKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        this.backspaceTick_();
      } else if (e.type == EventType.POINTER_UP || e.type == EventType.
          POINTER_OUT) {
        this.stopBackspaceAutoRepeat_();
        this.adapter_.sendKeyUpEvent('\u0008', KeyCodes.BACKSPACE);
      }
      break;

    case ElementType.TAB_KEY:
      key = /** @type {!content.FunctionalKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('\u0009', KeyCodes.TAB);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('\u0009', KeyCodes.TAB);
      }
      break;

    case ElementType.ENTER_KEY:
      key = /** @type {!content.FunctionalKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('\u000D', KeyCodes.ENTER);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('\u000D', KeyCodes.ENTER);
      }
      break;

    case ElementType.ARROW_UP:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_UP);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_UP);
      }
      break;

    case ElementType.ARROW_DOWN:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_DOWN);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_DOWN);
      }
      break;

    case ElementType.ARROW_LEFT:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_LEFT);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_LEFT);
      }
      break;

    case ElementType.ARROW_RIGHT:
      if (e.type == EventType.POINTER_DOWN) {
        this.adapter_.sendKeyDownEvent('', KeyCodes.ARROW_RIGHT);
      } else if (e.type == EventType.POINTER_UP) {
        this.adapter_.sendKeyUpEvent('', KeyCodes.ARROW_RIGHT);
      }
      break;
    case ElementType.EN_SWITCHER:
      if (e.type == EventType.POINTER_UP) {
        key = /** @type {!content.EnSwitcherKey} */ (softKey);
        this.adapter_.toggleLanguageState(this.model_.stateManager.isEnMode);
        this.model_.stateManager.isEnMode = !this.model_.stateManager.isEnMode;
        key.update();
      }
      break;
    case ElementType.SPACE_KEY:
      key = /** @type {!content.SpaceKey} */ (softKey);
      var doubleSpacePeriod = this.model_.settings.doubleSpacePeriod;
      if (e.type == EventType.POINTER_UP || (!doubleSpacePeriod && e.type ==
          EventType.DOUBLE_CLICK_END)) {
        this.adapter_.sendKeyDownAndUpEvent(key.getCharacter(),
            KeyCodes.SPACE);
        this.clearUnstickyState_();
      } else if (e.type == EventType.DOUBLE_CLICK && doubleSpacePeriod) {
        this.adapter_.doubleClickOnSpaceKey();
      }
      break;

    case ElementType.SWITCHER_KEY:
      key = /** @type {!content.SwitcherKey} */ (softKey);
      if (e.type == EventType.POINTER_UP) {
        this.statistics_.recordSwitch(key.toKeyset);
        if (this.isSubKeyset_(key.toKeyset, this.currentKeyset_)) {
          this.model_.stateManager.reset();
          this.container_.update();
          this.updateContextModifierState_();
          this.container_.menuView.hide();
        } else {
          this.resetAll_();
        }
        // Switch to the specific keyboard.
        this.switchToKeySet(key.toKeyset);
        if (key.record) {
          this.model_.settings.savePreference(
              util.getConfigName(key.toKeyset),
              key.toKeyset);
        }
      }
      break;

    case ElementType.COMPACT_KEY:
      key = /** @type {!content.CompactKey} */ (softKey);
      if (e.type == EventType.LONG_PRESS) {
        this.container_.altDataView.show(
            key, goog.i18n.bidi.isRtlLanguage(this.languageCode_));
      } else if (e.type == EventType.POINTER_UP) {
        this.model_.stateManager.triggerChording();
        this.adapter_.sendKeyDownAndUpEvent(key.getActiveCharacter(), '', 0,
            this.getSpatialData_(key, e.x, e.y));
        this.clearUnstickyState_();
        key.flickerredCharacter = '';
      }
      break;

    case ElementType.HIDE_KEYBOARD_KEY:
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.hideKeyboard();
      }
      break;

    case ElementType.MENU_KEY:
      key = /** @type {!content.MenuKey} */ (softKey);
      if (e.type == EventType.POINTER_DOWN) {
        var isCompact = this.currentKeyset_.indexOf('compact') != -1;
        var remappedKeyset = this.getRemappedKeyset_(this.currentKeyset_);
        var keysetData = this.keysetDataMap_[remappedKeyset];
        var enableCompact = !this.adapter_.isA11yMode &&
            !!keysetData[SpecNodeName.HAS_COMPACT_KEYBOARD] &&
            this.model_.settings.supportCompact;
        var self = this;
        var currentKeyset = this.currentKeyset_;
        var hasHwt = !this.adapter_.isPasswordBox() &&
            !Controller.DISABLE_HWT && goog.object.contains(
            InputToolCode, this.getHwtInputToolCode_());
        var enableSettings = this.shouldEnableSettings() &&
            window.inputview && inputview.openSettings;
        this.adapter_.getInputMethods(function(inputMethods) {
          this.container_.menuView.show(key, currentKeyset, isCompact,
              enableCompact, this.currentInputMethod_, inputMethods, hasHwt,
              enableSettings, this.adapter_.isExperimental);
        }.bind(this));
      }
      break;

    case ElementType.GLOBE_KEY:
      if (e.type == EventType.POINTER_UP) {
        this.adapter_.clearModifierStates();
        this.adapter_.setModifierState(
            i18n.input.chrome.inputview.StateType.CTRL, true);
        this.adapter_.sendKeyDownAndUpEvent(' ', KeyCodes.SPACE, 0x20);
        this.adapter_.setModifierState(
            i18n.input.chrome.inputview.StateType.CTRL, false);
      }
      break;
    case ElementType.IME_SWITCH:
      key = /** @type {!content.FunctionalKey} */ (softKey);
      this.adapter_.sendKeyDownAndUpEvent('', key.id);
      break;
  }
  // Play key sound on pointer up.
  if (e.type == EventType.POINTER_UP)
    this.soundController_.onKeyUp(softKey.type);
};


/**
 * Clears unsticky state.
 *
 * @private
 */
Controller.prototype.clearUnstickyState_ = function() {
  if (this.model_.stateManager.hasUnStickyState()) {
    for (var key in StateType) {
      var value = StateType[key];
      if (this.model_.stateManager.hasState(value) &&
          !this.model_.stateManager.isSticky(value)) {
        this.changeState_(value, false, false);
      }
    }
  }
  this.container_.update();
};


/**
 * Stops the auto-repeat for backspace.
 *
 * @private
 */
Controller.prototype.stopBackspaceAutoRepeat_ = function() {
  goog.dispose(this.backspaceAutoRepeat_);
  this.backspaceAutoRepeat_ = null;
  this.adapter_.sendKeyUpEvent('\u0008', KeyCodes.BACKSPACE);
  this.backspaceRepeated_ = 0;
};


/**
 * The tick for the backspace key.
 *
 * @private
 */
Controller.prototype.backspaceTick_ = function() {
  if (this.backspaceRepeated_ >= Controller.BACKSPACE_REPEAT_LIMIT_) {
    this.stopBackspaceAutoRepeat_();
    return;
  }
  this.backspaceRepeated_++;
  this.backspaceDown_();
  this.soundController_.onKeyRepeat(ElementType.BACKSPACE_KEY);

  if (this.backspaceAutoRepeat_) {
    this.backspaceAutoRepeat_.start(75);
  } else {
    this.backspaceAutoRepeat_ = new goog.async.Delay(
        goog.bind(this.backspaceTick_, this), 300);
    this.backspaceAutoRepeat_.start();
  }
};


/**
 * Callback for VISIBILITY_CHANGE.
 *
 * @private
 */
Controller.prototype.onVisibilityChange_ = function() {
  if (!this.adapter_.isVisible) {
    this.resetAll_();
  }
};


/**
 * Resets the whole keyboard include clearing candidates,
 * reset modifier state, etc.
 *
 * @private
 */
Controller.prototype.resetAll_ = function() {
  this.clearCandidates_();
  this.container_.candidateView.hideNumberRow();
  this.model_.stateManager.reset();
  this.container_.update();
  this.updateContextModifierState_();
  this.deadKey_ = '';
  this.resize();
  this.container_.expandedCandidateView.close();
  this.container_.menuView.hide();
};


/**
 * Callback when the context is changed.
 *
 * @private
 */
Controller.prototype.onContextFocus_ = function() {
  this.resetAll_();
  this.switchToKeySet(this.getActiveKeyset_());
};


/**
 * Callback when surrounding text is changed.
 *
 * @param {!i18n.input.chrome.inputview.events.SurroundingTextChangedEvent} e .
 * @private
 */
Controller.prototype.onSurroundingTextChanged_ = function(e) {
  if (!this.model_.settings.autoCapital || !e.text) {
    return;
  }

  var isShiftEnabled = this.model_.stateManager.hasState(StateType.SHIFT);
  var needAutoCap = this.model_.settings.autoCapital &&
      util.needAutoCap(e.text);
  if (needAutoCap && !isShiftEnabled) {
    this.changeState_(StateType.SHIFT, true, false);
    this.shiftForAutoCapital_ = true;
  } else if (!needAutoCap && this.shiftForAutoCapital_) {
    this.changeState_(StateType.SHIFT, false, false);
  }
};


/**
 * Callback for Context blurs.
 *
 * @private
 */
Controller.prototype.onContextBlur_ = function() {
  this.clearCandidates_();
  this.deadKey_ = '';
  this.container_.menuView.hide();
};


/**
 * Backspace key is down.
 *
 * @private
 */
Controller.prototype.backspaceDown_ = function() {
  if (this.container_.hasStrokesOnCanvas()) {
    this.clearCandidates_();
    this.container_.cleanStroke();
  } else {
    this.adapter_.sendKeyDownEvent('\u0008', KeyCodes.BACKSPACE);
  }
};


/**
 * Callback for state change.
 *
 * @param {StateType} stateType The state type.
 * @param {boolean} enable True to enable the state.
 * @param {boolean} isSticky True to make the state sticky.
 * @private
 */
Controller.prototype.changeState_ = function(stateType, enable, isSticky) {
  if (stateType == StateType.ALTGR) {
    var code = KeyCodes.ALT_RIGHT;
    if (enable) {
      this.adapter_.sendKeyDownEvent('', code);
    } else {
      this.adapter_.sendKeyUpEvent('', code);
    }
  }
  if (stateType == StateType.SHIFT) {
    this.shiftForAutoCapital_ = false;
  }
  var isEnabledBefore = this.model_.stateManager.hasState(stateType);
  var isStickyBefore = this.model_.stateManager.isSticky(stateType);
  this.model_.stateManager.setState(stateType, enable);
  this.model_.stateManager.setSticky(stateType, isSticky);
  if (isEnabledBefore != enable || isStickyBefore != isSticky) {
    this.container_.update();
  }
};


/**
 * Updates the modifier state for context.
 *
 * @private
 */
Controller.prototype.updateContextModifierState_ = function() {
  var stateManager = this.model_.stateManager;
  this.adapter_.setModifierState(StateType.ALT,
      stateManager.hasState(StateType.ALT));
  this.adapter_.setModifierState(StateType.CTRL,
      stateManager.hasState(StateType.CTRL));
  this.adapter_.setModifierState(StateType.CAPSLOCK,
      stateManager.hasState(StateType.CAPSLOCK));
  if (!this.shiftForAutoCapital_) {
    // If shift key is automatically on because of feature - autoCapital,
    // Don't set modifier state to adapter.
    this.adapter_.setModifierState(StateType.SHIFT,
        stateManager.hasState(StateType.SHIFT));
  }
};


/**
 * Callback for AUTO-COMPLETE event.
 *
 * @param {!i18n.input.chrome.DataSource.CandidatesBackEvent} e .
 * @private
 */
Controller.prototype.onCandidatesBack_ = function(e) {
  this.candidatesInfo_ = new i18n.input.chrome.inputview.CandidatesInfo(
      e.source, e.candidates);
  this.showCandidates_(e.source, e.candidates, Controller.CandidatesOperation.
      NONE);
};


/**
 * Shows the candidates to the candidate view.
 *
 * @param {string} source The source text.
 * @param {!Array.<!Object>} candidates The candidate text list.
 * @param {Controller.CandidatesOperation} operation .
 * @private
 */
Controller.prototype.showCandidates_ = function(source, candidates,
    operation) {
  var state = !!source ? ExpandedCandidateView.State.COMPLETION_CORRECTION :
      ExpandedCandidateView.State.PREDICTION;
  var expandView = this.container_.expandedCandidateView;
  var expand = false;
  if (operation == Controller.CandidatesOperation.NONE) {
    expand = expandView.state == state;
  } else {
    expand = operation == Controller.CandidatesOperation.EXPAND;
  }

  if (candidates.length == 0) {
    this.clearCandidates_();
    expandView.state = ExpandedCandidateView.State.NONE;
    return;
  }

  // The compact pinyin needs full candidates instead of three candidates.
  var isThreeCandidates = this.currentKeyset_.indexOf('compact') != -1 &&
      this.currentKeyset_.indexOf('pinyin-zh-CN') == -1;
  if (isThreeCandidates) {
    if (candidates.length > 1) {
      // Swap the first candidate and the second candidate.
      var tmp = candidates[0];
      candidates[0] = candidates[1];
      candidates[1] = tmp;
    }
  }
  var isHwt = Controller.HANDWRITING_VIEW_CODE_ == this.currentKeyset_;
  this.container_.candidateView.showCandidates(candidates, isThreeCandidates,
      this.model_.settings.candidatesNavigation && !isHwt);

  if (expand) {
    expandView.state = state;
    this.container_.currentKeysetView.setVisible(false);
    expandView.showCandidates(candidates,
        this.container_.candidateView.candidateCount);
    this.container_.candidateView.switchToIcon(CandidateView.IconType.
        SHRINK_CANDIDATES, true);
  } else {
    expandView.state = ExpandedCandidateView.State.NONE;
    expandView.setVisible(false);
    this.container_.currentKeysetView.setVisible(true);
  }
};


/**
 * Clears candidates.
 *
 * @private
 */
Controller.prototype.clearCandidates_ = function() {
  this.candidatesInfo_ = i18n.input.chrome.inputview.CandidatesInfo.getEmpty();
  this.container_.candidateView.clearCandidates();
  this.container_.expandedCandidateView.close();
  this.container_.expandedCandidateView.state = ExpandedCandidateView.State.
      NONE;
  if (this.container_.currentKeysetView) {
    this.container_.currentKeysetView.setVisible(true);
  }
  this.container_.candidateView.switchToIcon(CandidateView.IconType.BACK,
      Controller.HANDWRITING_VIEW_CODE_ == this.currentKeyset_);
};


/**
 * Callback when the layout is loaded.
 *
 * @param {!i18n.input.chrome.inputview.events.LayoutLoadedEvent} e The event.
 * @private
 */
Controller.prototype.onLayoutLoaded_ = function(e) {
  var layoutID = e.data['layoutID'];
  this.layoutDataMap_[layoutID] = e.data;
  this.perfTracker_.tick(PerfTracker.TickName.LAYOUT_LOADED);
  this.maybeCreateViews_();
};


/**
 * Creates the whole view.
 *
 * @private
 */
Controller.prototype.maybeCreateViews_ = function() {
  if (!this.isSettingReady) {
    return;
  }

  for (var keyset in this.keysetDataMap_) {
    var keysetData = this.keysetDataMap_[keyset];
    var layoutId = keysetData[SpecNodeName.LAYOUT];
    var layoutData = this.layoutDataMap_[layoutId];
    if (!this.container_.keysetViewMap[keyset] && layoutData) {
      var conditions = {};
      conditions[ConditionName.SHOW_ALTGR] =
          keysetData[SpecNodeName.HAS_ALTGR_KEY];

      conditions[ConditionName.SHOW_MENU] =
          keysetData[SpecNodeName.SHOW_MENU_KEY];
      // In symbol and more keysets, we want to show a symbol key in the globe
      // SoftKeyView. So this view should alway visible in the two keysets.
      // Currently, SHOW_MENU_KEY is false for the two keysets, so we use
      // !keysetData[SpecNodeName.SHOW_MENU_KEY] here.
      conditions[ConditionName.SHOW_GLOBE_OR_SYMBOL] =
          !keysetData[SpecNodeName.SHOW_MENU_KEY] ||
          this.adapter_.showGlobeKey;
      conditions[ConditionName.SHOW_EN_SWITCHER_KEY] = false;

      // If the view for the keyboard code doesn't exist, and the layout
      // data is ready, then creates the view.
      this.container_.addKeysetView(keysetData, layoutData, keyset,
          this.languageCode_, this.model_, this.title_, conditions);
      this.perfTracker_.tick(PerfTracker.TickName.KEYBOARD_CREATED);
    }
  }
  this.switchToKeySet(this.getActiveKeyset_());
};


/**
 * Switch to a specific keyboard.
 *
 * @param {string} keyset The keyset name.
 */
Controller.prototype.switchToKeySet = function(keyset) {
  if (!this.isSettingReady) {
    return;
  }

  var lastKeysetView = this.container_.currentKeysetView;
  var ret = this.container_.switchToKeyset(this.getRemappedKeyset_(keyset),
      this.title_, this.adapter_.isPasswordBox(), this.adapter_.isA11yMode,
      keyset, this.currentKeyset_, this.languageCode_);

  // Update the keyset of current context type.
  this.contextTypeToKeysetMap_[this.currentInputMethod_][
      this.adapter_.getContextType()] = keyset;

  if (ret) {
    this.updateLanguageState_(this.currentKeyset_, keyset);
    // Deactivate the last keyset view instance.
    if (lastKeysetView) {
      lastKeysetView.deactivate(this.currentKeyset_);
    }
    this.currentKeyset_ = keyset;

    this.resize(Controller.DEV);
    this.statistics_.setCurrentLayout(keyset);
    // Activate the current key set view instance.
    this.container_.currentKeysetView.activate(keyset);
    this.perfTracker_.tick(PerfTracker.TickName.KEYBOARD_SHOWN);
    this.perfTracker_.stop();
  } else {
    this.loadResource_(keyset);
  }
};


/**
 * Callback when the configuration is loaded.
 *
 * @param {!i18n.input.chrome.inputview.events.ConfigLoadedEvent} e The event.
 * @private
 */
Controller.prototype.onConfigLoaded_ = function(e) {
  var data = e.data;
  var keyboardCode = data[i18n.input.chrome.inputview.SpecNodeName.ID];
  this.keysetDataMap_[keyboardCode] = data;
  this.perfTracker_.tick(PerfTracker.TickName.KEYSET_LOADED);
  var context = data[i18n.input.chrome.inputview.SpecNodeName.ON_CONTEXT];
  if (context && !this.adapter_.isA11yMode) {
    var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
    if (!keySetMap) {
      keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_] = {};
    }
    keySetMap[context] = keyboardCode;
  }

  var layoutId = data[i18n.input.chrome.inputview.SpecNodeName.LAYOUT];
  var layoutData = this.layoutDataMap_[layoutId];
  if (layoutData) {
    this.maybeCreateViews_();
  } else {
    this.model_.loadLayout(data[i18n.input.chrome.inputview.SpecNodeName.
        LAYOUT]);
  }
};


/**
 * Resizes the whole UI.
 *
 * @param {boolean=} opt_ignoreWindowResize .
 */
Controller.prototype.resize = function(opt_ignoreWindowResize) {
  var height;
  var widthPercent;
  var candidateViewHeight;
  var isHorizontal = screen.width > screen.height;
  var isWideScreen = (Math.min(screen.width, screen.height) / Math.max(
      screen.width, screen.height)) < 0.6;
  this.model_.stateManager.covariance.update(isWideScreen, isHorizontal,
      this.adapter_.isA11yMode);
  if (this.adapter_.isA11yMode) {
    height = SizeSpec.A11Y_HEIGHT;
    widthPercent = screen.width > screen.height ? SizeSpec.A11Y_WIDTH_PERCENT.
        HORIZONTAL : SizeSpec.A11Y_WIDTH_PERCENT.VERTICAL;
    candidateViewHeight = SizeSpec.A11Y_CANDIDATE_VIEW_HEIGHT;
  } else {
    var keyset = this.keysetDataMap_[this.currentKeyset_];
    var layout = keyset && keyset[SpecNodeName.LAYOUT];
    var data = layout && this.layoutDataMap_[layout];
    var spec = data && data[SpecNodeName.WIDTH_PERCENT] ||
        SizeSpec.NON_A11Y_WIDTH_PERCENT;
    height = SizeSpec.NON_A11Y_HEIGHT;
    if (isHorizontal) {
      if (isWideScreen) {
        widthPercent = spec.HORIZONTAL_WIDE_SCREEN;
      } else {
        widthPercent = spec.HORIZONTAL;
      }
    } else {
      widthPercent = spec.VERTICAL;
    }
    candidateViewHeight = SizeSpec.NON_A11Y_CANDIDATE_VIEW_HEIGHT;
  }

  var viewportSize = goog.dom.getViewportSize();
  if (viewportSize.height != height && !opt_ignoreWindowResize) {
    window.resizeTo(screen.width, height);
    return;
  }

  this.container_.resize(screen.width, height, widthPercent,
      candidateViewHeight);
  if (this.container_.currentKeysetView) {
    this.isKeyboardReady = true;
  }
};


/**
 * Loads the resources, for currentKeyset, passwdKeyset, handwriting,
 * emoji, etc.
 *
 * @private
 */
Controller.prototype.loadAllResources_ = function() {
  var keysetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  goog.array.forEach([keysetMap[ContextType.DEFAULT],
    Controller.HANDWRITING_VIEW_CODE_,
    Controller.EMOJI_VIEW_CODE_,
    keysetMap[ContextType.PASSWORD]], function(keyset) {
    this.loadResource_(keyset);
  }, this);
};


/**
 * Gets the remapped keyset.
 *
 * @param {string} keyset .
 * @return {string} The remapped keyset.
 * @private
 */
Controller.prototype.getRemappedKeyset_ = function(keyset) {
  if (goog.array.contains(util.KEYSETS_USE_US, keyset)) {
    return 'us';
  }
  return keyset;
};


/**
 * Loads a single resource.
 *
 * @param {string} keyset .
 * loaded.
 * @private
 */
Controller.prototype.loadResource_ = function(keyset) {
  var remapped = this.getRemappedKeyset_(keyset);
  if (!this.keysetDataMap_[remapped]) {
    if (/^m17n:/.test(remapped)) {
      this.m17nModel_.loadConfig(remapped);
    } else {
      this.model_.loadConfig(remapped);
    }
    return;
  }

  var layoutId = this.keysetDataMap_[remapped][SpecNodeName.LAYOUT];
  if (!this.layoutDataMap_[layoutId]) {
    this.model_.loadLayout(layoutId);
    return;
  }
};


/**
 * Sets the keyboard.
 *
 * @param {string} keyset The keyboard keyset.
 * @param {string} languageCode The language code for this keyboard.
 * @param {string} passwordLayout The layout for password box.
 * @param {string} title The title for this keyboard.
 */
Controller.prototype.initialize = function(keyset, languageCode, passwordLayout,
    title) {
  this.perfTracker_.restart();
  this.adapter_.getCurrentInputMethod(function(currentInputMethod) {
    this.languageCode_ = languageCode;
    this.currentInputMethod_ = currentInputMethod;
    var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
    if (!keySetMap) {
      keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_] = {};
    }
    keySetMap[ContextType.PASSWORD] = passwordLayout;
    keySetMap[ContextType.DEFAULT] = keyset;

    this.title_ = title;
    this.isSettingReady = false;
    this.model_.settings = new i18n.input.chrome.inputview.Settings();
    this.adapter_.initialize(languageCode ? languageCode.split('-')[0] : '');
    this.loadAllResources_();
    this.switchToKeySet(this.getActiveKeyset_());

    // Set language attribute and font of body.
    document.body.setAttribute('lang', this.languageCode_);
    goog.dom.classlist.add(document.body, Css.FONT);
  }.bind(this));
};


/** @override */
Controller.prototype.disposeInternal = function() {
  goog.dispose(this.container_);
  goog.dispose(this.adapter_);
  goog.dispose(this.handler_);
  goog.dispose(this.soundController_);

  goog.base(this, 'disposeInternal');
};


/**
 * Gets the handwriting Input Tool code of current language code.
 *
 * @return {string} The handwriting Input Tool code.
 * @private
 */
Controller.prototype.getHwtInputToolCode_ = function() {
  return this.languageCode_.split(/_|-/)[0] +
      Controller.HANDWRITING_CODE_SUFFIX_;
};


/**
 * True to enable settings link.
 *
 * @return {boolean} .
 */
Controller.prototype.shouldEnableSettings = function() {
  return !this.adapter_.screen || this.adapter_.screen == 'normal';
};


/**
 * Gets the active keyset, if there is a keyset to switch, return it.
 * otherwise if it's a password box, return the password keyset,
 * otherwise return the current keyset.
 *
 * @return {string} .
 * @private
 */
Controller.prototype.getActiveKeyset_ = function() {
  var keySetMap = this.contextTypeToKeysetMap_[this.currentInputMethod_];
  return keySetMap[this.adapter_.getContextType()] ||
      keySetMap[ContextType.DEFAULT];
};


/**
 * True if keysetB is the sub keyset of keysetA.
 *
 * @param {string} keysetA .
 * @param {string} keysetB .
 * @return {boolean} .
 * @private
 */
Controller.prototype.isSubKeyset_ = function(keysetA, keysetB) {
  var segmentsA = keysetA.split('.');
  var segmentsB = keysetB.split('.');
  return segmentsA.length >= 2 && segmentsB.length >= 2 &&
      segmentsA[0] == segmentsB[0] && segmentsA[1] == segmentsB[1];
};


/**
 * Updates the compact pinyin to set the inputcode for english and pinyin.
 *
 * @param {string} fromRawKeyset .
 * @param {string} toRawKeyset .
 * @private
 */
Controller.prototype.updateLanguageState_ =
    function(fromRawKeyset, toRawKeyset) {
  if (fromRawKeyset == 'pinyin-zh-CN.en.compact.qwerty' &&
      toRawKeyset.indexOf('en.compact') == -1) {
    this.adapter_.toggleLanguageState(true);
  } else if (fromRawKeyset.indexOf('en.compact') == -1 &&
      toRawKeyset == 'pinyin-zh-CN.en.compact.qwerty') {
    this.adapter_.toggleLanguageState(false);
  } else if (goog.array.contains(
      i18n.input.chrome.inputview.util.KEYSETS_HAVE_EN_SWTICHER,
      toRawKeyset)) {
    this.adapter_.toggleLanguageState(true);
    this.model_.stateManager.isEnMode = false;
    this.container_.currentKeysetView.update();
  }
};
});  // goog.scope
