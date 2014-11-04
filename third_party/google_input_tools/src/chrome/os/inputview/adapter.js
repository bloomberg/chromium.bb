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
goog.provide('i18n.input.chrome.inputview.Adapter');

goog.require('goog.events.Event');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');
goog.require('goog.events.EventType');
goog.require('goog.object');
goog.require('i18n.input.chrome.DataSource');
goog.require('i18n.input.chrome.inputview.ReadyState');
goog.require('i18n.input.chrome.inputview.StateType');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.events.SurroundingTextChangedEvent');
goog.require('i18n.input.chrome.message.ContextType');
goog.require('i18n.input.chrome.message.Event');
goog.require('i18n.input.chrome.message.Name');
goog.require('i18n.input.chrome.message.Type');

goog.scope(function() {
var CandidatesBackEvent = i18n.input.chrome.DataSource.CandidatesBackEvent;
var ContextType = i18n.input.chrome.message.ContextType;
var Type = i18n.input.chrome.message.Type;
var Name = i18n.input.chrome.message.Name;



/**
 * The adapter for interview.
 *
 * @param {!i18n.input.chrome.inputview.ReadyState} readyState .
 * @extends {goog.events.EventTarget}
 * @constructor
 */
i18n.input.chrome.inputview.Adapter = function(readyState) {
  goog.base(this);

  /**
   * Whether the keyboard is visible.
   *
   * @type {boolean}
   */
  this.isVisible = !document.webkitHidden;

  /**
   * The modifier state map.
   *
   * @type {!Object.<i18n.input.chrome.inputview.StateType, boolean>}
   * @private
   */
  this.modifierState_ = {};

  /**
   * The system ready state.
   *
   * @private {!i18n.input.chrome.inputview.ReadyState}
   */
  this.readyState_ = readyState;

  chrome.runtime.onMessage.addListener(this.onMessage_.bind(this));

  /** @private {!goog.events.EventHandler} */
  this.handler_ = new goog.events.EventHandler(this);
  this.handler_.listen(document, 'webkitvisibilitychange',
      this.onVisibilityChange_);
};
goog.inherits(i18n.input.chrome.inputview.Adapter,
    goog.events.EventTarget);
var Adapter = i18n.input.chrome.inputview.Adapter;


/** @type {boolean} */
Adapter.prototype.isA11yMode = false;


/** @type {boolean} */
Adapter.prototype.isExperimental = false;


/** @type {boolean} */
Adapter.prototype.showGlobeKey = false;


/** @protected {string} */
Adapter.prototype.contextType = ContextType.DEFAULT;


/** @type {string} */
Adapter.prototype.screen = '';


/** @type {boolean} */
Adapter.prototype.isChromeVoxOn = false;


/** @type {string} */
Adapter.prototype.textBeforeCursor = '';


/**
 * Callback for updating settings.
 *
 * @param {!Object} message .
 * @private
 */
Adapter.prototype.onUpdateSettings_ = function(message) {
  this.contextType = message['contextType'];
  this.screen = message['screen'];
  this.dispatchEvent(new i18n.input.chrome.message.Event(Type.UPDATE_SETTINGS,
      message));
};


/**
 * Sets the modifier states.
 *
 * @param {i18n.input.chrome.inputview.StateType} stateType .
 * @param {boolean} enable True to enable the state, false otherwise.
 */
Adapter.prototype.setModifierState = function(stateType, enable) {
  this.modifierState_[stateType] = enable;
};


/**
 * Clears the modifier states.
 */
Adapter.prototype.clearModifierStates = function() {
  this.modifierState_ = {};
};


/**
 * Simulates to send 'keydown' and 'keyup' event.
 *
 * @param {string} key
 * @param {string} code
 * @param {number=} opt_keyCode The key code.
 * @param {!Object=} opt_spatialData .
 */
Adapter.prototype.sendKeyDownAndUpEvent = function(key, code, opt_keyCode,
    opt_spatialData) {
  this.sendKeyEvent_([
    this.generateKeyboardEvent_(
        goog.events.EventType.KEYDOWN, key, code, opt_keyCode, opt_spatialData),
    this.generateKeyboardEvent_(
        goog.events.EventType.KEYUP, key, code, opt_keyCode, opt_spatialData)
  ]);
};


/**
 * Simulates to send 'keydown' event.
 *
 * @param {string} key
 * @param {string} code
 * @param {number=} opt_keyCode The key code.
 * @param {!Object=} opt_spatialData .
 */
Adapter.prototype.sendKeyDownEvent = function(key, code, opt_keyCode,
    opt_spatialData) {
  this.sendKeyEvent_([this.generateKeyboardEvent_(
      goog.events.EventType.KEYDOWN, key, code, opt_keyCode,
      opt_spatialData)]);
};


/**
 * Simulates to send 'keyup' event.
 *
 * @param {string} key
 * @param {string} code
 * @param {number=} opt_keyCode The key code.
 * @param {!Object=} opt_spatialData .
 */
Adapter.prototype.sendKeyUpEvent = function(key, code, opt_keyCode,
    opt_spatialData) {
  this.sendKeyEvent_([this.generateKeyboardEvent_(
      goog.events.EventType.KEYUP, key, code, opt_keyCode, opt_spatialData)]);
};


/**
 * Use {@code chrome.input.ime.sendKeyEvents} to simulate key events.
 *
 * @param {!Array.<!Object.<string, string|boolean>>} keyData .
 * @private
 */
Adapter.prototype.sendKeyEvent_ = function(keyData) {
  chrome.runtime.sendMessage(
      goog.object.create(Name.TYPE, Type.SEND_KEY_EVENT, Name.KEY_DATA,
          keyData));
};


/**
 * Generates a {@code ChromeKeyboardEvent} by given values.
 *
 * @param {string} type .
 * @param {string} key The key.
 * @param {string} code The code.
 * @param {number=} opt_keyCode The key code.
 * @param {!Object=} opt_spatialData .
 * @return {!Object.<string, string|boolean>}
 * @private
 */
Adapter.prototype.generateKeyboardEvent_ = function(
    type, key, code, opt_keyCode, opt_spatialData) {
  var StateType = i18n.input.chrome.inputview.StateType;
  var ctrl = !!this.modifierState_[StateType.CTRL];
  var alt = !!this.modifierState_[StateType.ALT];

  if (ctrl || alt) {
    key = '';
  }
  var result = {
    'type': type,
    'key': key,
    'code': code,
    'keyCode': opt_keyCode || 0,
    'spatialData': opt_spatialData
  };

  result['altKey'] = alt;
  result['ctrlKey'] = ctrl;
  result['shiftKey'] = !!this.modifierState_[StateType.SHIFT];
  result['capsLock'] = !!this.modifierState_[StateType.CAPSLOCK];

  return result;
};


/**
 * Callback when surrounding text is changed.
 *
 * @param {string} text .
 * @private
 */
Adapter.prototype.onSurroundingTextChanged_ = function(text) {
  this.textBeforeCursor = text;
  this.dispatchEvent(new i18n.input.chrome.inputview.events.
      SurroundingTextChangedEvent(this.textBeforeCursor));
};


/**
 * Gets the context.
 *
 * @return {string} .
 */
Adapter.prototype.getContext = function() {
  var matches = this.textBeforeCursor.match(/([a-zA-Z'-Ḁ-ỹÀ-ȳ]+)\s+$/);
  var text = matches ? matches[1] : '';
  return text;
};


/**
 * Gets the context type.
 *
 * @return {string} .
 */
Adapter.prototype.getContextType = function() {
  return this.contextType || ContextType.DEFAULT;
};


/**
 * Sends request for handwriting.
 *
 * @param {!Object} payload .
 */
Adapter.prototype.sendHwtRequest = function(payload) {
  chrome.runtime.sendMessage(goog.object.create(
      Name.TYPE, Type.HWT_REQUEST, Name.MSG, payload
      ));
};


/**
 * True if it is a password box.
 *
 * @return {boolean} .
 */
Adapter.prototype.isPasswordBox = function() {
  return this.contextType == 'password';
};


/**
 * Callback when blurs in the context.
 *
 * @private
 */
Adapter.prototype.onContextBlur_ = function() {
  this.contextType = '';
  this.dispatchEvent(new goog.events.Event(i18n.input.chrome.inputview.events.
      EventType.CONTEXT_BLUR));
};


/**
 * Callback when focus on a context.
 *
 * @param {string} contextType .
 * @private
 */
Adapter.prototype.onContextFocus_ = function(contextType) {
  this.contextType = contextType;
  this.dispatchEvent(new goog.events.Event(i18n.input.chrome.inputview.events.
      EventType.CONTEXT_FOCUS));
};


/**
 * Intializes the communication to background page.
 *
 * @param {string} languageCode The language code.
 * @private
 */
Adapter.prototype.initBackground_ = function(languageCode) {
  chrome.runtime.getBackgroundPage((function() {
    chrome.runtime.sendMessage(
        goog.object.create(Name.TYPE, Type.CONNECT));
    chrome.runtime.sendMessage(goog.object.create(Name.TYPE,
        Type.VISIBILITY_CHANGE, Name.VISIBILITY, !document.webkitHidden));
    if (languageCode) {
      this.setLanguage(languageCode);
    }
  }).bind(this));
};


/**
 * Loads the keyboard settings.
 *
 * @param {string} languageCode The language code.
 */
Adapter.prototype.initialize = function(languageCode) {
  if (chrome.accessibilityFeatures &&
      chrome.accessibilityFeatures.spokenFeedback) {
    chrome.accessibilityFeatures.spokenFeedback.get({}, (function(details) {
      this.isChromeVoxOn = details['value'];
    }).bind(this));
    chrome.accessibilityFeatures.spokenFeedback.onChange.addListener((function(
        details) {
          this.isChromeVoxOn = details['value'];
        }).bind(this));
  }

  this.initBackground_(languageCode);

  var StateType = i18n.input.chrome.inputview.ReadyState.StateType;
  if (window.inputview) {
    if (inputview.getKeyboardConfig) {
      inputview.getKeyboardConfig((function(config) {
        this.isA11yMode = !!config['a11ymode'];
        this.isExperimental = !!config['experimental'];
        this.readyState_.markStateReady(StateType.KEYBOARD_CONFIG_READY);
        if (this.readyState_.isReady(StateType.IME_LIST_READY)) {
          this.dispatchEvent(new goog.events.Event(
              i18n.input.chrome.inputview.events.EventType.SETTINGS_READY));
        }
      }).bind(this));
    } else {
      this.readyState_.markStateReady(StateType.KEYBOARD_CONFIG_READY);
    }
    if (inputview.getInputMethods) {
      inputview.getInputMethods((function(inputMethods) {
        // Only show globe key to switching between IMEs when there are more
        // than one IME.
        this.showGlobeKey = inputMethods.length > 1;
        this.readyState_.markStateReady(StateType.IME_LIST_READY);
        if (this.readyState_.isReady(StateType.KEYBOARD_CONFIG_READY)) {
          this.dispatchEvent(new goog.events.Event(
              i18n.input.chrome.inputview.events.EventType.SETTINGS_READY));
        }
      }).bind(this));
    } else {
      this.readyState_.markStateReady(StateType.IME_LIST_READY);
    }
  } else {
    this.readyState_.markStateReady(StateType.IME_LIST_READY);
    this.readyState_.markStateReady(StateType.KEYBOARD_CONFIG_READY);
  }

  if (this.readyState_.isReady(StateType.KEYBOARD_CONFIG_READY) &&
      this.readyState_.isReady(StateType.IME_LIST_READY)) {
    window.setTimeout((function() {
      this.dispatchEvent(new goog.events.Event(
          i18n.input.chrome.inputview.events.EventType.SETTINGS_READY));
    }).bind(this), 0);
  }
};


/**
 * Gets the currently activated input method.
 *
 * @param {function(string)} callback .
 */
Adapter.prototype.getCurrentInputMethod = function(callback) {
  if (window.inputview && inputview.getCurrentInputMethod) {
    inputview.getCurrentInputMethod(callback);
  } else {
    callback('DU');
  }
};


/**
 * Gets the list of all activated input methods.
 *
 * @param {function(Array.<Object>)} callback .
 */
Adapter.prototype.getInputMethods = function(callback) {
  if (window.inputview && inputview.getInputMethods) {
    inputview.getInputMethods(callback);
  } else {
    // Provides a dummy IME item to enable IME switcher UI.
    callback([
      {'indicator': 'DU', 'id': 'DU', 'name': 'Dummy IME', 'command': 1}]);
  }
};


/**
 * Switches to the input method with id equals |inputMethodId|.
 *
 * @param {!string} inputMethodId .
 */
Adapter.prototype.switchToInputMethod = function(inputMethodId) {
  if (window.inputview && inputview.switchToInputMethod) {
    inputview.switchToInputMethod(inputMethodId);
  }
};


/**
 * Callback for visibility change on the input view window.
 *
 * @private
 */
Adapter.prototype.onVisibilityChange_ = function() {
  this.isVisible = !document.webkitHidden;
  this.dispatchEvent(new goog.events.Event(i18n.input.chrome.inputview.
      events.EventType.VISIBILITY_CHANGE));
  chrome.runtime.sendMessage(goog.object.create(Name.TYPE,
      Type.VISIBILITY_CHANGE, Name.VISIBILITY, !document.webkitHidden));
};


/**
 * Sends request for completion.
 *
 * @param {string} query .
 * @param {!Object=} opt_spatialData .
 */
Adapter.prototype.sendCompletionRequest = function(query, opt_spatialData) {
  var spatialData = {};
  if (opt_spatialData) {
    spatialData[Name.SOURCES] = opt_spatialData.sources;
    spatialData[Name.POSSIBILITIES] = opt_spatialData.possibilities;
  }
  chrome.runtime.sendMessage(goog.object.create(Name.TYPE,
      Type.COMPLETION, Name.TEXT, query, Name.SPATIAL_DATA, spatialData));
};


/**
 * Selects the candidate.
 *
 * @param {!Object} candidate .
 */
Adapter.prototype.selectCandidate = function(candidate) {
  chrome.runtime.sendMessage(goog.object.create(
      Name.TYPE, Type.SELECT_CANDIDATE, Name.CANDIDATE, candidate));
};


/**
 * Commits the text.
 *
 * @param {string} text .
 */
Adapter.prototype.commitText = function(text) {
  chrome.runtime.sendMessage(goog.object.create(
      Name.TYPE, Type.COMMIT_TEXT, Name.TEXT, text));
};


/**
 * Sets the language.
 *
 * @param {string} language .
 */
Adapter.prototype.setLanguage = function(language) {
  chrome.runtime.sendMessage(goog.object.create(
      Name.TYPE, Type.SET_LANGUAGE, Name.LANGUAGE, language));
};


/**
 * Callbck when completion is back.
 *
 * @param {!Object} message .
 * @private
 */
Adapter.prototype.onCandidatesBack_ = function(message) {
  var source = message['source'] || '';
  var candidates = message['candidates'] || [];
  this.dispatchEvent(new CandidatesBackEvent(source, candidates));
};


/**
 * Hides the keyboard.
 */
Adapter.prototype.hideKeyboard = function() {
  chrome.input.ime.hideInputView();
};


/**
 * Sends Input Tool code to background.
 *
 * @param {string} inputToolCode .
 */
Adapter.prototype.setInputToolCode = function(inputToolCode) {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.HWT_SET_INPUTTOOL,
          Name.MSG,
          inputToolCode));
};


/**
 * Sends DOUBLE_CLICK_ON_SPACE_KEY message.
 */
Adapter.prototype.doubleClickOnSpaceKey = function() {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.DOUBLE_CLICK_ON_SPACE_KEY));
};


/**
 * Sends message to the background when switch to emoji.
 *
 */
Adapter.prototype.setEmojiInputToolCode = function() {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.EMOJI_SET_INPUTTOOL));
};


/**
 * Sends message to the background when do internal inputtool switch.
 *
 * @param {boolean} inputToolValue The value of the language flag.
 */
Adapter.prototype.toggleLanguageState = function(inputToolValue) {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.TOGGLE_LANGUAGE_STATE,
          Name.MSG,
          inputToolValue));
};


/**
 * Sends unset Input Tool code to background.
 */
Adapter.prototype.unsetInputToolCode = function() {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.HWT_UNSET_INPUTTOOL));
};


/**
 * Sends message to the background when switch to other mode from emoji.
 *
 */
Adapter.prototype.unsetEmojiInputToolCode = function() {
  chrome.runtime.sendMessage(
      goog.object.create(
          Name.TYPE,
          Type.EMOJI_UNSET_INPUTTOOL));
};


/**
 * Processes incoming message from option page or inputview window.
 *
 * @param {*} request Message from option page or inputview window.
 * @param {*} sender Information about the script
 *     context that sent the message.
 * @param {function(*): void} sendResponse Function to call to send a response.
 * @return {boolean|undefined} {@code true} to keep the message channel open in
 *     order to send a response asynchronously.
 * @private
 */
Adapter.prototype.onMessage_ = function(request, sender, sendResponse) {
  var type = request[Name.TYPE];
  var msg = request[Name.MSG];
  switch (type) {
    case Type.CANDIDATES_BACK:
      this.onCandidatesBack_(msg);
      break;
    case Type.CONTEXT_FOCUS:
      this.onContextFocus_(request[Name.CONTEXT_TYPE]);
      break;
    case Type.CONTEXT_BLUR:
      this.onContextBlur_();
      break;
    case Type.SURROUNDING_TEXT_CHANGED:
      this.onSurroundingTextChanged_(request[Name.TEXT]);
      break;
    case Type.UPDATE_SETTINGS:
      this.onUpdateSettings_(msg);
      break;
    case Type.HWT_NETWORK_ERROR:
    case Type.HWT_PRIVACY_INFO:
      this.dispatchEvent(new i18n.input.chrome.message.Event(type, msg));
      break;
  }
};


/**
 * Sends the privacy confirmed message to background and broadcasts it.
 */
Adapter.prototype.sendHwtPrivacyConfirmMessage = function() {
  chrome.runtime.sendMessage(
      goog.object.create(Name.TYPE, Type.HWT_PRIVACY_GOT_IT));
  this.dispatchEvent(
      new goog.events.Event(Type.HWT_PRIVACY_GOT_IT));
};


/** @override */
Adapter.prototype.disposeInternal = function() {
  goog.dispose(this.handler_);

  goog.base(this, 'disposeInternal');
};
});  // goog.scope

