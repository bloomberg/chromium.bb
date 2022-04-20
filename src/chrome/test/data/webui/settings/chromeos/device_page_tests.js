// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, NoteAppLockScreenSupport, Router, routes, setDisplayApiForTesting, StorageSpaceState} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {FakeSystemDisplay} from './fake_system_display.js';

/** @enum {string} */
const TestNames = {
  DevicePage: 'device page',
  Display: 'display',
  Keyboard: 'keyboard',
  NightLight: 'night light',
  Pointers: 'pointers',
  PointingStick: 'pointing stick',
  Power: 'power',
  Storage: 'storage',
  Stylus: 'stylus',
  KeyboardArrangementDisabled: 'arrow_key_arrangement_disabled',
};

/**
 * @constructor
 * @implements {DevicePageBrowserProxy}
 */
function TestDevicePageBrowserProxy() {
  this.keyboardShortcutViewerShown_ = 0;
  this.updatePowerStatusCalled_ = 0;
  this.requestPowerManagementSettingsCalled_ = 0;
  this.requestNoteTakingApps_ = 0;
  this.onNoteTakingAppsUpdated_ = null;

  this.androidAppsReceived_ = false;
  this.noteTakingApps_ = [];
  this.setPreferredAppCount_ = 0;
  this.setAppOnLockScreenCount_ = 0;

  this.lastHighlightedDisplayId_ = '-1';
}

TestDevicePageBrowserProxy.prototype = {
  /** override */
  initializePointers: function() {
    // Enable mouse and touchpad.
    webUIListenerCallback('has-mouse-changed', true);
    webUIListenerCallback('has-pointing-stick-changed', true);
    webUIListenerCallback('has-touchpad-changed', true);
    webUIListenerCallback('has-haptic-touchpad-changed', true);
  },

  /** override */
  initializeStylus: function() {
    // Enable stylus.
    webUIListenerCallback('has-stylus-changed', true);
  },

  /** override */
  initializeKeyboard: function() {},

  /** override */
  showKeyboardShortcutViewer: function() {
    this.keyboardShortcutViewerShown_++;
  },

  /** override */
  updateAndroidEnabled: function() {},

  /** @override */
  updatePowerStatus: function() {
    this.updatePowerStatusCalled_++;
  },

  /** @override */
  setPowerSource: function(powerSourceId) {
    this.powerSourceId_ = powerSourceId;
  },

  /** @override */
  requestPowerManagementSettings: function() {
    this.requestPowerManagementSettingsCalled_++;
  },

  /** @override */
  setIdleBehavior: function(behavior, whenOnAc) {
    if (whenOnAc) {
      this.acIdleBehavior_ = behavior;
    } else {
      this.batteryIdleBehavior_ = behavior;
    }
  },

  /** @override */
  setLidClosedBehavior: function(behavior) {
    this.lidClosedBehavior_ = behavior;
  },

  /** @override */
  setNoteTakingAppsUpdatedCallback: function(callback) {
    this.onNoteTakingAppsUpdated_ = callback;
  },

  /** @override */
  requestNoteTakingApps: function() {
    this.requestNoteTakingApps_++;
  },

  /** @override */
  setPreferredNoteTakingApp: function(appId) {
    ++this.setPreferredAppCount_;

    let changed = false;
    this.noteTakingApps_.forEach(function(app) {
      changed = changed || app.preferred !== (app.value === appId);
      app.preferred = app.value === appId;
    });

    if (changed) {
      this.scheduleLockScreenAppsUpdated_();
    }
  },

  /** @override */
  setPreferredNoteTakingAppEnabledOnLockScreen: function(enabled) {
    ++this.setAppOnLockScreenCount_;

    this.noteTakingApps_.forEach(function(app) {
      if (enabled) {
        if (app.preferred) {
          assertEquals(
              NoteAppLockScreenSupport.SUPPORTED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.SUPPORTED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.ENABLED;
        }
      } else {
        if (app.preferred) {
          assertEquals(NoteAppLockScreenSupport.ENABLED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.ENABLED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.SUPPORTED;
        }
      }
    });

    this.scheduleLockScreenAppsUpdated_();
  },

  /** @override */
  highlightDisplay: function(id) {
    this.lastHighlightedDisplayId_ = id;
  },

  /** @override */
  updateStorageInfo: function() {},

  // Test interface:
  /**
   * Sets whether the app list contains Android apps.
   * @param {boolean} Whether the list of Android note-taking apps was
   *     received.
   */
  setAndroidAppsReceived: function(received) {
    this.androidAppsReceived_ = received;

    this.scheduleLockScreenAppsUpdated_();
  },

  /**
   * @return {string} App id of the app currently selected as preferred.
   */
  getPreferredNoteTakingAppId: function() {
    const app = this.noteTakingApps_.find(function(existing) {
      return existing.preferred;
    });

    return app ? app.value : '';
  },

  /**
   * @return {NoteAppLockScreenSupport | undefined} The lock screen
   *     support state of the app currently selected as preferred.
   */
  getPreferredAppLockScreenState: function() {
    const app = this.noteTakingApps_.find(function(existing) {
      return existing.preferred;
    });

    return app ? app.lockScreenSupport : undefined;
  },

  /**
   * Sets the current list of known note taking apps.
   * @param {Array<!NoteAppInfo>} The list of apps to set.
   */
  setNoteTakingApps: function(apps) {
    this.noteTakingApps_ = apps;
    this.scheduleLockScreenAppsUpdated_();
  },

  /**
   * Adds an app to the list of known note-taking apps.
   * @param {!NoteAppInfo}
   */
  addNoteTakingApp: function(app) {
    assert(!this.noteTakingApps_.find(function(existing) {
      return existing.value === app.value;
    }));

    this.noteTakingApps_.push(app);
    this.scheduleLockScreenAppsUpdated_();
  },

  /**
   * Invokes the registered note taking apps update callback.
   * @private
   */
  scheduleLockScreenAppsUpdated_: function() {
    this.onNoteTakingAppsUpdated_(
        this.noteTakingApps_.map(function(app) {
          return Object.assign({}, app);
        }),
        !this.androidAppsReceived_);
  }
};

function getFakePrefs() {
  return {
    ash: {
      ambient_color: {
        enabled: {
          key: 'ash.ambient_color.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      night_light: {
        enabled: {
          key: 'ash.night_light.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        color_temperature: {
          key: 'ash.night_light.color_temperature',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        schedule_type: {
          key: 'ash.night_light.schedule_type',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        custom_start_time: {
          key: 'ash.night_light.custom_start_time',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        custom_end_time: {
          key: 'ash.night_light.custom_end_time',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      },
    },
    settings: {
      // TODO(afakhry): Write tests to validate the Night Light slider
      // behavior with 24-hour setting.
      clock: {
        use_24hour_clock: {
          key: 'settings.clock.use_24hour_clock',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      enable_stylus_tools: {
        key: 'settings.enable_stylus_tools',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      launch_palette_on_eject_event: {
        key: 'settings.launch_palette_on_eject_event',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      restore_last_lock_screen_note: {
        key: 'settings.restore_last_lock_screen_note',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      touchpad: {
        enable_tap_to_click: {
          key: 'settings.touchpad.enable_tap_to_click',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        enable_tap_dragging: {
          key: 'settings.touchpad.enable_tap_dragging',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        natural_scroll: {
          key: 'settings.touchpad.natural_scroll',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.touchpad.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        scroll_acceleration: {
          key: 'settings.touchpad.scroll_acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity2: {
          key: 'settings.touchpad.sensitivity2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
        scroll_sensitivity: {
          key: 'settings.touchpad.scroll_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
        haptic_feedback: {
          key: 'settings.touchpad.haptic_feedback',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        haptic_click_sensitivity: {
          key: 'settings.touchpad.haptic_click_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 3,
        },
      },
      mouse: {
        primary_right: {
          key: 'settings.mouse.primary_right',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        reverse_scroll: {
          key: 'settings.mouse.reverse_scroll',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.mouse.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        scroll_acceleration: {
          key: 'settings.mouse.scroll_acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity2: {
          key: 'settings.mouse.sensitivity2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
        scroll_sensitivity: {
          key: 'settings.mouse.scroll_sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
      },
      pointing_stick: {
        primary_right: {
          key: 'settings.pointing_stick.primary_right',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        acceleration: {
          key: 'settings.pointing_stick.acceleration',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        sensitivity: {
          key: 'settings.pointing_stick.sensitivity',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
      },
      language: {
        xkb_remap_search_key_to: {
          key: 'settings.language.xkb_remap_search_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        xkb_remap_control_key_to: {
          key: 'settings.language.xkb_remap_control_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 1,
        },
        xkb_remap_alt_key_to: {
          key: 'settings.language.xkb_remap_alt_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 2,
        },
        remap_caps_lock_key_to: {
          key: 'settings.language.remap_caps_lock_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 4,
        },
        remap_escape_key_to: {
          key: 'settings.language.remap_escape_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 5,
        },
        remap_backspace_key_to: {
          key: 'settings.language.remap_backspace_key_to',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 6,
        },
        send_function_keys: {
          key: 'settings.language.send_function_keys',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        xkb_auto_repeat_enabled_r2: {
          key: 'prefs.settings.language.xkb_auto_repeat_enabled_r2',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        xkb_auto_repeat_delay_r2: {
          key: 'settings.language.xkb_auto_repeat_delay_r2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 500,
        },
        xkb_auto_repeat_interval_r2: {
          key: 'settings.language.xkb_auto_repeat_interval_r2',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 500,
        },
      }
    }
  };
}

suite('SettingsDevicePage', function() {
  /** @type {!SettingsDevicePage|undefined} */
  let devicePage;

  /** @type {!FakeSystemDisplay|undefined} */
  let fakeSystemDisplay;

  suiteSetup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function(done) {
    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay);

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.BASIC);

    devicePage = document.createElement('settings-device-page');
    devicePage.prefs = getFakePrefs();
    DevicePageBrowserProxyImpl.instance_ = new TestDevicePageBrowserProxy();

    // settings-animated-pages expects a parent with data-page set.
    const basicPage = document.createElement('div');
    basicPage.dataset.page = 'basic';
    basicPage.appendChild(devicePage);
    document.body.appendChild(basicPage);

    // Allow the light DOM to be distributed to settings-animated-pages.
    setTimeout(done);
  });

  /** @return {!Promise<!HTMLElement>} */
  function showAndGetDeviceSubpage(subpage, expectedRoute) {
    const row = assert(devicePage.$$(`#main #${subpage}Row`));
    row.click();
    assertEquals(expectedRoute, Router.getInstance().getCurrentRoute());
    const page = devicePage.$$('settings-' + subpage);
    assert(page);
    return Promise.resolve(page);
  }

  /** @param {number} n The number of the display to add. */
  function addDisplay(n) {
    const display = {
      id: 'fakeDisplayId' + n,
      name: 'fakeDisplayName' + n,
      mirroring: '',
      isPrimary: n === 1,
      isInternal: n === 1,
      rotation: 0,
      modes: [
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 60,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 1920,
          heightInNativePixels: 1080,
          width: 1920,
          height: 1080,
          refreshRate: 30,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 45,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 75,
        },
        // Include 3 copies of 3000x2000 mode to emulate duplicated modes
        // reported by some monitors.  Only one is marked 'isNative'.
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        },
        {
          isNative: true,
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        },
        {
          deviceScaleFactor: 1.0,
          widthInNativePixels: 3000,
          heightInNativePixels: 2000,
          width: 3000,
          height: 2000,
          refreshRate: 100,
        }
      ],
      bounds: {
        left: 0,
        top: 0,
        width: 1920,
        height: 1080,
      },
      availableDisplayZoomFactors: [1, 1.25, 1.5, 2],
    };
    fakeSystemDisplay.addDisplayForTest(display);
  }

  /**
   * @param {!Array<!IdleBehavior>} possibleAcIdleBehaviors
   * @param {!Array<!IdleBehavior>} possibleBatteryIdleBehaviors
   * @param {IdleBehavior} currAcIdleBehavior
   * @param {IdleBehavior} currBatteryIdleBehavior
   * @param {boolean} acIdleManaged
   * @param {boolean} batteryIdleManaged
   * @param {LidClosedBehavior} lidClosedBehavior
   * @param {boolean} lidClosedControlled
   * @param {boolean} hasLid
   * @param {boolean} adaptiveCharging
   */
  function sendPowerManagementSettings(
      possibleAcIdleBehaviors, possibleBatteryIdleBehaviors, currAcIdleBehavior,
      currBatteryIdleBehavior, acIdleManaged, batteryIdleManaged,
      lidClosedBehavior, lidClosedControlled, hasLid, adaptiveCharging) {
    webUIListenerCallback('power-management-settings-changed', {
      possibleAcIdleBehaviors: possibleAcIdleBehaviors,
      possibleBatteryIdleBehaviors: possibleBatteryIdleBehaviors,
      currentAcIdleBehavior: currAcIdleBehavior,
      currentBatteryIdleBehavior: currBatteryIdleBehavior,
      acIdleManaged: acIdleManaged,
      batteryIdleManaged: batteryIdleManaged,
      lidClosedBehavior: lidClosedBehavior,
      lidClosedControlled: lidClosedControlled,
      hasLid: hasLid,
      adaptiveCharging: adaptiveCharging,
    });
    flush();
  }

  /**
   * @param {!HTMLElement} select
   * @param {!value} string
   */
  function selectValue(select, value) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  /**
   * @param {!HTMLElement} pointersPage
   * @param {boolean} expected
   */
  function expectReverseScrollValue(pointersPage, expected) {
    const reverseScrollToggle =
        pointersPage.$$('#enableReverseScrollingToggle');
    assertEquals(expected, reverseScrollToggle.checked);
    assertEquals(
        expected, devicePage.prefs.settings.touchpad.natural_scroll.value);
  }

  /**
   * Returns whether the element both exists and is visible.
   * @param {?Element} element
   * @return {boolean}
   */
  function isVisible(element) {
    // offsetWidth and offsetHeight reflect more ways that an element could be
    // hidden, compared to checking the hidden attribute directly.
    return !!element && element.offsetWidth > 0 && element.offsetHeight > 0;
  }

  /**
   * Checks that the deep link to a setting focuses the correct element.
   * @param {!Route} route
   * @param {!string} settingId
   * @param {!Element} deepLinkElement The element that should be focused by
   *                                   the deep link
   * @param {!string} elementDesc A human-readable description of the element,
   *                              for assertion messages
   */
  async function checkDeepLink(route, settingId, deepLinkElement, elementDesc) {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(route, params);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `${elementDesc} should be focused for settingId=${settingId}.`);
  }

  test(assert(TestNames.DevicePage), function() {
    assertTrue(isVisible(devicePage.$$('#pointersRow')));
    assertTrue(isVisible(devicePage.$$('#keyboardRow')));
    assertTrue(isVisible(devicePage.$$('#displayRow')));

    webUIListenerCallback('has-mouse-changed', false);
    assertTrue(isVisible(devicePage.$$('#pointersRow')));
    webUIListenerCallback('has-pointing-stick-changed', false);
    assertTrue(isVisible(devicePage.$$('#pointersRow')));
    webUIListenerCallback('has-touchpad-changed', false);
    assertFalse(isVisible(devicePage.$$('#pointersRow')));
    webUIListenerCallback('has-mouse-changed', true);
    assertTrue(isVisible(devicePage.$$('#pointersRow')));
  });

  suite(assert(TestNames.Pointers), function() {
    let pointersPage;

    setup(function() {
      return showAndGetDeviceSubpage('pointers', routes.POINTERS)
          .then(function(page) {
            pointersPage = page;
          });
    });

    test('subpage responds to pointer attach/detach', function() {
      assertEquals(routes.POINTERS, Router.getInstance().getCurrentRoute());
      assertTrue(isVisible(pointersPage.$$('#mouse')));
      assertTrue(isVisible(pointersPage.$$('#mouse h2')));
      assertTrue(isVisible(pointersPage.$$('#pointingStick')));
      assertTrue(isVisible(pointersPage.$$('#pointingStick h2')));
      assertTrue(isVisible(pointersPage.$$('#touchpad')));
      assertTrue(isVisible(pointersPage.$$('#touchpad h2')));

      webUIListenerCallback('has-touchpad-changed', false);
      assertEquals(routes.POINTERS, Router.getInstance().getCurrentRoute());
      assertTrue(isVisible(pointersPage.$$('#mouse')));
      assertTrue(isVisible(pointersPage.$$('#mouse h2')));
      assertTrue(isVisible(pointersPage.$$('#pointingStick')));
      assertTrue(isVisible(pointersPage.$$('#pointingStick h2')));
      assertFalse(isVisible(pointersPage.$$('#touchpad')));
      assertFalse(isVisible(pointersPage.$$('#touchpad h2')));

      webUIListenerCallback('has-pointing-stick-changed', false);
      assertEquals(routes.POINTERS, Router.getInstance().getCurrentRoute());
      assertTrue(isVisible(pointersPage.$$('#mouse')));
      assertFalse(isVisible(pointersPage.$$('#mouse h2')));
      assertFalse(isVisible(pointersPage.$$('#pointingStick')));
      assertFalse(isVisible(pointersPage.$$('#pointingStick h2')));
      assertFalse(isVisible(pointersPage.$$('#touchpad')));
      assertFalse(isVisible(pointersPage.$$('#touchpad h2')));

      webUIListenerCallback('has-mouse-changed', false);
      assertEquals(routes.DEVICE, Router.getInstance().getCurrentRoute());
      assertFalse(isVisible(devicePage.$$('#main #pointersRow')));

      webUIListenerCallback('has-touchpad-changed', true);
      assertTrue(isVisible(devicePage.$$('#main #pointersRow')));

      return showAndGetDeviceSubpage('pointers', routes.POINTERS)
          .then(function(page) {
            assertFalse(isVisible(pointersPage.$$('#mouse')));
            assertFalse(isVisible(pointersPage.$$('#mouse h2')));
            assertFalse(isVisible(pointersPage.$$('#pointingStick')));
            assertFalse(isVisible(pointersPage.$$('#pointingStick h2')));
            assertTrue(isVisible(pointersPage.$$('#touchpad')));
            assertFalse(isVisible(pointersPage.$$('#touchpad h2')));

            webUIListenerCallback('has-mouse-changed', true);
            assertEquals(
                routes.POINTERS, Router.getInstance().getCurrentRoute());
            assertTrue(isVisible(pointersPage.$$('#mouse')));
            assertTrue(isVisible(pointersPage.$$('#mouse h2')));
            assertFalse(isVisible(pointersPage.$$('#pointingStick')));
            assertFalse(isVisible(pointersPage.$$('#pointingStick h2')));
            assertTrue(isVisible(pointersPage.$$('#touchpad')));
            assertTrue(isVisible(pointersPage.$$('#touchpad h2')));
          });
    });

    test('mouse', function() {
      assertTrue(isVisible(pointersPage.$$('#mouse')));

      const slider = assert(pointersPage.$$('#mouse settings-slider'));
      assertEquals(4, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 37, [], 'ArrowLeft');
      assertEquals(3, devicePage.prefs.settings.mouse.sensitivity2.value);

      pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
      assertEquals(5, slider.pref.value);
    });

    test('touchpad', function() {
      assertTrue(isVisible(pointersPage.$$('#touchpad')));

      assertTrue(pointersPage.$$('#touchpad #enableTapToClick').checked);
      assertFalse(pointersPage.$$('#touchpad #enableTapDragging').checked);

      const slider = assert(pointersPage.$$('#touchpad settings-slider'));
      assertEquals(3, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 39 /* right */, [],
          'ArrowRight');
      assertEquals(4, devicePage.prefs.settings.touchpad.sensitivity2.value);

      pointersPage.set('prefs.settings.touchpad.sensitivity2.value', 2);
      assertEquals(2, slider.pref.value);
    });

    test('haptic touchpad', function() {
      assertTrue(pointersPage.$$('#touchpadHapticFeedbackToggle').checked);

      const slider = assert(pointersPage.$$('#touchpadHapticClickSensitivity'));
      assertEquals(3, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 39 /* right */, [],
          'ArrowRight');
      assertEquals(
          5, devicePage.prefs.settings.touchpad.haptic_click_sensitivity.value);

      pointersPage.set(
          'prefs.settings.touchpad.haptic_click_sensitivity.value', 1);
      assertEquals(1, slider.pref.value);
    });

    test('link doesn\'t activate control', function() {
      expectReverseScrollValue(pointersPage, false);

      // Tapping the link shouldn't enable the radio button.
      const reverseScrollLabel =
          pointersPage.$$('#enableReverseScrollingLabel');
      const a = reverseScrollLabel.$.container.querySelector('a');
      assertTrue(!!a);
      // Prevent actually opening a link, which would block test.
      a.removeAttribute('href');
      a.click();
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking toggle changes pref.
      const reverseScrollToggle =
          pointersPage.$$('#enableReverseScrollingToggle');
      reverseScrollToggle.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking the row changes pref.
      const reverseScrollSettings = pointersPage.$$('#reverseScrollRow');
      reverseScrollSettings.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);
    });

    test('pointing stick acceleration toggle', function() {
      const toggle = assert(pointersPage.$$('#pointingStickAcceleration'));
      assertEquals(true, toggle.pref.value);
      toggle.click();
      assertEquals(
          false, devicePage.prefs.settings.pointing_stick.acceleration.value);

      pointersPage.set(
          'prefs.settings.pointing_stick.acceleration.value', true);
      assertEquals(true, toggle.pref.value);
    });

    test('pointing stick speed slider', function() {
      const slider = assert(pointersPage.$$('#pointingStick settings-slider'));
      assertEquals(4, slider.pref.value);
      MockInteractions.pressAndReleaseKeyOn(
          slider.shadowRoot.querySelector('cr-slider'), 37, [], 'ArrowLeft');
      assertEquals(
          3, devicePage.prefs.settings.pointing_stick.sensitivity.value);

      pointersPage.set('prefs.settings.pointing_stick.sensitivity.value', 5);
      assertEquals(5, slider.pref.value);
    });

    test('Deep link to pointing stick primary button setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '437',
          pointersPage.$$('#pointingStickSwapButtonDropdown')
              .shadowRoot.querySelector('select'),
          'Pointing stick primary button dropdown');
    });

    test('Deep link to pointing stick acceleration setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '436',
          pointersPage.$$('#pointingStickAcceleration')
              .shadowRoot.querySelector('cr-toggle'),
          'Pointing stick acceleration slider');
    });

    test('Deep link to pointing stick speed setting', async () => {
      return checkDeepLink(
          routes.POINTERS, '435',
          pointersPage.$$('#pointingStickSpeedSlider')
              .shadowRoot.querySelector('cr-slider'),
          'Pointing stick speed slider');
    });

    test('Deep link to touchpad speed', async () => {
      return checkDeepLink(
          routes.POINTERS, '405',
          pointersPage.$$('#touchpadSensitivity')
              .shadowRoot.querySelector('cr-slider'),
          'Touchpad speed slider');
    });
  });

  suite(assert(TestNames.Keyboard), function() {
    const name = k => `prefs.settings.language.${k}.value`;
    const get = k => devicePage.get(name(k));
    const set = (k, v) => devicePage.set(name(k), v);
    let keyboardPage;

    setup(async () => {
      keyboardPage = await showAndGetDeviceSubpage('keyboard', routes.KEYBOARD);
    });

    test('keyboard', async () => {
      // Initially, the optional keys are hidden.
      assertFalse(!!keyboardPage.$$('#capsLockKey'));

      // Pretend no internal keyboard is available.
      const keyboardParams = {
        'showCapsLock': false,
        'showExternalMetaKey': false,
        'showAppleCommandKey': false,
        'hasLauncherKey': false,
        'hasAssistantKey': false,
      };
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.$$('#launcherKey'));
      assertFalse(!!keyboardPage.$$('#capsLockKey'));
      assertFalse(!!keyboardPage.$$('#externalMetaKey'));
      assertFalse(!!keyboardPage.$$('#externalCommandKey'));
      assertFalse(!!keyboardPage.$$('#assistantKey'));

      // Pretend a Caps Lock key is now available.
      keyboardParams['showCapsLock'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.$$('#launcherKey'));
      assertTrue(!!keyboardPage.$$('#capsLockKey'));
      assertFalse(!!keyboardPage.$$('#externalMetaKey'));
      assertFalse(!!keyboardPage.$$('#externalCommandKey'));
      assertFalse(!!keyboardPage.$$('#assistantKey'));

      // Add a non-Apple external keyboard.
      keyboardParams['showExternalMetaKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.$$('#launcherKey'));
      assertTrue(!!keyboardPage.$$('#capsLockKey'));
      assertTrue(!!keyboardPage.$$('#externalMetaKey'));
      assertFalse(!!keyboardPage.$$('#externalCommandKey'));
      assertFalse(!!keyboardPage.$$('#assistantKey'));

      // Add an Apple keyboard.
      keyboardParams['showAppleCommandKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertFalse(!!keyboardPage.$$('#launcherKey'));
      assertTrue(!!keyboardPage.$$('#capsLockKey'));
      assertTrue(!!keyboardPage.$$('#externalMetaKey'));
      assertTrue(!!keyboardPage.$$('#externalCommandKey'));
      assertFalse(!!keyboardPage.$$('#assistantKey'));

      // Add an internal keyboard.
      keyboardParams['hasLauncherKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.$$('#launcherKey'));
      assertTrue(!!keyboardPage.$$('#capsLockKey'));
      assertTrue(!!keyboardPage.$$('#externalMetaKey'));
      assertTrue(!!keyboardPage.$$('#externalCommandKey'));
      assertFalse(!!keyboardPage.$$('#assistantKey'));

      // Pretend an Assistant key is now available.
      keyboardParams['hasAssistantKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.$$('#launcherKey'));
      assertTrue(!!keyboardPage.$$('#capsLockKey'));
      assertTrue(!!keyboardPage.$$('#externalMetaKey'));
      assertTrue(!!keyboardPage.$$('#externalCommandKey'));
      assertTrue(!!keyboardPage.$$('#assistantKey'));

      const collapse = keyboardPage.$$('iron-collapse');
      assertTrue(!!collapse);
      assertTrue(collapse.opened);

      assertEquals(500, keyboardPage.$$('#delaySlider').pref.value);
      assertEquals(500, keyboardPage.$$('#repeatRateSlider').pref.value);

      // Test interaction with the settings-slider's underlying cr-slider.
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$$('#delaySlider').shadowRoot.querySelector('cr-slider'),
          37 /* left */, [], 'ArrowLeft');
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$$('#repeatRateSlider')
              .shadowRoot.querySelector('cr-slider'),
          39, [], 'ArrowRight');
      await flushTasks();
      assertEquals(1000, get('xkb_auto_repeat_delay_r2'));
      assertEquals(300, get('xkb_auto_repeat_interval_r2'));

      // Test sliders change when prefs change.
      set('xkb_auto_repeat_delay_r2', 1500);
      await flushTasks();
      assertEquals(1500, keyboardPage.$$('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 2000);
      await flushTasks();
      assertEquals(2000, keyboardPage.$$('#repeatRateSlider').pref.value);

      // Test sliders round to nearest value when prefs change.
      set('xkb_auto_repeat_delay_r2', 600);
      await flushTasks();
      assertEquals(500, keyboardPage.$$('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 45);
      await flushTasks();
      assertEquals(50, keyboardPage.$$('#repeatRateSlider').pref.value);

      set('xkb_auto_repeat_enabled_r2', false);
      assertFalse(collapse.opened);

      // Test keyboard shortcut viewer button.
      keyboardPage.$$('#keyboardShortcutViewer').click();
      assertEquals(
          1,
          DevicePageBrowserProxyImpl.getInstance()
              .keyboardShortcutViewerShown_);
    });

    test('Deep link to keyboard shortcuts', async () => {
      return checkDeepLink(
          routes.KEYBOARD, '413',
          keyboardPage.$$('#keyboardShortcutViewer')
              .shadowRoot.querySelector('cr-icon-button'),
          'Keyboard shortcuts button');
    });
  });

  suite(assert(TestNames.Display), function() {
    let displayPage;
    let browserProxy;

    setup(async () => {
      displayPage = await showAndGetDeviceSubpage('display', routes.DISPLAY);
      browserProxy = DevicePageBrowserProxyImpl.getInstance();
      await fakeSystemDisplay.getInfoCalled.promise;
    });

    test('display tests', function() {
      // Verify all the conditionals that get run during page load
      // before the display info has been populated.
      assertEquals(undefined, displayPage.displays);
      assertFalse(displayPage.showMirror_(true, displayPage.displays));
      assertFalse(displayPage.showMirror_(false, displayPage.displays));
      assertFalse(displayPage.isMirrored_(displayPage.displays));
      assertFalse(
          displayPage.showUnifiedDesktop_(true, true, displayPage.displays));
      assertFalse(
          displayPage.showUnifiedDesktop_(false, false, displayPage.displays));
      assertEquals(
          displayPage.invalidDisplayId_,
          browserProxy.lastHighlightedDisplayId_);

      // Add a display.
      addDisplay(1);
      fakeSystemDisplay.onDisplayChanged.callListeners();

      return Promise
          .all([
            fakeSystemDisplay.getInfoCalled.promise,
            fakeSystemDisplay.getLayoutCalled.promise,
          ])
          .then(function() {
            // There should be a single display which should be primary and
            // selected. Mirroring should be disabled.
            assertEquals(1, displayPage.displays.length);
            assertEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            assertEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
            assertFalse(displayPage.showMirror_(false, displayPage.displays));
            assertFalse(displayPage.isMirrored_(displayPage.displays));

            // Verify unified desktop only shown when enabled.
            assertTrue(displayPage.showUnifiedDesktop_(
                true, true, displayPage.displays));
            assertFalse(displayPage.showUnifiedDesktop_(
                false, false, displayPage.displays));

            // Sanity check the first display is internal.
            assertTrue(displayPage.displays[0].isInternal);

            // Ambient EQ only shown when enabled.
            assertTrue(displayPage.showAmbientColorSetting_(
                true, displayPage.displays[0]));
            assertFalse(displayPage.showAmbientColorSetting_(
                false, displayPage.displays[0]));

            // Verify that the arrangement section is not shown.
            assertEquals(null, displayPage.$$('#arrangement-section'));

            // Add a second display.
            addDisplay(2);
            fakeSystemDisplay.onDisplayChanged.callListeners();

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
            // There should be two displays, the first should be primary and
            // selected. Mirroring should be enabled but set to false.
            assertEquals(2, displayPage.displays.length);
            assertEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            assertEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
            assertTrue(displayPage.showMirror_(false, displayPage.displays));
            assertFalse(displayPage.isMirrored_(displayPage.displays));

            // Verify unified desktop only shown when enabled.
            assertTrue(displayPage.showUnifiedDesktop_(
                true, true, displayPage.displays));
            assertFalse(displayPage.showUnifiedDesktop_(
                false, false, displayPage.displays));

            // Sanity check the second display is not internal.
            assertFalse(displayPage.displays[1].isInternal);


            // Verify the display modes are parsed correctly.

            // 5 total modes, 2 parent modes.
            assertEquals(7, displayPage.modeToParentModeMap_.size);
            assertEquals(0, displayPage.modeToParentModeMap_.get(0));
            assertEquals(0, displayPage.modeToParentModeMap_.get(1));
            assertEquals(5, displayPage.modeToParentModeMap_.get(2));
            assertEquals(5, displayPage.modeToParentModeMap_.get(3));
            assertEquals(5, displayPage.modeToParentModeMap_.get(4));
            assertEquals(5, displayPage.modeToParentModeMap_.get(5));
            assertEquals(5, displayPage.modeToParentModeMap_.get(6));

            // Two resolution options, one for each parent mode.
            assertEquals(2, displayPage.refreshRateList_.length);

            // Each parent mode has the correct number of refresh rates.
            assertEquals(2, displayPage.parentModeToRefreshRateMap_.size);
            assertEquals(
                2, displayPage.parentModeToRefreshRateMap_.get(0).length);
            assertEquals(
                3, displayPage.parentModeToRefreshRateMap_.get(5).length);

            // Ambient EQ never shown on non-internal display regardless of
            // whether it is enabled.
            assertFalse(displayPage.showAmbientColorSetting_(
                true, displayPage.displays[1]));
            assertFalse(displayPage.showAmbientColorSetting_(
                false, displayPage.displays[1]));

            // Verify that the arrangement section is shown.
            assertTrue(!!displayPage.$$('#arrangement-section'));

            // Select the second display and make it primary. Also change the
            // orientation of the second display.
            const displayLayout = displayPage.$$('#displayLayout');
            assertTrue(!!displayLayout);
            const displayDiv = displayLayout.$$('#_fakeDisplayId2');
            assertTrue(!!displayDiv);
            displayDiv.click();
            assertEquals(
                displayPage.displays[1].id, displayPage.selectedDisplay.id);

            displayPage.updatePrimaryDisplay_({target: {value: '0'}});
            displayPage.onOrientationChange_({target: {value: '90'}});
            fakeSystemDisplay.onDisplayChanged.callListeners();

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
            // Confirm that the second display is selected, primary, and
            // rotated.
            assertEquals(2, displayPage.displays.length);
            assertEquals(
                displayPage.displays[1].id, displayPage.selectedDisplay.id);
            assertTrue(displayPage.displays[1].isPrimary);
            assertEquals(
                displayPage.displays[1].id, displayPage.primaryDisplayId);
            assertEquals(90, displayPage.displays[1].rotation);

            // Mirror the displays.
            displayPage.onMirroredTap_({target: {blur: function() {}}});
            fakeSystemDisplay.onDisplayChanged.callListeners();

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
              new Promise(function(resolve, reject) {
                setTimeout(resolve);
              })
            ]);
          })
          .then(function() {
            // Confirm that there is now only one display and that it is
            // primary and mirroring is enabled.
            assertEquals(1, displayPage.displays.length);
            assertEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            assertTrue(displayPage.displays[0].isPrimary);
            assertTrue(displayPage.showMirror_(false, displayPage.displays));
            assertTrue(displayPage.isMirrored_(displayPage.displays));

            // Verify that the arrangement section is shown while mirroring.
            assertTrue(!!displayPage.$$('#arrangement-section'));

            // Ensure that the zoom value remains unchanged while draggging.
            function pointerEvent(eventType, ratio) {
              const crSlider = displayPage.$.displaySizeSlider.$.slider;
              const rect = crSlider.$.container.getBoundingClientRect();
              crSlider.dispatchEvent(new PointerEvent(eventType, {
                buttons: 1,
                pointerId: 1,
                clientX: rect.left + (ratio * rect.width),
              }));
            }

            assertEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointerdown', .6);
            assertEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointermove', .3);
            assertEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointerup', 0);
            assertEquals(1.25, displayPage.selectedZoomPref_.value);

            // Navigate out of the display page.
            return showAndGetDeviceSubpage('power', routes.POWER);
          })
          .then(function() {
            // Moving out of the display page should set selected display to
            // invalid.
            assertEquals(
                displayPage.invalidDisplayId_,
                browserProxy.lastHighlightedDisplayId_);

            // Navigate back to the display page.
            return showAndGetDeviceSubpage('display', routes.DISPLAY);
          });
    });

    test('Deep link to display mirroring', async () => {
      const params = new URLSearchParams();
      params.append('settingId', '428');
      Router.getInstance().navigateTo(routes.DISPLAY, params);
      // await fakeSystemDisplay.getInfoCalled.promise;

      addDisplay(1);
      addDisplay(1);
      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      assertEquals(2, displayPage.displays.length);

      flush();
      // await fakeSystemDisplay.getInfoCalled.promise;
      assert(displayPage);
      assertEquals(2, displayPage.displays.length);
      assertTrue(displayPage.shouldShowArrangementSection_());

      const deepLinkElement =
          displayPage.$$('#displayMirrorCheckbox').$$('#checkbox');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Display mirroring checkbox should be focused for settingId=428.');
    });

    test('Keyboard display arrangement', async () => {
      addDisplay(1);
      addDisplay(2);
      fakeSystemDisplay.onDisplayChanged.callListeners();

      return Promise
          .all([
            fakeSystemDisplay.getInfoCalled.promise,
            fakeSystemDisplay.getLayoutCalled.promise,
          ])
          .then(() => {
            return new Promise(resolve => {
              flush();

              assert(displayPage);
              assertEquals(2, displayPage.displays.length);
              assertTrue(displayPage.shouldShowArrangementSection_());

              assertTrue(!!displayPage.$$('#arrangement-section'));

              assertTrue(displayPage.showMirror_(false, displayPage.displays));
              assertFalse(displayPage.isMirrored_(displayPage.displays));

              flush();

              displayPage.async(resolve);
            });
          })
          .then(() => {
            const displayLayout = displayPage.$$('#displayLayout');
            const display = displayLayout.$$('#_fakeDisplayId2');
            const layout =
                displayLayout.displayLayoutMap_.get('fakeDisplayId2');

            assertEquals(layout.parentId, 'fakeDisplayId1');
            assertEquals(layout.position, 'right');

            const offset =
                displayLayout.keyboardDragStepSize / displayLayout.visualScale;

            display.focus();

            display.dispatchEvent(new KeyboardEvent(
                'keydown', {key: 'ArrowDown', bubbles: true}));
            display.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
            assertEquals(offset, layout.offset);

            display.dispatchEvent(new KeyboardEvent(
                'keydown', {key: 'ArrowDown', bubbles: true}));
            display.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
            assertEquals(offset * 2, layout.offset);

            display.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'ArrowUp', bubbles: true}));
            display.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
            assertEquals(offset, layout.offset);
          });
    });
  });

  test(assert(TestNames.NightLight), async function() {
    // Set up a single display.
    const displayPage =
        await showAndGetDeviceSubpage('display', routes.DISPLAY);
    await fakeSystemDisplay.getInfoCalled.promise;
    addDisplay(1);
    fakeSystemDisplay.onDisplayChanged.callListeners();
    await fakeSystemDisplay.getInfoCalled.promise;
    await fakeSystemDisplay.getLayoutCalled.promise;
    assertEquals(1, displayPage.displays.length);

    const temperature = displayPage.$$('#nightLightTemperatureDiv');
    const schedule = displayPage.$$('#nightLightScheduleTypeDropDown');

    // Night Light is off, so temperature is hidden. Schedule is always shown.
    assertTrue(temperature.hidden);
    assertFalse(schedule.hidden);

    // Enable Night Light. Use an atomic update of |displayPage.prefs| so
    // Polymer notices the change.
    const newPrefs = getFakePrefs();
    newPrefs.ash.night_light.enabled.value = true;
    displayPage.prefs = newPrefs;
    flush();

    // Night Light is on, so temperature is visible.
    assertFalse(temperature.hidden);
    assertFalse(schedule.hidden);
  });

  suite(assert(TestNames.Power), function() {
    /**
     * Sets power sources using a deep copy of |sources|.
     * @param {Array<PowerSource>} sources
     * @param {string} powerSourceId
     * @param {bool} isLowPowerCharger
     */
    function setPowerSources(sources, powerSourceId, isLowPowerCharger) {
      const sourcesCopy = sources.map(function(source) {
        return Object.assign({}, source);
      });
      webUIListenerCallback(
          'power-sources-changed', sourcesCopy, powerSourceId,
          isLowPowerCharger);
    }

    suite('power settings', function() {
      let powerPage;
      let powerSourceRow;
      let powerSourceSelect;
      let acIdleSelect;
      let lidClosedToggle;
      let adaptiveChargingToggle;

      suiteSetup(function() {
        // Adaptive charging setting should be shown.
        loadTimeData.overrideValues({
          isAdaptiveChargingEnabled: true,
        });
      });

      setup(function() {
        return showAndGetDeviceSubpage('power', routes.POWER)
            .then(function(page) {
              powerPage = page;
              powerSourceRow = assert(powerPage.$$('#powerSourceRow'));
              powerSourceSelect = assert(powerPage.$$('#powerSource'));
              assertEquals(
                  1,
                  DevicePageBrowserProxyImpl.getInstance()
                      .updatePowerStatusCalled_);

              lidClosedToggle = assert(powerPage.$$('#lidClosedToggle'));
              adaptiveChargingToggle =
                  assert(powerPage.$$('#adaptiveChargingToggle'));

              assertEquals(
                  1,
                  DevicePageBrowserProxyImpl.getInstance()
                      .requestPowerManagementSettingsCalled_);
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON
                  ],
                  IdleBehavior.DISPLAY_OFF_SLEEP,
                  IdleBehavior.DISPLAY_OFF_SLEEP, false /* acIdleManaged */,
                  false /* batteryIdleManaged */, LidClosedBehavior.SUSPEND,
                  false /* lidClosedControlled */, true /* hasLid */,
                  false /* adaptiveCharging */);
            });
      });

      test('no battery', function() {
        const batteryStatus = {
          present: false,
          charging: false,
          calculating: false,
          percent: -1,
          statusText: '',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        flush();

        // Power source row is hidden since there's no battery.
        assertTrue(powerSourceRow.hidden);
        // Idle settings while on battery and while charging should not be
        // visible if the battery is not present.
        assertEquals(null, powerPage.$$('#batteryIdleSettingBox'));
        assertEquals(null, powerPage.$$('#acIdleSettingBox'));

        const acIdleSelect = assert(powerPage.$$('#noBatteryAcIdleSelect'));
        // Expect the "When idle" dropdown options to appear instead.
        assert(acIdleSelect);

        // Select a "When idle" selection and expect it to be set.
        selectValue(acIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(
            IdleBehavior.DISPLAY_ON,
            DevicePageBrowserProxyImpl.getInstance().acIdleBehavior_);
      });

      test('power sources', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false);
        flush();

        // Power sources row is visible but dropdown is hidden.
        assertFalse(powerSourceRow.hidden);
        assertTrue(powerSourceSelect.hidden);

        // Attach a dual-role USB device.
        const powerSource = {
          id: '2',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false);
        flush();

        // "Battery" should be selected.
        assertFalse(powerSourceSelect.hidden);
        assertEquals('', powerSourceSelect.value);

        // Select the power source.
        setPowerSources([powerSource], powerSource.id, true);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);

        // Send another power source; the first should still be selected.
        const otherPowerSource = Object.assign({}, powerSource);
        otherPowerSource.id = '3';
        setPowerSources([otherPowerSource, powerSource], powerSource.id, true);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);
      });

      test('choose power source', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));

        // Attach a dual-role USB device.
        const powerSource = {
          id: '3',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false);
        flush();

        // Select the device.
        selectValue(powerSourceSelect, powerSourceSelect.children[1].value);
        assertEquals(
            powerSource.id,
            DevicePageBrowserProxyImpl.getInstance().powerSourceId_);
      });

      test('set AC idle behavior', function() {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false);
        flush();

        acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
        selectValue(acIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(
            IdleBehavior.DISPLAY_ON,
            DevicePageBrowserProxyImpl.getInstance().acIdleBehavior_);
      });

      test('set battery idle behavior', function() {
        return new Promise(function(resolve) {
                 // Indicate battery presence so that idle settings box while
                 // on battery is visible.
                 const batteryStatus = {
                   present: true,
                   charging: false,
                   calculating: false,
                   percent: 50,
                   statusText: '5 hours left',
                 };
                 webUIListenerCallback(
                     'battery-status-changed',
                     Object.assign({}, batteryStatus));
                 powerPage.async(resolve);
               })
            .then(function() {
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              selectValue(batteryIdleSelect, IdleBehavior.DISPLAY_ON);
              assertEquals(
                  IdleBehavior.DISPLAY_ON,
                  DevicePageBrowserProxyImpl.getInstance()
                      .batteryIdleBehavior_);
            });
      });

      test('set lid behavior', function() {
        const sendLid = function(lidBehavior) {
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON
              ],
              IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
              false /* acIdleManaged */, false /* batteryIdleManaged */,
              lidBehavior, false /* lidClosedControlled */, true /* hasLid */,
              false /* adaptiveCharging */);
        };

        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);

        lidClosedToggle.shadowRoot.querySelector('#control').click();
        assertEquals(
            LidClosedBehavior.DO_NOTHING,
            DevicePageBrowserProxyImpl.getInstance().lidClosedBehavior_);
        sendLid(LidClosedBehavior.DO_NOTHING);
        assertFalse(lidClosedToggle.checked);

        lidClosedToggle.shadowRoot.querySelector('#control').click();
        assertEquals(
            LidClosedBehavior.SUSPEND,
            DevicePageBrowserProxyImpl.getInstance().lidClosedBehavior_);
        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);
      });
      test('display idle behavior for shut_down/stop_session', function() {
        return new Promise(function(resolve) {
                 // Send power management settings first.
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                       IdleBehavior.SHUT_DOWN,
                       IdleBehavior.STOP_SESSION,
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP,
                       IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON,
                       IdleBehavior.SHUT_DOWN,
                       IdleBehavior.STOP_SESSION,
                     ],
                     IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                     true /* acIdleManaged */, true /* batteryIdleManaged */,
                     LidClosedBehavior.DO_NOTHING,
                     false /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */);
                 powerPage.async(resolve);
               })
            .then(function() {
              // Indicate battery presence so that battery idle settings
              // box becomes visible. Default option should be selected
              // properly even when battery idle settings box is stamped
              // later.
              const batteryStatus = {
                present: true,
                charging: false,
                calculating: false,
                percent: 50,
                statusText: '5 hours left',
              };
              webUIListenerCallback(
                  'battery-status-changed', Object.assign({}, batteryStatus));
              return new Promise(function(resolve) {
                powerPage.async(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertFalse(batteryIdleSelect.disabled);
              const acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                    IdleBehavior.SHUT_DOWN,
                    IdleBehavior.STOP_SESSION,
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP,
                    IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON,
                    IdleBehavior.SHUT_DOWN,
                    IdleBehavior.STOP_SESSION,
                  ],
                  IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                  true /* acIdleManaged */, true /* batteryIdleManaged */,
                  LidClosedBehavior.DO_NOTHING, false /* lidClosedControlled */,
                  true /* hasLid */, false /* adaptiveCharging */);
              return new Promise(function(resolve) {
                powerPage.async(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertFalse(batteryIdleSelect.disabled);
              const acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            });
      });
      test('display idle and lid behavior', function() {
        return new Promise(function(resolve) {
                 // Send power management settings first.
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON
                     ],
                     IdleBehavior.DISPLAY_ON, IdleBehavior.DISPLAY_OFF,
                     false /* acIdleManaged */, false /* batteryIdleManaged */,
                     LidClosedBehavior.DO_NOTHING,
                     false /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */);
                 powerPage.async(resolve);
               })
            .then(function() {
              // Indicate battery presence so that battery idle settings
              // box becomes visible. Default option should be selected
              // properly even when battery idle settings box is stamped
              // later.
              const batteryStatus = {
                present: true,
                charging: false,
                calculating: false,
                percent: 50,
                statusText: '5 hours left',
              };
              webUIListenerCallback(
                  'battery-status-changed', Object.assign({}, batteryStatus));
              return new Promise(function(resolve) {
                powerPage.async(resolve);
              });
            })
            .then(function() {
              acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_ON.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertEquals(null, powerPage.$$('#acIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertFalse(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON
                  ],
                  [
                    IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                    IdleBehavior.DISPLAY_ON
                  ],
                  IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_ON,
                  false /* acIdleManaged */, false /* batteryIdleManaged */,
                  LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
                  true /* hasLid */, false /* adaptiveCharging */);
              return new Promise(function(resolve) {
                powerPage.async(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_ON.toString(), batteryIdleSelect.value);
              assertFalse(acIdleSelect.disabled);
              assertFalse(batteryIdleSelect.disabled);
              assertEquals(null, powerPage.$$('#acIdleManagedIndicator'));
              assertEquals(null, powerPage.$$('#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSleepLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertFalse(lidClosedToggle.isPrefEnforced());
            });
      });

      test('display managed idle and lid behavior', function() {
        // When settings are managed, the controls should be disabled and
        // the indicators should be shown.
        return new Promise(function(resolve) {
                 // Indicate battery presence so that idle settings box while
                 // on battery is visible.
                 const batteryStatus = {
                   present: true,
                   charging: false,
                   calculating: false,
                   percent: 50,
                   statusText: '5 hours left',
                 };
                 webUIListenerCallback(
                     'battery-status-changed',
                     Object.assign({}, batteryStatus));
                 sendPowerManagementSettings(
                     [IdleBehavior.SHUT_DOWN], [IdleBehavior.SHUT_DOWN],
                     IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
                     true /* acIdleManaged */, true /* batteryIdleManaged */,
                     LidClosedBehavior.SHUT_DOWN,
                     true /* lidClosedControlled */, true /* hasLid */,
                     false /* adaptiveCharging */);
                 powerPage.async(resolve);
               })
            .then(function() {
              acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
              assertTrue(acIdleSelect.disabled);
              assertTrue(batteryIdleSelect.disabled);
              expectNotEquals(null, powerPage.$$('#acIdleManagedIndicator'));
              expectNotEquals(
                  null, powerPage.$$('#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidShutDownLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertTrue(lidClosedToggle.isPrefEnforced());
            })
            .then(function() {
              sendPowerManagementSettings(
                  [IdleBehavior.DISPLAY_OFF], [IdleBehavior.DISPLAY_OFF],
                  IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
                  false /* acIdleManaged */, false /* batteryIdleManaged */,
                  LidClosedBehavior.STOP_SESSION,
                  true /* lidClosedControlled */, true /* hasLid */,
                  false /* adaptiveCharging */);
              return new Promise(function(resolve) {
                powerPage.async(resolve);
              });
            })
            .then(function() {
              const batteryIdleSelect =
                  assert(powerPage.$$('#batteryIdleSelect'));
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
              assertEquals(
                  IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
              assertTrue(acIdleSelect.disabled);
              assertTrue(batteryIdleSelect.disabled);
              assertEquals(null, powerPage.$$('#acIdleManagedIndicator'));
              assertEquals(null, powerPage.$$('#batteryIdleManagedIndicator'));
              assertEquals(
                  loadTimeData.getString('powerLidSignOutLabel'),
                  lidClosedToggle.label);
              assertTrue(lidClosedToggle.checked);
              assertTrue(lidClosedToggle.isPrefEnforced());
            });
      });

      test('hide lid behavior when lid not present', function() {
        return new Promise(function(resolve) {
                 assertFalse(powerPage.$$('#lidClosedToggle').hidden);
                 sendPowerManagementSettings(
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON
                     ],
                     [
                       IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF,
                       IdleBehavior.DISPLAY_ON
                     ],
                     IdleBehavior.DISPLAY_OFF_SLEEP,
                     IdleBehavior.DISPLAY_OFF_SLEEP, false /* acIdleManaged */,
                     false /* batteryIdleManaged */, LidClosedBehavior.SUSPEND,
                     false /* lidClosedControlled */, false /* hasLid */,
                     false /* adaptiveCharging */);
                 powerPage.async(resolve);
               })
            .then(function() {
              assertTrue(powerPage.$$('#lidClosedToggle').hidden);
            });
      });

      test(
          'hide display controlled battery idle behavior when battery not present',
          function() {
            return new Promise(function(resolve) {
                     const batteryStatus = {
                       present: false,
                       charging: false,
                       calculating: false,
                       percent: -1,
                       statusText: '',
                     };
                     webUIListenerCallback(
                         'battery-status-changed',
                         Object.assign({}, batteryStatus));
                     flush();
                     powerPage.async(resolve);
                   })
                .then(function() {
                  assertEquals(null, powerPage.$$('#batteryIdleSettingBox'));
                });
          });

      test('Deep link to sleep when laptop lid closed', async () => {
        return checkDeepLink(
            routes.POWER, '424',
            lidClosedToggle.shadowRoot.querySelector('cr-toggle'),
            'Sleep when closed toggle');
      });

      test('Deep link to adaptive charging', async () => {
        return checkDeepLink(
            routes.POWER, '440',
            adaptiveChargingToggle.shadowRoot.querySelector('cr-toggle'),
            'Adaptive charging toggle');
      });
    });
  });

  suite(assert(TestNames.Stylus), function() {
    let stylusPage;
    let appSelector;
    let browserProxy;
    let noAppsDiv;
    let waitingDiv;

    // Shorthand for NoteAppLockScreenSupport.
    let LockScreenSupport;

    suiteSetup(function() {
      // Always show stylus settings.
      loadTimeData.overrideValues({
        hasInternalStylus: true,
      });
    });

    setup(function() {
      return showAndGetDeviceSubpage('stylus', routes.STYLUS)
          .then(function(page) {
            stylusPage = page;
            browserProxy = DevicePageBrowserProxyImpl.getInstance();
            appSelector = assert(page.$$('#selectApp'));
            noAppsDiv = assert(page.$$('#no-apps'));
            waitingDiv = assert(page.$$('#waiting'));
            LockScreenSupport = NoteAppLockScreenSupport;

            assertEquals(1, browserProxy.requestNoteTakingApps_);
            assert(browserProxy.onNoteTakingAppsUpdated_);
          });
    });

    // Helper function to allocate a note app entry.
    function entry(name, value, preferred, lockScreenSupport) {
      return {
        name: name,
        value: value,
        preferred: preferred,
        lockScreenSupport: lockScreenSupport
      };
    }

    /**  @return {?Element} */
    function noteTakingAppLockScreenSettings() {
      return stylusPage.$$('#note-taking-app-lock-screen-settings');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenToggle() {
      return stylusPage.$$('#enable-app-on-lock-screen-toggle');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenPolicyIndicator() {
      return stylusPage.$$('#enable-app-on-lock-screen-policy-indicator');
    }

    /** @return {?Element} */
    function enableAppOnLockScreenToggleLabel() {
      return stylusPage.$$('#lock-screen-toggle-label');
    }

    /** @return {?Element} */
    function keepLastNoteOnLockScreenToggle() {
      return stylusPage.$$('#keep-last-note-on-lock-screen-toggle');
    }

    test('stylus tools prefs', function() {
      // Both stylus tools prefs are initially false.
      assertFalse(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(
          devicePage.prefs.settings.launch_palette_on_eject_event.value);

      // Since both prefs are initially false, the launch palette on eject pref
      // toggle is disabled.
      assertTrue(isVisible(stylusPage.$$('#enableStylusToolsToggle')));
      assertTrue(isVisible(stylusPage.$$('#launchPaletteOnEjectEventToggle')));
      assertTrue(stylusPage.$$('#launchPaletteOnEjectEventToggle').disabled);
      assertFalse(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(
          devicePage.prefs.settings.launch_palette_on_eject_event.value);

      // Tapping the enable stylus tools pref causes the launch palette on
      // eject pref toggle to not be disabled anymore.
      stylusPage.$$('#enableStylusToolsToggle').click();
      assertTrue(devicePage.prefs.settings.enable_stylus_tools.value);
      assertFalse(stylusPage.$$('#launchPaletteOnEjectEventToggle').disabled);
      stylusPage.$$('#launchPaletteOnEjectEventToggle').click();
      assertTrue(devicePage.prefs.settings.launch_palette_on_eject_event.value);
    });

    test('choose first app if no preferred ones', function() {
      // Selector chooses the first value in list if there is no preferred
      // value set.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED)
      ]);
      flush();
      assertEquals('v1', appSelector.value);
    });

    test('choose prefered app if exists', function() {
      // Selector chooses the preferred value if set.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
      ]);
      flush();
      assertEquals('v2', appSelector.value);
    });

    test('change preferred app', function() {
      // Load app list.
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
      ]);
      flush();
      assertEquals(0, browserProxy.setPreferredAppCount_);
      assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

      // Update select element to new value, verify browser proxy is called.
      appSelector.value = 'v1';
      stylusPage.onSelectedAppChanged_();
      assertEquals(1, browserProxy.setPreferredAppCount_);
      assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());
    });

    test('preferred app does not change without interaction', function() {
      // Pass various types of data to page, verify the preferred note-app
      // does not change.
      browserProxy.setNoteTakingApps([]);
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.onNoteTakingAppsUpdated_([], true);
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.addNoteTakingApp(
          entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
      flush();
      assertEquals('', browserProxy.getPreferredNoteTakingAppId());

      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
      ]);
      flush();
      assertEquals(0, browserProxy.setPreferredAppCount_);
      assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
    });

    test('Deep link to preferred app', async () => {
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED)
      ]);
      browserProxy.setAndroidAppsReceived(true);

      return checkDeepLink(
          routes.STYLUS, '417', stylusPage.$$('#selectApp'),
          'Note-taking apps dropdown');
    });

    test('app-visibility', function() {
      // No apps available.
      browserProxy.setNoteTakingApps([]);
      assert(noAppsDiv.hidden);
      assert(!waitingDiv.hidden);
      assert(appSelector.hidden);

      // Waiting for apps to finish loading.
      browserProxy.setAndroidAppsReceived(true);
      assert(!noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(appSelector.hidden);

      // Apps loaded, show selector.
      browserProxy.addNoteTakingApp(
          entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
      assert(noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(!appSelector.hidden);

      // Waiting for Android apps again.
      browserProxy.setAndroidAppsReceived(false);
      assert(noAppsDiv.hidden);
      assert(!waitingDiv.hidden);
      assert(appSelector.hidden);

      browserProxy.setAndroidAppsReceived(true);
      assert(noAppsDiv.hidden);
      assert(waitingDiv.hidden);
      assert(!appSelector.hidden);
    });

    test('enabled-on-lock-screen', function() {
      assertFalse(isVisible(noteTakingAppLockScreenSettings()));
      assertFalse(isVisible(enableAppOnLockScreenToggle()));
      assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

      return new Promise(function(resolve) {
               // No apps available.
               browserProxy.setNoteTakingApps([]);
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Single app which does not support lock screen note taking.
            browserProxy.addNoteTakingApp(
                entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED));
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Add an app with lock screen support, but do not select it yet.
            browserProxy.addNoteTakingApp(
                entry('n2', 'v2', false, LockScreenSupport.SUPPORTED));
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // Select the app with lock screen app support.
            appSelector.value = 'v2';
            stylusPage.onSelectedAppChanged_();
            assertEquals(1, browserProxy.setPreferredAppCount_);
            assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);

            // Preferred app updated to be enabled on lock screen.
            browserProxy.setNoteTakingApps([
              entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
              entry('n2', 'v2', true, LockScreenSupport.ENABLED)
            ]);
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertTrue(enableAppOnLockScreenToggle().checked);

            // Select the app that does not support lock screen again.
            appSelector.value = 'v1';
            stylusPage.onSelectedAppChanged_();
            assertEquals(2, browserProxy.setPreferredAppCount_);
            assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
          });
    });

    test('initial-app-lock-screen-enabled', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();

            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps(
                [entry('n1', 'v1', true, LockScreenSupport.ENABLED)]);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps(
                [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            browserProxy.setNoteTakingApps([]);
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(enableAppOnLockScreenToggle()));
          });
    });

    test('tap-on-enable-note-taking-on-lock-screen', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();

            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            enableAppOnLockScreenToggle().click();
            assertEquals(1, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.ENABLED,
                browserProxy.getPreferredAppLockScreenState());

            enableAppOnLockScreenToggle().click();
            assertEquals(2, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertEquals(
                LockScreenSupport.SUPPORTED,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('tap-on-enable-note-taking-on-lock-screen-label', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps(
                   [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();

            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);

            enableAppOnLockScreenToggleLabel().click();
            assertEquals(1, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(enableAppOnLockScreenToggle().checked);
            assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.ENABLED,
                browserProxy.getPreferredAppLockScreenState());

            enableAppOnLockScreenToggleLabel().click();
            assertEquals(2, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertEquals(
                LockScreenSupport.SUPPORTED,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('lock-screen-apps-disabled-by-policy', function() {
      assertFalse(isVisible(enableAppOnLockScreenToggle()));
      assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

      return new Promise(function(resolve) {
               // Add an app with lock screen support.
               browserProxy.addNoteTakingApp(entry(
                   'n2', 'v2', true, LockScreenSupport.NOT_ALLOWED_BY_POLICY));
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

            // The toggle should be disabled, so enabling app on lock screen
            // should not be attempted.
            enableAppOnLockScreenToggle().click();
            assertEquals(0, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();

            // Tap on label should not work either.
            enableAppOnLockScreenToggleLabel().click();
            assertEquals(0, browserProxy.setAppOnLockScreenCount_);

            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(enableAppOnLockScreenToggle()));
            assertFalse(enableAppOnLockScreenToggle().checked);
            assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

            assertEquals(
                LockScreenSupport.NOT_ALLOWED_BY_POLICY,
                browserProxy.getPreferredAppLockScreenState());
          });
    });

    test('keep-last-note-on-lock-screen', function() {
      return new Promise(function(resolve) {
               browserProxy.setNoteTakingApps([
                 entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED),
                 entry('n2', 'v2', false, LockScreenSupport.SUPPORTED)
               ]);
               stylusPage.async(resolve);
             })
          .then(function() {
            flush();
            assertFalse(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));

            browserProxy.setNoteTakingApps([
              entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
              entry('n2', 'v2', true, LockScreenSupport.SUPPORTED)
            ]);
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));

            browserProxy.setNoteTakingApps([
              entry('n2', 'v2', true, LockScreenSupport.ENABLED),
            ]);
            return new Promise(function(resolve) {
              stylusPage.async(resolve);
            });
          })
          .then(function() {
            flush();
            assertTrue(isVisible(noteTakingAppLockScreenSettings()));
            assert(isVisible(keepLastNoteOnLockScreenToggle()));
            assertTrue(keepLastNoteOnLockScreenToggle().checked);

            // Clicking the toggle updates the pref value.
            keepLastNoteOnLockScreenToggle()
                .shadowRoot.querySelector('#control')
                .click();
            assertFalse(keepLastNoteOnLockScreenToggle().checked);

            assertFalse(
                devicePage.prefs.settings.restore_last_lock_screen_note.value);

            // Changing the pref value updates the toggle.
            devicePage.set(
                'prefs.settings.restore_last_lock_screen_note.value', true);
            assertTrue(keepLastNoteOnLockScreenToggle().checked);
          });
    });
  });

  suite(assert(TestNames.Storage), function() {
    /** @type {!Element} */
    let storagePage;

    /**
     * Simulate storage size stat callback.
     * @param {string} availableSize
     * @param {string} usedSize
     * @param {number} usedRatio
     * @param {number} spaceState
     */
    function sendStorageSizeStat(
        usedSize, availableSize, usedRatio, spaceState) {
      webUIListenerCallback('storage-size-stat-changed', {
        usedSize: usedSize,
        availableSize: availableSize,
        usedRatio: usedRatio,
        spaceState: spaceState,
      });
      flush();
    }

    /**
     * @param {string} id
     * @return {string}
     */
    function getStorageItemLabelFromId(id) {
      const rowItem = storagePage.$$('#' + id).shadowRoot;
      return rowItem.querySelector('#label').innerText;
    }

    /**
     * @param {string} id
     * @return {string}
     */
    function getStorageItemSubLabelFromId(id) {
      const rowItem = storagePage.$$('#' + id).shadowRoot;
      return rowItem.querySelector('#subLabel').innerText;
    }

    suiteSetup(function() {
      // Disable animations so sub-pages open within one event loop.
      testing.Test.disableAnimationsAndTransitions();
    });

    setup(function() {
      return showAndGetDeviceSubpage('storage', routes.STORAGE)
          .then(function(page) {
            storagePage = page;
            storagePage.stopPeriodicUpdate_();
          });
    });

    test('storage stats size', async function() {
      // Low available storage space.
      sendStorageSizeStat('9.1 GB', '0.9 GB', 0.91, StorageSpaceState.LOW);
      assertEquals('91%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('9%', storagePage.$.availableLabelArea.style.width);
      assertTrue(isVisible(storagePage.$$('#lowMessage')));
      assertFalse(isVisible(storagePage.$$('#criticallyLowMessage')));
      assertTrue(!!storagePage.$$('#bar.space-low'));
      assertFalse(!!storagePage.$$('#bar.space-critically-low'));
      assertEquals(
          '9.1 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '0.9 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);

      // Critically low available storage space.
      sendStorageSizeStat(
          '9.7 GB', '0.3 GB', 0.97, StorageSpaceState.CRITICALLY_LOW);
      assertEquals('97%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('3%', storagePage.$.availableLabelArea.style.width);
      assertFalse(isVisible(storagePage.$$('#lowMessage')));
      assertTrue(isVisible(storagePage.$$('#criticallyLowMessage')));
      assertFalse(!!storagePage.$$('#bar.space-low'));
      assertTrue(!!storagePage.$$('#bar.space-critically-low'));
      assertEquals(
          '9.7 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '0.3 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);

      // Normal storage usage.
      sendStorageSizeStat('2.5 GB', '7.5 GB', 0.25, StorageSpaceState.NORMAL);
      assertEquals('25%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('75%', storagePage.$.availableLabelArea.style.width);
      assertFalse(isVisible(storagePage.$$('#lowMessage')));
      assertFalse(isVisible(storagePage.$$('#criticallyLowMessage')));
      assertFalse(!!storagePage.$$('#bar.space-low'));
      assertFalse(!!storagePage.$$('#bar.space-critically-low'));
      assertEquals(
          '2.5 GB',
          storagePage.$.inUseLabelArea.querySelector('.storage-size')
              .innerText);
      assertEquals(
          '7.5 GB',
          storagePage.$.availableLabelArea.querySelector('.storage-size')
              .innerText);
    });

    test('system size', async function() {
      assertEquals('System', storagePage.$$('#systemSizeLabel').innerText);
      assertEquals(
          'Calculating…', storagePage.$$('#systemSizeSubLabel').innerText);

      // Send system size callback.
      webUIListenerCallback('storage-system-size-changed', '8.4 GB');
      flush();
      assertEquals('8.4 GB', storagePage.$$('#systemSizeSubLabel').innerText);

      // In guest mode, the system row should be hidden.
      storagePage.isGuest_ = true;
      flush();
      assertFalse(isVisible(storagePage.$$('#systemSize')));
    });

    test('apps extensions size', async function() {
      assertEquals(
          'Apps and extensions', getStorageItemLabelFromId('appsSize'));
      assertEquals('Calculating…', getStorageItemSubLabelFromId('appsSize'));

      // Send apps size callback.
      webUIListenerCallback('storage-apps-size-changed', '59.5 KB');
      flush();
      assertEquals('59.5 KB', getStorageItemSubLabelFromId('appsSize'));
    });

    test('other users size', async function() {
      // The other users row is visible by default, displaying
      // "calculating...".
      assertTrue(isVisible(storagePage.$$('#otherUsersSize')));
      assertEquals('Other users', getStorageItemLabelFromId('otherUsersSize'));
      assertEquals(
          'Calculating…', getStorageItemSubLabelFromId('otherUsersSize'));

      // Simulate absence of other users.
      webUIListenerCallback('storage-other-users-size-changed', '0 B', true);
      flush();
      assertFalse(isVisible(storagePage.$$('#otherUsersSize')));

      // Send other users callback with a size that is not null.
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertTrue(isVisible(storagePage.$$('#otherUsersSize')));
      assertEquals('322 MB', getStorageItemSubLabelFromId('otherUsersSize'));

      // If the user is in Guest mode, the row is not visible.
      storagePage.isGuest_ = true;
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertFalse(isVisible(storagePage.$$('#otherUsersSize')));
    });
  });
});
