// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-a11y-page' is the small section of advanced settings with
 * a link to the web store accessibility page on most platforms, and
 * a subpage with lots of other settings on Chrome OS.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_shared_css.js';

// <if expr="not is_macosx and not chromeos">
import './captions_subpage.js';
import '../settings_page/settings_subpage.js';
// </if>

// <if expr="is_win or is_macosx">
import './live_caption_section.js';
// </if>

import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

// <if expr="is_win or is_macosx">
import {CaptionsBrowserProxyImpl} from './captions_browser_proxy.js';
// </if>

const SettingsA11YPageElementBase =
    WebUIListenerMixin(BaseMixin(PolymerElement));

class SettingsA11YPageElement extends SettingsA11YPageElementBase {
  static get is() {
    return 'settings-a11y-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The current active route.
       */
      currentRoute: {
        type: Object,
        notify: true,
      },

      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      // <if expr="not chromeos">
      enableLiveCaption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaption');
        },
      },

      /**
       * Whether to show the focus highlight setting.
       * Depends on feature flag for focus highlight.
       */
      showFocusHighlightOption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('showFocusHighlightOption');
        }
      },
      // </if>

      /**
       * Whether to show accessibility labels settings.
       */
      showAccessibilityLabelsSetting_: {
        type: Boolean,
        value: false,
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.CAPTIONS) {
            map.set(routes.CAPTIONS.path, '#captions');
          }
          return map;
        },
      },

      /**
       * Whether the caption settings link opens externally.
       */
      captionSettingsOpensExternally_: {
        type: Boolean,
        value() {
          let opensExternally = false;
          // <if expr="is_macosx">
          opensExternally = true;
          // </if>

          // <if expr="is_win">
          opensExternally = loadTimeData.getBoolean('isWindows10OrNewer');
          // </if>

          return opensExternally;
        },
      },
    };
  }

  // <if expr="not chromeos">
  private enableLiveCaption_: boolean;
  private showFocusHighlightOption_: boolean;
  // </if>

  private showAccessibilityLabelsSetting_: boolean;
  private captionSettingsOpensExternally_: boolean;

  ready() {
    super.ready();

    this.addWebUIListener(
        'screen-reader-state-changed',
        (hasScreenReader: boolean) =>
            this.onScreenReaderStateChanged_(hasScreenReader));

    // Enables javascript and gets the screen reader state.
    chrome.send('a11yPageReady');
  }

  /**
   * @param hasScreenReader Whether a screen reader is enabled.
   */
  private onScreenReaderStateChanged_(hasScreenReader: boolean) {
    this.showAccessibilityLabelsSetting_ = hasScreenReader;
  }

  private onA11yCaretBrowsingChange_(event: Event) {
    if ((event.target as SettingsToggleButtonElement).checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  }

  private onA11yImageLabelsChange_(event: Event) {
    const a11yImageLabelsOn =
        (event.target as SettingsToggleButtonElement).checked;
    if (a11yImageLabelsOn) {
      chrome.send('confirmA11yImageLabels');
    }
  }

  // <if expr="not chromeos">
  private onFocusHighlightChange_(event: Event) {
    chrome.metricsPrivate.recordBoolean(
        'Accessibility.FocusHighlight.ToggleEnabled',
        (event.target as SettingsToggleButtonElement).checked);
  }
  // </if>

  // <if expr="chromeos">
  private onManageSystemAccessibilityFeaturesTap_() {
    window.location.href = 'chrome://os-settings/manageAccessibility';
  }
  // </if>

  /** private */
  private onMoreFeaturesLinkClick_() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }

  private onCaptionsClick_() {
    if (this.captionSettingsOpensExternally_) {
      // <if expr="is_win or is_macosx">
      CaptionsBrowserProxyImpl.getInstance().openSystemCaptionsDialog();
      // </if>
    } else {
      Router.getInstance().navigateTo(routes.CAPTIONS);
    }
  }
}

customElements.define(SettingsA11YPageElement.is, SettingsA11YPageElement);
