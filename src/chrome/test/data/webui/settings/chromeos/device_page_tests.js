// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('device_page_tests', function() {
  /** @enum {string} */
  const TestNames = {
    DevicePage: 'device page',
    Display: 'display',
    Keyboard: 'keyboard',
    NightLight: 'night light',
    Pointers: 'pointers',
    Power: 'power',
    Storage: 'storage',
    Stylus: 'stylus',
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
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
      cr.webUIListenerCallback('has-mouse-changed', true);
      cr.webUIListenerCallback('has-touchpad-changed', true);
    },

    /** override */
    initializeStylus: function() {
      // Enable stylus.
      cr.webUIListenerCallback('has-stylus-changed', true);
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
                settings.NoteAppLockScreenSupport.SUPPORTED,
                app.lockScreenSupport);
          }
          if (app.lockScreenSupport ===
              settings.NoteAppLockScreenSupport.SUPPORTED) {
            app.lockScreenSupport = settings.NoteAppLockScreenSupport.ENABLED;
          }
        } else {
          if (app.preferred) {
            assertEquals(
                settings.NoteAppLockScreenSupport.ENABLED,
                app.lockScreenSupport);
          }
          if (app.lockScreenSupport ===
              settings.NoteAppLockScreenSupport.ENABLED) {
            app.lockScreenSupport = settings.NoteAppLockScreenSupport.SUPPORTED;
          }
        }
      });

      this.scheduleLockScreenAppsUpdated_();
    },

    /** @override */
    highlightDisplay: function(id) {
      this.lastHighlightedDisplayId_ = id;
    },

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
     * @return {settings.NoteAppLockScreenSupport | undefined} The lock screen
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
     * @param {Array<!settings.NoteAppInfo>} The list of apps to set.
     */
    setNoteTakingApps: function(apps) {
      this.noteTakingApps_ = apps;
      this.scheduleLockScreenAppsUpdated_();
    },

    /**
     * Adds an app to the list of known note-taking apps.
     * @param {!settings.NoteAppInfo}
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
      fakeSystemDisplay = new settings.FakeSystemDisplay();
      settings.display.systemDisplayApi = fakeSystemDisplay;

      PolymerTest.clearBody();
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);

      devicePage = document.createElement('settings-device-page');
      devicePage.prefs = getFakePrefs();
      settings.DevicePageBrowserProxyImpl.instance_ =
          new TestDevicePageBrowserProxy();

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
      assertEquals(
          expectedRoute, settings.Router.getInstance().getCurrentRoute());
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
     * @param {!Array<!settings.IdleBehavior>} possibleAcIdleBehaviors
     * @param {!Array<!settings.IdleBehavior>} possibleBatteryIdleBehaviors
     * @param {settings.IdleBehavior} currAcIdleBehavior
     * @param {settings.IdleBehavior} currBatteryIdleBehavior
     * @param {boolean} acIdleManaged
     * @param {boolean} batteryIdleManaged
     * @param {settings.LidClosedBehavior} lidClosedBehavior
     * @param {boolean} lidClosedControlled
     * @param {boolean} hasLid
     */
    function sendPowerManagementSettings(
        possibleAcIdleBehaviors, possibleBatteryIdleBehaviors,
        currAcIdleBehavior, currBatteryIdleBehavior, acIdleManaged,
        batteryIdleManaged, lidClosedBehavior, lidClosedControlled, hasLid) {
      cr.webUIListenerCallback('power-management-settings-changed', {
        possibleAcIdleBehaviors: possibleAcIdleBehaviors,
        possibleBatteryIdleBehaviors: possibleBatteryIdleBehaviors,
        currentAcIdleBehavior: currAcIdleBehavior,
        currentBatteryIdleBehavior: currBatteryIdleBehavior,
        acIdleManaged: acIdleManaged,
        batteryIdleManaged: batteryIdleManaged,
        lidClosedBehavior: lidClosedBehavior,
        lidClosedControlled: lidClosedControlled,
        hasLid: hasLid,
      });
      Polymer.dom.flush();
    }

    /**
     * @param {!HTMLElement} select
     * @param {!value} string
     */
    function selectValue(select, value) {
      select.value = value;
      select.dispatchEvent(new CustomEvent('change'));
      Polymer.dom.flush();
    }

    /**
     * @param {!HTMLElement} pointersPage
     * @param {boolean} expected
     */
    function expectReverseScrollValue(pointersPage, expected) {
      const reverseScrollToggle =
          pointersPage.$$('#enableReverseScrollingToggle');
      assertEquals(expected, reverseScrollToggle.checked);
      expectNotEquals(
          expected, devicePage.prefs.settings.touchpad.natural_scroll.value);
    }

    test(assert(TestNames.DevicePage), function() {
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
      expectLT(0, devicePage.$$('#keyboardRow').offsetHeight);
      expectLT(0, devicePage.$$('#displayRow').offsetHeight);

      cr.webUIListenerCallback('has-mouse-changed', false);
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
      cr.webUIListenerCallback('has-touchpad-changed', false);
      expectEquals(0, devicePage.$$('#pointersRow').offsetHeight);
      cr.webUIListenerCallback('has-mouse-changed', true);
      expectLT(0, devicePage.$$('#pointersRow').offsetHeight);
    });

    suite(assert(TestNames.Pointers), function() {
      let pointersPage;

      setup(function() {
        return showAndGetDeviceSubpage('pointers', settings.routes.POINTERS)
            .then(function(page) {
              pointersPage = page;
            });
      });

      test('subpage responds to pointer attach/detach', function() {
        assertEquals(
            settings.routes.POINTERS,
            settings.Router.getInstance().getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
        assertLT(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertLT(0, pointersPage.$$('#touchpad h2').offsetHeight);

        cr.webUIListenerCallback('has-touchpad-changed', false);
        assertEquals(
            settings.routes.POINTERS,
            settings.Router.getInstance().getCurrentRoute());
        assertLT(0, pointersPage.$$('#mouse').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad').offsetHeight);
        assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
        assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

        cr.webUIListenerCallback('has-mouse-changed', false);
        assertEquals(
            settings.routes.DEVICE,
            settings.Router.getInstance().getCurrentRoute());
        assertEquals(0, devicePage.$$('#main #pointersRow').offsetHeight);

        cr.webUIListenerCallback('has-touchpad-changed', true);
        assertLT(0, devicePage.$$('#main #pointersRow').offsetHeight);

        return showAndGetDeviceSubpage('pointers', settings.routes.POINTERS)
            .then(function(page) {
              assertEquals(0, pointersPage.$$('#mouse').offsetHeight);
              assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
              assertEquals(0, pointersPage.$$('#mouse h2').offsetHeight);
              assertEquals(0, pointersPage.$$('#touchpad h2').offsetHeight);

              cr.webUIListenerCallback('has-mouse-changed', true);
              assertEquals(
                  settings.routes.POINTERS,
                  settings.Router.getInstance().getCurrentRoute());
              assertLT(0, pointersPage.$$('#mouse').offsetHeight);
              assertLT(0, pointersPage.$$('#touchpad').offsetHeight);
              assertLT(0, pointersPage.$$('#mouse h2').offsetHeight);
              assertLT(0, pointersPage.$$('#touchpad h2').offsetHeight);
            });
      });

      test('mouse', function() {
        expectLT(0, pointersPage.$$('#mouse').offsetHeight);

        expectFalse(pointersPage.$$('#mouse settings-toggle-button').checked);

        const slider = assert(pointersPage.$$('#mouse settings-slider'));
        expectEquals(4, slider.pref.value);
        MockInteractions.pressAndReleaseKeyOn(
            slider.$$('cr-slider'), 37, [], 'ArrowLeft');
        expectEquals(3, devicePage.prefs.settings.mouse.sensitivity2.value);

        pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
        expectEquals(5, slider.pref.value);
      });

      test('touchpad', function() {
        expectLT(0, pointersPage.$$('#touchpad').offsetHeight);

        expectTrue(pointersPage.$$('#touchpad #enableTapToClick').checked);
        expectFalse(pointersPage.$$('#touchpad #enableTapDragging').checked);

        const slider = assert(pointersPage.$$('#touchpad settings-slider'));
        expectEquals(3, slider.pref.value);
        MockInteractions.pressAndReleaseKeyOn(
            slider.$$('cr-slider'), 39 /* right */, [], 'ArrowRight');
        expectEquals(4, devicePage.prefs.settings.touchpad.sensitivity2.value);

        pointersPage.set('prefs.settings.touchpad.sensitivity2.value', 2);
        expectEquals(2, slider.pref.value);
      });

      test('link doesn\'t activate control', function() {
        expectReverseScrollValue(pointersPage, true);

        // Tapping the link shouldn't enable the radio button.
        const reverseScrollLabel =
            pointersPage.$$('#enableReverseScrollingLabel');
        const a = reverseScrollLabel.$.container.querySelector('a');
        expectTrue(!!a);
        // Prevent actually opening a link, which would block test.
        a.removeAttribute('href');
        a.click();
        expectReverseScrollValue(pointersPage, true);

        // Check specifically clicking toggle changes pref.
        const reverseScrollToggle =
            pointersPage.$$('#enableReverseScrollingToggle');
        reverseScrollToggle.click();
        expectReverseScrollValue(pointersPage, false);
        devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
        expectReverseScrollValue(pointersPage, true);

        // Check specifically clicking the row changes pref.
        const reverseScrollSettings = pointersPage.$$('#reverseScrollRow');
        reverseScrollToggle.click();
        expectReverseScrollValue(pointersPage, false);
        devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
        expectReverseScrollValue(pointersPage, true);
      });
    });

    test(assert(TestNames.Keyboard), async () => {
      const name = k => `prefs.settings.language.${k}.value`;
      const get = k => devicePage.get(name(k));
      const set = (k, v) => devicePage.set(name(k), v);
      // Open the keyboard subpage.
      const keyboardPage =
          await showAndGetDeviceSubpage('keyboard', settings.routes.KEYBOARD);
      // Initially, the optional keys are hidden.
      expectFalse(!!keyboardPage.$$('#capsLockKey'));

      // Pretend no internal keyboard is available.
      const keyboardParams = {
        'showCapsLock': false,
        'showExternalMetaKey': false,
        'showAppleCommandKey': false,
        'hasInternalKeyboard': false,
        'hasAssistantKey': false,
      };
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectFalse(!!keyboardPage.$$('#internalSearchKey'));
      expectFalse(!!keyboardPage.$$('#capsLockKey'));
      expectFalse(!!keyboardPage.$$('#externalMetaKey'));
      expectFalse(!!keyboardPage.$$('#externalCommandKey'));
      expectFalse(!!keyboardPage.$$('#assistantKey'));

      // Pretend a Caps Lock key is now available.
      keyboardParams['showCapsLock'] = true;
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectFalse(!!keyboardPage.$$('#internalSearchKey'));
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectFalse(!!keyboardPage.$$('#externalMetaKey'));
      expectFalse(!!keyboardPage.$$('#externalCommandKey'));
      expectFalse(!!keyboardPage.$$('#assistantKey'));

      // Add a non-Apple external keyboard.
      keyboardParams['showExternalMetaKey'] = true;
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectFalse(!!keyboardPage.$$('#internalSearchKey'));
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#externalMetaKey'));
      expectFalse(!!keyboardPage.$$('#externalCommandKey'));
      expectFalse(!!keyboardPage.$$('#assistantKey'));

      // Add an Apple keyboard.
      keyboardParams['showAppleCommandKey'] = true;
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectFalse(!!keyboardPage.$$('#internalSearchKey'));
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#externalMetaKey'));
      expectTrue(!!keyboardPage.$$('#externalCommandKey'));
      expectFalse(!!keyboardPage.$$('#assistantKey'));

      // Add an internal keyboard.
      keyboardParams['hasInternalKeyboard'] = true;
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectTrue(!!keyboardPage.$$('#internalSearchKey'));
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#externalMetaKey'));
      expectTrue(!!keyboardPage.$$('#externalCommandKey'));
      expectFalse(!!keyboardPage.$$('#assistantKey'));

      // Pretend an Assistant key is now available.
      keyboardParams['hasAssistantKey'] = true;
      cr.webUIListenerCallback('show-keys-changed', keyboardParams);
      Polymer.dom.flush();
      expectTrue(!!keyboardPage.$$('#internalSearchKey'));
      expectTrue(!!keyboardPage.$$('#capsLockKey'));
      expectTrue(!!keyboardPage.$$('#externalMetaKey'));
      expectTrue(!!keyboardPage.$$('#externalCommandKey'));
      expectTrue(!!keyboardPage.$$('#assistantKey'));

      const collapse = keyboardPage.$$('iron-collapse');
      assertTrue(!!collapse);
      expectTrue(collapse.opened);

      expectEquals(500, keyboardPage.$$('#delaySlider').pref.value);
      expectEquals(500, keyboardPage.$$('#repeatRateSlider').pref.value);

      // Test interaction with the settings-slider's underlying cr-slider.
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$$('#delaySlider').$$('cr-slider'), 37 /* left */, [],
          'ArrowLeft');
      MockInteractions.pressAndReleaseKeyOn(
          keyboardPage.$$('#repeatRateSlider').$$('cr-slider'), 39, [],
          'ArrowRight');
      expectEquals(1000, get('xkb_auto_repeat_delay_r2'));
      expectEquals(300, get('xkb_auto_repeat_interval_r2'));

      // Test sliders change when prefs change.
      set('xkb_auto_repeat_delay_r2', 1500);
      await test_util.flushTasks();
      expectEquals(1500, keyboardPage.$$('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 2000);
      await test_util.flushTasks();
      expectEquals(2000, keyboardPage.$$('#repeatRateSlider').pref.value);

      // Test sliders round to nearest value when prefs change.
      set('xkb_auto_repeat_delay_r2', 600);
      await test_util.flushTasks();
      expectEquals(500, keyboardPage.$$('#delaySlider').pref.value);
      set('xkb_auto_repeat_interval_r2', 45);
      await test_util.flushTasks();
      expectEquals(50, keyboardPage.$$('#repeatRateSlider').pref.value);

      set('xkb_auto_repeat_enabled_r2', false);
      expectFalse(collapse.opened);

      // Test keyboard shortcut viewer button.
      keyboardPage.$$('#keyboardShortcutViewer').click();
      expectEquals(
          1,
          settings.DevicePageBrowserProxyImpl.getInstance()
              .keyboardShortcutViewerShown_);
    });

    test(assert(TestNames.Display), function() {
      let displayPage;
      const browserProxy = settings.DevicePageBrowserProxyImpl.getInstance();
      return Promise
          .all([
            // Get the display sub-page.
            showAndGetDeviceSubpage('display', settings.routes.DISPLAY)
                .then(function(page) {
                  displayPage = page;
                  // Verify all the conditionals that get run during page load
                  // before the display info has been populated.
                  expectEquals(undefined, displayPage.displays);
                  expectFalse(
                      displayPage.showMirror_(true, displayPage.displays));
                  expectFalse(
                      displayPage.showMirror_(false, displayPage.displays));
                  expectFalse(displayPage.isMirrored_(displayPage.displays));
                  expectFalse(displayPage.showUnifiedDesktop_(
                      true, true, displayPage.displays));
                  expectFalse(displayPage.showUnifiedDesktop_(
                      false, false, displayPage.displays));
                  expectEquals(
                      displayPage.invalidDisplayId_,
                      browserProxy.lastHighlightedDisplayId_);
                }),
            // Wait for the initial call to getInfo.
            fakeSystemDisplay.getInfoCalled.promise,
          ])
          .then(function() {
            // Add a display.
            addDisplay(1);
            fakeSystemDisplay.onDisplayChanged.callListeners();

            return Promise.all([
              fakeSystemDisplay.getInfoCalled.promise,
              fakeSystemDisplay.getLayoutCalled.promise,
            ]);
          })
          .then(function() {
            // There should be a single display which should be primary and
            // selected. Mirroring should be disabled.
            expectEquals(1, displayPage.displays.length);
            expectEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            expectEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
            expectFalse(displayPage.showMirror_(false, displayPage.displays));
            expectFalse(displayPage.isMirrored_(displayPage.displays));

            // Verify unified desktop only shown when enabled.
            expectTrue(displayPage.showUnifiedDesktop_(
                true, true, displayPage.displays));
            expectFalse(displayPage.showUnifiedDesktop_(
                false, false, displayPage.displays));

            // Sanity check the first display is internal.
            expectTrue(displayPage.displays[0].isInternal);

            // Ambient EQ only shown when enabled.
            expectTrue(displayPage.showAmbientColorSetting_(
                true, displayPage.displays[0]));
            expectFalse(displayPage.showAmbientColorSetting_(
                false, displayPage.displays[0]));

            // Verify that the arrangement section is not shown.
            expectEquals(null, displayPage.$$('#arrangement-section'));

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
            expectEquals(2, displayPage.displays.length);
            expectEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            expectEquals(
                displayPage.displays[0].id, displayPage.primaryDisplayId);
            expectTrue(displayPage.showMirror_(false, displayPage.displays));
            expectFalse(displayPage.isMirrored_(displayPage.displays));

            // Set display identification highlights for the selected display as
            // there are now multiple displays.
            expectEquals(
                displayPage.displays[0].id,
                browserProxy.lastHighlightedDisplayId_);

            // Verify unified desktop only shown when enabled.
            expectTrue(displayPage.showUnifiedDesktop_(
                true, true, displayPage.displays));
            expectFalse(displayPage.showUnifiedDesktop_(
                false, false, displayPage.displays));

            // Sanity check the second display is not internal.
            expectFalse(displayPage.displays[1].isInternal);


            // Verify the display modes are parsed correctly.

            // 5 total modes, 2 parent modes.
            expectEquals(5, displayPage.modeToParentModeMap_.size);
            expectEquals(0, displayPage.modeToParentModeMap_.get(0));
            expectEquals(0, displayPage.modeToParentModeMap_.get(1));
            expectEquals(2, displayPage.modeToParentModeMap_.get(2));
            expectEquals(2, displayPage.modeToParentModeMap_.get(3));
            expectEquals(2, displayPage.modeToParentModeMap_.get(4));

            // Two resolution options, one for each parent mode.
            expectEquals(2, displayPage.refreshRateList_.length);

            // Each parent mode has the correct number of refresh rates.
            expectEquals(2, displayPage.parentModeToRefreshRateMap_.size);
            expectEquals(
                2, displayPage.parentModeToRefreshRateMap_.get(0).length);
            expectEquals(
                3, displayPage.parentModeToRefreshRateMap_.get(2).length);

            // Ambient EQ never shown on non-internal display regardless of
            // whether it is enabled.
            expectFalse(displayPage.showAmbientColorSetting_(
                true, displayPage.displays[1]));
            expectFalse(displayPage.showAmbientColorSetting_(
                false, displayPage.displays[1]));

            // Verify that the arrangement section is shown.
            expectTrue(!!displayPage.$$('#arrangement-section'));

            // Select the second display and make it primary. Also change the
            // orientation of the second display.
            const displayLayout = displayPage.$$('#displayLayout');
            assertTrue(!!displayLayout);
            const displayDiv = displayLayout.$$('#_fakeDisplayId2');
            assertTrue(!!displayDiv);
            displayDiv.click();
            expectEquals(
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
            expectEquals(2, displayPage.displays.length);
            expectEquals(
                displayPage.displays[1].id, displayPage.selectedDisplay.id);
            expectTrue(displayPage.displays[1].isPrimary);
            expectEquals(
                displayPage.displays[1].id, displayPage.primaryDisplayId);
            expectEquals(90, displayPage.displays[1].rotation);

            // Change the display that display identification highlight renders
            // on to the newly selected display.
            expectEquals(
                displayPage.displays[1].id,
                browserProxy.lastHighlightedDisplayId_);

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
            // Confirm that there is now only one display and that it is primary
            // and mirroring is enabled.
            expectEquals(1, displayPage.displays.length);
            expectEquals(
                displayPage.displays[0].id, displayPage.selectedDisplay.id);
            expectTrue(displayPage.displays[0].isPrimary);
            expectTrue(displayPage.showMirror_(false, displayPage.displays));
            expectTrue(displayPage.isMirrored_(displayPage.displays));

            // setSelectedDisplay is called on a new display id even though no
            // display identification highlight is generated in mirrored mode.
            expectEquals(
                displayPage.displays[0].id,
                browserProxy.lastHighlightedDisplayId_);

            // Verify that the arrangement section is shown while mirroring.
            expectTrue(!!displayPage.$$('#arrangement-section'));

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

            expectEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointerdown', .6);
            expectEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointermove', .3);
            expectEquals(1, displayPage.selectedZoomPref_.value);
            pointerEvent('pointerup', 0);
            expectEquals(1.25, displayPage.selectedZoomPref_.value);

            // Navigate out of the display page.
            return showAndGetDeviceSubpage('power', settings.routes.POWER);
          })
          .then(function() {
            // Moving out of the display page should set selected display to
            // invalid.
            expectEquals(
                displayPage.invalidDisplayId_,
                browserProxy.lastHighlightedDisplayId_);

            // Navigate back to the display page.
            return showAndGetDeviceSubpage('display', settings.routes.DISPLAY);
          })
          .then(function() {
            // Moving back into the display page should call setSelectedDisplay
            // with selectedDisplay_.
            expectEquals(
                displayPage.selectedDisplay.id,
                browserProxy.lastHighlightedDisplayId_);
          });
    });

    test(assert(TestNames.NightLight), async function() {
      // Set up a single display.
      const displayPage =
          await showAndGetDeviceSubpage('display', settings.routes.DISPLAY);
      await fakeSystemDisplay.getInfoCalled.promise;
      addDisplay(1);
      fakeSystemDisplay.onDisplayChanged.callListeners();
      await fakeSystemDisplay.getInfoCalled.promise;
      await fakeSystemDisplay.getLayoutCalled.promise;
      expectEquals(1, displayPage.displays.length);

      const temperature = displayPage.$$('#nightLightTemperatureDiv');
      const schedule = displayPage.$$('#nightLightScheduleTypeDropDown');

      // Night Light is off, so temperature is hidden. Schedule is always shown.
      expectTrue(temperature.hidden);
      expectFalse(schedule.hidden);

      // Enable Night Light. Use an atomic update of |displayPage.prefs| so
      // Polymer notices the change.
      const newPrefs = getFakePrefs();
      newPrefs.ash.night_light.enabled.value = true;
      displayPage.prefs = newPrefs;
      Polymer.dom.flush();

      // Night Light is on, so temperature is visible.
      expectFalse(temperature.hidden);
      expectFalse(schedule.hidden);
    });

    suite(assert(TestNames.Power), function() {
      /**
       * Sets power sources using a deep copy of |sources|.
       * @param {Array<settings.PowerSource>} sources
       * @param {string} powerSourceId
       * @param {bool} isLowPowerCharger
       */
      function setPowerSources(sources, powerSourceId, isLowPowerCharger) {
        const sourcesCopy = sources.map(function(source) {
          return Object.assign({}, source);
        });
        cr.webUIListenerCallback(
            'power-sources-changed', sourcesCopy, powerSourceId,
            isLowPowerCharger);
      }

      suite('power settings', function() {
        let powerPage;
        let powerSourceRow;
        let powerSourceSelect;
        let acIdleSelect;
        let lidClosedToggle;

        setup(function() {
          return showAndGetDeviceSubpage('power', settings.routes.POWER)
              .then(function(page) {
                powerPage = page;
                powerSourceRow = assert(powerPage.$$('#powerSourceRow'));
                powerSourceSelect = assert(powerPage.$$('#powerSource'));
                assertEquals(
                    1,
                    settings.DevicePageBrowserProxyImpl.getInstance()
                        .updatePowerStatusCalled_);

                acIdleSelect = assert(powerPage.$$('#acIdleSelect'));
                lidClosedToggle = assert(powerPage.$$('#lidClosedToggle'));

                assertEquals(
                    1,
                    settings.DevicePageBrowserProxyImpl.getInstance()
                        .requestPowerManagementSettingsCalled_);
                sendPowerManagementSettings(
                    [
                      settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                      settings.IdleBehavior.DISPLAY_OFF,
                      settings.IdleBehavior.DISPLAY_ON
                    ],
                    [
                      settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                      settings.IdleBehavior.DISPLAY_OFF,
                      settings.IdleBehavior.DISPLAY_ON
                    ],
                    settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                    settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                    false /* acIdleManaged */, false /* batteryIdleManaged */,
                    settings.LidClosedBehavior.SUSPEND,
                    false /* lidClosedControlled */, true /* hasLid */);
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
          cr.webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));
          Polymer.dom.flush();

          // Power source row is hidden since there's no battery.
          assertTrue(powerSourceRow.hidden);
          // Idle settings while on battery should not be visible if the
          // battery is not present.
          assertEquals(null, powerPage.$$('#batteryIdleSettingBox'));
        });

        test('power sources', function() {
          const batteryStatus = {
            present: true,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: '5 hours left',
          };
          cr.webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));
          setPowerSources([], '', false);
          Polymer.dom.flush();

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
          Polymer.dom.flush();

          // "Battery" should be selected.
          assertFalse(powerSourceSelect.hidden);
          assertEquals('', powerSourceSelect.value);

          // Select the power source.
          setPowerSources([powerSource], powerSource.id, true);
          Polymer.dom.flush();
          assertFalse(powerSourceSelect.hidden);
          assertEquals(powerSource.id, powerSourceSelect.value);

          // Send another power source; the first should still be selected.
          const otherPowerSource = Object.assign({}, powerSource);
          otherPowerSource.id = '3';
          setPowerSources(
              [otherPowerSource, powerSource], powerSource.id, true);
          Polymer.dom.flush();
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
          cr.webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));

          // Attach a dual-role USB device.
          const powerSource = {
            id: '3',
            is_dedicated_charger: false,
            description: 'USB-C device',
          };
          setPowerSources([powerSource], '', false);
          Polymer.dom.flush();

          // Select the device.
          selectValue(powerSourceSelect, powerSourceSelect.children[1].value);
          expectEquals(
              powerSource.id,
              settings.DevicePageBrowserProxyImpl.getInstance().powerSourceId_);
        });

        test('set AC idle behavior', function() {
          selectValue(acIdleSelect, settings.IdleBehavior.DISPLAY_ON);
          expectEquals(
              settings.IdleBehavior.DISPLAY_ON,
              settings.DevicePageBrowserProxyImpl.getInstance()
                  .acIdleBehavior_);
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
                   cr.webUIListenerCallback(
                       'battery-status-changed',
                       Object.assign({}, batteryStatus));
                   powerPage.async(resolve);
                 })
              .then(function() {
                const batteryIdleSelect =
                    assert(powerPage.$$('#batteryIdleSelect'));
                selectValue(
                    batteryIdleSelect, settings.IdleBehavior.DISPLAY_ON);
                expectEquals(
                    settings.IdleBehavior.DISPLAY_ON,
                    settings.DevicePageBrowserProxyImpl.getInstance()
                        .batteryIdleBehavior_);
              });
        });

        test('set lid behavior', function() {
          const sendLid = function(lidBehavior) {
            sendPowerManagementSettings(
                [
                  settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                  settings.IdleBehavior.DISPLAY_OFF,
                  settings.IdleBehavior.DISPLAY_ON
                ],
                [
                  settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                  settings.IdleBehavior.DISPLAY_OFF,
                  settings.IdleBehavior.DISPLAY_ON
                ],
                settings.IdleBehavior.DISPLAY_OFF,
                settings.IdleBehavior.DISPLAY_OFF, false /* acIdleManaged */,
                false /* batteryIdleManaged */, lidBehavior,
                false /* lidClosedControlled */, true /* hasLid */);
          };

          sendLid(settings.LidClosedBehavior.SUSPEND);
          assertTrue(lidClosedToggle.checked);

          lidClosedToggle.$$('#control').click();
          expectEquals(
              settings.LidClosedBehavior.DO_NOTHING,
              settings.DevicePageBrowserProxyImpl.getInstance()
                  .lidClosedBehavior_);
          sendLid(settings.LidClosedBehavior.DO_NOTHING);
          expectFalse(lidClosedToggle.checked);

          lidClosedToggle.$$('#control').click();
          expectEquals(
              settings.LidClosedBehavior.SUSPEND,
              settings.DevicePageBrowserProxyImpl.getInstance()
                  .lidClosedBehavior_);
          sendLid(settings.LidClosedBehavior.SUSPEND);
          expectTrue(lidClosedToggle.checked);
        });

        test('display idle and lid behavior', function() {
          return new Promise(function(resolve) {
                   // Send power management settings first.
                   sendPowerManagementSettings(
                       [
                         settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                         settings.IdleBehavior.DISPLAY_OFF,
                         settings.IdleBehavior.DISPLAY_ON
                       ],
                       [
                         settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                         settings.IdleBehavior.DISPLAY_OFF,
                         settings.IdleBehavior.DISPLAY_ON
                       ],
                       settings.IdleBehavior.DISPLAY_ON,
                       settings.IdleBehavior.DISPLAY_OFF,
                       false /* acIdleManaged */,
                       false /* batteryIdleManaged */,
                       settings.LidClosedBehavior.DO_NOTHING,
                       false /* lidClosedControlled */, true /* hasLid */);
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
                cr.webUIListenerCallback(
                    'battery-status-changed', Object.assign({}, batteryStatus));
                return new Promise(function(resolve) {
                  powerPage.async(resolve);
                });
              })
              .then(function() {
                const batteryIdleSelect =
                    assert(powerPage.$$('#batteryIdleSelect'));
                expectEquals(
                    settings.IdleBehavior.DISPLAY_ON.toString(),
                    acIdleSelect.value);
                expectEquals(
                    settings.IdleBehavior.DISPLAY_OFF.toString(),
                    batteryIdleSelect.value);
                expectFalse(acIdleSelect.disabled);
                expectEquals(null, powerPage.$$('#acIdleManagedIndicator'));
                expectEquals(
                    loadTimeData.getString('powerLidSleepLabel'),
                    lidClosedToggle.label);
                expectFalse(lidClosedToggle.checked);
                expectFalse(lidClosedToggle.isPrefEnforced());
              })
              .then(function() {
                sendPowerManagementSettings(
                    [
                      settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                      settings.IdleBehavior.DISPLAY_OFF,
                      settings.IdleBehavior.DISPLAY_ON
                    ],
                    [
                      settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                      settings.IdleBehavior.DISPLAY_OFF,
                      settings.IdleBehavior.DISPLAY_ON
                    ],
                    settings.IdleBehavior.DISPLAY_OFF,
                    settings.IdleBehavior.DISPLAY_ON, false /* acIdleManaged */,
                    false /* batteryIdleManaged */,
                    settings.LidClosedBehavior.SUSPEND,
                    false /* lidClosedControlled */, true /* hasLid */);
                return new Promise(function(resolve) {
                  powerPage.async(resolve);
                });
              })
              .then(function() {
                const batteryIdleSelect =
                    assert(powerPage.$$('#batteryIdleSelect'));
                expectEquals(
                    settings.IdleBehavior.DISPLAY_OFF.toString(),
                    acIdleSelect.value);
                expectEquals(
                    settings.IdleBehavior.DISPLAY_ON.toString(),
                    batteryIdleSelect.value);
                expectFalse(acIdleSelect.disabled);
                expectFalse(batteryIdleSelect.disabled);
                expectEquals(null, powerPage.$$('#acIdleManagedIndicator'));
                expectEquals(
                    null, powerPage.$$('#batteryIdleManagedIndicator'));
                expectEquals(
                    loadTimeData.getString('powerLidSleepLabel'),
                    lidClosedToggle.label);
                expectTrue(lidClosedToggle.checked);
                expectFalse(lidClosedToggle.isPrefEnforced());
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
                   cr.webUIListenerCallback(
                       'battery-status-changed',
                       Object.assign({}, batteryStatus));
                   sendPowerManagementSettings(
                       [settings.IdleBehavior.OTHER],
                       [settings.IdleBehavior.OTHER],
                       settings.IdleBehavior.OTHER, settings.IdleBehavior.OTHER,
                       true /* acIdleManaged */, true /* batteryIdleManaged */,
                       settings.LidClosedBehavior.SHUT_DOWN,
                       true /* lidClosedControlled */, true /* hasLid */);
                   powerPage.async(resolve);
                 })
              .then(function() {
                const batteryIdleSelect =
                    assert(powerPage.$$('#batteryIdleSelect'));
                expectEquals(
                    settings.IdleBehavior.OTHER.toString(), acIdleSelect.value);
                expectEquals(
                    settings.IdleBehavior.OTHER.toString(),
                    batteryIdleSelect.value);
                expectTrue(acIdleSelect.disabled);
                expectTrue(batteryIdleSelect.disabled);
                expectNotEquals(null, powerPage.$$('#acIdleManagedIndicator'));
                expectNotEquals(
                    null, powerPage.$$('#batteryIdleManagedIndicator'));
                expectEquals(
                    loadTimeData.getString('powerLidShutDownLabel'),
                    lidClosedToggle.label);
                expectTrue(lidClosedToggle.checked);
                expectTrue(lidClosedToggle.isPrefEnforced());
              })
              .then(function() {
                sendPowerManagementSettings(
                    [settings.IdleBehavior.DISPLAY_OFF],
                    [settings.IdleBehavior.DISPLAY_OFF],
                    settings.IdleBehavior.DISPLAY_OFF,
                    settings.IdleBehavior.DISPLAY_OFF,
                    false /* acIdleManaged */, false /* batteryIdleManaged */,
                    settings.LidClosedBehavior.STOP_SESSION,
                    true /* lidClosedControlled */, true /* hasLid */);
                return new Promise(function(resolve) {
                  powerPage.async(resolve);
                });
              })
              .then(function() {
                const batteryIdleSelect =
                    assert(powerPage.$$('#batteryIdleSelect'));
                expectEquals(
                    settings.IdleBehavior.DISPLAY_OFF.toString(),
                    acIdleSelect.value);
                expectEquals(
                    settings.IdleBehavior.DISPLAY_OFF.toString(),
                    batteryIdleSelect.value);
                expectTrue(acIdleSelect.disabled);
                expectTrue(batteryIdleSelect.disabled);
                expectEquals(null, powerPage.$$('#acIdleManagedIndicator'));
                expectEquals(
                    null, powerPage.$$('#batteryIdleManagedIndicator'));
                expectEquals(
                    loadTimeData.getString('powerLidSignOutLabel'),
                    lidClosedToggle.label);
                expectTrue(lidClosedToggle.checked);
                expectTrue(lidClosedToggle.isPrefEnforced());
              });
        });

        test('hide lid behavior when lid not present', function() {
          return new Promise(function(resolve) {
                   expectFalse(powerPage.$$('#lidClosedToggle').hidden);
                   sendPowerManagementSettings(
                       [
                         settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                         settings.IdleBehavior.DISPLAY_OFF,
                         settings.IdleBehavior.DISPLAY_ON
                       ],
                       [
                         settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                         settings.IdleBehavior.DISPLAY_OFF,
                         settings.IdleBehavior.DISPLAY_ON
                       ],
                       settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                       settings.IdleBehavior.DISPLAY_OFF_SLEEP,
                       false /* acIdleManaged */,
                       false /* batteryIdleManaged */,
                       settings.LidClosedBehavior.SUSPEND,
                       false /* lidClosedControlled */, false /* hasLid */);
                   powerPage.async(resolve);
                 })
              .then(function() {
                expectTrue(powerPage.$$('#lidClosedToggle').hidden);
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
                       cr.webUIListenerCallback(
                           'battery-status-changed',
                           Object.assign({}, batteryStatus));
                       Polymer.dom.flush();
                       powerPage.async(resolve);
                     })
                  .then(function() {
                    expectEquals(null, powerPage.$$('#batteryIdleSettingBox'));
                  });
            });
      });
    });

    suite(assert(TestNames.Stylus), function() {
      let stylusPage;
      let appSelector;
      let browserProxy;
      let noAppsDiv;
      let waitingDiv;

      // Shorthand for settings.NoteAppLockScreenSupport.
      let LockScreenSupport;

      suiteSetup(function() {
        // Always show stylus settings.
        loadTimeData.overrideValues({
          hasInternalStylus: true,
        });
      });

      setup(function() {
        return showAndGetDeviceSubpage('stylus', settings.routes.STYLUS)
            .then(function(page) {
              stylusPage = page;
              browserProxy = settings.DevicePageBrowserProxyImpl.getInstance();
              appSelector = assert(page.$$('#selectApp'));
              noAppsDiv = assert(page.$$('#no-apps'));
              waitingDiv = assert(page.$$('#waiting'));
              LockScreenSupport = settings.NoteAppLockScreenSupport;

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

      /**
       * @param {?Element} element
       * @return {boolean}
       */
      function isVisible(element) {
        return !!element && element.offsetWidth > 0 && element.offsetHeight > 0;
      }

      test('stylus tools prefs', function() {
        // Both stylus tools prefs are intially false.
        assertFalse(devicePage.prefs.settings.enable_stylus_tools.value);
        assertFalse(
            devicePage.prefs.settings.launch_palette_on_eject_event.value);

        // Since both prefs are intially false, the launch palette on eject pref
        // toggle is disabled.
        expectTrue(isVisible(stylusPage.$$('#enableStylusToolsToggle')));
        expectTrue(
            isVisible(stylusPage.$$('#launchPaletteOnEjectEventToggle')));
        expectTrue(stylusPage.$$('#launchPaletteOnEjectEventToggle').disabled);
        expectFalse(devicePage.prefs.settings.enable_stylus_tools.value);
        expectFalse(
            devicePage.prefs.settings.launch_palette_on_eject_event.value);

        // Tapping the enable stylus tools pref causes the launch palette on
        // eject pref toggle to not be disabled anymore.
        stylusPage.$$('#enableStylusToolsToggle').click();
        expectTrue(devicePage.prefs.settings.enable_stylus_tools.value);
        expectFalse(stylusPage.$$('#launchPaletteOnEjectEventToggle').disabled);
        stylusPage.$$('#launchPaletteOnEjectEventToggle').click();
        expectTrue(
            devicePage.prefs.settings.launch_palette_on_eject_event.value);
      });

      test('choose first app if no preferred ones', function() {
        // Selector chooses the first value in list if there is no preferred
        // value set.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals('v1', appSelector.value);
      });

      test('choose prefered app if exists', function() {
        // Selector chooses the preferred value if set.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals('v2', appSelector.value);
      });

      test('change preferred app', function() {
        // Load app list.
        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
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
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.onNoteTakingAppsUpdated_([], true);
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.addNoteTakingApp(
            entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
        Polymer.dom.flush();
        assertEquals('', browserProxy.getPreferredNoteTakingAppId());

        browserProxy.setNoteTakingApps([
          entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
          entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED)
        ]);
        Polymer.dom.flush();
        assertEquals(0, browserProxy.setPreferredAppCount_);
        assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
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
        expectFalse(isVisible(noteTakingAppLockScreenSettings()));
        expectFalse(isVisible(enableAppOnLockScreenToggle()));
        expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

        return new Promise(function(resolve) {
                 // No apps available.
                 browserProxy.setNoteTakingApps([]);
                 stylusPage.async(resolve);
               })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              // Single app which does not support lock screen note taking.
              browserProxy.addNoteTakingApp(
                  entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED));
              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              // Add an app with lock screen support, but do not select it yet.
              browserProxy.addNoteTakingApp(
                  entry('n2', 'v2', false, LockScreenSupport.SUPPORTED));
              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

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
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);

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
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectTrue(enableAppOnLockScreenToggle().checked);

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
              Polymer.dom.flush();
              expectFalse(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
            });
      });

      test('initial-app-lock-screen-enabled', function() {
        return new Promise(function(resolve) {
                 browserProxy.setNoteTakingApps(
                     [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
                 stylusPage.async(resolve);
               })
            .then(function() {
              Polymer.dom.flush();

              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              browserProxy.setNoteTakingApps(
                  [entry('n1', 'v1', true, LockScreenSupport.ENABLED)]);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectTrue(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              browserProxy.setNoteTakingApps(
                  [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              browserProxy.setNoteTakingApps([]);
              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(isVisible(enableAppOnLockScreenToggle()));
            });
      });

      test('tap-on-enable-note-taking-on-lock-screen', function() {
        return new Promise(function(resolve) {
                 browserProxy.setNoteTakingApps(
                     [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
                 stylusPage.async(resolve);
               })
            .then(function() {
              Polymer.dom.flush();

              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              enableAppOnLockScreenToggle().click();
              assertEquals(1, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              expectEquals(
                  LockScreenSupport.ENABLED,
                  browserProxy.getPreferredAppLockScreenState());

              enableAppOnLockScreenToggle().click();
              assertEquals(2, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectEquals(
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
              Polymer.dom.flush();

              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);

              enableAppOnLockScreenToggleLabel().click();
              assertEquals(1, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(enableAppOnLockScreenToggle().checked);
              expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

              expectEquals(
                  LockScreenSupport.ENABLED,
                  browserProxy.getPreferredAppLockScreenState());

              enableAppOnLockScreenToggleLabel().click();
              assertEquals(2, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectEquals(
                  LockScreenSupport.SUPPORTED,
                  browserProxy.getPreferredAppLockScreenState());
            });
      });

      test('lock-screen-apps-disabled-by-policy', function() {
        expectFalse(isVisible(enableAppOnLockScreenToggle()));
        expectFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

        return new Promise(function(resolve) {
                 // Add an app with lock screen support.
                 browserProxy.addNoteTakingApp(entry(
                     'n2', 'v2', true,
                     LockScreenSupport.NOT_ALLOWED_BY_POLICY));
                 stylusPage.async(resolve);
               })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

              // The toggle should be disabled, so enabling app on lock screen
              // should not be attempted.
              enableAppOnLockScreenToggle().click();
              assertEquals(0, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();

              // Tap on label should not work either.
              enableAppOnLockScreenToggleLabel().click();
              assertEquals(0, browserProxy.setAppOnLockScreenCount_);

              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(enableAppOnLockScreenToggle()));
              expectFalse(enableAppOnLockScreenToggle().checked);
              expectTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));

              expectEquals(
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
              Polymer.dom.flush();
              expectFalse(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(keepLastNoteOnLockScreenToggle()));

              browserProxy.setNoteTakingApps([
                entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
                entry('n2', 'v2', true, LockScreenSupport.SUPPORTED)
              ]);
              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              expectFalse(isVisible(keepLastNoteOnLockScreenToggle()));

              browserProxy.setNoteTakingApps([
                entry('n2', 'v2', true, LockScreenSupport.ENABLED),
              ]);
              return new Promise(function(resolve) {
                stylusPage.async(resolve);
              });
            })
            .then(function() {
              Polymer.dom.flush();
              expectTrue(isVisible(noteTakingAppLockScreenSettings()));
              assert(isVisible(keepLastNoteOnLockScreenToggle()));
              expectTrue(keepLastNoteOnLockScreenToggle().checked);

              // Clicking the toggle updates the pref value.
              keepLastNoteOnLockScreenToggle().$$('#control').click();
              expectFalse(keepLastNoteOnLockScreenToggle().checked);

              expectFalse(devicePage.prefs.settings
                              .restore_last_lock_screen_note.value);

              // Changing the pref value updates the toggle.
              devicePage.set(
                  'prefs.settings.restore_last_lock_screen_note.value', true);
              expectTrue(keepLastNoteOnLockScreenToggle().checked);
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
        cr.webUIListenerCallback('storage-size-stat-changed', {
          usedSize: usedSize,
          availableSize: availableSize,
          usedRatio: usedRatio,
          spaceState: spaceState,
        });
        Polymer.dom.flush();
      }

      /**
       * @param {?Element} element
       * @return {boolean}
       */
      function isHidden(element) {
        return !element ||
            (element.offsetWidth === 0 && element.offsetHeight === 0);
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
        // Avoid unwanted callbacks by disabling storage computations when the
        // storage page is loaded.
        registerMessageCallback(
            'updateStorageInfo', null /* message handler */,
            () => {} /* callback */);

        return showAndGetDeviceSubpage('storage', settings.routes.STORAGE)
            .then(function(page) {
              storagePage = page;
              storagePage.stopPeriodicUpdate_();
            });
      });

      test('storage stats size', async function() {
        // Low available storage space.
        sendStorageSizeStat(
            '9.1 GB', '0.9 GB', 0.91, settings.StorageSpaceState.LOW);
        assertEquals('91%', storagePage.$.inUseLabelArea.style.width);
        assertEquals('9%', storagePage.$.availableLabelArea.style.width);
        assertFalse(isHidden(storagePage.$$('#lowMessage')));
        assertTrue(isHidden(storagePage.$$('#criticallyLowMessage')));
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
            '9.7 GB', '0.3 GB', 0.97,
            settings.StorageSpaceState.CRITICALLY_LOW);
        assertEquals('97%', storagePage.$.inUseLabelArea.style.width);
        assertEquals('3%', storagePage.$.availableLabelArea.style.width);
        assertTrue(isHidden(storagePage.$$('#lowMessage')));
        assertFalse(isHidden(storagePage.$$('#criticallyLowMessage')));
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
        sendStorageSizeStat(
            '2.5 GB', '7.5 GB', 0.25, settings.StorageSpaceState.NORMAL);
        assertEquals('25%', storagePage.$.inUseLabelArea.style.width);
        assertEquals('75%', storagePage.$.availableLabelArea.style.width);
        assertTrue(isHidden(storagePage.$$('#lowMessage')));
        assertTrue(isHidden(storagePage.$$('#criticallyLowMessage')));
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
        cr.webUIListenerCallback('storage-system-size-changed', '8.4 GB');
        Polymer.dom.flush();
        assertEquals('8.4 GB', storagePage.$$('#systemSizeSubLabel').innerText);

        // In guest mode, the system row should be hidden.
        storagePage.isGuest_ = true;
        Polymer.dom.flush();
        assertTrue(isHidden(storagePage.$$('#systemSize')));
      });

      test('apps extensions size', async function() {
        assertEquals(
            'Apps and extensions', getStorageItemLabelFromId('appsSize'));
        assertEquals('Calculating…', getStorageItemSubLabelFromId('appsSize'));

        // Send apps size callback.
        cr.webUIListenerCallback('storage-apps-size-changed', '59.5 KB');
        Polymer.dom.flush();
        assertEquals('59.5 KB', getStorageItemSubLabelFromId('appsSize'));
      });

      test('other users size', async function() {
        // The other users row is visible by default, displaying
        // "calculating...".
        assertFalse(isHidden(storagePage.$$('#otherUsersSize')));
        assertEquals(
            'Other users', getStorageItemLabelFromId('otherUsersSize'));
        assertEquals(
            'Calculating…', getStorageItemSubLabelFromId('otherUsersSize'));

        // Simulate absence of other users.
        cr.webUIListenerCallback(
            'storage-other-users-size-changed', '0 B', true);
        Polymer.dom.flush();
        assertTrue(isHidden(storagePage.$$('#otherUsersSize')));

        // Send other users callback with a size that is not null.
        cr.webUIListenerCallback(
            'storage-other-users-size-changed', '322 MB', false);
        Polymer.dom.flush();
        assertFalse(isHidden(storagePage.$$('#otherUsersSize')));
        assertEquals('322 MB', getStorageItemSubLabelFromId('otherUsersSize'));

        // If the user is in Guest mode, the row is not visible.
        storagePage.isGuest_ = true;
        cr.webUIListenerCallback(
            'storage-other-users-size-changed', '322 MB', false);
        Polymer.dom.flush();
        assertTrue(isHidden(storagePage.$$('#otherUsersSize')));
      });
    });
  });

  return {TestNames: TestNames};
});
