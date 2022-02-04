// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './profile_picker_shared_css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

export interface ProfileSwitchElement {
  $: {
    iconContainer: HTMLElement,
    profileName: HTMLElement,
    gaiaName: HTMLElement,
    cancelButton: CrButtonElement,
    switchButton: CrButtonElement,
  };
}

export class ProfileSwitchElement extends PolymerElement {
  static get is() {
    return 'profile-switch';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      profileState_: Object,

      isProfileStateInitialized_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private profileState_: ProfileState;
  private isProfileStateInitialized_: boolean;
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  ready() {
    super.ready();
    this.manageProfilesBrowserProxy_.getSwitchProfile().then(profileState => {
      this.profileState_ = profileState;
      this.isProfileStateInitialized_ = true;
    });
  }

  private onCancelClick_() {
    this.manageProfilesBrowserProxy_.cancelProfileSwitch();
  }

  private onSwitchClick_() {
    this.manageProfilesBrowserProxy_.confirmProfileSwitch(
        this.profileState_.profilePath);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-switch': ProfileSwitchElement;
  }
}

customElements.define(ProfileSwitchElement.is, ProfileSwitchElement);
