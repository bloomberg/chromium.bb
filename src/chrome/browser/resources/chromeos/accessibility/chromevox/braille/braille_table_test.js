// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js', '../testing/assert_additions.js'
]);

/**
 * Test fixture for BrailleTable tests.
 * This is an E2E test because there's no easy way to load a data file in
 * a webui-style test.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
function ChromeVoxBrailleTableTest() {
  ChromeVoxE2ETest.call(this);
}

ChromeVoxBrailleTableTest.prototype = {
  __proto__: ChromeVoxE2ETest.prototype,
};

/**
 * Tests that {@code getAll} can fetch and parse the tables file.
 * NOTE: This will need to be adjusted when more tables are added.
 */
TEST_F('ChromeVoxBrailleTableTest', 'testGetAllAndValidate', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    expectEquals(166, tables.length);
    assertNotNullNorUndefined(
        BrailleTable.forId(tables, 'en-US-g1'),
        'Can\'t find US English grade 1 table');
    for (var i = 0, table; table = tables[i]; ++i) {
      expectEquals('string', typeof table.id);
      expectTrue(table.dots === '6' || table.dots === '8');
      expectTrue(BrailleTable.getDisplayName(table).length > 0);
    }
  }));
});

/** Tests getDisplayName for some specific representative cases. */
TEST_F('ChromeVoxBrailleTableTest', 'testGetDisplayName', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    var table = BrailleTable.forId(tables, 'bg-comp8');
    expectEquals('Bulgarian', BrailleTable.getDisplayName(table));
    table = BrailleTable.forId(tables, 'ar-g1');
    expectEquals('Arabic, Grade 1', BrailleTable.getDisplayName(table));
    table = BrailleTable.forId(tables, 'en-UEB-g1');
    expectEquals('English (UEB), Grade 1', BrailleTable.getDisplayName(table));
  }));
});

/**
 * Tests the getUncontracted function.
 */
TEST_F('ChromeVoxBrailleTableTest', 'testGetUncontracted', function() {
  BrailleTable.getAll(this.newCallback(function(tables) {
    function expectUncontracted(uncontractedId, idToCheck) {
      var checkedTable = BrailleTable.forId(tables, idToCheck);
      var uncontractedTable =
          BrailleTable.getUncontracted(tables, checkedTable);
      expectNotEquals(null, uncontractedTable);
      expectEquals(uncontractedId, uncontractedTable.id);
    }
    expectUncontracted('en-US-comp8', 'en-US-g2');
    expectUncontracted('en-US-comp8', 'en-US-comp8');
    expectUncontracted('sv-comp8', 'sv-g1');
    expectUncontracted('ar-g1', 'ar-g1');
    expectUncontracted('de-comp8', 'de-CH-g2');
  }));
});
