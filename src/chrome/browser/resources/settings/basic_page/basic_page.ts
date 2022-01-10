// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../appearance_page/appearance_page.js';
import '../privacy_page/privacy_page.js';
import '../privacy_page/privacy_review_promo.js';
import '../safety_check_page/safety_check_page.js';
import '../autofill_page/autofill_page.js';
import '../controls/settings_idle_load.js';
import '../on_startup_page/on_startup_page.js';
import '../people_page/people_page.js';
import '../reset_page/reset_profile_banner.js';
import '../search_page/search_page.js';
import '../settings_page/settings_section.js';
import '../settings_page_css.js';
// <if expr="chromeos or lacros">
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
// </if>

// <if expr="not chromeos and not lacros">
import '../default_browser_page/default_browser_page.js';

// </if>

import {assert} from 'chrome://resources/js/assert.m.js';
import {beforeNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsIdleLoadElement} from '../controls/settings_idle_load.js';
import {loadTimeData} from '../i18n_setup.js';
import {PageVisibility} from '../page_visibility.js';
// <if expr="chromeos or lacros">
import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js';
// </if>
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';
import {getSearchManager, SearchResult} from '../search_settings.js';
import {MainPageMixin, MainPageMixinInterface} from '../settings_page/main_page_mixin.js';

// <if expr="chromeos or lacros">
const OS_BANNER_INTERACTION_METRIC_NAME =
    'ChromeOS.Settings.OsBannerInteraction';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const CrosSettingsOsBannerInteraction = {
  NotShown: 0,
  Shown: 1,
  Clicked: 2,
  Closed: 3,
};
// </if>

// TODO(crbug.com/1234307): Remove when RouteObserverMixin is converted to
// TypeScript.
type Constructor<T> = new (...args: any[]) => T;

// <if expr="chromeos or lacros">
const SettingsBasicPageElementBase =
    PrefsMixin(MainPageMixin(
        RouteObserverMixin(PolymerElement) as unknown as
        Constructor<PolymerElement>)) as unknown as {
      new (): PolymerElement & PrefsMixinInterface &
      RouteObserverMixinInterface & MainPageMixinInterface
    };
// </if>
// <if expr="not chromeos and not lacros">
const SettingsBasicPageElementBase = MainPageMixin(
                                         RouteObserverMixin(PolymerElement) as
                                         unknown as
                                         Constructor<PolymerElement>) as {
  new (): PolymerElement & RouteObserverMixinInterface & MainPageMixinInterface
};
// </if>

export class SettingsBasicPageElement extends SettingsBasicPageElementBase {
  static get is() {
    return 'settings-basic-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: {
        type: Object,
        value() {
          return {};
        },
      },

      /**
       * Whether a search operation is in progress or previous search
       * results are being displayed.
       */
      inSearchMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      advancedToggleExpanded: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'advancedToggleExpandedChanged_',
      },

      /**
       * True if a section is fully expanded to hide other sections beneath it.
       * False otherwise (even while animating a section open/closed).
       */
      hasExpandedSection_: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the basic page should currently display the reset profile
       * banner.
       */
      showResetProfileBanner_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showResetProfileBanner');
        },
      },

      // <if expr="chromeos or lacros">
      showOSSettingsBanner_: {
        type: Boolean,
        computed: 'computeShowOSSettingsBanner_(' +
            'prefs.settings.cros.show_os_banner.value, currentRoute_)',
      },
      // </if>

      currentRoute_: Object,

      /**
       * Used to avoid handling a new toggle while currently toggling.
       */
      advancedTogglingInProgress_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  pageVisibility: PageVisibility;
  inSearchMode: boolean;
  advancedToggleExpanded: boolean;
  private hasExpandedSection_: boolean;
  private showResetProfileBanner_: boolean;

  // <if expr="chromeos or lacros">
  private showOSSettingsBanner_: boolean;
  private osBannerShowMetricRecorded_: boolean = false;
  // </if>

  private currentRoute_: Route;
  private advancedTogglingInProgress_: boolean;

  ready() {
    super.ready();

    this.setAttribute('role', 'main');
    this.addEventListener('subpage-expand', this.onSubpageExpanded_);
  }


  connectedCallback() {
    super.connectedCallback();

    this.currentRoute_ = Router.getInstance().getCurrentRoute();
  }

  currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    if (routes.ADVANCED && routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section !== oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    super.currentRouteChanged(newRoute, oldRoute);
  }

  /**
   * Override MainPageMixin method.
   */
  containsRoute(route: Route|null): boolean {
    return !route || routes.BASIC.contains(route) ||
        routes.ADVANCED.contains(route);
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private getIdleLoad_(): Promise<Element> {
    return (this.shadowRoot!.querySelector('#advancedPageTemplate') as
            SettingsIdleLoadElement)
        .get();
  }

  private showPrivacyReviewPromo_(visibility: boolean|undefined): boolean {
    // TODO(crbug/1215630): Only show on the first look at the privacy page.
    return this.showPage_(visibility) &&
        loadTimeData.getBoolean('privacyReviewEnabled');
  }

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param query The text to search for.
   * @return A signal indicating that searching finished.
   */
  searchContents(query: string): Promise<SearchResult> {
    const whenSearchDone = [
      getSearchManager().search(
          query,
          assert(this.shadowRoot!.querySelector('#basicPage') as HTMLElement)),
    ];

    if (this.pageVisibility.advancedSettings !== false) {
      whenSearchDone.push(this.getIdleLoad_().then(function(advancedPage) {
        return getSearchManager().search(query, advancedPage);
      }));
    }

    return Promise.all(whenSearchDone).then(function(requests) {
      // Combine the SearchRequests results to a single SearchResult object.
      return {
        canceled: requests.some(function(r) {
          return r.canceled;
        }),
        didFindMatches: requests.some(function(r) {
          return r.didFindMatches();
        }),
        // All requests correspond to the same user query, so only need to check
        // one of them.
        wasClearSearch: requests[0].isSame(''),
      };
    });
  }

  // <if expr="chromeos or lacros">
  private computeShowOSSettingsBanner_(): boolean|undefined {
    // this.prefs is implicitly used by this.getPref() below.
    if (!this.prefs || !this.currentRoute_) {
      return;
    }
    const showPref = this.getPref('settings.cros.show_os_banner').value;

    // Banner only shows on the main page because direct navigations to a
    // sub-page are unlikely to be due to a user looking for an OS setting.
    const show = showPref && !this.currentRoute_.isSubpage();

    // Record the show metric once. We can't record the metric in attached()
    // because prefs might not be ready yet.
    if (!this.osBannerShowMetricRecorded_) {
      chrome.metricsPrivate.recordEnumerationValue(
          OS_BANNER_INTERACTION_METRIC_NAME,
          show ? CrosSettingsOsBannerInteraction.Shown :
                 CrosSettingsOsBannerInteraction.NotShown,
          Object.keys(CrosSettingsOsBannerInteraction).length);
      this.osBannerShowMetricRecorded_ = true;
    }
    return show;
  }

  private onOSSettingsBannerClick_() {
    // The label has a link that opens the page, so just record the metric.
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Clicked,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  }

  private onOSSettingsBannerClosed_() {
    this.setPrefValue('settings.cros.show_os_banner', false);
    chrome.metricsPrivate.recordEnumerationValue(
        OS_BANNER_INTERACTION_METRIC_NAME,
        CrosSettingsOsBannerInteraction.Closed,
        Object.keys(CrosSettingsOsBannerInteraction).length);
  }
  // </if>

  // <if expr="chromeos">
  private onOpenChromeOSLanguagesSettingsClick_() {
    const chromeOSLanguagesSettingsPath =
        loadTimeData.getString('chromeOSLanguagesSettingsPath');
    window.location.href =
        `chrome://os-settings/${chromeOSLanguagesSettingsPath}`;
  }
  // </if>

  private onResetProfileBannerClosed_() {
    this.showResetProfileBanner_ = false;
  }

  /**
   * Hides everything but the newly expanded subpage.
   */
  private onSubpageExpanded_() {
    this.hasExpandedSection_ = true;
  }

  /**
   * Render the advanced page now (don't wait for idle).
   */
  private advancedToggleExpandedChanged_() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // beforeNextRender() must be used instead.
    beforeNextRender(this, () => {
      this.getIdleLoad_();
    });
  }

  private fire_(eventName: string, detail: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * @return Whether to show the basic page, taking into account both routing
   *     and search state.
   */
  private showBasicPage_(
      currentRoute: Route, _inSearchMode: boolean,
      hasExpandedSection: boolean): boolean {
    return !hasExpandedSection || routes.BASIC.contains(currentRoute);
  }

  /**
   * @return Whether to show the advanced page, taking into account both routing
   *     and search state.
   */
  private showAdvancedPage_(
      currentRoute: Route, inSearchMode: boolean, hasExpandedSection: boolean,
      advancedToggleExpanded: boolean): boolean {
    return hasExpandedSection ?
        (routes.ADVANCED && routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded || inSearchMode;
  }

  private showAdvancedSettings_(visibility?: boolean): boolean {
    return visibility !== false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-basic-page': SettingsBasicPageElement;
  }
}

customElements.define(SettingsBasicPageElement.is, SettingsBasicPageElement);
