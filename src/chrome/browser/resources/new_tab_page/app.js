// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './iframe.js';
import './realbox/realbox.js';
import './logo.js';
import './modules/modules.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BackgroundManager} from './background_manager.js';
import {BackgroundSelection, BackgroundSelectionType, CustomizeDialogPage} from './customize_dialog_types.js';
import {I18nBehavior, loadTimeData} from './i18n_setup.js';
import {recordLoadDuration} from './metrics_utils.js';
import {ModuleRegistry} from './modules/module_registry.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {PromoBrowserCommandProxy} from './promo_browser_command_proxy.js';
import {$$} from './utils.js';
import {Action as VoiceAction, recordVoiceAction} from './voice_search_overlay.js';
import {WindowProxy} from './window_proxy.js';

/**
 * @typedef {{
 *   commandId: promoBrowserCommand.mojom.Command<number>,
 *   clickInfo: !promoBrowserCommand.mojom.ClickInfo
 * }}
 */
let CommandData;

/**
 * Elements on the NTP. This enum must match the numbering for NTPElement in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 * @enum {number}
 */
export const NtpElement = {
  kOther: 0,
  kBackground: 1,
  kOneGoogleBar: 2,
  kLogo: 3,
  kRealbox: 4,
  kMostVisited: 5,
  kMiddleSlotPromo: 6,
  kModule: 7,
  kCustomize: 8,
};

/** @param {NtpElement} element */
function recordClick(element) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Click', element, Object.keys(NtpElement).length);
}

// Adds a <script> tag that holds the lazy loaded code.
function ensureLazyLoaded() {
  const script = document.createElement('script');
  script.type = 'module';
  script.src = './lazy_load.js';
  document.body.appendChild(script);
}

/**
 * @polymer
 * @extends {PolymerElement}
 */
class AppElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      oneGoogleBarIframePath_: {
        type: String,
        value: () => {
          const params = new URLSearchParams();
          params.set(
              'paramsencoded',
              btoa(window.location.search.replace(/^[?]/, '&')));
          return `chrome-untrusted://new-tab-page/one-google-bar?${params}`;
        },
      },

      /** @private */
      oneGoogleBarLoaded_: {
        type: Boolean,
        observer: 'notifyOneGoogleBarDarkThemeEnabledChange_',
      },

      /** @private */
      oneGoogleBarDarkThemeEnabled_: {
        type: Boolean,
        computed: `computeOneGoogleBarDarkThemeEnabled_(oneGoogleBarLoaded_,
            theme_, backgroundSelection_)`,
        observer: 'notifyOneGoogleBarDarkThemeEnabledChange_',
      },

      /** @private {!newTabPage.mojom.Theme} */
      theme_: {
        observer: 'onThemeChange_',
        type: Object,
      },

      /** @private */
      showCustomizeDialog_: Boolean,

      /** @private {?string} */
      selectedCustomizeDialogPage_: String,

      /** @private */
      showVoiceSearchOverlay_: Boolean,

      /** @private */
      showBackgroundImage_: {
        computed: 'computeShowBackgroundImage_(theme_, backgroundSelection_)',
        observer: 'onShowBackgroundImageChange_',
        reflectToAttribute: true,
        type: Boolean,
      },

      /** @private {!BackgroundSelection} */
      backgroundSelection_: {
        type: Object,
        value: () => ({type: BackgroundSelectionType.NO_SELECTION}),
        observer: 'updateBackgroundImagePath_',
      },

      /** @private */
      backgroundImageAttribution1_: {
        type: String,
        computed: `computeBackgroundImageAttribution1_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttribution2_: {
        type: String,
        computed: `computeBackgroundImageAttribution2_(theme_,
            backgroundSelection_)`,
      },

      /** @private */
      backgroundImageAttributionUrl_: {
        type: String,
        computed: `computeBackgroundImageAttributionUrl_(theme_,
            backgroundSelection_)`,
      },

      /** @private {SkColor} */
      backgroundColor_: {
        computed: 'computeBackgroundColor_(showBackgroundImage_, theme_)',
        type: Object,
      },

      /** @private */
      logoColor_: {
        type: String,
        computed: 'computeLogoColor_(theme_, backgroundSelection_)',
      },

      /** @private */
      singleColoredLogo_: {
        computed: 'computeSingleColoredLogo_(theme_, backgroundSelection_)',
        type: Boolean,
      },

      /** @private */
      realboxShown_: {
        type: Boolean,
        computed: 'computeRealboxShown_(theme_)',
      },

      /** @private */
      logoEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('logoEnabled'),
      },

      /** @private */
      shortcutsEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shortcutsEnabled'),
      },

      /** @private */
      middleSlotPromoEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('middleSlotPromoEnabled'),
      },

      /** @private */
      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
      },

      /** @private */
      modulesRedesignedEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesRedesignedEnabled'),
        reflectToAttribute: true,
      },

      /** @private */
      middleSlotPromoLoaded_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      modulesLoaded_: {
        type: Boolean,
        value: false,
      },

      /**
       * In order to avoid flicker, the promo and modules are hidden until both
       * are loaded. If modules are disabled, the promo is shown as soon as it
       * is loaded.
       * @private
       */
      promoAndModulesLoaded_: {
        type: Boolean,
        computed: `computePromoAndModulesLoaded_(middleSlotPromoLoaded_,
            modulesLoaded_)`,
        observer: 'onPromoAndModulesLoadedChange_',
      },

      /**
       * If true, renders additional elements that were not deemed crucial to
       * to show up immediately on load.
       * @private
       */
      lazyRender_: Boolean,
    };
  }

  constructor() {
    performance.mark('app-creation-start');
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = NewTabPageProxy.getInstance().callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    /** @private {!BackgroundManager} */
    this.backgroundManager_ = BackgroundManager.getInstance();
    /** @private {?number} */
    this.setThemeListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
    /** @private {boolean} */
    this.shouldPrintPerformance_ =
        new URLSearchParams(location.search).has('print_perf');
    /**
     * Initialized with the start of the performance timeline in case the
     * background image load is not triggered by JS.
     * @private {number}
     */
    this.backgroundImageLoadStartEpoch_ = performance.timeOrigin;
    /** @private {number} */
    this.backgroundImageLoadStart_ = 0;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        this.callbackRouter_.setTheme.addListener(theme => {
          performance.measure('theme-set');
          this.theme_ = theme;
        });
    this.eventTracker_.add(window, 'message', (event) => {
      /** @type {!Object} */
      const data = event.data;
      // Something in OneGoogleBar is sending a message that is received here.
      // Need to ignore it.
      if (typeof data !== 'object') {
        return;
      }
      if ('frameType' in data && data.frameType === 'one-google-bar') {
        this.handleOneGoogleBarMessage_(event);
      }
    });
    this.eventTracker_.add(window, 'keydown', e => this.onWindowKeydown_(e));
    this.eventTracker_.add(
        window, 'click', e => this.onWindowClick_(e), /*capture=*/ true);
    if (this.shouldPrintPerformance_) {
      // It is possible that the background image has already loaded by now.
      // If it has, we request it to re-send the load time so that we can
      // actually catch the load time.
      this.backgroundManager_.getBackgroundImageLoadTime().then(
          time => {
            const duration = time - this.backgroundImageLoadStartEpoch_;
            this.printPerformanceDatum_(
                'background-image-load', this.backgroundImageLoadStart_,
                duration);
            this.printPerformanceDatum_(
                'background-image-loaded',
                this.backgroundImageLoadStart_ + duration);
          },
          () => {
            console.error('Failed to capture background image load time');
          });
    }
    FocusOutlineManager.forDocument(document);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(assert(this.setThemeListenerId_));
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    this.pageHandler_.onAppRendered(WindowProxy.getInstance().now());
    // Let the browser breath and then render remaining elements.
    WindowProxy.getInstance().waitForLazyRender().then(() => {
      ensureLazyLoaded();
      this.lazyRender_ = true;
    });
    this.printPerformance_();
    performance.measure('app-creation', 'app-creation-start');
  }

  /**
   * @return {boolean}
   * @private
   */
  computeOneGoogleBarDarkThemeEnabled_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_ && this.theme_.isDark;
    }
  }

  /** @private */
  notifyOneGoogleBarDarkThemeEnabledChange_() {
    if (this.oneGoogleBarLoaded_) {
      $$(this, '#oneGoogleBar').postMessage({
        type: 'enableDarkTheme',
        enabled: this.oneGoogleBarDarkThemeEnabled_,
      });
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution1_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution1 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution1;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttribution2_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttribution2 || '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attribution2;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {string}
   * @private
   */
  computeBackgroundImageAttributionUrl_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return this.theme_ && this.theme_.backgroundImageAttributionUrl ?
            this.theme_.backgroundImageAttributionUrl.url :
            '';
      case BackgroundSelectionType.IMAGE:
        return this.backgroundSelection_.image.attributionUrl.url;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return '';
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeRealboxShown_() {
    // If realbox is to match the Omnibox's theme, keep it hidden until the
    // theme arrives. Otherwise mismatching colors will cause flicker.
    return !loadTimeData.getBoolean('realboxMatchOmniboxTheme') ||
        !!this.theme_;
  }

  /**
   * @return {boolean}
   * @private
   */
  computePromoAndModulesLoaded_() {
    return (!loadTimeData.getBoolean('middleSlotPromoEnabled') ||
            this.middleSlotPromoLoaded_) &&
        (!loadTimeData.getBoolean('modulesEnabled') || this.modulesLoaded_);
  }

  /** @private */
  async onLazyRendered_() {
    // Instantiate modules even if |modulesEnabled| is false to counterfactually
    // trigger a HaTS survey in a potential control group.
    if (!loadTimeData.getBoolean('modulesLoadEnabled') ||
        loadTimeData.getBoolean('modulesEnabled')) {
      return;
    }
    const modules = await ModuleRegistry.getInstance().initializeModules(
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      this.pageHandler_.onModulesLoadedWithData();
    }
  }

  /** @private */
  onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    recordVoiceAction(VoiceAction.kActivateSearchBox);
  }

  /** @private */
  onCustomizeClick_() {
    this.showCustomizeDialog_ = true;
  }

  /** @private */
  onCustomizeDialogClose_() {
    this.showCustomizeDialog_ = false;
    // Let customize dialog decide what page to show on next open.
    this.selectedCustomizeDialogPage_ = null;
  }

  /** @private */
  onVoiceSearchOverlayClose_() {
    this.showVoiceSearchOverlay_ = false;
  }

  /**
   * Handles <CTRL> + <SHIFT> + <.> (also <CMD> + <SHIFT> + <.> on mac) to open
   * voice search.
   * @param {KeyboardEvent} e
   * @private
   */
  onWindowKeydown_(e) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.code === 'Period' && e.shiftKey) {
      this.showVoiceSearchOverlay_ = true;
      recordVoiceAction(VoiceAction.kActivateKeyboard);
    }
  }

  /**
   * @param {SkColor} skColor
   * @return {string}
   * @private
   */
  rgbaOrInherit_(skColor) {
    return skColor ? skColorToRgba(skColor) : 'inherit';
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowBackgroundImage_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        return !!this.theme_ && !!this.theme_.backgroundImage;
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return false;
    }
  }

  /** @private */
  onShowBackgroundImageChange_() {
    this.backgroundManager_.setShowBackgroundImage(this.showBackgroundImage_);
  }

  /** @private */
  onThemeChange_() {
    if (this.theme_) {
      this.backgroundManager_.setBackgroundColor(this.theme_.backgroundColor);
    }
    this.updateBackgroundImagePath_();
  }

  /** @private */
  onPromoAndModulesLoadedChange_() {
    if (this.promoAndModulesLoaded_) {
      recordLoadDuration(
          'NewTabPage.Modules.ShownTime', WindowProxy.getInstance().now());
    }
  }

  /**
   * Set the #backgroundImage |path| only when different and non-empty. Reset
   * the customize dialog background selection if the dialog is closed.
   *
   * The ntp-untrusted-iframe |path| is set directly. When using a data binding
   * instead, the quick updates to the |path| result in iframe loading an error
   * page.
   * @private
   */
  updateBackgroundImagePath_() {
    // The |backgroundSelection_| is retained after the dialog commits the
    // change to the theme. Since |backgroundSelection_| has precedence over
    // the theme background, the |backgroundSelection_| needs to be reset when
    // the theme is updated. This is only necessary when the dialog is closed.
    // If the dialog is open, it will either commit the |backgroundSelection_|
    // or reset |backgroundSelection_| on cancel.
    //
    // Update after background image path is updated so the image is not shown
    // before the path is updated.
    if (!this.showCustomizeDialog_ &&
        this.backgroundSelection_.type !==
            BackgroundSelectionType.NO_SELECTION) {
      // Wait when local image is selected, then no background is previewed
      // followed by selecting a new local image. This avoids a flicker. The
      // iframe with the old image is shown briefly before it navigates to a new
      // iframe location, then fetches and renders the new local image.
      if (this.backgroundSelection_.type ===
          BackgroundSelectionType.NO_BACKGROUND) {
        setTimeout(() => {
          this.backgroundSelection_ = {
            type: BackgroundSelectionType.NO_SELECTION
          };
        }, 100);
      } else {
        this.backgroundSelection_ = {
          type: BackgroundSelectionType.NO_SELECTION
        };
      }
    }
    /** @type {newTabPage.mojom.BackgroundImage|undefined} */
    let backgroundImage;
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.NO_SELECTION:
        backgroundImage = this.theme_ && this.theme_.backgroundImage;
        break;
      case BackgroundSelectionType.IMAGE:
        backgroundImage = {
          url: {url: this.backgroundSelection_.image.imageUrl.url}
        };
        break;
    }
    if (backgroundImage) {
      this.backgroundManager_.setBackgroundImage(backgroundImage);
    }
  }

  /**
   * @return {SkColor}
   * @private
   */
  computeBackgroundColor_() {
    if (this.showBackgroundImage_) {
      return null;
    }
    return this.theme_ && this.theme_.backgroundColor;
  }

  /**
   * @return {SkColor}
   * @private
   */
  computeLogoColor_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return hexColorToSkColor('#ffffff');
      case BackgroundSelectionType.NO_SELECTION:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.DAILY_REFRESH:
      default:
        return this.theme_ &&
            (this.theme_.logoColor ||
             (this.theme_.isDark ? hexColorToSkColor('#ffffff') : null));
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSingleColoredLogo_() {
    switch (this.backgroundSelection_.type) {
      case BackgroundSelectionType.IMAGE:
        return true;
      case BackgroundSelectionType.DAILY_REFRESH:
      case BackgroundSelectionType.NO_BACKGROUND:
      case BackgroundSelectionType.NO_SELECTION:
      default:
        return this.theme_ && (!!this.theme_.logoColor || this.theme_.isDark);
    }
  }

  /**
   * Sends the command received from the given source and origin to the browser.
   * Relays the browser response to whether or not a promo containing the given
   * command can be shown back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the response back to the source promo
   * frame and should not be used for anything else.
   * @param {Object} messageData Data received from the source promo frame.
   * @param {Window} commandSource Source promo frame.
   * @param {string} commandOrigin Origin of the source promo frame.
   * @private
   */
  canShowPromoWithBrowserCommand_(messageData, commandSource, commandOrigin) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!promoBrowserCommand.mojom.Command} */
    const commandId = Object.values(promoBrowserCommand.mojom.Command)
                          .includes(messageData.commandId) ?
        messageData.commandId :
        promoBrowserCommand.mojom.Command.kUnknownCommand;

    PromoBrowserCommandProxy.getInstance()
        .handler.canShowPromoWithCommand(commandId)
        .then(({canShow}) => {
          const response = {messageType: messageData.messageType};
          response[messageData.commandId] = canShow;
          commandSource.postMessage(response, commandOrigin);
        });
  }

  /**
   * Sends the command and the accompanying mouse click info received from the
   * promo of the given source and origin to the browser. Relays the execution
   * status response back to the source promo frame. |commandSource| and
   * |commandOrigin| are used only to send the execution status response back to
   * the source promo frame and should not be used for anything else.
   * @param {!CommandData} commandData Command and mouse click info.
   * @param {Window} commandSource Source promo frame.
   * @param {string} commandOrigin Origin of the source promo frame.
   * @private
   */
  executePromoBrowserCommand_(commandData, commandSource, commandOrigin) {
    // Make sure we don't send unsupported commands to the browser.
    /** @type {!promoBrowserCommand.mojom.Command} */
    const commandId = Object.values(promoBrowserCommand.mojom.Command)
                          .includes(commandData.commandId) ?
        commandData.commandId :
        promoBrowserCommand.mojom.Command.kUnknownCommand;

    PromoBrowserCommandProxy.getInstance()
        .handler.executeCommand(commandId, commandData.clickInfo)
        .then(({commandExecuted}) => {
          commandSource.postMessage(commandExecuted, commandOrigin);
        });
  }

  /**
   * Handles messages from the OneGoogleBar iframe. The messages that are
   * handled include show bar on load and overlay updates.
   *
   * 'overlaysUpdated' message includes the updated array of overlay rects that
   * are shown.
   * @param {!MessageEvent} event
   * @private
   */
  handleOneGoogleBarMessage_(event) {
    /** @type {!Object} */
    const data = event.data;
    if (data.messageType === 'loaded') {
      const oneGoogleBar = $$(this, '#oneGoogleBar');
      oneGoogleBar.style.clipPath = 'url(#oneGoogleBarClipPath)';
      oneGoogleBar.style.zIndex = '1000';
      this.oneGoogleBarLoaded_ = true;
      this.pageHandler_.onOneGoogleBarRendered(WindowProxy.getInstance().now());
    } else if (data.messageType === 'overlaysUpdated') {
      this.$.oneGoogleBarClipPath.querySelectorAll('rect').forEach(el => {
        el.remove();
      });
      const overlayRects = /** @type {!Array<!DOMRect>} */ (data.data);
      overlayRects.forEach(({x, y, width, height}) => {
        const rectElement =
            document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        // Add 8px around every rect to ensure shadows are not cutoff.
        rectElement.setAttribute('x', x - 8);
        rectElement.setAttribute('y', y - 8);
        rectElement.setAttribute('width', width + 16);
        rectElement.setAttribute('height', height + 16);
        this.$.oneGoogleBarClipPath.appendChild(rectElement);
      });
    } else if (data.messageType === 'can-show-promo-with-browser-command') {
      this.canShowPromoWithBrowserCommand_(data, event.source, event.origin);
    } else if (data.messageType === 'execute-browser-command') {
      this.executePromoBrowserCommand_(
          /** @type {!CommandData} */ (data.data), event.source, event.origin);
    } else if (data.messageType === 'click') {
      recordClick(NtpElement.kOneGoogleBar);
    }
  }

  /** @private */
  onMiddleSlotPromoLoaded_() {
    this.middleSlotPromoLoaded_ = true;
  }

  /** @private */
  onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  /** @private */
  onCustomizeModule_() {
    this.showCustomizeDialog_ = true;
    this.selectedCustomizeDialogPage_ = CustomizeDialogPage.MODULES;
  }

  /** @private */
  printPerformanceDatum_(name, time, auxTime = 0) {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    if (!auxTime) {
      console.log(`${name}: ${time}`);
    } else {
      console.log(`${name}: ${time} (${auxTime})`);
    }
  }

  /**
   * Prints performance measurements to the console. Also, installs  performance
   * observer to continuously print performance measurements after.
   * @private
   */
  printPerformance_() {
    if (!this.shouldPrintPerformance_) {
      return;
    }
    const entryTypes = ['paint', 'measure'];
    const log = (entry) => {
      this.printPerformanceDatum_(
          entry.name, entry.duration ? entry.duration : entry.startTime,
          entry.duration && entry.startTime ? entry.startTime : 0);
    };
    const observer = new PerformanceObserver(list => {
      list.getEntries().forEach((entry) => {
        log(entry);
      });
    });
    observer.observe({entryTypes: entryTypes});
    performance.getEntries().forEach((entry) => {
      if (!entryTypes.includes(entry.entryType)) {
        return;
      }
      log(entry);
    });
  }

  /**
   * @param {Event} e
   * @private
   */
  onWindowClick_(e) {
    if (e.composedPath() && e.composedPath()[0] === $$(this, '#content')) {
      recordClick(NtpElement.kBackground);
      return;
    }
    for (const target of e.composedPath()) {
      switch (target) {
        case $$(this, 'ntp-logo'):
          recordClick(NtpElement.kLogo);
          return;
        case $$(this, 'ntp-realbox'):
          recordClick(NtpElement.kRealbox);
          return;
        case $$(this, 'cr-most-visited'):
          recordClick(NtpElement.kMostVisited);
          return;
        case $$(this, 'ntp-middle-slot-promo'):
          recordClick(NtpElement.kMiddleSlotPromo);
          return;
        case $$(this, 'ntp-modules'):
          recordClick(NtpElement.kModule);
          return;
        case $$(this, '#customizeButton'):
        case $$(this, 'ntp-customize-dialog'):
          recordClick(NtpElement.kCustomize);
          return;
      }
    }
    recordClick(NtpElement.kOther);
  }
}

customElements.define(AppElement.is, AppElement);
