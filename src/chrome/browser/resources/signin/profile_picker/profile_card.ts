// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './profile_card_menu.js';
import './profile_picker_shared_css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

interface ProfileCardElement {
  $: {
    gaiaName: HTMLElement,
    gaiaNameTooltip: PaperTooltipElement,
    nameInput: CrInputElement,
    tooltip: PaperTooltipElement,
  };
}

const ProfileCardElementBase = mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class ProfileCardElement extends ProfileCardElementBase {
  static get is() {
    return 'profile-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      profileState: {
        type: Object,
      },

      pattern_: {
        type: String,
        value: '.*\\S.*',
      },
    };
  }

  profileState: ProfileState;
  private pattern_: string;
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  connectedCallback() {
    super.connectedCallback();
    this.addNameInputTooltipListeners_();
    this.addGaiaNameTooltipListeners_();
  }

  private addNameInputTooltipListeners_() {
    const showTooltip = () => {
      const inputElement = this.$.tooltip.target.inputElement;
      // Disable tooltip if the local name editing is in progress.
      if (this.isNameTruncated_(inputElement) &&
          !this.$.nameInput.hasAttribute('focused_')) {
        this.$.tooltip.show();
        return;
      }
      this.$.tooltip.hide();
    };
    const hideTooltip = () => this.$.tooltip.hide();
    const target = this.$.tooltip.target;
    target.addEventListener('mouseenter', showTooltip);
    target.addEventListener('focus', hideTooltip);
    target.addEventListener('mouseleave', hideTooltip);
    target.addEventListener('click', hideTooltip);
    this.$.tooltip.addEventListener('mouseenter', hideTooltip);
  }

  private addGaiaNameTooltipListeners_() {
    const showTooltip = () => {
      if (this.isNameTruncated_(this.$.gaiaName)) {
        this.$.gaiaNameTooltip.show();
        return;
      }
      this.$.gaiaNameTooltip.hide();
    };
    const hideTooltip = () => this.$.gaiaNameTooltip.hide();
    const target = this.$.gaiaNameTooltip.target;
    target.addEventListener('mouseenter', showTooltip);
    target.addEventListener('focus', showTooltip);
    target.addEventListener('mouseleave', hideTooltip);
    target.addEventListener('blur', hideTooltip);
    target.addEventListener('tap', hideTooltip);
    this.$.gaiaNameTooltip.addEventListener('mouseenter', hideTooltip);
  }

  private isNameTruncated_(element: HTMLElement): boolean {
    return !!element && element.scrollWidth > element.offsetWidth;
  }

  private onProfileClick_() {
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.profileState.profilePath);
  }

  /**
   * Handler for when the profile name field is changed, then blurred.
   */
  private onProfileNameChanged_(event: Event) {
    const target = event.target as CrInputElement;

    if (target.invalid) {
      return;
    }

    this.manageProfilesBrowserProxy_.setProfileName(
        this.profileState.profilePath, target.value);

    target.blur();
  }

  /**
   * Handler for profile name keydowns.
   */
  private onProfileNameKeydown_(event: KeyboardEvent) {
    if (event.key === 'Escape' || event.key === 'Enter') {
      (event.target as HTMLElement).blur();
    }
  }

  /**
   * Handler for profile name blur.
   */
  private onProfileNameInputBlur_() {
    if (this.$.nameInput.invalid) {
      this.$.nameInput.value = this.profileState.localProfileName;
    }
  }
}

customElements.define(ProfileCardElement.is, ProfileCardElement);
