// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('model_test', function() {
  /** @enum {string} */
  const TestNames = {
    SetStickySettings: 'set sticky settings',
    SetPolicySettings: 'set policy settings',
    GetPrintTicket: 'get print ticket',
    GetCloudPrintTicket: 'get cloud print ticket',
    UpdateRecentDestinations: 'update recent destinations',
    ChangeDestination: 'change destination'
  };

  const suiteName = 'ModelTest';
  suite(suiteName, function() {
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);
    });

    /**
     * Tests state restoration with all boolean settings set to true, scaling =
     * 90, dpi = 100, custom square paper, and custom margins.
     */
    test(assert(TestNames.SetStickySettings), function() {
      // Default state of the model.
      const stickySettingsDefault = {
        version: 2,
        recentDestinations: [],
        dpi: {},
        mediaSize: {},
        marginsType: 0, /* default */
        customScaling: false,
        scaling: '100',
        isHeaderFooterEnabled: true,
        isCssBackgroundEnabled: false,
        isFitToPageEnabled: false,
        isCollateEnabled: true,
        isDuplexEnabled: true,
        isLandscapeEnabled: false,
        isColorEnabled: true,
        vendorOptions: {},
      };

      // Non-default state
      const stickySettingsChange = {
        version: 2,
        recentDestinations: [],
        dpi: {horizontal_dpi: 1000, vertical_dpi: 500},
        mediaSize: {width_microns: 43180, height_microns: 21590},
        marginsType: 2, /* none */
        customScaling: true,
        scaling: '85',
        isHeaderFooterEnabled: false,
        isCssBackgroundEnabled: true,
        isFitToPageEnabled: true,
        isCollateEnabled: false,
        isDuplexEnabled: false,
        isLandscapeEnabled: true,
        isColorEnabled: false,
        vendorOptions: {
          paperType: 1,
          printArea: 6,
        },
      };

      /**
       * @param {string} setting The name of the setting to check.
       * @param {string} field The name of the field in the serialized state
       *     corresponding to the setting.
       * @return {!Promise} Promise that resolves when the setting has been set,
       *     the saved string has been validated, and the setting has been
       *     reset to its default value.
       */
      const testStickySetting = function(setting, field) {
        let promise = test_util.eventToPromise('save-sticky-settings', model);
        model.set(`settings.${setting}.value`, stickySettingsChange[field]);
        return promise.then(
            /**
             * @param {!CustomEvent} e Event containing the serialized settings
             * @return {!Promise} Promise that resolves when setting is reset.
             */
            function(e) {
              let settings = JSON.parse(e.detail);
              Object.keys(stickySettingsDefault).forEach(settingName => {
                let toCompare = settingName == field ? stickySettingsChange :
                                                       stickySettingsDefault;
                assertDeepEquals(toCompare[settingName], settings[settingName]);
              });
              let restorePromise =
                  test_util.eventToPromise('save-sticky-settings', model);
              model.set(
                  `settings.${setting}.value`, stickySettingsDefault[field]);
              return restorePromise;
            });
      };

      model.applyStickySettings();
      return testStickySetting('collate', 'isCollateEnabled')
          .then(() => testStickySetting('color', 'isColorEnabled'))
          .then(
              () =>
                  testStickySetting('cssBackground', 'isCssBackgroundEnabled'))
          .then(() => testStickySetting('dpi', 'dpi'))
          .then(() => testStickySetting('duplex', 'isDuplexEnabled'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(
              () => testStickySetting('headerFooter', 'isHeaderFooterEnabled'))
          .then(() => testStickySetting('layout', 'isLandscapeEnabled'))
          .then(() => testStickySetting('margins', 'marginsType'))
          .then(() => testStickySetting('mediaSize', 'mediaSize'))
          .then(() => testStickySetting('customScaling', 'customScaling'))
          .then(() => testStickySetting('scaling', 'scaling'))
          .then(() => testStickySetting('fitToPage', 'isFitToPageEnabled'))
          .then(() => testStickySetting('vendorItems', 'vendorOptions'));
    });

    /**
     * Tests that setSetting() won't change the value if there is already a
     * policy for that setting.
     */
    test(assert(TestNames.SetPolicySettings), function() {
      model.setSetting('headerFooter', false);
      assertFalse(model.settings.headerFooter.value);

      // Sets to true, but doesn't mark as controlled by a policy.
      model.setPolicySettings(true, false);
      model.setStickySettings(JSON.stringify({
        version: 2,
        headerFooter: false,
      }));
      model.applyStickySettings();
      assertTrue(model.settings.headerFooter.value);
      model.setSetting('headerFooter', false);
      assertFalse(model.settings.headerFooter.value);

      model.setPolicySettings(true, true);
      model.applyStickySettings();
      assertTrue(model.settings.headerFooter.value);

      model.setSetting('headerFooter', false);
      // The value didn't change after setSetting(), because the policy takes
      // priority.
      assertTrue(model.settings.headerFooter.value);
    });

    /** @param {!print_preview.Destination} testDestination */
    function toggleSettings(testDestination) {
      // Some non default setting values to change to.
      // Manually set fit to page to available so it can be toggled.
      model.settings.fitToPage.available = true;
      const settingsChange = {
        pages: [2],
        copies: '2',
        collate: false,
        layout: true,
        color: false,
        mediaSize: testDestination.capabilities.printer.media_size.option[1],
        margins: print_preview.ticket_items.MarginsTypeValue.CUSTOM,
        customMargins: {
          marginTop: 100,
          marginRight: 200,
          marginBottom: 300,
          marginLeft: 400,
        },
        dpi: {
          horizontal_dpi: 100,
          vertical_dpi: 100,
        },
        fitToPage: false,
        customScaling: true,
        scaling: '90',
        duplex: true,
        cssBackground: true,
        selectionOnly: true,
        headerFooter: false,
        rasterize: true,
        vendorItems: {
          printArea: 6,
          paperType: 1,
        },
        ranges: [{from: 2, to: 2}],
      };

      // Update settings
      Object.keys(settingsChange).forEach(setting => {
        model.set(`settings.${setting}.value`, settingsChange[setting]);
      });
    }

    function initializeModel() {
      model.documentSettings = {
        hasCssMediaStyles: false,
        hasSelection: true,
        isModifiable: true,
        isScalingDisabled: false,
        fitToPageScaling: 100,
        pageCount: 3,
        title: 'title',
      };
      model.pageSize = new print_preview.Size(612, 792);

      // Update pages accordingly.
      model.set('settings.pages.value', [1, 2, 3]);

      // Initialize some settings that don't have defaults to the destination
      // defaults.
      model.set('settings.dpi.value', {horizontal_dpi: 200, vertical_dpi: 200});
      model.set('settings.vendorItems.value', {paperType: 0, printArea: 4});

      // Set rasterize available so that it can be tested.
      model.set('settings.rasterize.available', true);
    }

    /**
     * Tests that toggling each setting results in the expected change to the
     * print ticket.
     */
    test(assert(TestNames.GetPrintTicket), function() {
      const testDestination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination.capabilities =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(2)
              .capabilities;

      initializeModel();
      model.destination = testDestination;
      const defaultTicket =
          model.createPrintTicket(testDestination, false, false);
      const expectedDefaultTicket = JSON.stringify({
        mediaSize: testDestination.capabilities.printer.media_size.option[0],
        pageCount: 3,
        landscape: false,
        color: testDestination.getNativeColorModel(true),
        headerFooterEnabled: false,  // Only used in print preview
        marginsType: print_preview.ticket_items.MarginsTypeValue.DEFAULT,
        duplex: print_preview_new.DuplexMode.SIMPLEX,
        copies: 1,
        collate: true,
        shouldPrintBackgrounds: false,
        shouldPrintSelectionOnly: false,
        previewModifiable: true,
        printToPDF: false,
        printToGoogleDrive: false,
        printWithCloudPrint: false,
        printWithPrivet: false,
        printWithExtension: false,
        rasterizePDF: false,
        scaleFactor: 100,
        pagesPerSheet: 1,
        dpiHorizontal: 200,
        dpiVertical: 200,
        dpiDefault: true,
        deviceName: 'FooDevice',
        fitToPageEnabled: true,
        pageWidth: 612,
        pageHeight: 792,
        showSystemDialog: false,
      });
      expectEquals(expectedDefaultTicket, defaultTicket);

      // Toggle all the values and create a new print ticket.
      toggleSettings(testDestination);
      const newTicket = model.createPrintTicket(testDestination, false, false);
      const expectedNewTicket = JSON.stringify({
        mediaSize: testDestination.capabilities.printer.media_size.option[1],
        pageCount: 1,
        landscape: true,
        color: testDestination.getNativeColorModel(false),
        headerFooterEnabled: false,
        marginsType: print_preview.ticket_items.MarginsTypeValue.CUSTOM,
        duplex: print_preview_new.DuplexMode.LONG_EDGE,
        copies: 2,
        collate: false,
        shouldPrintBackgrounds: true,
        shouldPrintSelectionOnly: false,  // Only for Print Preview.
        previewModifiable: true,
        printToPDF: false,
        printToGoogleDrive: false,
        printWithCloudPrint: false,
        printWithPrivet: false,
        printWithExtension: false,
        rasterizePDF: true,
        scaleFactor: 90,
        pagesPerSheet: 1,
        dpiHorizontal: 100,
        dpiVertical: 100,
        dpiDefault: false,
        deviceName: 'FooDevice',
        fitToPageEnabled: false,
        pageWidth: 612,
        pageHeight: 792,
        showSystemDialog: false,
        marginsCustom: {
          marginTop: 100,
          marginRight: 200,
          marginBottom: 300,
          marginLeft: 400,
        },
      });
      expectEquals(expectedNewTicket, newTicket);
    });

    /**
     * Tests that toggling each setting results in the expected change to the
     * cloud job print ticket.
     */
    test(assert(TestNames.GetCloudPrintTicket), function() {
      initializeModel();

      // Create a test cloud destination.
      const testDestination = new print_preview.Destination(
          'FooCloudDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination.capabilities =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(2)
              .capabilities;
      model.destination = testDestination;

      const defaultTicket = model.createCloudJobTicket(testDestination);
      const expectedDefaultTicket = JSON.stringify({
        version: '1.0',
        print: {
          collate: {collate: true},
          color: {
            type: testDestination.getSelectedColorOption(true).type,
          },
          copies: {copies: 1},
          duplex: {type: 'NO_DUPLEX'},
          media_size: {
            width_microns: 215900,
            height_microns: 279400,
          },
          page_orientation: {type: 'PORTRAIT'},
          dpi: {
            horizontal_dpi: 200,
            vertical_dpi: 200,
          },
          vendor_ticket_item: [
            {id: 'printArea', value: 4},
            {id: 'paperType', value: 0},
          ],
        },
      });
      expectEquals(expectedDefaultTicket, defaultTicket);

      // Toggle all the values and create a new cloud job ticket.
      toggleSettings(testDestination);
      const newTicket = model.createCloudJobTicket(testDestination);
      const expectedNewTicket = JSON.stringify({
        version: '1.0',
        print: {
          collate: {collate: false},
          color: {
            type: testDestination.getSelectedColorOption(false).type,
          },
          copies: {copies: 2},
          duplex: {type: 'LONG_EDGE'},
          media_size: {
            width_microns: 215900,
            height_microns: 215900,
          },
          page_orientation: {type: 'LANDSCAPE'},
          dpi: {
            horizontal_dpi: 100,
            vertical_dpi: 100,
          },
          vendor_ticket_item: [
            {id: 'printArea', value: 6},
            {id: 'paperType', value: 1},
          ],
        },
      });
      expectEquals(expectedNewTicket, newTicket);
    });

    /**
     * @param {!Array<string>} expectedDestinationIds An array of the expected
     *     recent destination ids.
     */
    function assertRecentDestinations(expectedDestinationIds) {
      assertEquals(
          expectedDestinationIds.length, model.recentDestinations.length);
      expectedDestinationIds.forEach((expectedId, index) => {
        assertEquals(expectedId, model.recentDestinations[index].id);
      });
    }

    /**
     * Tests that the destination being set correctly updates the recent
     * destinations array.
     */
    test(assert(TestNames.UpdateRecentDestinations), function() {
      initializeModel();
      model.applyStickySettings();

      let localDestinations = [];
      let destinations =
          print_preview_test_utils.getDestinations(null, localDestinations);

      // Recent destinations start out empty.
      assertRecentDestinations([]);

      // Simulate setting a destination.
      model.destination = destinations[0];
      assertRecentDestinations(['ID1']);

      // Set a new destination
      model.destination = destinations[1];
      assertRecentDestinations(['ID2', 'ID1']);

      // Reselect a recent destination. Still 2 destinations, but in a
      // different order.
      model.destination = destinations[0];
      assertRecentDestinations(['ID1', 'ID2']);

      // Select a third destination
      model.destination = destinations[2];
      assertRecentDestinations(['ID3', 'ID1', 'ID2']);

      // Select a fourth destination. List does not grow.
      model.destination = destinations[3];
      assertRecentDestinations(['ID4', 'ID3', 'ID1']);
    });

    test(assert(TestNames.ChangeDestination), function() {
      const testDestination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination.capabilities =
          print_preview_test_utils.getCddTemplateWithAdvancedSettings(2)
              .capabilities;
      // Make black and white printing the default.
      testDestination.capabilities.printer.color = {
        option: [
          {type: 'STANDARD_COLOR'},
          {type: 'STANDARD_MONOCHROME', is_default: true}
        ]
      };

      const testDestination2 = new print_preview.Destination(
          'BarDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'BarName',
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination2.capabilities =
          Object.assign({}, testDestination.capabilities);

      // Initialize
      initializeModel();
      model.destination = testDestination;
      model.applyStickySettings();

      // Confirm some defaults.
      assertEquals(false, model.getSettingValue('color'));
      assertEquals('NA_LETTER', model.getSettingValue('mediaSize').name);
      assertEquals(200, model.getSettingValue('dpi').horizontal_dpi);
      assertEquals(false, model.getSettingValue('duplex'));

      // Toggle some printer specified settings.
      model.setSetting('duplex', true);
      model.setSetting(
          'mediaSize',
          testDestination.capabilities.printer.media_size.option[1]);
      model.setSetting('color', true);
      model.setSetting(
          'dpi', testDestination.capabilities.printer.dpi.option[1]);

      // Confirm toggles.
      assertEquals(true, model.getSettingValue('color'));
      assertEquals('CUSTOM_SQUARE', model.getSettingValue('mediaSize').name);
      assertEquals(100, model.getSettingValue('dpi').horizontal_dpi);
      assertEquals(true, model.getSettingValue('duplex'));

      // Set to a new destination with the same capabilities. Confirm that
      // everything stays the same.
      const oldSettings = JSON.stringify(model.settings);
      model.destination = testDestination2;
      const newSettings = JSON.stringify(model.settings);

      // Should be the same (same printer capabilities).
      assertEquals(oldSettings, newSettings);

      // Create a printer with different capabilities.
      const testDestination3 = new print_preview.Destination(
          'Device1', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'One',
          print_preview.DestinationConnectionStatus.ONLINE);
      testDestination3.capabilities =
          Object.assign({}, testDestination.capabilities);
      testDestination3.capabilities.printer.media_size = {
        option: [
          {
            name: 'ISO_A4',
            width_microns: 210000,
            height_microns: 297000,
            custom_display_name: 'A4',
            is_default: true,
          },
        ]
      };
      testDestination3.capabilities.printer.color = {
        option: [
          {type: 'STANDARD_MONOCHROME', is_default: true},
        ]
      };
      testDestination3.capabilities.printer.duplex = {
        option: [
          {type: 'NO_DUPLEX', is_default: true},
        ]
      };
      testDestination3.capabilities.printer.dpi = {
        option: [
          {horizontal_dpi: 400, vertical_dpi: 400, is_default: true},
          {horizontal_dpi: 800, vertical_dpi: 800},
        ]
      };

      model.destination = testDestination3;
      Polymer.dom.flush();

      // Verify things changed.
      const updatedSettings = JSON.stringify(model.settings);
      assertNotEquals(oldSettings, updatedSettings);
      assertEquals(false, model.getSettingValue('color'));
      assertEquals('ISO_A4', model.getSettingValue('mediaSize').name);
      assertEquals(400, model.getSettingValue('dpi').horizontal_dpi);
      assertEquals(false, model.getSettingValue('duplex'));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
