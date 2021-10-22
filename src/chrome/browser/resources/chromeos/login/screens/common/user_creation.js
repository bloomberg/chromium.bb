// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const UserCreationScreenElementBase = Polymer.mixinBehaviors(
    [
      OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior
    ],
    Polymer.Element);

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const UserCreationUIState = {
  CREATE: 'create',
  CHILD: 'child',
};

/**
 * User type for setting up the device.
 * @enum {string}
 */
const UserCreationUserType = {
  SELF: 'self',
  CHILD: 'child',
};

/**
 * Sign in method for setting up the device for child.
 * @enum {string}
 */
const UserCreationSignInMethod = {
  CREATE: 'create',
  SIGNIN: 'signin',
};

class UserCreation extends UserCreationScreenElementBase {
  static get is() {
    return 'user-creation-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * The currently selected user type.
       */
      selectedUserType: {
        type: String,
      },

      /**
       * The currently selected sign in method.
       */
      selectedSignInMethod: {
        type: String,
      },

      /**
       * Is the back button visible on the first step of the screen. Back button
       * is visible iff we are in the add person flow.
       * @private
       */
      isBackButtonVisible_: {
        type: Boolean,
      },

      /** @private */
      titleKey_: {
        type: String,
      },

      /** @private */
      subtitleKey_: {
        type: String,
      },

    };
  }

  constructor() {
    super();
    this.selectedUserType = UserCreationUserType.SELF;
    this.selectedSignInMethod = '';
    this.isBackButtonVisible_ = true;
    this.titleKey_ = 'userCreationTitle';
    this.subtitleKey_ = 'userCreationSubtitle';
  }

  /** @override */
  get EXTERNAL_API() {
    return ['setIsBackButtonVisible'];
  }

  /** @override */
  defaultUIStep() {
    return UserCreationUIState.CREATE;
  }

  get UI_STEPS() {
    return UserCreationUIState;
  }

  onBeforeShow() {
    this.selectedUserType = UserCreationUserType.SELF;
    this.selectedSignInMethod = '';
    this.titleKey_ = this.isBackButtonVisible_ ? 'userCreationAddPersonTitle' :
                                                 'userCreationTitle';
    this.subtitleKey_ = this.isBackButtonVisible_ ?
        'userCreationAddPersonSubtitle' :
        'userCreationSubtitle';
    if (this.uiStep === UserCreationUIState.CHILD) {
      chrome.send('updateOobeUIState', [OOBE_UI_STATE.GAIA_SIGNIN]);
    }
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('UserCreationScreen', {
      resetAllowed: true,
    });
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.USER_CREATION;
  }

  setIsBackButtonVisible(isVisible) {
    this.isBackButtonVisible_ = isVisible;
  }

  cancel() {
    if (this.isBackButtonVisible_) {
      this.onBackClicked_();
    }
  }

  onBackClicked_() {
    if (this.uiStep === UserCreationUIState.CHILD) {
      chrome.send('updateOobeUIState', [OOBE_UI_STATE.USER_CREATION]);
      this.setUIStep(UserCreationUIState.CREATE);
    } else {
      this.userActed('cancel');
    }
  }

  onNextClicked_() {
    if (this.uiStep === UserCreationUIState.CREATE) {
      if (this.selectedUserType === UserCreationUserType.SELF) {
        this.userActed('signin');
      } else if (this.selectedUserType === UserCreationUserType.CHILD) {
        chrome.send('updateOobeUIState', [OOBE_UI_STATE.GAIA_SIGNIN]);
        this.setUIStep(UserCreationUIState.CHILD);
      }
    } else if (this.uiStep === UserCreationUIState.CHILD) {
      if (this.selectedSignInMethod === UserCreationSignInMethod.CREATE) {
        this.userActed('child-account-create');
      } else if (this.selectedSignInMethod === UserCreationSignInMethod.SIGNIN) {
        this.userActed('child-signin');
      }
    }
  }

  /**
   * @suppress {missingProperties}
   */
  onLearnMoreClicked_() {
    this.$.learnMoreDialog.showDialog();
  }

  /**
   * @suppress {missingProperties}
   */
  focusLearnMoreLink_() {
    this.$.learnMoreLink.focus();
  }
}
customElements.define(UserCreation.is, UserCreation);