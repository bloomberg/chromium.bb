// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './profile_card.js';
import './profile_picker_shared_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationMixin, Routes} from './navigation_mixin.js';
import {isAskOnStartupAllowed, isGuestModeEnabled, isProfileCreationAllowed} from './policy_helper.js';

export interface ProfilePickerMainViewElement {
  $: {
    addProfile: HTMLElement,
    'product-logo': HTMLElement,
    browseAsGuestButton: HTMLElement,
    profilesContainer: HTMLElement,
  };
}

const ProfilePickerMainViewElementBase =
    WebUIListenerMixin(NavigationMixin(PolymerElement));

export class ProfilePickerMainViewElement extends
    ProfilePickerMainViewElementBase {
  static get is() {
    return 'profile-picker-main-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Profiles list supplied by ManageProfilesBrowserProxy.
       */
      profilesList_: {
        type: Array,
        value: () => [],
      },

      profilesListLoaded_: {
        type: Boolean,
        value: false,
      },

      hideAskOnStartup_: {
        type: Boolean,
        value: true,
        computed: 'computeHideAskOnStartup_(profilesList_.length)',

      },

      askOnStartup_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('askOnStartup');
        }
      },
    };
  }

  private profilesList_: Array<ProfileState>;
  private profilesListLoaded_: boolean;
  private hideAskOnStartup_: boolean;
  private askOnStartup_: boolean;
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();
  private resizeObserver_: ResizeObserver|null = null;

  ready() {
    super.ready();
    if (!isGuestModeEnabled()) {
      this.$.browseAsGuestButton.style.display = 'none';
    }

    if (!isProfileCreationAllowed()) {
      this.$.addProfile.style.display = 'none';
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.addResizeObserver_();
    this.addWebUIListener(
        'profiles-list-changed', this.handleProfilesListChanged_.bind(this));
    this.addWebUIListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.manageProfilesBrowserProxy_.initializeMainView();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_!.disconnect();
  }

  private addResizeObserver_() {
    const profilesContainer = this.$.profilesContainer;
    this.resizeObserver_ = new ResizeObserver(() => {
      this.shadowRoot!.querySelector('.footer')!.classList.toggle(
          'division-line',
          profilesContainer.scrollHeight > profilesContainer.clientHeight);
    });
    this.resizeObserver_.observe(profilesContainer);
  }

  private onProductLogoTap_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  /**
   * Handler for when the profiles list are updated.
   */
  private handleProfilesListChanged_(profilesList: Array<ProfileState>) {
    this.profilesListLoaded_ = true;
    this.profilesList_ = profilesList;
  }

  /**
   * Called when the user modifies 'Ask on startup' preference.
   */
  private onAskOnStartupChangedByUser_() {
    if (this.hideAskOnStartup_) {
      return;
    }

    this.manageProfilesBrowserProxy_.askOnStartupChanged(this.askOnStartup_);
  }

  private onAddProfileClick_() {
    if (!isProfileCreationAllowed()) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('ProfilePicker_AddClicked');
    navigateTo(Routes.NEW_PROFILE);
  }

  private onLaunchGuestProfileClick_() {
    if (!isGuestModeEnabled()) {
      return;
    }
    this.manageProfilesBrowserProxy_.launchGuestProfile();
  }

  private handleProfileRemoved_(profilePath: string) {
    for (let i = 0; i < this.profilesList_.length; i += 1) {
      if (this.profilesList_[i].profilePath === profilePath) {
        // TODO(crbug.com/1063856): Add animation.
        this.splice('profilesList_', i, 1);
        break;
      }
    }
  }

  private computeHideAskOnStartup_(): boolean {
    return !isAskOnStartupAllowed() || !this.profilesList_ ||
        this.profilesList_.length < 2;
  }
}

customElements.define(
    ProfilePickerMainViewElement.is, ProfilePickerMainViewElement);
