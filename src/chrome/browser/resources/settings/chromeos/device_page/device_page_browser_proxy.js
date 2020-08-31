// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * @typedef {{
   *   id: string,
   *   is_dedicated_charger: boolean,
   *   description: string
   * }}
   */
  let PowerSource;

  /**
   * @typedef {{
   *   present: boolean,
   *   charging: boolean,
   *   calculating: boolean,
   *   percent: number,
   *   statusText: string,
   * }}
   */
  let BatteryStatus;

  /**
   * Mirrors chromeos::settings::PowerHandler::IdleBehavior.
   * @enum {number}
   */
  const IdleBehavior = {
    DISPLAY_OFF_SLEEP: 0,
    DISPLAY_OFF: 1,
    DISPLAY_ON: 2,
    OTHER: 3,
  };

  /**
   * Mirrors chromeos::PowerPolicyController::Action.
   * @enum {number}
   */
  const LidClosedBehavior = {
    SUSPEND: 0,
    STOP_SESSION: 1,
    SHUT_DOWN: 2,
    DO_NOTHING: 3,
  };

  /**
   * @typedef {{
   *   possibleAcIdleBehaviors: !Array<settings.IdleBehavior>,
   *   possibleBatteryIdleBehaviors: !Array<settings.IdleBehavior>,
   *   acIdleManaged: boolean,
   *   batteryIdleManaged: boolean,
   *   currentAcIdleBehavior: settings.IdleBehavior,
   *   currentBatteryIdleBehavior: settings.IdleBehavior,
   *   lidClosedBehavior: settings.LidClosedBehavior,
   *   lidClosedControlled: boolean,
   *   hasLid: boolean,
   * }}
   */
  let PowerManagementSettings;

  /**
   * A note app's availability for running as note handler app from lock screen.
   * Mirrors chromeos::NoteTakingLockScreenSupport.
   * @enum {number}
   */
  const NoteAppLockScreenSupport =
      {NOT_SUPPORTED: 0, NOT_ALLOWED_BY_POLICY: 1, SUPPORTED: 2, ENABLED: 3};

  /**
   * @typedef {{
   *   name:string,
   *   value:string,
   *   preferred:boolean,
   *   lockScreenSupport: settings.NoteAppLockScreenSupport,
   * }}
   */
  let NoteAppInfo;

  /**
   * @typedef {{
   *   label: string,
   *   uuid: string
   * }}
   */
  let ExternalStorage;

  /**
   * @typedef {{
   *   id: string,
   *   name: string,
   *   description: string,
   *   diskUsageLabel: string,
   * }}
   */
  let DlcMetadata;

  /** @interface */
  class DevicePageBrowserProxy {
    /** Initializes the mouse and touchpad handler. */
    initializePointers() {}

    /** Initializes the stylus handler. */
    initializeStylus() {}

    /** Initializes the keyboard WebUI handler. */
    initializeKeyboard() {}

    /** Shows the Ash keyboard shortcut viewer. */
    showKeyboardShortcutViewer() {}

    /** Requests an ARC status update. */
    updateAndroidEnabled() {}

    /** Requests a power status update. */
    updatePowerStatus() {}

    /**
     * Sets the ID of the power source to use.
     * @param {string} powerSourceId ID of the power source. '' denotes the
     *     battery (no external power source).
     */
    setPowerSource(powerSourceId) {}

    /** Requests the current power management settings. */
    requestPowerManagementSettings() {}

    /**
     * Sets the idle power management behavior.
     * @param {settings.IdleBehavior} behavior Idle behavior.
     * @param {boolean} whenOnAc If true sets AC idle behavior. Otherwise sets
     *     battery idle behavior.
     */
    setIdleBehavior(behavior, whenOnAc) {}

    /**
     * Sets the lid-closed power management behavior.
     * @param {settings.LidClosedBehavior} behavior Lid-closed behavior.
     */
    setLidClosedBehavior(behavior) {}

    /**
     * |callback| is run when there is new note-taking app information
     * available or after |requestNoteTakingApps| has been called.
     * @param {function(Array<settings.NoteAppInfo>, boolean):void} callback
     */
    setNoteTakingAppsUpdatedCallback(callback) {}

    /**
     * Open up the play store with the given URL.
     * @param {string} url
     */
    showPlayStore(url) {}

    /**
     * Request current note-taking app info. Invokes any callback registered in
     * |onNoteTakingAppsUpdated|.
     */
    requestNoteTakingApps() {}

    /**
     * Changes the preferred note taking app.
     * @param {string} appId The app id. This should be a value retrieved from a
     *     |onNoteTakingAppsUpdated| callback.
     */
    setPreferredNoteTakingApp(appId) {}

    /**
     * Sets whether the preferred note taking app should be enabled to run as a
     * lock screen note action handler.
     * @param {boolean} enabled Whether the app should be enabled to handle note
     *     actions from the lock screen.
     */
    setPreferredNoteTakingAppEnabledOnLockScreen(enabled) {}

    /** Requests an external storage list update. */
    updateExternalStorages() {}

    /**
     * |callback| is run when the list of plugged-in external storages is
     * available after |updateExternalStorages| has been called.
     * @param {function(Array<!settings.ExternalStorage>):void} callback
     */
    setExternalStoragesUpdatedCallback(callback) {}

    /**
     * Sets |id| of display to render identification highlight on. Invalid |id|
     * turns identification highlight off. Handles any invalid input string as
     * invalid id.
     * @param {string} id Display id of selected display.
     */
    highlightDisplay(id) {}

    /**
     * @return {!Promise<!Array<!settings.DlcMetadata>>} A list of DLC metadata.
     */
    getDlcList() {}

    /**
     * Purges the DLC with the provided |dlcId| from the device.
     * @param {string} dlcId The ID of the DLC to purge from the device.
     * @return {!Promise<boolean>} Whether purging of DLC was successful.
     */
    purgeDlc(dlcId) {}
  }

  /**
   * @implements {settings.DevicePageBrowserProxy}
   */
  class DevicePageBrowserProxyImpl {
    /** @override */
    initializePointers() {
      chrome.send('initializePointerSettings');
    }

    /** @override */
    initializeStylus() {
      chrome.send('initializeStylusSettings');
    }

    /** @override */
    initializeKeyboard() {
      chrome.send('initializeKeyboardSettings');
    }

    /** @override */
    showKeyboardShortcutViewer() {
      chrome.send('showKeyboardShortcutViewer');
    }

    /** @override */
    updateAndroidEnabled() {
      chrome.send('updateAndroidEnabled');
    }

    /** @override */
    updatePowerStatus() {
      chrome.send('updatePowerStatus');
    }

    /** @override */
    setPowerSource(powerSourceId) {
      chrome.send('setPowerSource', [powerSourceId]);
    }

    /** @override */
    requestPowerManagementSettings() {
      chrome.send('requestPowerManagementSettings');
    }

    /** @override */
    setIdleBehavior(behavior, whenOnAc) {
      chrome.send('setIdleBehavior', [behavior, whenOnAc]);
    }

    /** @override */
    setLidClosedBehavior(behavior) {
      chrome.send('setLidClosedBehavior', [behavior]);
    }

    /** @override */
    setNoteTakingAppsUpdatedCallback(callback) {
      cr.addWebUIListener('onNoteTakingAppsUpdated', callback);
    }

    /** @override */
    showPlayStore(url) {
      chrome.send('showPlayStoreApps', [url]);
    }

    /** @override */
    requestNoteTakingApps() {
      chrome.send('requestNoteTakingApps');
    }

    /** @override */
    setPreferredNoteTakingApp(appId) {
      chrome.send('setPreferredNoteTakingApp', [appId]);
    }

    /** @override */
    setPreferredNoteTakingAppEnabledOnLockScreen(enabled) {
      chrome.send('setPreferredNoteTakingAppEnabledOnLockScreen', [enabled]);
    }

    /** @override */
    updateExternalStorages() {
      chrome.send('updateExternalStorages');
    }

    /** @override */
    setExternalStoragesUpdatedCallback(callback) {
      cr.addWebUIListener('onExternalStoragesUpdated', callback);
    }

    /** @override */
    highlightDisplay(id) {
      chrome.send('highlightDisplay', [id]);
    }

    /** @override */
    getDlcList() {
      return cr.sendWithPromise('getDlcList');
    }

    /** @override */
    purgeDlc(dlcId) {
      return cr.sendWithPromise('purgeDlc', dlcId);
    }
  }

  cr.addSingletonGetter(DevicePageBrowserProxyImpl);

  // #cr_define_end
  return {
    BatteryStatus,
    DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl,
    ExternalStorage,
    IdleBehavior,
    LidClosedBehavior,
    NoteAppInfo,
    NoteAppLockScreenSupport,
    PowerManagementSettings,
    PowerSource,
    DlcMetadata,
  };
});
