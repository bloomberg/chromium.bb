// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../settings_shared_css.js';
import './recent_site_permissions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, Router} from '../router.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import {CategoryListItem} from './site_settings_list.js';

const Id = ContentSettingsTypes;

let categoryItemMap: Map<ContentSettingsTypes, CategoryListItem>|null = null;

type FocusConfig = Map<string, (string|(() => void))>;

function getCategoryItemMap(): Map<ContentSettingsTypes, CategoryListItem> {
  if (categoryItemMap !== null) {
    return categoryItemMap;
  }
  // The following list is ordered alphabetically by |id|. The order in which
  // these appear in the UI is determined elsewhere in this file.
  const categoryList = [
    {
      route: routes.SITE_SETTINGS_ADS,
      id: Id.ADS,
      label: 'siteSettingsAds',
      icon: 'settings:ads',
      enabledLabel: 'siteSettingsAdsAllowed',
      disabledLabel: 'siteSettingsAdsBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'),
    },
    {
      route: routes.SITE_SETTINGS_AR,
      id: Id.AR,
      label: 'siteSettingsAr',
      icon: 'settings:vr-headset',
      // TODO(crbug.com/1196900): Fix redesign string when available.
      enabledLabel: 'siteSettingsArAsk',
      disabledLabel: 'siteSettingsArBlock',
    },
    {
      route: routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
      id: Id.AUTOMATIC_DOWNLOADS,
      label: 'siteSettingsAutomaticDownloads',
      icon: 'cr:file-download',
      enabledLabel: 'siteSettingsAutomaticDownloadsAllowed',
      disabledLabel: 'siteSettingsAutomaticDownloadsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_BACKGROUND_SYNC,
      id: Id.BACKGROUND_SYNC,
      label: 'siteSettingsBackgroundSync',
      icon: 'cr:sync',
      enabledLabel: 'siteSettingsBackgroundSyncAllowed',
      disabledLabel: 'siteSettingsBackgroundSyncBlocked',
    },
    {
      route: routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
      id: Id.BLUETOOTH_DEVICES,
      label: 'siteSettingsBluetoothDevices',
      icon: 'settings:bluetooth',
      enabledLabel: 'siteSettingsBluetoothDevicesAllowed',
      disabledLabel: 'siteSettingsBluetoothDevicesBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
    },
    {
      route: routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
      id: Id.BLUETOOTH_SCANNING,
      label: 'siteSettingsBluetoothScanning',
      icon: 'settings:bluetooth-scanning',
      enabledLabel: 'siteSettingsBluetoothScanningAsk',
      disabledLabel: 'siteSettingsBluetoothScanningBlock',
      shouldShow: () =>
          loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
    },
    {
      route: routes.SITE_SETTINGS_CAMERA,
      id: Id.CAMERA,
      label: 'siteSettingsCamera',
      icon: 'cr:videocam',
      enabledLabel: 'siteSettingsCameraAllowed',
      disabledLabel: 'siteSettingsCameraBlocked',
    },
    {
      route: routes.SITE_SETTINGS_CLIPBOARD,
      id: Id.CLIPBOARD,
      label: 'siteSettingsClipboard',
      icon: 'settings:clipboard',
      enabledLabel: 'siteSettingsClipboardAllowed',
      disabledLabel: 'siteSettingsClipboardBlocked',
    },
    {
      route: routes.COOKIES,
      id: Id.COOKIES,
      label: 'siteSettingsCookies',
      icon: 'settings:cookie',
      enabledLabel: 'siteSettingsCookiesAllowed',
      disabledLabel: 'siteSettingsBlocked',
      otherLabel: 'cookiePageClearOnExit',
    },
    {
      route: routes.SITE_SETTINGS_LOCATION,
      id: Id.GEOLOCATION,
      label: 'siteSettingsLocation',
      icon: 'settings:location-on',
      enabledLabel: 'siteSettingsLocationAllowed',
      disabledLabel: 'siteSettingsLocationBlocked',
    },
    {
      route: routes.SITE_SETTINGS_HID_DEVICES,
      id: Id.HID_DEVICES,
      label: 'siteSettingsHidDevices',
      icon: 'settings:hid-device',
      enabledLabel: 'siteSettingsHidDevicesAsk',
      disabledLabel: 'siteSettingsHidDevicesBlock',
    },
    {
      route: routes.SITE_SETTINGS_IDLE_DETECTION,
      id: Id.IDLE_DETECTION,
      label: 'siteSettingsIdleDetection',
      icon: 'settings:devices',
      enabledLabel: 'siteSettingsDeviceUseAllowed',
      disabledLabel: 'siteSettingsDeviceUseBlocked',
    },
    {
      route: routes.SITE_SETTINGS_IMAGES,
      id: Id.IMAGES,
      label: 'siteSettingsImages',
      icon: 'settings:photo',
      enabledLabel: 'siteSettingsImagesAllowed',
      disabledLabel: 'siteSettingsImagesBlocked',
    },
    {
      route: routes.SITE_SETTINGS_JAVASCRIPT,
      id: Id.JAVASCRIPT,
      label: 'siteSettingsJavascript',
      icon: 'settings:code',
      enabledLabel: 'siteSettingsJavascriptAllowed',
      disabledLabel: 'siteSettingsJavascriptBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MICROPHONE,
      id: Id.MIC,
      label: 'siteSettingsMic',
      icon: 'cr:mic',
      enabledLabel: 'siteSettingsMicAllowed',
      disabledLabel: 'siteSettingsMicBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIDI_DEVICES,
      id: Id.MIDI_DEVICES,
      label: 'siteSettingsMidiDevices',
      icon: 'settings:midi',
      enabledLabel: 'siteSettingsMidiAllowed',
      disabledLabel: 'siteSettingsMidiBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIXEDSCRIPT,
      id: Id.MIXEDSCRIPT,
      label: 'siteSettingsInsecureContent',
      icon: 'settings:insecure-content',
      disabledLabel: 'siteSettingsInsecureContentBlock',
    },
    {
      route: routes.SITE_SETTINGS_FILE_HANDLING,
      id: Id.FILE_HANDLING,
      label: 'siteSettingsFileHandling',
      icon: 'settings:file-handling',
      enabledLabel: 'siteSettingsFileHandlingAsk',
      disabledLabel: 'siteSettingsFileHandlingBlock',
    },
    {
      route: routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
      id: Id.FILE_SYSTEM_WRITE,
      label: 'siteSettingsFileSystemWrite',
      icon: 'settings:save-original',
      enabledLabel: 'siteSettingsFileSystemWriteAllowed',
      disabledLabel: 'siteSettingsFileSystemWriteBlocked',
    },
    {
      route: routes.SITE_SETTINGS_FONT_ACCESS,
      id: Id.FONT_ACCESS,
      label: 'fonts',
      icon: 'settings:font-access',
      enabledLabel: 'siteSettingsFontsAllowed',
      disabledLabel: 'siteSettingsFontsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_NOTIFICATIONS,
      id: Id.NOTIFICATIONS,
      label: 'siteSettingsNotifications',
      icon: 'settings:notifications',
      enabledLabel: 'siteSettingsAskBeforeSending',
      disabledLabel: 'siteSettingsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_PAYMENT_HANDLER,
      id: Id.PAYMENT_HANDLER,
      label: 'siteSettingsPaymentHandler',
      icon: 'settings:payment-handler',
      enabledLabel: 'siteSettingsPaymentHandlersAllowed',
      disabledLabel: 'siteSettingsPaymentHandlersBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enablePaymentHandlerContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_PDF_DOCUMENTS,
      id: Id.PDF_DOCUMENTS,
      label: 'siteSettingsPdfDocuments',
      icon: 'settings:pdf',
      enabledLabel: 'siteSettingsPdfsAllowed',
      disabledLabel: 'siteSettingsPdfsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_POPUPS,
      id: Id.POPUPS,
      label: 'siteSettingsPopups',
      icon: 'cr:open-in-new',
      enabledLabel: 'siteSettingsPopupsAllowed',
      disabledLabel: 'siteSettingsPopupsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_PROTECTED_CONTENT,
      id: Id.PROTECTED_CONTENT,
      label: 'siteSettingsProtectedContent',
      icon: 'settings:protected-content',
      enabledLabel: 'siteSettingsProtectedContentAllowed',
      disabledLabel: 'siteSettingsProtectedContentBlocked',
    },
    {
      route: routes.SITE_SETTINGS_HANDLERS,
      id: Id.PROTOCOL_HANDLERS,
      label: 'siteSettingsHandlers',
      icon: 'settings:protocol-handler',
      enabledLabel: 'siteSettingsProtocolHandlersAllowed',
      disabledLabel: 'siteSettingsProtocolHandlersBlocked',
      shouldShow: () => !loadTimeData.getBoolean('isGuest'),
    },
    {
      route: routes.SITE_SETTINGS_SENSORS,
      id: Id.SENSORS,
      label: 'siteSettingsSensors',
      icon: 'settings:sensors',
      enabledLabel: 'siteSettingsMotionSensorsAllowed',
      disabledLabel: 'siteSettingsMotionSensorsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SERIAL_PORTS,
      id: Id.SERIAL_PORTS,
      label: 'siteSettingsSerialPorts',
      icon: 'settings:serial-port',
      enabledLabel: 'siteSettingsSerialPortsAllowed',
      disabledLabel: 'siteSettingsSerialPortsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SOUND,
      id: Id.SOUND,
      label: 'siteSettingsSound',
      icon: 'settings:volume-up',
      enabledLabel: 'siteSettingsSoundAllowed',
      disabledLabel: 'siteSettingsSoundBlocked',
    },
    {
      route: routes.SITE_SETTINGS_USB_DEVICES,
      id: Id.USB_DEVICES,
      label: 'siteSettingsUsbDevices',
      icon: 'settings:usb',
      enabledLabel: 'siteSettingsUsbAllowed',
      disabledLabel: 'siteSettingsUsbBlocked',
    },
    {
      route: routes.SITE_SETTINGS_VR,
      id: Id.VR,
      label: 'siteSettingsVr',
      icon: 'settings:vr-headset',
      enabledLabel: 'siteSettingsVrAllowed',
      disabledLabel: 'siteSettingsVrBlocked',
    },
    {
      route: routes.SITE_SETTINGS_WINDOW_PLACEMENT,
      id: Id.WINDOW_PLACEMENT,
      label: 'siteSettingsWindowPlacement',
      icon: 'settings:window-placement',
      enabledLabel: 'siteSettingsWindowPlacementAsk',
      disabledLabel: 'siteSettingsWindowPlacementBlock',
    },
    {
      route: routes.SITE_SETTINGS_ZOOM_LEVELS,
      id: Id.ZOOM_LEVELS,
      label: 'siteSettingsZoomLevels',
      icon: 'settings:zoom-in',
    },
  ];

  categoryItemMap = new Map(categoryList.map(item => [item.id, item]));
  return categoryItemMap;
}

function buildItemListFromIds(orderedIdList: Array<ContentSettingsTypes>):
    Array<CategoryListItem> {
  const map = getCategoryItemMap();
  const orderedList = [];
  for (const id of orderedIdList) {
    const item = map.get(id)!;
    if (item.shouldShow === undefined || item.shouldShow()) {
      orderedList.push(item);
    }
  }
  return orderedList;
}

export class SettingsSiteSettingsPageElement extends PolymerElement {
  static get is() {
    return 'settings-site-settings-page';
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

      lists_: {
        type: Object,
        value: function() {
          return {
            permissionsBasic: buildItemListFromIds([
              Id.GEOLOCATION,
              Id.CAMERA,
              Id.MIC,
              Id.NOTIFICATIONS,
              Id.BACKGROUND_SYNC,
            ]),
            permissionsAdvanced: buildItemListFromIds([
              Id.SENSORS,
              Id.AUTOMATIC_DOWNLOADS,
              Id.PROTOCOL_HANDLERS,
              Id.MIDI_DEVICES,
              Id.USB_DEVICES,
              Id.SERIAL_PORTS,
              Id.BLUETOOTH_DEVICES,
              Id.FILE_SYSTEM_WRITE,
              Id.HID_DEVICES,
              Id.CLIPBOARD,
              Id.PAYMENT_HANDLER,
              Id.BLUETOOTH_SCANNING,
              Id.AR,
              Id.VR,
              Id.IDLE_DETECTION,
              Id.WINDOW_PLACEMENT,
              Id.FONT_ACCESS,
              Id.FILE_HANDLING,
            ]),
            contentBasic: buildItemListFromIds([
              Id.COOKIES,
              Id.JAVASCRIPT,
              Id.IMAGES,
              Id.POPUPS,
            ]),
            contentAdvanced: buildItemListFromIds([
              Id.SOUND,
              Id.ADS,
              Id.ZOOM_LEVELS,
              Id.PDF_DOCUMENTS,
              Id.PROTECTED_CONTENT,
              Id.MIXEDSCRIPT,
            ]),
          };
        }
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      permissionsExpanded_: Boolean,
      contentExpanded_: Boolean,
      noRecentSitePermissions_: Boolean,
    };
  }

  focusConfig: FocusConfig;
  private permissionsExpanded_: boolean;
  private contentExpanded_: boolean;
  private noRecentSitePermissions_: boolean;

  private lists_: {
    all: Array<CategoryListItem>,
    permissionsBasic: Array<CategoryListItem>,
    permissionsAdvanced: Array<CategoryListItem>,
    contentBasic: Array<CategoryListItem>,
    contentAdvanced: Array<CategoryListItem>,
  };

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);
    this.focusConfig.set(routes.SITE_SETTINGS_ALL.path, () => {
      focusWithoutInk(assert(this.shadowRoot!.querySelector('#allSites')!));
    });
  }

  private onSiteSettingsAllClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  /** @return Class for the all site settings link */
  private getClassForSiteSettingsAllLink_(): string {
    return this.noRecentSitePermissions_ ? '' : 'hr';
  }
}

customElements.define(
    SettingsSiteSettingsPageElement.is, SettingsSiteSettingsPageElement);
