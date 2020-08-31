// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the liblouis wasm wrapper, as seen from
 *    the JavaScript interface.
 */

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js', '../testing/assert_additions.js'
]);

ChromeVoxLibLouisTest = class extends ChromeVoxE2ETest {
  createLiblouis() {
    return new LibLouis(
        chrome.extension.getURL('chromevox/braille/liblouis_wrapper.js'), '',
        () => {});
  }

  withTranslator(liblouis, tableNames, callback) {
    liblouis.getTranslator(tableNames, this.newCallback(callback));
  }
};


function assertEqualsUint8Array(expected, actual) {
  const asArray = [];
  const uint8array = new Uint8Array(actual);
  for (let i = 0; i < uint8array.length; ++i) {
    asArray[i] = uint8array[i];
  }
  assertEqualsJSON(expected, asArray);
}

function LIBLOUIS_TEST_F(testName, testFunc, opt_preamble) {
  const wrappedTestFunc = function() {
    const liblouis = new LibLouis(
        chrome.extension.getURL('chromevox/braille/liblouis_wrapper.js'), '',
        testFunc.bind(this));
  };
  TEST_F('ChromeVoxLibLouisTest', testName, wrappedTestFunc, opt_preamble);
}

function LIBLOUIS_TEST_F_WITH_PREAMBLE(preamble, testName, testFunc) {
  LIBLOUIS_TEST_F(testName, testFunc, preamble);
}

LIBLOUIS_TEST_F('testTranslateComputerBraille', function(liblouis) {
  this.withTranslator(liblouis, 'en-us-comp8.ctb', function(translator) {
    translator.translate(
        'Hello!', [],
        this.newCallback(function(cells, textToBraille, brailleToText) {
          assertEqualsUint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e], cells);
          assertEqualsJSON([0, 1, 2, 3, 4, 5], textToBraille);
          assertEqualsJSON([0, 1, 2, 3, 4, 5], brailleToText);
        }));
  });
});

LIBLOUIS_TEST_F_WITH_PREAMBLE(
    `
#if defined(MEMORY_SANITIZER)
#define MAYBE_checkAllTables DISABLED_checkAllTables
#else
// Flaky, see crbug.com/1048585.
#define MAYBE_checkAllTables DISABLED_checkAllTables
#endif
`,
    'MAYBE_checkAllTables', function(liblouis) {
      BrailleTable.getAll(this.newCallback(function(tables) {
        let i = 0;
        const checkNextTable = function() {
          const table = tables[i++];
          if (table) {
            this.withTranslator(
                liblouis, table.fileNames, function(translator) {
                  assertNotEquals(
                      null, translator,
                      'Table ' + JSON.stringify(table) + ' should be valid');
                  checkNextTable();
                });
          }
        }.bind(this);
        checkNextTable();
      }.bind(this)));
    });

LIBLOUIS_TEST_F('testBackTranslateComputerBraille', function(liblouis) {
  this.withTranslator(liblouis, 'en-us-comp8.ctb', function(translator) {
    const cells = new Uint8Array([0x53, 0x11, 0x07, 0x07, 0x15, 0x2e]);
    translator.backTranslate(cells.buffer, this.newCallback(function(text) {
      assertEquals('Hello!', text);
    }));
  });
});

LIBLOUIS_TEST_F('testTranslateGermanGrade2Braille', function(liblouis) {
  // This is one of the moderately large tables.
  this.withTranslator(liblouis, 'de-g2.ctb', function(translator) {
    translator.translate(
        'München', [],
        this.newCallback(function(cells, textToBraille, brailleToText) {
          assertEqualsUint8Array([0x0d, 0x33, 0x1d, 0x39, 0x09], cells);
          assertEqualsJSON([0, 1, 2, 3, 3, 4, 4], textToBraille);
          assertEqualsJSON([0, 1, 2, 3, 5], brailleToText);
        }));
  });
});

LIBLOUIS_TEST_F('testBackTranslateGermanComputerBraille', function(liblouis) {
  this.withTranslator(liblouis, 'de-de-comp8.ctb', function(translator) {
    const cells = new Uint8Array([0xb3]);
    translator.backTranslate(cells.buffer, this.newCallback(function(text) {
      assertEquals('ü', text);
    }));
  });
});

LIBLOUIS_TEST_F('testBackTranslateEmptyCells', function(liblouis) {
  this.withTranslator(liblouis, 'de-de-comp8.ctb', function(translator) {
    translator.backTranslate(
        new Uint8Array().buffer, this.newCallback(function(text) {
          assertNotEquals(null, text);
          assertEquals(0, text.length);
        }));
  });
});

LIBLOUIS_TEST_F('testGetInvalidTranslator', function(liblouis) {
  this.withTranslator(liblouis, 'nonexistant-table', function(translator) {
    assertEquals(null, translator);
  });
});

LIBLOUIS_TEST_F('testKeyEventStaticData', function(liblouis) {
  this.withTranslator(liblouis, 'en-us-comp8.ctb', function(translator) {
    translator.translate(
        'abcdefghijklmnopqrstuvwxyz 0123456789', [],
        this.newCallback(function(cells, textToBraille, brailleToText) {
          // A-Z.
          const view = new Uint8Array(cells);
          for (let i = 0; i < 26; i++) {
            assertEquals(
                String.fromCharCode(i + 65),
                BrailleKeyEvent.brailleDotsToStandardKeyCode[view[i]]);
          }

          // 0-9.
          for (let i = 27; i < 37; i++) {
            assertEquals(
                String.fromCharCode(i + 21),
                BrailleKeyEvent.brailleDotsToStandardKeyCode[view[i]]);
          }
        }));
  });
});
