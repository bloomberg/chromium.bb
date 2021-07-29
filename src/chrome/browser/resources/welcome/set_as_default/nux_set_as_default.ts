// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// <if expr="is_win">
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
// </if>
import '../shared/animations_css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateToNextStep, NavigationMixin, NavigationMixinInterface} from '../navigation_mixin.js';
import {DefaultBrowserInfo, stepIndicatorModel} from '../shared/nux_types.js';

import {NuxSetAsDefaultProxy, NuxSetAsDefaultProxyImpl} from './nux_set_as_default_proxy.js';

const NuxSetAsDefaultElementBase =
    mixinBehaviors([WebUIListenerBehavior], NavigationMixin(PolymerElement)) as
    {new (): PolymerElement & WebUIListenerBehavior & NavigationMixinInterface};

/** @polymer */
export class NuxSetAsDefaultElement extends NuxSetAsDefaultElementBase {
  static get is() {
    return 'nux-set-as-default';
  }

  static get properties() {
    return {
      indicatorModel: Object,

      // <if expr="is_win">
      isWin10: {
        type: Boolean,
        value: loadTimeData.getBoolean('is_win10'),
      },
      // </if>

      subtitle: {
        type: String,
        value: loadTimeData.getString('setDefaultHeader'),
      },
    };
  }

  private browserProxy_: NuxSetAsDefaultProxy;
  private finalized_: boolean = false;
  private navigateToNextStep_: Function;
  indicatorModel?: stepIndicatorModel;

  constructor() {
    super();
    this.navigateToNextStep_ = navigateToNextStep;
    this.browserProxy_ = NuxSetAsDefaultProxyImpl.getInstance();
  }

  ready() {
    super.ready();

    this.addWebUIListener(
        'browser-default-state-changed',
        this.onDefaultBrowserChange_.bind(this));
  }

  onRouteEnter() {
    this.finalized_ = false;
    this.browserProxy_.recordPageShown();
  }

  onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAwayThroughBrowserHistory();
  }

  onRouteUnload() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAway();
  }

  private onDeclineClick_() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordSkip();
    this.finished_();
  }

  private onSetDefaultClick_() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordBeginSetDefault();
    this.browserProxy_.setAsDefault();
  }

  /**
   * Automatically navigate to the next onboarding step once default changed.
   */
  private onDefaultBrowserChange_(status: DefaultBrowserInfo) {
    if (status.isDefault) {
      this.browserProxy_.recordSuccessfullySetDefault();
      // Triggers toast in the containing welcome-app.
      this.dispatchEvent(new CustomEvent(
          'default-browser-change', {bubbles: true, composed: true}));
      this.finished_();
      return;
    }

    // <if expr="is_macosx">
    // On Mac OS, we do not get a notification when the default browser changes.
    // This will fake the notification.
    window.setTimeout(() => {
      this.browserProxy_.requestDefaultBrowserState().then(
          this.onDefaultBrowserChange_.bind(this));
    }, 100);
    // </if>
  }

  private finished_() {
    this.finalized_ = true;
    this.navigateToNextStep_();
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(NuxSetAsDefaultElement.is, NuxSetAsDefaultElement);
