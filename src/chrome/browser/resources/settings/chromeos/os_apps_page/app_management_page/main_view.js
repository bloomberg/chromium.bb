// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_item.js';
import './shared_style.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/shared_style_css.m.js';

import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {alphabeticalSort} from 'chrome://resources/cr_components/app_management/util.js';

import {Route} from '../../../router.js';
import {routes} from '../../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../../route_observer_behavior.js';

import {AppManagementStore} from './store.js';
import {AppManagementStoreClient, AppManagementStoreClientInterface} from './store_client.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const AppManagementMainViewElementBase = mixinBehaviors(
    [AppManagementStoreClient, RouteObserverBehavior], PolymerElement);

/** @polymer */
class AppManagementMainViewElement extends AppManagementMainViewElementBase {
  static get is() {
    return 'app-management-main-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {string}
       */
      searchTerm: {
        type: String,
      },

      /**
       * @private {AppMap}
       */
      apps_: {
        type: Object,
      },

      /**
       * List of apps displayed.
       * @private {Array<App>}
       */
      appList_: {
        type: Array,
        value: () => [],
        computed: 'computeAppList_(apps_, searchTerm)'
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    if (route === routes.APP_MANAGEMENT) {
      const appId = AppManagementStore.getInstance().data.selectedAppId;

      // Expect this to be false the first time the "Manage your apps" page
      // is requested as no app has been selected yet.
      if (appId) {
        const button =
            this.shadowRoot.querySelector(`#app-subpage-button-${appId}`);
        if (button) {
          focusWithoutInk(button);
        }
      }
    }
  }

  /**
   * @private
   * @param {Array<App>} appList
   * @return {boolean}
   */
  isAppListEmpty_(appList) {
    return appList.length === 0;
  }

  /**
   * @private
   * @param {AppMap} apps
   * @param {String} searchTerm
   * @return {Array<App>}
   */
  computeAppList_(apps, searchTerm) {
    if (!apps) {
      return [];
    }

    // This is calculated locally as once the user leaves this page the state
    // should reset.
    const appArray = Object.values(apps);

    let filteredApps;
    if (searchTerm) {
      const lowerCaseSearchTerm = searchTerm.toLowerCase();
      filteredApps = appArray.filter(app => {
        assert(app.title);
        return app.title.toLowerCase().includes(lowerCaseSearchTerm);
      });
    } else {
      filteredApps = appArray;
    }

    filteredApps.sort(
        (a, b) => alphabeticalSort(
            /** @type {string} */ (a.title), /** @type {string} */ (b.title)));

    return filteredApps;
  }
}

customElements.define(
    AppManagementMainViewElement.is, AppManagementMainViewElement);
