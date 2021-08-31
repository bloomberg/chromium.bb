// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(
    ['../chromevox/testing/chromevox_next_e2e_test_base.js', 'array_util.js']);

/** Test fixture for automation_predicate.js. */
AutomationPredicateTest = class extends ChromeVoxNextE2ETest {};

TEST_F('AutomationPredicateTest', 'EquivalentRoles', function() {
  const site = `
    <input type="text"></input>
    <input role="combobox"></input>
  `;
  this.runWithLoadedTree(site, (root) => {
    // Text field is equivalent to text field with combo box.
    const textField = root.find({role: RoleType.TEXT_FIELD});
    assertTrue(!!textField, 'No text field found.');
    const textFieldWithComboBox =
        root.find({role: RoleType.TEXT_FIELD_WITH_COMBO_BOX});
    assertTrue(!!textFieldWithComboBox, 'No text field with combo box found.');

    // Gather all potential predicate names.
    const keys = Object.getOwnPropertyNames(AutomationPredicate);
    for (const key of keys) {
      // Not all keys are functions or predicates e.g. makeTableCellPredicate.
      if (typeof (AutomationPredicate[key]) !== 'function' ||
          key.indexOf('make') === 0) {
        continue;
      }

      const predicate = AutomationPredicate[key];
      if (predicate(textField)) {
        assertTrue(
            !!predicate(textFieldWithComboBox),
            `Textfield with combo box should match predicate ${key}`);
      }
    }
  });
});
