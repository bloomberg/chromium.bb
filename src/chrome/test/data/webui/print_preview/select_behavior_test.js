// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('select_behavior_test', function() {
  /** @enum {string} */
  const TestNames = {
    CallProcessSelectChange: 'call process select change',
  };

  const suiteName = 'SelectBehaviorTest';
  suite(suiteName, function() {
    /** @type {?TestSelectElement} */
    let testSelect = null;

    /** @type {string} */
    let settingValue = '0';

    /** @override */
    suiteSetup(function() {
      document.body.innerHTML = `
        <dom-module id="test-select">
          <template>
            <select value="{{selectedValue::change}}">
              <option value="0" selected>0</option>
              <option value="1">1</option>
              <option value="2">2</option>
            </select>
          </template>
        </dom-module>
      `;

      Polymer({
        is: 'test-select',
        behaviors: [print_preview_new.SelectBehavior],

        onProcessSelectChange: function(value) {
          settingValue = value;
          this.fire('process-select-change-called', value);
        },
      });
    });

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      testSelect = document.createElement('test-select');
      document.body.appendChild(testSelect);
      testSelect.selectedValue = '0';
    });

    // Tests that onProcessSelectChange() is called when the select value is
    // set programmatically or by changing the select element.
    test(assert(TestNames.CallProcessSelectChange), function() {
      const select = testSelect.$$('select');
      assertEquals('0', testSelect.selectedValue);
      assertEquals('0', select.value);
      let whenProcessSelectCalled =
          test_util.eventToPromise('process-select-change-called', testSelect);
      testSelect.selectedValue = '1';
      // Should be debounced so settingValue has not changed yet.
      assertEquals('0', settingValue);
      return whenProcessSelectCalled
          .then((e) => {
            assertEquals('1', e.detail);
            assertEquals('1', select.value);
            whenProcessSelectCalled = test_util.eventToPromise(
                'process-select-change-called', testSelect);
            select.value = '0';
            select.dispatchEvent(new CustomEvent('change'));
            assertEquals('1', settingValue);
            return whenProcessSelectCalled;
          })
          .then((e) => {
            assertEquals('0', e.detail);
            assertEquals('0', testSelect.selectedValue);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
