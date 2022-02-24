// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './shared_style.js';
import './synced_device_card.js';
import './strings.m.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {FocusGrid} from 'chrome://resources/js/cr/ui/focus_grid.js';
import {FocusRow} from 'chrome://resources/js/cr/ui/focus_row.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Debouncer, html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import {SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
import {ForeignSession, ForeignSessionTab} from './externs.js';
import {HistorySyncedDeviceCardElement} from './synced_device_card.js';
import {getTemplate} from './synced_device_manager.html.js';

type ForeignDeviceInternal = {
  device: string,
  lastUpdateTime: string,
  opened: boolean,
  separatorIndexes: Array<number>,
  timestamp: number,
  tabs: Array<ForeignSessionTab>,
  tag: string,
};

declare global {
  interface HTMLElementEventMap {
    'synced-device-card-open-menu':
        CustomEvent<{tag: string, target: HTMLElement}>;
  }
}

export interface HistorySyncedDeviceManagerElement {
  $: {
    'menu': CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class HistorySyncedDeviceManagerElement extends PolymerElement {
  static get is() {
    return 'history-synced-device-manager';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sessionList: {
        type: Array,
        observer: 'updateSyncedDevices',
      },

      searchTerm: {
        type: String,
        observer: 'searchTermChanged',
      },

      /**
       * An array of synced devices with synced tab data.
       */
      syncedDevices_: Array,

      signInState: {
        type: Boolean,
        observer: 'signInStateChanged_',
      },

      guestSession_: Boolean,
      signInAllowed_: Boolean,
      fetchingSyncedTabs_: Boolean,
      hasSeenForeignData_: Boolean,

      /**
       * The session ID referring to the currently active action menu.
       */
      actionMenuModel_: String,
    };
  }

  private focusGrid_: FocusGrid|null = null;
  private syncedDevices_: Array<ForeignDeviceInternal> = [];
  private hasSeenForeignData_: boolean;
  private fetchingSyncedTabs_: boolean = false;
  private actionMenuModel_: string|null = null;
  private guestSession_: boolean = loadTimeData.getBoolean('isGuestSession');
  private signInAllowed_: boolean = loadTimeData.getBoolean('isSignInAllowed');
  private debouncer_: Debouncer|null = null;
  private signInState: boolean;

  searchTerm: string;
  sessionList: Array<ForeignSession>;

  ready() {
    super.ready();
    this.addEventListener('synced-device-card-open-menu', this.onOpenMenu_);
    this.addEventListener('update-focus-grid', this.updateFocusGrid_);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.focusGrid_ = new FocusGrid();

    // Update the sign in state.
    BrowserServiceImpl.getInstance().otherDevicesInitialized();
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.INITIALIZED,
        SyncedTabsHistogram.LIMIT);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.focusGrid_!.destroy();
  }

  /** @return {HTMLElement} */
  getContentScrollTarget() {
    return this;
  }

  private createInternalDevice_(session: ForeignSession):
      ForeignDeviceInternal {
    let tabs: Array<ForeignSessionTab> = [];
    const separatorIndexes = [];
    for (let i = 0; i < session.windows.length; i++) {
      const windowId = session.windows[i].sessionId;
      const newTabs = session.windows[i].tabs;
      if (newTabs.length === 0) {
        continue;
      }

      newTabs.forEach(function(tab) {
        tab.windowId = windowId;
      });

      let windowAdded = false;
      if (!this.searchTerm) {
        // Add all the tabs if there is no search term.
        tabs = tabs.concat(newTabs);
        windowAdded = true;
      } else {
        const searchText = this.searchTerm.toLowerCase();
        for (let j = 0; j < newTabs.length; j++) {
          const tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) !== -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
      }
      if (windowAdded && i !== session.windows.length - 1) {
        separatorIndexes.push(tabs.length - 1);
      }
    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      opened: true,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag,
    };
  }

  private onSignInTap_() {
    BrowserServiceImpl.getInstance().startSignInFlow();
  }

  private onOpenMenu_(e: CustomEvent<{tag: string, target: HTMLElement}>) {
    this.actionMenuModel_ = e.detail.tag;
    this.$.menu.get().showAt(e.detail.target);
    BrowserServiceImpl.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.SHOW_SESSION_MENU,
        SyncedTabsHistogram.LIMIT);
  }

  private onOpenAllTap_() {
    const menu = this.$.menu.getIfExists();
    assert(menu);
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.OPEN_ALL,
        SyncedTabsHistogram.LIMIT);
    assert(this.actionMenuModel_);
    browserService.openForeignSessionAllTabs(this.actionMenuModel_);
    this.actionMenuModel_ = null;
    menu.close();
  }

  private updateFocusGrid_() {
    if (!this.focusGrid_) {
      return;
    }

    this.focusGrid_.destroy();

    this.debouncer_ = Debouncer.debounce(this.debouncer_, microTask, () => {
      const cards =
          this.shadowRoot!.querySelectorAll('history-synced-device-card');
      Array.from(cards)
          .reduce(
              (prev: Array<FocusRow>, cur: HistorySyncedDeviceCardElement) =>
                  prev.concat(cur.createFocusRows()),
              [])
          .forEach((row) => {
            this.focusGrid_!.addRow(row);
          });
      this.focusGrid_!.ensureRowActive(1);
    });
  }

  private onDeleteSessionTap_() {
    const menu = this.$.menu.getIfExists();
    assert(menu);
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HIDE_FOR_NOW,
        SyncedTabsHistogram.LIMIT);
    assert(this.actionMenuModel_);
    browserService.deleteForeignSession(this.actionMenuModel_);
    this.actionMenuModel_ = null;
    menu.close();
  }

  private clearDisplayedSyncedDevices_() {
    this.syncedDevices_ = [];
  }

  /**
   * Decide whether or not should display no synced tabs message.
   */
  showNoSyncedMessage(
      signInState: boolean, syncedDevicesLength: number,
      guestSession: boolean): boolean {
    if (guestSession) {
      return true;
    }

    return signInState && syncedDevicesLength === 0;
  }

  /**
   * Shows the signin guide when the user is not signed in, signin is allowed
   * and not in a guest session.
   */
  showSignInGuide(
      signInState: boolean, guestSession: boolean,
      signInAllowed: boolean): boolean {
    const show = !signInState && !guestSession && signInAllowed;
    if (show) {
      BrowserServiceImpl.getInstance().recordAction(
          'Signin_Impression_FromRecentTabs');
    }

    return show;
  }

  /**
   * Decide what message should be displayed when user is logged in and there
   * are no synced tabs.
   */
  noSyncedTabsMessage(): string {
    let stringName = this.fetchingSyncedTabs_ ? 'loading' : 'noSyncedResults';
    if (this.searchTerm !== '') {
      stringName = 'noSearchResults';
    }
    return loadTimeData.getString(stringName);
  }

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   */
  updateSyncedDevices(sessionList: Array<ForeignSession>) {
    this.fetchingSyncedTabs_ = false;

    if (!sessionList) {
      return;
    }

    if (sessionList.length > 0 && !this.hasSeenForeignData_) {
      this.hasSeenForeignData_ = true;
      BrowserServiceImpl.getInstance().recordHistogram(
          SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HAS_FOREIGN_DATA,
          SyncedTabsHistogram.LIMIT);
    }

    const devices: Array<ForeignDeviceInternal> = [];
    sessionList.forEach((session) => {
      const device = this.createInternalDevice_(session);
      if (device.tabs.length !== 0) {
        devices.push(device);
      }
    });

    this.syncedDevices_ = devices;
  }

  /**
   * Get called when user's sign in state changes, this will affect UI of synced
   * tabs page. Sign in promo gets displayed when user is signed out, and
   * different messages are shown when there are no synced tabs.
   */
  signInStateChanged_(_current: boolean, previous?: boolean) {
    if (previous === undefined) {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'history-view-changed', {bubbles: true, composed: true}));

    // User signed out, clear synced device list and show the sign in promo.
    if (!this.signInState) {
      this.clearDisplayedSyncedDevices_();
      return;
    }
    // User signed in, show the loading message when querying for synced
    // devices.
    this.fetchingSyncedTabs_ = true;
  }

  searchTermChanged() {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices(this.sessionList);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-synced-device-manager': HistorySyncedDeviceManagerElement;
  }
}

customElements.define(
    HistorySyncedDeviceManagerElement.is, HistorySyncedDeviceManagerElement);
