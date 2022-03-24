// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, DuplexMode, MarginsType, PrinterType, PrintPreviewModelElement, PrintTicket, ScalingType, Size} from 'chrome://print/print_preview.js';
// <if expr="chromeos or lacros">
import {GooglePromotedDestinationId} from 'chrome://print/print_preview.js';
// </if>
import {assert} from 'chrome://resources/js/assert.m.js';
// <if expr="chromeos or lacros">
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// </if>
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getCddTemplateWithAdvancedSettings} from './print_preview_test_utils.js';

const model_test = {
  suiteName: 'ModelTest',
  TestNames: {
    SetStickySettings: 'set sticky settings',
    SetPolicySettings: 'set policy settings',
    GetPrintTicket: 'get print ticket',
    GetCloudPrintTicket: 'get cloud print ticket',
    ChangeDestination: 'change destination',
    PrintToGoogleDriveCros: 'print to google drive cros',
    CddResetToDefault: 'CDD reset_to_default property',
  },
};

Object.assign(window, {model_test: model_test});

suite(model_test.suiteName, function() {
  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = '';
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
  });

  /**
   * Tests state restoration with all boolean settings set to true, scaling =
   * 90, dpi = 100, custom square paper, and custom margins.
   */
  test(assert(model_test.TestNames.SetStickySettings), function() {
    // Default state of the model.
    const stickySettingsDefault: {[key: string]: any} = {
      version: 2,
      recentDestinations: [],
      dpi: {},
      mediaSize: {},
      marginsType: 0, /* default */
      scaling: '100',
      scalingType: ScalingType.DEFAULT,
      scalingTypePdf: ScalingType.DEFAULT,
      isHeaderFooterEnabled: true,
      isCssBackgroundEnabled: false,
      isCollateEnabled: true,
      isDuplexEnabled: true,
      isDuplexShortEdge: false,
      isLandscapeEnabled: false,
      isColorEnabled: true,
      vendorOptions: {},
      // <if expr="chromeos or lacros">
      isPinEnabled: false,
      pinValue: '',
      // </if>
    };

    // Non-default state
    const stickySettingsChange: {[key: string]: any} = {
      version: 2,
      recentDestinations: [],
      dpi: {horizontal_dpi: 1000, vertical_dpi: 500},
      mediaSize: {width_microns: 43180, height_microns: 21590},
      marginsType: 2, /* none */
      scaling: '85',
      scalingType: ScalingType.CUSTOM,
      scalingTypePdf: ScalingType.FIT_TO_PAGE,
      isHeaderFooterEnabled: false,
      isCssBackgroundEnabled: true,
      isCollateEnabled: false,
      isDuplexEnabled: false,
      isDuplexShortEdge: true,
      isLandscapeEnabled: true,
      isColorEnabled: false,
      vendorOptions: {
        paperType: 1,
        printArea: 6,
      },
      // <if expr="chromeos or lacros">
      isPinEnabled: true,
      pinValue: '0000',
      // </if>
    };

    const settingsSet = ['version'];

    /**
     * @param setting The name of the setting to check.
     * @param field The name of the field in the serialized state
     *     corresponding to the setting.
     * @return Promise that resolves when the setting has been set,
     *     the saved string has been validated, and the setting has been
     *     reset to its default value.
     */
    const testStickySetting = function(
        setting: string, field: string): Promise<void> {
      const promise = eventToPromise('sticky-setting-changed', model);
      model.setSetting(setting, stickySettingsChange[field]);
      settingsSet.push(field);
      return promise.then(
          /**
           * @param e Event containing the serialized settings
           * @return Promise that resolves when setting is reset.
           */
          function(e: CustomEvent<string>): Promise<void> {
            const settings = JSON.parse(e.detail);
            Object.keys(stickySettingsDefault).forEach(settingName => {
              const set = settingsSet.includes(settingName);
              assertEquals(set, settings[settingName] !== undefined);
              if (set) {
                const toCompare = settingName === field ? stickySettingsChange :
                                                          stickySettingsDefault;
                assertDeepEquals(toCompare[settingName], settings[settingName]);
              }
            });
            const restorePromise =
                eventToPromise('sticky-setting-changed', model);
            model.setSetting(setting, stickySettingsDefault[field]);
            return restorePromise;
          });
    };

    model.applyStickySettings();
    let promise =
        testStickySetting('collate', 'isCollateEnabled')
            .then(() => testStickySetting('color', 'isColorEnabled'))
            .then(
                () => testStickySetting(
                    'cssBackground', 'isCssBackgroundEnabled'))
            .then(() => testStickySetting('dpi', 'dpi'))
            .then(() => testStickySetting('duplex', 'isDuplexEnabled'))
            .then(
                () => testStickySetting('duplexShortEdge', 'isDuplexShortEdge'))
            .then(
                () =>
                    testStickySetting('headerFooter', 'isHeaderFooterEnabled'))
            .then(() => testStickySetting('layout', 'isLandscapeEnabled'))
            .then(() => testStickySetting('margins', 'marginsType'))
            .then(() => testStickySetting('mediaSize', 'mediaSize'))
            .then(() => testStickySetting('scaling', 'scaling'))
            .then(() => testStickySetting('scalingType', 'scalingType'))
            .then(() => testStickySetting('scalingTypePdf', 'scalingTypePdf'))
            .then(() => testStickySetting('vendorItems', 'vendorOptions'));
    // <if expr="chromeos or lacros">
    promise = promise.then(() => testStickySetting('pin', 'isPinEnabled'))
                  .then(() => testStickySetting('pinValue', 'pinValue'));
    // </if>
    return promise;
  });

  /**
   * Tests that setSetting() won't change the value if there is already a
   * policy for that setting.
   */
  test(assert(model_test.TestNames.SetPolicySettings), function() {
    model.setSetting('headerFooter', false);
    assertFalse(model.settings.headerFooter.value as boolean);

    // Sets to true, but doesn't mark as controlled by a policy.
    model.setPolicySettings({headerFooter: {defaultMode: true}});
    model.setStickySettings(JSON.stringify({
      version: 2,
      headerFooter: false,
    }));
    model.applyStickySettings();
    assertTrue(model.settings.headerFooter.value as boolean);
    model.setSetting('headerFooter', false);
    assertFalse(model.settings.headerFooter.value as boolean);

    model.setPolicySettings({headerFooter: {allowedMode: true}});
    model.applyStickySettings();
    assertTrue(model.settings.headerFooter.value as boolean);

    model.setSetting('headerFooter', false);
    // The value didn't change after setSetting(), because the policy takes
    // priority.
    assertTrue(model.settings.headerFooter.value as boolean);
  });

  function toggleSettings(testDestination: Destination) {
    const settingsChange: {[key: string]: any} = {
      pages: [2],
      copies: 2,
      collate: false,
      layout: true,
      color: false,
      mediaSize: testDestination.capabilities!.printer!.media_size!.option[1]!,
      margins: MarginsType.CUSTOM,
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
      scaling: '90',
      scalingType: ScalingType.CUSTOM,
      scalingTypePdf: ScalingType.CUSTOM,
      duplex: true,
      duplexShortEdge: true,
      cssBackground: true,
      selectionOnly: true,
      headerFooter: false,
      rasterize: true,
      vendorItems: {
        printArea: 6,
        paperType: 1,
      },
      ranges: [{from: 2, to: 2}],
      // <if expr="chromeos or lacros">
      pin: true,
      pinValue: '0000',
      // </if>
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
      isFromArc: false,
      title: 'title',
    };
    model.pageSize = new Size(612, 792);

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
  test(assert(model_test.TestNames.GetPrintTicket), function() {
    // <if expr="chromeos or lacros">
    const origin = DestinationOrigin.CROS;
    // </if>
    // <if expr="not chromeos and not lacros">
    const origin = DestinationOrigin.LOCAL;
    // </if>
    const testDestination = new Destination(
        'FooDevice', DestinationType.LOCAL, origin, 'FooName',
        DestinationConnectionStatus.ONLINE);
    testDestination.capabilities =
        getCddTemplateWithAdvancedSettings(2, 'FooDevice').capabilities;

    // <if expr="chromeos or lacros">
    // Make device managed. It's used for testing pin setting behavior.
    loadTimeData.overrideValues({isEnterpriseManaged: true});
    // </if>
    initializeModel();
    model.destination = testDestination;
    const defaultTicket =
        model.createPrintTicket(testDestination, false, false);

    const expectedDefaultTicketObject: PrintTicket = {
      mediaSize: testDestination.capabilities!.printer!.media_size!.option[0]!,
      pageCount: 3,
      landscape: false,
      color: testDestination.getNativeColorModel(true),
      headerFooterEnabled: false,  // Only used in print preview
      marginsType: MarginsType.DEFAULT,
      duplex: DuplexMode.SIMPLEX,
      copies: 1,
      collate: true,
      shouldPrintBackgrounds: false,
      shouldPrintSelectionOnly: false,
      previewModifiable: true,
      printToGoogleDrive: false,
      printerType: PrinterType.LOCAL_PRINTER,
      rasterizePDF: false,
      scaleFactor: 100,
      scalingType: ScalingType.DEFAULT,
      pagesPerSheet: 1,
      dpiHorizontal: 200,
      dpiVertical: 200,
      dpiDefault: true,
      deviceName: 'FooDevice',
      pageWidth: 612,
      pageHeight: 792,
      showSystemDialog: false,
    };
    // <if expr="chromeos or lacros">
    expectedDefaultTicketObject.advancedSettings = {
      printArea: 4,
      paperType: 0,
    };
    // </if>
    assertEquals(JSON.stringify(expectedDefaultTicketObject), defaultTicket);

    // Toggle all the values and create a new print ticket.
    toggleSettings(testDestination);
    const newTicket = model.createPrintTicket(testDestination, false, false);
    const expectedNewTicketObject: PrintTicket = {
      mediaSize: testDestination.capabilities!.printer!.media_size!.option[1]!,
      pageCount: 1,
      landscape: true,
      color: testDestination.getNativeColorModel(false),
      headerFooterEnabled: false,
      marginsType: MarginsType.CUSTOM,
      duplex: DuplexMode.SHORT_EDGE,
      copies: 2,
      collate: false,
      shouldPrintBackgrounds: true,
      shouldPrintSelectionOnly: false,  // Only for Print Preview.
      previewModifiable: true,
      printToGoogleDrive: false,
      printerType: PrinterType.LOCAL_PRINTER,
      rasterizePDF: true,
      scaleFactor: 90,
      scalingType: ScalingType.CUSTOM,
      pagesPerSheet: 1,
      dpiHorizontal: 100,
      dpiVertical: 100,
      dpiDefault: false,
      deviceName: 'FooDevice',
      pageWidth: 612,
      pageHeight: 792,
      showSystemDialog: false,
      marginsCustom: {
        marginTop: 100,
        marginRight: 200,
        marginBottom: 300,
        marginLeft: 400,
      },
    };
    // <if expr="chromeos or lacros">
    expectedNewTicketObject.pinValue = '0000';
    expectedNewTicketObject.advancedSettings = {
      printArea: 6,
      paperType: 1,
    };
    // </if>

    assertEquals(JSON.stringify(expectedNewTicketObject), newTicket);
  });

  /**
   * Tests that toggling each setting results in the expected change to the
   * cloud job print ticket.
   */
  test(assert(model_test.TestNames.GetCloudPrintTicket), function() {
    initializeModel();

    // Create a test cloud destination.
    const testDestination = new Destination(
        'FooCloudDevice', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
        'FooCloudName', DestinationConnectionStatus.ONLINE);
    testDestination.capabilities =
        getCddTemplateWithAdvancedSettings(2, 'FooDevice').capabilities;
    model.destination = testDestination;

    const defaultTicket = model.createCloudJobTicket(testDestination);
    const expectedDefaultTicket = JSON.stringify({
      version: '1.0',
      print: {
        collate: {collate: true},
        color: {
          type: testDestination.getSelectedColorOption(true)!.type,
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
    assertEquals(expectedDefaultTicket, defaultTicket);

    // Toggle all the values and create a new cloud job ticket.
    toggleSettings(testDestination);
    const newTicket = model.createCloudJobTicket(testDestination);
    const expectedNewTicket = JSON.stringify({
      version: '1.0',
      print: {
        collate: {collate: false},
        color: {
          type: testDestination.getSelectedColorOption(false)!.type,
        },
        copies: {copies: 2},
        duplex: {type: 'SHORT_EDGE'},
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
    assertEquals(expectedNewTicket, newTicket);
  });

  test(assert(model_test.TestNames.ChangeDestination), function() {
    const testDestination = new Destination(
        'FooDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'FooName',
        DestinationConnectionStatus.ONLINE);
    testDestination.capabilities =
        getCddTemplateWithAdvancedSettings(2, 'FooDevice').capabilities;
    // Make black and white printing the default.
    testDestination.capabilities!.printer!.color = {
      option: [
        {type: 'STANDARD_COLOR'},
        {type: 'STANDARD_MONOCHROME', is_default: true}
      ]
    };

    const testDestination2 = new Destination(
        'BarDevice', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'BarName',
        DestinationConnectionStatus.ONLINE);
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
        testDestination.capabilities!.printer!.media_size!.option[1]!);
    model.setSetting('color', true);
    model.setSetting(
        'dpi', testDestination.capabilities!.printer!.dpi!.option[1]!);

    // Confirm toggles.
    assertEquals(true, model.getSettingValue('color'));
    assertEquals('CUSTOM', model.getSettingValue('mediaSize').name);
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
    const testDestination3 = new Destination(
        'Device1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
        DestinationConnectionStatus.ONLINE);
    testDestination3.capabilities =
        Object.assign({}, testDestination.capabilities);
    testDestination3.capabilities!.printer!.media_size = {
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
    testDestination3.capabilities!.printer!.color = {
      option: [
        {type: 'STANDARD_MONOCHROME', is_default: true},
      ]
    };
    testDestination3.capabilities!.printer!.duplex = {
      option: [
        {type: 'NO_DUPLEX', is_default: true},
      ]
    };
    testDestination3.capabilities!.printer!.dpi = {
      option: [
        {horizontal_dpi: 400, vertical_dpi: 400, is_default: true},
        {horizontal_dpi: 800, vertical_dpi: 800},
      ]
    };

    model.destination = testDestination3;
    flush();

    // Verify things changed.
    const updatedSettings = JSON.stringify(model.settings);
    assertNotEquals(oldSettings, updatedSettings);
    assertEquals(false, model.getSettingValue('color'));
    assertEquals('ISO_A4', model.getSettingValue('mediaSize').name);
    assertEquals(400, model.getSettingValue('dpi').horizontal_dpi);
    assertEquals(false, model.getSettingValue('duplex'));
  });

  // <if expr="chromeos or lacros">
  // Tests that printToGoogleDrive is set correctly on the print ticket for Save
  // to Drive CrOS.
  test(assert(model_test.TestNames.PrintToGoogleDriveCros), function() {
    const driveDestination = new Destination(
        GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS, DestinationType.LOCAL,
        DestinationOrigin.LOCAL, 'Save to Google Drive',
        DestinationConnectionStatus.ONLINE);
    initializeModel();
    model.destination = driveDestination;
    const ticket = model.createPrintTicket(driveDestination, false, false);
    assertTrue(JSON.parse(ticket).printToGoogleDrive);
  });
  // </if>

  /**
   * Tests the behaviour of the CDD attribute `reset_to_default`, specifically
   * that when a setting has a default value in CDD and the user selects another
   * value in the UI:
   * - if `reset_to_default`=true, the value of the setting will always be reset
   * to the CDD default.
   * - if `reset_to_default`=false, the value of the setting will always be read
   * from the sticky settings.
   */
  test(assert(model_test.TestNames.CddResetToDefault), function() {
    const cddColorEnabled = true;
    const stickyColorEnabled = false;
    const cddDuplexEnabled = false;
    const stickyDuplexEnabled = true;
    const cddDpi = 200;
    const stickyDpi = 100;
    const cddMediaSizeDisplayName = 'CDD_NAME';
    const stickyMediaSizeDisplayName = 'STICKY_NAME';

    /**
     * Returns the CDD description of a destination with default values
     * specified for color, dpi, duplex and media size.
     * @param resetToDefault Whether the settings should
     * always reset to their default value or not.
     */
    const getTestCapabilities = (resetToDefault: boolean) => {
      return {
        version: '1.0',
        printer: {
          color: {
            option: [
              {type: 'STANDARD_COLOR', is_default: true},
              {type: 'STANDARD_MONOCHROME'}
            ],
            reset_to_default: resetToDefault
          },
          duplex: {
            option: [
              {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
              {type: 'SHORT_EDGE'}
            ],
            reset_to_default: resetToDefault
          },
          dpi: {
            option: [
              {
                horizontal_dpi: cddDpi,
                vertical_dpi: cddDpi,
                is_default: true,
              },
              {horizontal_dpi: stickyDpi, vertical_dpi: stickyDpi},
            ],
            reset_to_default: resetToDefault
          },
          media_size: {
            option: [
              {
                name: 'NA_LETTER',
                width_microns: 215900,
                height_microns: 279400,
                is_default: true,
                custom_display_name: cddMediaSizeDisplayName,
              },
              {
                name: 'CUSTOM',
                width_microns: 215900,
                height_microns: 215900,
                custom_display_name: stickyMediaSizeDisplayName,
              }
            ],
            reset_to_default: resetToDefault
          }
        }
      };
    };
    // Sticky settings that contain different values from the default values
    // returned by `getTestCapabilities`.
    const stickySettings = {
      version: 2,
      isColorEnabled: stickyColorEnabled,
      isDuplexEnabled: stickyDuplexEnabled,
      dpi: {horizontal_dpi: stickyDpi, vertical_dpi: stickyDpi},
      mediaSize: {
        name: 'CUSTOM',
        width_microns: 215900,
        height_microns: 215900,
        custom_display_name: stickyMediaSizeDisplayName,
      },
    };

    const testDestination = new Destination(
        'FooDevice', DestinationType.LOCAL, DestinationOrigin.EXTENSION,
        'FooName', DestinationConnectionStatus.ONLINE);
    testDestination.capabilities =
        getTestCapabilities(/*resetToDefault=*/ true);
    initializeModel();
    model.destination = testDestination;
    model.setStickySettings(JSON.stringify(stickySettings));
    model.applyStickySettings();
    assertEquals(model.settings.color.value, cddColorEnabled);
    assertEquals(model.settings.duplex.value, cddDuplexEnabled);
    assertEquals(model.settings.dpi.value.horizontal_dpi, cddDpi);
    assertEquals(
        model.settings.mediaSize.value.custom_display_name,
        cddMediaSizeDisplayName);

    testDestination.capabilities =
        getTestCapabilities(/*resetToDefault=*/ false);
    model.destination = testDestination;
    model.setStickySettings(JSON.stringify(stickySettings));
    model.applyStickySettings();
    assertEquals(model.settings.color.value, stickyColorEnabled);
    assertEquals(model.settings.duplex.value, stickyDuplexEnabled);
    assertEquals(model.settings.dpi.value.horizontal_dpi, stickyDpi);
    assertEquals(
        model.settings.mediaSize.value.custom_display_name,
        stickyMediaSizeDisplayName);

    const testDestination2 = new Destination(
        'FooDevice2', DestinationType.LOCAL, DestinationOrigin.EXTENSION,
        'FooName2', DestinationConnectionStatus.ONLINE);
    testDestination2.capabilities =
        getTestCapabilities(/*resetToDefault=*/ true);
    // Remove the `is_default` attribute from all the settings.
    delete testDestination2.capabilities!.printer!.color!.option[0]!.is_default;
    delete testDestination2.capabilities!.printer!.duplex!.option[0]!
        .is_default;
    delete testDestination2.capabilities!.printer!.media_size!.option[0]!
        .is_default;
    delete testDestination2.capabilities!.printer!.dpi!.option[0]!.is_default;

    model.destination = testDestination2;

    // Even if `reset_to_default` is true for all options, the model settings
    // should have values from the sticky settings because the CDD doesn't
    // specify default values to reset to.
    model.setStickySettings(JSON.stringify(stickySettings));
    model.applyStickySettings();
    assertEquals(model.settings.color.value, stickyColorEnabled);
    assertEquals(model.settings.duplex.value, stickyDuplexEnabled);
    assertEquals(model.settings.dpi.value.horizontal_dpi, stickyDpi);
    assertEquals(
        model.settings.mediaSize.value.custom_display_name,
        stickyMediaSizeDisplayName);
  });
});
