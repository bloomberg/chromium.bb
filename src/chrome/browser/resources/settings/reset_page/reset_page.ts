// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 */
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_shared_css.js';
import './reset_profile_dialog.js';

// <if expr="_google_chrome and is_win">
import '../chrome_cleanup_page/chrome_cleanup_page.js';
import '../incompatible_applications_page/incompatible_applications_page.js';
// </if>

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';

// <if expr="_google_chrome and is_win">
import {loadTimeData} from '../i18n_setup.js';
// </if>

import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {SettingsResetProfileDialogElement} from './reset_profile_dialog.js';

export interface SettingsResetPageElement {
  $: {
    resetProfileDialog: CrLazyRenderElement<SettingsResetProfileDialogElement>,
    resetProfile: HTMLElement,
  };
}

const SettingsResetPageElementBase =
    RouteObserverMixin(BaseMixin(PolymerElement)) as
    {new (): PolymerElement & RouteObserverMixinInterface};

export class SettingsResetPageElement extends SettingsResetPageElementBase {
  static get is() {
    return 'settings-reset-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: Object,

      // <if expr="_google_chrome and is_win">
      showIncompatibleApplications_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showIncompatibleApplications');
        },
      },
      // </if>
    };
  }

  // <if expr="_google_chrome and is_win">
  private showIncompatibleApplications_: boolean;
  // </if>

  /**
   * RouteObserverMixin
   */
  currentRouteChanged(route: Route) {
    const lazyRender = this.$.resetProfileDialog;

    if (route === routes.TRIGGERED_RESET_DIALOG ||
        route === routes.RESET_DIALOG) {
      lazyRender.get().show();
    } else {
      const dialog = lazyRender.getIfExists();
      if (dialog) {
        dialog.cancel();
      }
    }
  }

  private onShowResetProfileDialog_() {
    Router.getInstance().navigateTo(
        routes.RESET_DIALOG, new URLSearchParams('origin=userclick'));
  }

  private onResetProfileDialogClose_() {
    Router.getInstance().navigateToPreviousRoute();
    focusWithoutInk(assert(this.$.resetProfile));
  }

  // <if expr="_google_chrome and is_win">
  private onChromeCleanupTap_() {
    Router.getInstance().navigateTo(routes.CHROME_CLEANUP);
  }

  private onIncompatibleApplicationsTap_() {
    Router.getInstance().navigateTo(routes.INCOMPATIBLE_APPLICATIONS);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-page': SettingsResetPageElement;
  }
}

customElements.define(SettingsResetPageElement.is, SettingsResetPageElement);
