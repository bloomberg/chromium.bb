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
goog.provide('i18n.input.chrome.message');
goog.provide('i18n.input.chrome.message.Type');


/**
 * The message type. "Background->Inputview" don't allow to share the same
 * message type with "Inputview->Background".
 *
 * @enum {string}
 */
i18n.input.chrome.message.Type = {
  // Background -> Inputview
  CANDIDATES_BACK: 'candidates_back',
  CONTEXT_BLUR: 'context_blur',
  CONTEXT_FOCUS: 'context_focus',
  FRONT_TOGGLE_LANGUAGE_STATE: 'front_toggle_language_state',
  HWT_NETWORK_ERROR: 'hwt_network_error',
  SURROUNDING_TEXT_CHANGED: 'surrounding_text_changed',
  UPDATE_SETTINGS: 'update_settings',
  VOICE_STATE_CHANGE: 'voice_state_change',

  // Inputview -> Background
  COMMIT_TEXT: 'commit_text',
  COMPLETION: 'completion',
  CONNECT: 'connect',
  DATASOURCE_READY: 'datasource_ready',
  DISCONNECT: 'disconnect',
  DOUBLE_CLICK_ON_SPACE_KEY: 'double_click_on_space_key',
  EXEC_ALL: 'exec_all',
  HWT_REQUEST: 'hwt_request',
  KEY_CLICK: 'key_click',
  KEY_EVENT: 'key_event',
  OPTION_CHANGE: 'option_change',
  PREDICTION: 'prediction',
  SELECT_CANDIDATE: 'select_candidate',
  SEND_KEY_EVENT: 'send_key_event',
  SET_COMPOSITION: 'set_composition',
  SET_LANGUAGE: 'set_language',
  SWITCH_KEYSET: 'switch_keyset',
  TOGGLE_LANGUAGE_STATE: 'toggle_language_state',
  VISIBILITY_CHANGE: 'visibility_change',
  SET_CONTROLLER: 'set_controller',
  UNSET_CONTROLLER: 'unset_controller',
  VOICE_VIEW_STATE_CHANGE: 'voice_view_state_change',


  // Inputview -> Elements
  HWT_PRIVACY_GOT_IT: 'hwt_privacy_got_it',
  VOICE_PRIVACY_GOT_IT: 'voice_privacy_got_it',

  // Options -> Background
  USER_DICT_ADD_ENTRY: 'user_dict_add_entry',
  USER_DICT_CLEAR: 'user_dict_clear',
  USER_DICT_LIST: 'user_dict_list',
  USER_DICT_SET_THRESHOLD: 'user_dict_set_threshold',
  USER_DICT_START: 'user_dict_start',
  USER_DICT_STOP: 'user_dict_stop',
  USER_DICT_REMOVE_ENTRY: 'user_dict_remove_entry',

  // Background -> Options
  USER_DICT_ENTRIES: 'user_dict_entries',

  // Background->Background
  HEARTBEAT: 'heart_beat'
};


/**
 * Returns whether the message type belong to "Background->Inputview" group;
 *
 * @param {string} type The message type.
 * @return {boolean} .
 */
i18n.input.chrome.message.isFromBackground = function(type) {
  var Type = i18n.input.chrome.message.Type;
  switch (type) {
    case Type.CANDIDATES_BACK:
    case Type.CONTEXT_BLUR:
    case Type.CONTEXT_FOCUS:
    case Type.FRONT_TOGGLE_LANGUAGE_STATE:
    case Type.HWT_NETWORK_ERROR:
    case Type.SURROUNDING_TEXT_CHANGED:
    case Type.UPDATE_SETTINGS:
    case Type.USER_DICT_ENTRIES:
    case Type.VOICE_STATE_CHANGE:
      return true;
    default:
      return false;
  }
};
