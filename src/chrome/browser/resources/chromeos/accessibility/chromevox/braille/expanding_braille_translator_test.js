// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_unittest_base.js', '../testing/assert_additions.js'
]);

/**
 * Test fixture.
 * @constructor
 * @extends {ChromeVoxUnitTestBase}
 */
function ChromeVoxExpandingBrailleTranslatorUnitTest() {}

ChromeVoxExpandingBrailleTranslatorUnitTest.prototype = {
  __proto__: ChromeVoxUnitTestBase.prototype,

  /** @override */
  closureModuleDeps: [
    'Spannable',
    'BrailleTextStyleSpan',
    'ExpandingBrailleTranslator',
    'LibLouis',
    'ValueSelectionSpan',
    'ValueSpan',
  ]
};

/**
 * An implementation of {@link LibLouis.Translator} whose translation
 * output is an array buffer of the same byte length as the input and where
 * each byte is equal to the character code of {@code resultChar}.  The
 * position mappings are one to one in both directions.
 * @param {string} resultChar A one character string used for each byte of the
 *     result.
 * @constructor
 * @extends {LibLouis.Translator}
 */
function FakeTranslator(resultChar, opt_styleMap) {
  /** @private {string} */
  this.resultChar_ = resultChar;
  /** @private {!Object} */
  this.styleMap_ = opt_styleMap || {};
}

FakeTranslator.prototype = {
  /** @Override */
  translate: function(text, formTypeMap, callback) {
    var result = new Uint8Array(text.length);
    var textToBraille = [];
    var brailleToText = [];
    for (var i = 0; i < text.length; ++i) {
      var formType = this.styleMap_[formTypeMap[i]];
      if (formType) {
        formType = formType.charCodeAt(0);
      }
      result[i] = formType || this.resultChar_.charCodeAt(0);
      textToBraille.push(i);
      brailleToText.push(i);
    }
    callback(result.buffer, textToBraille, brailleToText);
  }
};

/**
 * Asserts that a array buffer, viewed as an uint8 array, matches
 * the contents of a string.  The character code of each character of the
 * string shall match the corresponding byte in the array buffer.
 * @param {ArrayBuffer} actual Actual array buffer.
 * @param {string} expected Array of expected bytes.
 */
function assertArrayBufferMatches(expected, actual) {
  assertTrue(actual instanceof ArrayBuffer);
  var a = new Uint8Array(actual);
  assertEquals(expected.length, a.length);
  for (var i = 0; i < a.length; ++i) {
    assertEquals(expected.charCodeAt(i), a[i], 'Position ' + i);
  }
}

TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'TranslationError',
    function() {
      var text = new Spannable('error ok', new ValueSpan());
      text.setSpan(new ValueSelectionSpan, 0, 0);
      var contractedTranslator = new FakeTranslator('c');
      // Translator that always results in an error.
      var uncontractedTranslator = {
        translate: function(text, formTypeMap, callback) {
          callback(null, null, null);
        }
      };
      var translationResult = null;

      var expandingTranslator = new ExpandingBrailleTranslator(
          contractedTranslator, uncontractedTranslator);
      expandingTranslator.translate(
          text, ExpandingBrailleTranslator.ExpansionType.SELECTION,
          function(cells, textToBraille, brailleToText) {
            // Expect the string ' ok' to be translated using the contracted
            // translator.  The preceding part isn't included because it
            // resulted in a translation error.
            assertArrayBufferMatches('ccc', cells);
            assertEqualsJSON([0, 0, 0, 0, 0, 0, 1, 2], textToBraille);
            assertEqualsJSON([5, 6, 7], brailleToText);
          });
    });

// Test for many variations of successful translations.

var totalRunTranslationTests = 0;

/**
 * Performs the translation and checks the output.
 * @param {string} name Name that describes the input for error messages.
 * @param {boolean} contracted Whether to use a contracted translator
 *     in addition to the uncontracted one.
 * @param {ExpandingBrailleTranslator.ExpansionType} valueExpansion
 *     Value expansion argument to pass to the translator.
 * @param {string} text Input string.
 * @param {string} expectedOutput Expected output as a string (see
 *     {@code TESTDATA} below for a description of the format).
 */
function doTranslationTest(
    name, contracted, valueExpansion, text, expectedOutput) {
  try {
    totalRunTranslationTests++;
    var uncontractedTranslator = new FakeTranslator('u');
    var expandingTranslator;
    if (contracted) {
      var contractedTranslator = new FakeTranslator('c');
      expandingTranslator = new ExpandingBrailleTranslator(
          contractedTranslator, uncontractedTranslator);
    } else {
      expandingTranslator =
          new ExpandingBrailleTranslator(uncontractedTranslator);
    }
    var extraCellsSpan = text.getSpanInstanceOf(ExtraCellsSpan);
    if (extraCellsSpan) {
      var extraCellsSpanPos = text.getSpanStart(extraCellsSpan);
    }
    var expectedTextToBraille = [];
    var expectedBrailleToText = [];
    for (var i = 0, pos = 0; i < text.length; ++i, ++pos) {
      if (i === extraCellsSpanPos) {
        ++pos;
      }
      expectedTextToBraille.push(pos);
      expectedBrailleToText.push(i);
    }
    if (extraCellsSpan) {
      expectedBrailleToText.splice(extraCellsSpanPos, 0, extraCellsSpanPos);
    }

    expandingTranslator.translate(
        text, valueExpansion, function(cells, textToBraille, brailleToText) {
          assertArrayBufferMatches(expectedOutput, cells, name);
          assertEqualsJSON(expectedTextToBraille, textToBraille, name);
          assertEqualsJSON(expectedBrailleToText, brailleToText, name);
        });
  } catch (e) {
    console.error('Subtest ' + name + ' failed.');
    throw e;
  }
}

/**
 * Runs two tests, one with the given values and one with the given values
 * where the text is surrounded by a typical name and role.
 * @param {{name: string, input: string, contractedOutput: string}}
 *     testCase An entry of {@code TESTDATA}.
 * @param {boolean} contracted Whether to use both uncontracted
 *     and contracted translators.
 * @param {ExpandingBrailleTranslation.ExpansionType} valueExpansion
 *     What kind of value expansion to apply.
 * @param {boolean} withExtraCells Whether to insert an extra cells span
 *     right before the selection in the input.
 */
function runTranslationTestVariants(
    testCase, contracted, valueExpansion, withExtraCells) {
  var expType = ExpandingBrailleTranslator.ExpansionType;
  // Construct the full name.
  var fullName = contracted ? 'Contracted_' : 'Uncontracted_';
  fullName += 'Expansion' + valueExpansion + '_';
  if (withExtraCells) {
    fullName += 'ExtraCells_';
  }
  fullName += testCase.name;
  var input = testCase.input;
  if (withExtraCells) {
    input = input.substring(0);  // Shallow copy.
    var selectionStart =
        input.getSpanStart(input.getSpanInstanceOf(ValueSelectionSpan));
    var extraCellsSpan = new ExtraCellsSpan();
    extraCellsSpan.cells = new Uint8Array(['e'.charCodeAt(0)]).buffer;
    input.setSpan(extraCellsSpan, selectionStart, selectionStart);
  }
  // The expected output depends on the contraction mode and value expansion.
  var outputChar = contracted ? 'c' : 'u';
  var expectedOutput;
  if (contracted && valueExpansion === expType.SELECTION) {
    expectedOutput = testCase.contractedOutput;
  } else if (contracted && valueExpansion === expType.ALL) {
    expectedOutput = new Array(testCase.input.length + 1).join('u');
  } else {
    expectedOutput = new Array(testCase.input.length + 1).join(outputChar);
  }
  if (withExtraCells) {
    expectedOutput = expectedOutput.substring(0, selectionStart) + 'e' +
        expectedOutput.substring(selectionStart);
  }
  doTranslationTest(
      fullName, contracted, valueExpansion, input, expectedOutput);

  // Run another test, with the value surrounded by some text.
  var surroundedText = new Spannable('Name: ');
  var surroundedExpectedOutput =
      new Array('Name: '.length + 1).join(outputChar);
  surroundedText.append(input);
  surroundedExpectedOutput += expectedOutput;
  if (testCase.input.length > 0) {
    surroundedText.append(' ');
    surroundedExpectedOutput += outputChar;
  }
  surroundedText.append('edtxt');
  surroundedExpectedOutput += new Array('edtxt'.length + 1).join(outputChar);
  doTranslationTest(
      fullName + '_Surrounded', contracted, valueExpansion, surroundedText,
      surroundedExpectedOutput);
}

/**
 * Creates a spannable text with optional selection.
 * @param {string} text The text.
 * @param {=opt_selectionStart} Selection start or caret position.  No
 *     selection is added if undefined.
 * @param {=opt_selectionEnd} Selection end if selection is not a caret.
 */
function createText(text, opt_selectionStart, opt_selectionEnd, opt_style) {
  var result = new Spannable(text);

  result.setSpan(new ValueSpan, 0, text.length);
  if (goog.isDef(opt_selectionStart)) {
    result.setSpan(
        new ValueSelectionSpan, opt_selectionStart,
        goog.isDef(opt_selectionEnd) ? opt_selectionEnd : opt_selectionStart);
  }

  if (goog.isDef(opt_style)) {
    result.setSpan(
        new BrailleTextStyleSpan(opt_style.formType), opt_style.start,
        opt_style.end);
  }
  return result;
}


var TEXT = 'Hello, world!';

TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'successfulTranslations',
    function() {
      /**
       * Dictionary of test strings, keyed on a descriptive name for the
       * test case.  The value is an array of the input string to the
       * translation and the expected output using a translator with both
       * uncontracted and contracted underlying translators.  The expected
       * output is in the form of a string of the same length as the input,
       * where an 'u' means that the uncontracted translator was used at this
       * location and a 'c' means that the contracted translator was used.
       */
      var TESTDATA = [
        {name: 'emptyText', input: createText(''), contractedOutput: ''}, {
          name: 'emptyTextWithCaret',
          input: createText('', 0),
          contractedOutput: ''
        },
        {
          name: 'textWithNoSelection',
          input: createText(TEXT),
          contractedOutput: 'ccccccccccccc'
        },
        {
          name: 'textWithCaretAtStart',
          input: createText(TEXT, 0),
          contractedOutput: 'uuuuuuccccccc'
        },
        {
          name: 'textWithCaretAtEnd',
          input: createText(TEXT, TEXT.length),
          contractedOutput: 'cccccccuuuuuu'
        },
        {
          name: 'textWithCaretInWhitespace',
          input: createText(TEXT, 6),
          contractedOutput: 'uuuuuuucccccc'
        },
        {
          name: 'textWithSelectionEndInWhitespace',
          input: createText(TEXT, 0, 7),
          contractedOutput: 'uuuuuuucccccc'
        },
        {
          name: 'textWithSelectionInTwoWords',
          input: createText(TEXT, 2, 9),
          contractedOutput: 'uuuuuucuuuuuu'
        }
      ];
      var TESTDATA_WITH_SELECTION = TESTDATA.filter(function(testCase) {
        return testCase.input.getSpanInstanceOf(ValueSelectionSpan);
      });

      var expType = ExpandingBrailleTranslator.ExpansionType;
      for (var i = 0, testCase; testCase = TESTDATA[i]; ++i) {
        runTranslationTestVariants(testCase, false, expType.SELECTION, false);
        runTranslationTestVariants(testCase, true, expType.NONE, false);
        runTranslationTestVariants(testCase, true, expType.SELECTION, false);
        runTranslationTestVariants(testCase, true, expType.ALL, false);
      }
      for (var i = 0, testCase; testCase = TESTDATA_WITH_SELECTION[i]; ++i)
        runTranslationTestVariants(testCase, true, expType.SELECTION, true);

      // Make sure that the logic above runs the tests, adjust when adding more
      // test variants.
      var totalExpectedTranslationTests =
          2 * (TESTDATA.length * 4 + TESTDATA_WITH_SELECTION.length);
      assertEquals(totalExpectedTranslationTests, totalRunTranslationTests);
    });

TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'StyleTranslations',
    function() {
      var formTypeMap = {};
      formTypeMap[LibLouis.FormType.BOLD] = 'b';
      formTypeMap[LibLouis.FormType.ITALIC] = 'i';
      formTypeMap[LibLouis.FormType.UNDERLINE] = 'u';
      var translator = new ExpandingBrailleTranslator(
          new FakeTranslator('c', formTypeMap), new FakeTranslator('u'));
      translator.translate(
          createText(
              'a test of text', undefined, undefined,
              {start: 2, end: 6, formType: LibLouis.FormType.BOLD}),
          ExpandingBrailleTranslator.ExpansionType.NONE, function(cells) {
            assertArrayBufferMatches('ccbbbbcccccccc', cells);
          });
    });
