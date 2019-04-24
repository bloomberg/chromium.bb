// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_select_test', function() {
  suite('SettingsSelectTest', function() {
    let settingsSelect = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      settingsSelect = document.createElement('print-preview-settings-select');
      settingsSelect.disabled = false;
      document.body.appendChild(settingsSelect);
    });

    // Test that destinations are correctly displayed in the lists.
    test('custom media names', function() {
      // Set a capability with custom paper sizes.
      settingsSelect.settingName = 'mediaSize';
      settingsSelect.capability =
          print_preview_test_utils.getMediaSizeCapabilityWithCustomNames();
      const customLocalizedMediaName = settingsSelect.capability.option[0]
                                           .custom_display_name_localized[0]
                                           .value;
      const customMediaName =
          settingsSelect.capability.option[1].custom_display_name;
      Polymer.dom.flush();

      const select = settingsSelect.$$('select');
      // Verify that the selected option and names are as expected.
      assertEquals(0, select.selectedIndex);
      assertEquals(2, select.options.length);
      assertEquals(
          customLocalizedMediaName, select.options[0].textContent.trim());
      assertEquals(customMediaName, select.options[1].textContent.trim());
    });

    test('set setting', async () => {
      // Fake setting.
      settingsSelect.settings = {
        fruit: {
          value: {},
          unavailableValue: {},
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'fruit',
        },
      };
      settingsSelect.settingName = 'fruit';
      settingsSelect.capability = {
        option: [
          {name: 'lime', color: 'green', size: 3},
          {name: 'orange', color: 'orange', size: 5, is_default: true},
        ],
      };
      Polymer.dom.flush();
      const option0 = JSON.stringify(settingsSelect.capability.option[0]);
      const option1 = JSON.stringify(settingsSelect.capability.option[1]);
      const select = settingsSelect.$$('select');

      // Normally done for initialization by the model and parent section.
      settingsSelect.set(
          'settings.fruit.value', settingsSelect.capability.option[0]);
      settingsSelect.selectValue(option1);

      // Verify that the selected option and names are as expected.
      assertEquals(2, select.options.length);
      assertEquals(1, select.selectedIndex);
      assertEquals('lime', select.options[0].textContent.trim());
      assertEquals('orange', select.options[1].textContent.trim());
      assertEquals(option0, select.options[0].value);
      assertEquals(option1, select.options[1].value);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(settingsSelect, option0);
      assertEquals(
          option0, JSON.stringify(settingsSelect.getSettingValue('fruit')));
      assertEquals(0, select.selectedIndex);

      // Verify that selecting from outside works.
      settingsSelect.selectValue(option1);
      await test_util.eventToPromise('process-select-change', settingsSelect);
      assertEquals(
          option1, JSON.stringify(settingsSelect.getSettingValue('fruit')));
      assertEquals(1, select.selectedIndex);
    });
  });
});
