// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ChangeButtonState: 'change button state',
  };

  const suiteName = 'DestinationSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationSettingsElement} */
    let destinationSettings = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      const nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      destinationSettings =
          document.createElement('print-preview-destination-settings');
      destinationSettings.destinationStore = null;
      destinationSettings.state = print_preview_new.State.NOT_READY;
      // Disabled is true when state is NOT_READY.
      destinationSettings.disabled = true;
      document.body.appendChild(destinationSettings);
    });

    // Tests that the change button is enabled or disabled correctly based on
    // the state.
    test(assert(TestNames.ChangeButtonState), function() {
      const button = destinationSettings.$$('paper-button');
      // Initial state: No destination store, button should be disabled.
      assertTrue(button.disabled);

      // Set up the destination store, but no destination yet. Button is
      // disabled.
      const userInfo = new print_preview.UserInfo();
      const destinationStore = new print_preview.DestinationStore(
          userInfo, new WebUIListenerTracker());
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* recentDestinations */);
      destinationSettings.destinationStore = destinationStore;
      destinationSettings.state = print_preview_new.State.NOT_READY;
      assertTrue(button.disabled);

      // Simulate loading a destination and setting state to ready. The button
      // is still enabled.
      destinationSettings.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName', true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      destinationSettings.state = print_preview_new.State.READY;
      destinationSettings.disabled = false;
      assertFalse(button.disabled);

      // Simulate setting a setting to an invalid value. Button is disabled due
      // to validation error on another control.
      destinationSettings.state = print_preview_new.State.INVALID_TICKET;
      destinationSettings.disabled = true;
      assertTrue(button.disabled);

      // Simulate the user fixing the validation error, and then selecting an
      // invalid printer. Button is enabled, so that the user can fix the error.
      destinationSettings.state = print_preview_new.State.READY;
      destinationSettings.disabled = false;
      destinationSettings.state = print_preview_new.State.INVALID_PRINTER;
      destinationSettings.disabled = true;
      assertFalse(button.disabled);

      // Simulate the user having no printers.
      destinationSettings.destination = null;
      destinationSettings.state = print_preview_new.State.INVALID_PRINTER;
      destinationSettings.disabled = true;
      destinationSettings.noDestinationsFound = true;
      assertTrue(button.disabled);
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
