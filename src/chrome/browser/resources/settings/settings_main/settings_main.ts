// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/js/search_highlight_utils.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../about_page/about_page.js';
import '../basic_page/basic_page.js';
import '../prefs/prefs.js';
import '../search_settings.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

type MainPageVisibility = {
  about: boolean,
  settings: boolean,
};

const SettingsMainElementBase = RouteObserverMixin(PolymerElement) as
    {new (): PolymerElement & RouteObserverMixinInterface};

export class SettingsMainElement extends SettingsMainElementBase {
  static get is() {
    return 'settings-main';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      advancedToggleExpanded: {
        type: Boolean,
        notify: true,
      },

      /**
       * Controls which main pages are displayed via dom-ifs, based on the
       * current route.
       */
      showPages_: {
        type: Object,
        value() {
          return {about: false, settings: false};
        },
      },

      /**
       * Whether a search operation is in progress or previous search results
       * are being displayed.
       */
      inSearchMode_: {
        type: Boolean,
        value: false,
      },

      showNoResultsFound_: {
        type: Boolean,
        value: false,
      },

      showingSubpage_: Boolean,

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: Object,
    };
  }

  advancedToggleExpanded: boolean;
  private showPages_: MainPageVisibility;
  private inSearchMode_: boolean;
  private showNoResultsFound_: boolean;
  private showingSubpage_: boolean;
  toolbarSpinnerActive: boolean;
  pageVisibility: PageVisibility;

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   */
  currentRouteChanged(newRoute: Route) {
    const inAbout =
        routes.ABOUT.contains(Router.getInstance().getCurrentRoute());
    this.showPages_ = {about: inAbout, settings: !inAbout};

    if (!newRoute.isSubpage()) {
      document.title = inAbout ? loadTimeData.getStringF(
                                     'settingsAltPageTitle',
                                     loadTimeData.getString('aboutPageTitle')) :
                                 loadTimeData.getString('settings');
    }
  }

  private onShowingSubpage_() {
    this.showingSubpage_ = true;
  }

  private onShowingMainPage_() {
    this.showingSubpage_ = false;
  }

  /**
   * @return A promise indicating that searching finished.
   */
  searchContents(query: string): Promise<void> {
    // Trigger rendering of the basic and advanced pages and search once ready.
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;

    return new Promise((resolve, _reject) => {
      setTimeout(() => {
        const page = this.shadowRoot!.querySelector('settings-basic-page')!;
        page.searchContents(query).then(result => {
          resolve();
          if (result.canceled) {
            // Nothing to do here. A previous search request was canceled
            // because a new search request was issued with a different query
            // before the previous completed.
            return;
          }

          this.toolbarSpinnerActive = false;
          this.inSearchMode_ = !result.wasClearSearch;
          this.showNoResultsFound_ =
              this.inSearchMode_ && !result.didFindMatches;

          if (this.inSearchMode_) {
            IronA11yAnnouncer.requestAvailability();
            this.dispatchEvent(new CustomEvent('iron-announce', {
              bubbles: true,
              composed: true,
              detail: {
                text: this.showNoResultsFound_ ?
                    loadTimeData.getString('searchNoResults') :
                    loadTimeData.getStringF('searchResults', query)
              }
            }));
          }
        });
      }, 0);
    });
  }

  private showManagedHeader_(): boolean {
    return !this.inSearchMode_ && !this.showingSubpage_ &&
        !this.showPages_.about;
  }
}

customElements.define(SettingsMainElement.is, SettingsMainElement);
