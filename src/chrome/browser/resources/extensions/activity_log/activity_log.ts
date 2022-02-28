// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './activity_log_stream.js';
import './activity_log_history.js';
import '../strings.m.js';
import '../shared_style.js';
import '../shared_vars.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from '../navigation_helper.js';

import {ActivityLogDelegate} from './activity_log_history.js';

/**
 * Subpages/views for the activity log. HISTORY shows extension activities
 * fetched from the activity log database with some fields such as args
 * omitted. STREAM displays extension activities in a more verbose format in
 * real time. NONE is used when user is away from the page.
 */
const enum ActivityLogSubpage {
  NONE = -1,
  HISTORY = 0,
  STREAM = 1,
}


/**
 * A struct used as a placeholder for chrome.developerPrivate.ExtensionInfo
 * for this component if the extensionId from the URL does not correspond to
 * installed extension.
 */
export type ActivityLogExtensionPlaceholder = {
  id: string,
  isPlaceholder: boolean,
}

interface ExtensionsActivityLogElement {
  $: {
    closeButton: HTMLElement,
  };
}

const ExtensionsActivityLogElementBase =
    I18nMixin(CrContainerShadowMixin(PolymerElement));

class ExtensionsActivityLogElement extends ExtensionsActivityLogElementBase {
  static get is() {
    return 'extensions-activity-log';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       */
      extensionInfo: Object,

      delegate: Object,

      selectedSubpage_: {
        type: Number,
        value: ActivityLogSubpage.NONE,
        observer: 'onSelectedSubpageChanged_',
      },

      tabNames_: {
        type: Array,
        value: () => ([
          loadTimeData.getString('activityLogHistoryTabHeading'),
          loadTimeData.getString('activityLogStreamTabHeading'),
        ]),
      }
    };
  }

  extensionInfo: chrome.developerPrivate.ExtensionInfo|
      ActivityLogExtensionPlaceholder;
  delegate: ActivityLogDelegate;
  selectedSubpage_: ActivityLogSubpage;
  private tabNames_: string[];

  ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  /**
   * Focuses the back button when page is loaded and set the activie view to
   * be HISTORY when we navigate to the page.
   */
  private onViewEnterStart_() {
    this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
  }

  /**
   * Set |selectedSubpage_| to NONE to remove the active view from the DOM.
   */
  private onViewExitFinish_() {
    this.selectedSubpage_ = ActivityLogSubpage.NONE;
    // clear the stream if the user is exiting the activity log page.
    const activityLogStream =
        this.shadowRoot!.querySelector('activity-log-stream');
    if (activityLogStream) {
      activityLogStream.clearStream();
    }
  }

  private getActivityLogHeading_(): string {
    const headingName =
        (this.extensionInfo as ActivityLogExtensionPlaceholder).isPlaceholder ?
        this.i18n('missingOrUninstalledExtension') :
        (this.extensionInfo as chrome.developerPrivate.ExtensionInfo).name;
    return this.i18n('activityLogPageHeading', headingName);
  }

  private isHistoryTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
  }

  private isStreamTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
  }

  private onSelectedSubpageChanged_(
      newTab: ActivityLogSubpage, oldTab: ActivityLogSubpage) {
    const activityLogStream =
        this.shadowRoot!.querySelector('activity-log-stream');
    if (activityLogStream) {
      if (newTab === ActivityLogSubpage.STREAM) {
        // Start the stream if the user is switching to the real-time tab.
        // This will not handle the first tab switch to the real-time tab as
        // the stream has not been attached to the DOM yet, and is handled
        // instead by the stream's |connectedCallback| method.
        activityLogStream.startStream();
      } else if (oldTab === ActivityLogSubpage.STREAM) {
        // Pause the stream if the user is navigating away from the real-time
        // tab.
        activityLogStream.pauseStream();
      }
    }
  }

  private onCloseButtonTap_() {
    if ((this.extensionInfo as ActivityLogExtensionPlaceholder).isPlaceholder) {
      navigation.navigateTo({page: Page.LIST});
    } else {
      navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionInfo.id});
    }
  }
}

customElements.define(
    ExtensionsActivityLogElement.is, ExtensionsActivityLogElement);
