// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import '../icons.js';
import '../settings_main/settings_main.js';
import '../settings_menu/settings_menu.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {CrContainerShadowMixin, CrContainerShadowMixinInterface} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {FindShortcutMixin, FindShortcutMixinInterface} from 'chrome://resources/cr_elements/find_shortcut_mixin.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {Debouncer, DomIf, html, PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {resetGlobalScrollTargetForTesting, setGlobalScrollTarget} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {PageVisibility, pageVisibility} from '../page_visibility.js';
import {SettingsPrefsElement} from '../prefs/prefs.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';
import {SettingsMainElement} from '../settings_main/settings_main.js';
import {SettingsMenuElement} from '../settings_menu/settings_menu.js';

declare global {
  interface HTMLElementEventMap {
    'scroll-to-top': CustomEvent<{top: number, callback: () => void}>;
    'scroll-to-bottom': CustomEvent<{bottom: number, callback: () => void}>;
    'refresh-pref': CustomEvent<string>;
  }

  interface Window {
    CrPolicyStrings: {[key: string]: string},
  }
}

export interface SettingsUiElement {
  $: {
    container: HTMLElement,
    drawer: CrDrawerElement,
    drawerTemplate: DomIf,
    leftMenu: SettingsMenuElement,
    main: SettingsMainElement,
    toolbar: CrToolbarElement,
    prefs: SettingsPrefsElement,
  };
}

const SettingsUiElementBase = RouteObserverMixin(CrContainerShadowMixin(
                                  FindShortcutMixin(PolymerElement))) as {
  new (): PolymerElement & RouteObserverMixinInterface &
  FindShortcutMixinInterface & CrContainerShadowMixinInterface
};

export class SettingsUiElement extends SettingsUiElementBase {
  static get is() {
    return 'settings-ui';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: Object,

      advancedOpenedInMain_: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onAdvancedOpenedInMainChanged_',
      },

      advancedOpenedInMenu_: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'onAdvancedOpenedInMenuChanged_',
      },

      toolbarSpinnerActive_: {
        type: Boolean,
        value: false,
      },

      narrow_: {
        type: Boolean,
        observer: 'onNarrowChanged_',
      },

      pageVisibility_: {type: Object, value: pageVisibility},

      lastSearchQuery_: {
        type: String,
        value: '',
      },
    };
  }

  private advancedOpenedInMain_: boolean;
  private advancedOpenedInMenu_: boolean;
  private toolbarSpinnerActive_: boolean;
  private narrow_: boolean;
  private pageVisibility_: PageVisibility;
  private lastSearchQuery_: string;
  private debouncer_: Debouncer|null = null;

  constructor() {
    super();

    Router.getInstance().initializeRouteFromUrl();
  }

  ready() {
    super.ready();

    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.if = true;
    });

    window.addEventListener('popstate', () => {
      this.$.drawer.cancel();
    });

    window.CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingExtensionWithoutName:
          loadTimeData.getString('controlledSettingExtensionWithoutName'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
      // <if expr="chromeos">
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      controlledSettingParent:
          loadTimeData.getString('controlledSettingParent'),
      controlledSettingChildRestriction:
          loadTimeData.getString('controlledSettingChildRestriction'),
      // </if>
    };

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });

    this.addEventListener('refresh-pref', this.onRefreshPref_.bind(this));
  }

  connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');

    setTimeout(function() {
      chrome.send(
          'metricsHandler:recordTime',
          ['Settings.TimeUntilInteractive', window.performance.now()]);
    });

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    // https://github.com/microsoft/TypeScript/issues/13569
    (document as any).fonts.load('bold 12px Roboto');
    setGlobalScrollTarget(this.$.container);

    const scrollToTop = (top: number) => new Promise<void>(resolve => {
      if (this.$.container.scrollTop === top) {
        resolve();
        return;
      }

      // When transitioning  back to main page from a subpage on ChromeOS,
      // using 'smooth' scroll here results in the scroll changing to whatever
      // is last value of |top|. This happens even after setting the scroll
      // position the UI or programmatically.
      const behavior = isChromeOS ? 'auto' : 'smooth';
      this.$.container.scrollTo({top: top, behavior: behavior});
      const onScroll = () => {
        this.debouncer_ =
            Debouncer.debounce(this.debouncer_, timeOut.after(75), () => {
              this.$.container.removeEventListener('scroll', onScroll);
              resolve();
            });
      };
      this.$.container.addEventListener('scroll', onScroll);
    });
    this.addEventListener(
        'scroll-to-top',
        (e: CustomEvent<{top: number, callback: () => void}>) => {
          scrollToTop(e.detail.top).then(e.detail.callback);
        });
    this.addEventListener(
        'scroll-to-bottom',
        (e: CustomEvent<{bottom: number, callback: () => void}>) => {
          scrollToTop(e.detail.bottom - this.$.container.clientHeight)
              .then(e.detail.callback);
        });
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    Router.getInstance().resetRouteForTesting();
    resetGlobalScrollTargetForTesting();
  }

  currentRouteChanged(route: Route) {
    if (document.documentElement.hasAttribute('enable-branding-update')) {
      if (route.depth <= 1) {
        // Main page uses scroll position to determine whether a shadow should
        // be shown.
        this.enableShadowBehavior(true);
      } else if (!route.isNavigableDialog) {
        // Sub-pages always show the top shadow, regardless of scroll position.
        this.enableShadowBehavior(false);
        this.showDropShadows();
      }
    }

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search') || '';
    if (urlSearchQuery === this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const toolbar =
        this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!;
    const searchField = toolbar.getSearchField();

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery !== searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  }

  // Override FindShortcutMixin methods.
  handleFindShortcut(modalContextOpen: boolean) {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!
        .getSearchField()
        .showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  searchInputHasFocus() {
    return this.shadowRoot!.querySelector<CrToolbarElement>('cr-toolbar')!
        .getSearchField()
        .isSearchFocused();
  }

  private onRefreshPref_(e: CustomEvent<string>) {
    return this.$.prefs.refresh(e.detail);
  }

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   */
  private onSearchChanged_(e: CustomEvent<string>) {
    const query = e.detail;
    Router.getInstance().navigateTo(
        routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  }

  /**
   * Called when a section is selected.
   */
  private onIronActivate_() {
    this.$.drawer.close();
  }

  private onMenuButtonTap_() {
    this.$.drawer.toggle();
  }

  /**
   * When this is called, The drawer animation is finished, and the dialog no
   * longer has focus. The selected section will gain focus if one was
   * selected. Otherwise, the drawer was closed due being canceled, and the
   * main settings container is given focus. That way the arrow keys can be
   * used to scroll the container, and pressing tab focuses a component in
   * settings.
   */
  private onMenuClose_() {
    if (!this.$.drawer.wasCanceled()) {
      // If a navigation happened, MainPageMixin#currentRouteChanged
      // handles focusing the corresponding section.
      return;
    }

    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  }

  private onAdvancedOpenedInMainChanged_() {
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  }

  private onAdvancedOpenedInMenuChanged_() {
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  }

  private onNarrowChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }

    const focusedElement = this.shadowRoot!.activeElement;
    if (this.narrow_ && focusedElement === this.$.leftMenu) {
      // If changed from non-narrow to narrow and the focus was on the left
      // menu, move focus to the button that opens the drawer menu.
      this.$.toolbar.focusMenuButton();
    } else if (!this.narrow_ && this.$.toolbar.isMenuFocused()) {
      // If changed from narrow to non-narrow and the focus was on the button
      // that opens the drawer menu, move focus to the left menu.
      this.$.leftMenu.focusFirstItem();
    } else if (
        !this.narrow_ &&
        focusedElement === this.shadowRoot!.querySelector('#drawerMenu')) {
      // If changed from narrow to non-narrow and the focus was in the drawer
      // menu, wait for the drawer to close and then move focus on the left
      // menu. The drawer has a dialog element in it so moving focus to an
      // element outside the dialog while it is open will not work.
      const boundCloseListener = () => {
        this.$.leftMenu.focusFirstItem();
        this.$.drawer.removeEventListener('close', boundCloseListener);
      };
      this.$.drawer.addEventListener('close', boundCloseListener);
    }
  }

  /**
   * Only used in tests.
   */
  getAdvancedOpenedInMainForTest(): boolean {
    return this.advancedOpenedInMain_;
  }

  /**
   * Only used in tests.
   */
  getAdvancedOpenedInMenuForTest(): boolean {
    return this.advancedOpenedInMenu_;
  }
}

customElements.define(SettingsUiElement.is, SettingsUiElement);
