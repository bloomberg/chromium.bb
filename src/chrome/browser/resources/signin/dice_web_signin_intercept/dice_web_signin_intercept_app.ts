// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl, InterceptionParameters} from './dice_web_signin_intercept_browser_proxy.js';

const DiceWebSigninInterceptAppElementBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement) as
    {new (): PolymerElement & WebUIListenerBehavior};

export class DiceWebSigninInterceptAppElement extends
    DiceWebSigninInterceptAppElementBase {
  static get is() {
    return 'dice-web-signin-intercept-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      interceptionParameters_: {
        type: Object,
        value: null,
      },

      acceptButtonClicked_: {
        type: Boolean,
        value: false,
      },

      guestLink_: {
        type: String,
        value() {
          return loadTimeData.getString('guestLink');
        },
      },
    };
  }

  private interceptionParameters_: InterceptionParameters;
  private acceptButtonClicked_: boolean;
  private guestLink_: string;
  private diceWebSigninInterceptBrowserProxy_:
      DiceWebSigninInterceptBrowserProxy =
          DiceWebSigninInterceptBrowserProxyImpl.getInstance();

  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'interception-parameters-changed',
        this.handleParametersChanged_.bind(this));
    this.diceWebSigninInterceptBrowserProxy_.pageLoaded().then(
        parameters => this.onPageLoaded_(parameters));
  }

  private onPageLoaded_(parameters: InterceptionParameters) {
    this.handleParametersChanged_(parameters);
    afterNextRender(this, () => {
      // |showGuestOption| is constant during the lifetime of this bubble,
      // therefore it's safe to set the listener only during initialization.
      if (this.interceptionParameters_.showGuestOption) {
        this.shadowRoot!.querySelector('#footer-description a')!
            .addEventListener('click', () => this.onGuest_());
      }
    });
  }

  private onAccept_() {
    this.acceptButtonClicked_ = true;
    this.diceWebSigninInterceptBrowserProxy_.accept();
  }

  private onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  }

  private onGuest_() {
    if (this.acceptButtonClicked_) {
      return;
    }
    this.acceptButtonClicked_ = true;
    this.diceWebSigninInterceptBrowserProxy_.guest();
  }

  /** Called when the interception parameters are updated. */
  private handleParametersChanged_(parameters: InterceptionParameters) {
    this.interceptionParameters_ = parameters;
    this.style.setProperty(
        '--header-background-color', parameters.headerBackgroundColor);
    this.style.setProperty('--header-text-color', parameters.headerTextColor);
    this.notifyPath('interceptionParameters_.interceptedAccount.isManaged');
  }
}

customElements.define(
    DiceWebSigninInterceptAppElement.is, DiceWebSigninInterceptAppElement);
