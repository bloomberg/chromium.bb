// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Buttons = {
  LEFT_BUTTON: 0,
  RIGHT_BUTTON: 2
};

const RecorderStateEnum = {
  // Not recording.
  STOPPED: 'stopped',
  // Recording, extension is showing the recorder UI on the page.
  SHOWN: 'shown',
  // Recording, extension is hiding the recorder UI on the page.
  HIDDEN: 'hidden'
};

const ActionTypeEnum = {
  CLICK: 'click',
  HOVER: 'hover',
  LOAD_PAGE: 'loadPage',
  PRESS_ENTER: 'pressEnter',
  SELECT: 'select',
  TYPE: 'type',
  TYPE_PASSWORD: 'typePassword',
  VALIDATE_FIELD: 'validateField',
  // Autofill actions
  AUTOFILL: 'autofill',
  // Password manager actions
  SAVE_PASSWORD: 'savePassword',
  REJECT_SAVE_PASSWORD: 'rejectSavePassword',
  UPDATE_PASSWORD: 'updatePassword',
  REJECT_UPDATE_PASSWORD: 'rejectUpdatePassword'
};

const RecorderUiMsgEnum = {
  CREATE_UI: 'add-ui',
  DESTROY_UI: 'remove-ui',
  HIDE_UI: 'hide-ui',
  SHOW_UI: 'show-ui',
  ADD_ACTION: 'add-action',
  REMOVE_ACTION: 'remove-action',
  GET_RECIPE: 'get-recipe',
  GET_SAVED_ACTION_PARAMS: 'get-saved-action-params',
  SET_PASSWORD_MANAGER_ACTION_PARAMS: 'set-password-manager-action-params'
};

const RecorderMsgEnum = {
  SAVE: 'save-recording',
  START: 'start-recording',
  STOP: 'stop-recording',
  CANCEL: 'cancel-recording',
  GET_IFRAME_NAME: 'get-iframe-name',
  ADD_ACTION: 'record-action',
  SET_PASSWORD_MANAGER_ACTION_PARAMS: 'set-password-manager-action-params',
  SET_AUTOFILL_PROFILE_ENTRY: 'set-autofill-profile-entry',
  SET_PASSWORD_MANAGER_PROFILE_ENTRY: 'set-password-manager-profile-entry'
};

const Local_Storage_Vars = {
  RECORDING_STATE: 'state',
  RECORDING_TAB_ID: 'target_tab_id',
  RECORDING_UI_FRAME_ID: 'ui_frame_id'
};

const Indexed_DB_Vars = {
  RECIPE_DB: 'Action_Recorder_Extension_Recipe',
  VERSION: 1,
  ACTIONS: 'Actions',
  // The 'Attributes' table stores properties of the current recipe.
  ATTRIBUTES: 'Attributes',
  NAME: 'name',
  URL: 'url',
  // The 'Saved Action Params' table stores parameters of potential Chrome
  // actions.
  SAVED_ACTION_PARAMS: 'Saved_Action_Params',
  PASSWORD_MANAGER_PARAMS: 'password_manager_params',
  // The 'Profile' tables stores the user's Chrome autofill profile and Chrome
  // password manager profile.
  AUTOFILL_PROFILE: 'autofill_profile',
  PASSWORD_MANAGER_PROFILE: 'password_manager_profile'
};
