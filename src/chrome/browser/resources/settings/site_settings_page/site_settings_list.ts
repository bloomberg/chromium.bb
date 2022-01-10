// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../icons.js';
import '../settings_shared_css.js';
import '../i18n_setup.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {Route, Router} from '../router.js';
import {ContentSetting, ContentSettingsTypes, NotificationSetting} from '../site_settings/constants.js';
import {SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

export type CategoryListItem = {
  route: Route,
  id: ContentSettingsTypes,
  label: string,
  icon?: string,
  enabledLabel?: string,
  disabledLabel?: string,
  otherLabel?: string,
  shouldShow?: () => boolean,
};

type FocusConfig = Map<string, (string|(() => void))>;

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    index: number,
  };
}

export function defaultSettingLabel(
    setting: string, enabled: string, disabled: string,
    other?: string): string {
  if (setting === ContentSetting.BLOCK) {
    return disabled;
  }
  if (setting === ContentSetting.ALLOW) {
    return enabled;
  }

  return other || enabled;
}


const SettingsSiteSettingsListElementBase =
    PrefsMixin(BaseMixin(WebUIListenerMixin(I18nMixin(PolymerElement))));

class SettingsSiteSettingsListElement extends
    SettingsSiteSettingsListElementBase {
  static get is() {
    return 'settings-site-settings-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      categoryList: Array,

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },
    };
  }

  static get observers() {
    return [
      // The prefs object is only populated for the instance of this element
      // which contains the notifications link row, avoiding non-actionable
      // firing of the observer.
      'updateNotificationsLabel_(prefs.generated.notification.*)'
    ];
  }

  categoryList: Array<CategoryListItem>;
  focusConfig: FocusConfig;
  private browserProxy_: SiteSettingsPrefsBrowserProxy =
      SiteSettingsPrefsBrowserProxyImpl.getInstance();

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    for (const item of this.categoryList) {
      this.focusConfig.set(item.route.path, () => microTask.run(() => {
        focusWithoutInk(assert(this.shadowRoot!.querySelector(`#${item.id}`)!));
      }));
    }
  }

  ready() {
    super.ready();

    Promise
        .all(this.categoryList.map(
            item => this.refreshDefaultValueLabel_(item.id)))
        .then(() => {
          this.fire('site-settings-list-labels-updated-for-testing');
        });

    this.addWebUIListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) =>
            this.refreshDefaultValueLabel_(category));

    const hasProtocolHandlers = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.PROTOCOL_HANDLERS;
    });

    if (hasProtocolHandlers) {
      // The protocol handlers have a separate enabled/disabled notifier.
      this.addWebUIListener('setHandlersEnabled', (enabled: boolean) => {
        this.updateDefaultValueLabel_(
            ContentSettingsTypes.PROTOCOL_HANDLERS,
            enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK);
      });
      this.browserProxy_.observeProtocolHandlersEnabledState();
    }

    const hasCookies = this.categoryList.some(item => {
      return item.id === ContentSettingsTypes.COOKIES;
    });
    if (hasCookies) {
      // The cookies sub-label is provided by an update from C++.
      this.browserProxy_.getCookieSettingDescription().then(
          (label: string) => this.updateCookiesLabel_(label));
      this.addWebUIListener(
          'cookieSettingDescriptionChanged',
          (label: string) => this.updateCookiesLabel_(label));
    }
  }

  /**
   * @param category The category to refresh (fetch current value + update UI)
   * @return A promise firing after the label has been updated.
   */
  private refreshDefaultValueLabel_(category: ContentSettingsTypes):
      Promise<void> {
    // Default labels are not applicable to ZOOM_LEVELS, PDF or
    // PROTECTED_CONTENT
    if (category === ContentSettingsTypes.ZOOM_LEVELS ||
        category === ContentSettingsTypes.PROTECTED_CONTENT ||
        category === ContentSettingsTypes.PDF_DOCUMENTS) {
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.COOKIES) {
      // Updates to the cookies label are handled by the
      // cookieSettingDescriptionChanged event listener.
      return Promise.resolve();
    }

    if (category === ContentSettingsTypes.NOTIFICATIONS) {
      // Updates to the notifications label are handled by a preference
      // observer.
      return Promise.resolve();
    }

    return this.browserProxy_.getDefaultValueForContentType(category).then(
        defaultValue => {
          this.updateDefaultValueLabel_(category, defaultValue.setting);
        });
  }

  /**
   * Updates the DOM for the given |category| to display a label that
   * corresponds to the given |setting|.
   */
  private updateDefaultValueLabel_(
      category: ContentSettingsTypes, setting: ContentSetting) {
    const element = this.$$<HTMLElement>(`#${category}`);
    if (!element) {
      // |category| is not part of this list.
      return;
    }

    const index =
        this.shadowRoot!.querySelector('dom-repeat')!.indexForElement(element);
    const dataItem = this.categoryList[index!];
    this.set(
        `categoryList.${index}.subLabel`,
        defaultSettingLabel(
            setting,
            dataItem.enabledLabel ? this.i18n(dataItem.enabledLabel) : '',
            dataItem.disabledLabel ? this.i18n(dataItem.disabledLabel) : '',
            dataItem.otherLabel ? this.i18n(dataItem.otherLabel) : undefined));
  }

  /**
   * Update the cookies link row label when the cookies setting description
   * changes.
   */
  private updateCookiesLabel_(label: string) {
    const index = this.shadowRoot!.querySelector('dom-repeat')!.indexForElement(
        this.shadowRoot!.querySelector('#cookies')!);
    this.set(`categoryList.${index}.subLabel`, label);
  }

  /**
   * Update the notifications link row label when the notifications setting
   * description changes.
   */
  private updateNotificationsLabel_() {
    const state = this.getPref('generated.notification').value;
    const index = this.categoryList.map(e => e.id).indexOf(
        ContentSettingsTypes.NOTIFICATIONS);

    // updateNotificationsLabel_ should only be called for the
    // site-settings-list instance which contains notifications.
    assert(index !== -1);

    let label = 'siteSettingsNotificationsBlocked';
    if (state === NotificationSetting.ASK) {
      label = 'siteSettingsNotificationsAllowed';
    } else if (state === NotificationSetting.QUIETER_MESSAGING) {
      label = 'siteSettingsNotificationsPartial';
    }
    this.set(`categoryList.${index}.subLabel`, this.i18n(label));
  }

  private onClick_(event: RepeaterEvent) {
    Router.getInstance().navigateTo(this.categoryList[event.model.index].route);
  }
}

customElements.define(
    SettingsSiteSettingsListElement.is, SettingsSiteSettingsListElement);
