// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file provides
// |spellcheck_test(sample, tester, expectedMarkers, opt_title)| asynchronous
// test to W3C test harness for easier writing of editing test cases.
//
// |sample| is an HTML fragment text which is inserted as |innerHTML|. It should
// have at least one focus boundary point marker "|" and at most one anchor
// boundary point marker "^" indicating the initial selection.
//
// |tester| is either name with parameter of execCommand or function taking
// one parameter |Document|.
//
// |expectedMarkers| is either a |Marker| or a |Marker| array, where each
// |Marker| is an |Object| with the following properties:
// - |location| and |length| are integers indicating the range of the marked
//   text. It must hold that |location >= 0| and |length > 0|.
// - |type| is an optional string indicating the marker type. When present, it
//   must be equal to either "spelling" or "grammer". When absent, it is
//   regarded as "spelling".
// - |description| is an optional string indicating the description of a marker.
//
// |opt_title| is an optional string giving the title of the test case.
//
// Examples:
//
// spellcheck_test(
//     '<div contentEditable>|</div>',
//     'insertText wellcome.',
//     spellingMarker(0, 8, 'welcome'), // 'wellcome'
//     'Mark misspellings and give replacement suggestions after typing.');
//
// spellcheck_test(
//     '<div contentEditable>|</div>',
//     'insertText You has the right.',
//     grammarMarker(4, 3), // 'has'
//     'Mark ungrammatical phrases after typing.');

(function() {
const Sample = window.Sample;

/** @type {string} */
const kSpelling = 'spelling';
/** @type {string} */
const kGrammar = 'grammar';

class Marker {
  /**
   * @public
   * @param {number} location
   * @param {number} length
   * @param {string=} opt_type
   * @param {string=} opt_description
   */
  constructor(location, length, opt_type, opt_description) {
    /** @type {number} */
    this.location_ = location;
    /** @type {number} */
    this.length_ = length;
    /** @type {string} */
    this.type_ = opt_type || 'spelling';
    /** @type {boolean} */
    this.ignoreDescription_ = opt_description === undefined;
    /** @type {string} */
    this.description_ = opt_description || '';
  }

  /** @return {number} */
  get location() { return this.location_; }

  /** @return {number} */
  get length() { return this.length_; }

  /** @return {string} */
  get type() { return this.type_; }

  /** @return {boolean} */
  get ignoreDescription() { return this.ignoreDescription_; }

  /** @return {string} */
  get description() { return this.description_; }

  /**
   * @public
   */
  assertValid() {
    // TODO(xiaochengh): Add proper assert descriptions when needed.
    assert_true(Number.isInteger(this.location_));
    assert_greater_than_equal(this.location_, 0);
    assert_true(Number.isInteger(this.length_));
    assert_greater_than(this.length_, 0);
    assert_true(this.type_ === kSpelling || this.type_ === kGrammar);
    assert_true(typeof this.description_ === 'string');
  }

  /**
   * @public
   * @param {!Marker} expected
   */
  assertMatch(expected) {
    try {
      assert_equals(this.location, expected.location);
      assert_equals(this.length, expected.length);
      assert_equals(this.type, expected.type);
      if (expected.ignoreDescription)
        return;
      assert_equals(this.description, expected.description);
    } catch (error) {
      throw new Error(`Expected ${expected} but got ${this}.`);
    }
  }

  /** @override */
  toString() {
    return `${this.type_} marker at ` +
        `[${this.location_}, ${this.location_ + this.length_}]` +
        (this.description_ ? ` with description "${this.description_}"` : ``);
  }
}

/**
 * @param {number} location
 * @param {number} length
 * @param {string=} opt_description
 * @return {!Marker}
 */
function spellingMarker(location, length, opt_description) {
  return new Marker(location, length, kSpelling, opt_description);
}

/**
 * @param {number} location
 * @param {number} length
 * @param {string=} opt_description
 * @return {!Marker}
 */
function grammarMarker(location, length, opt_description) {
  return new Marker(location, length, kGrammar, opt_description);
}

/**
 * @param {!Marker} marker1
 * @param {!Marker} marker2
 * @return {number}
 */
function markerComparison(marker1, marker2) {
  return marker1.location - marker2.location;
}

/**
 * @param {!Array<!Marker>} expectedMarkers
 */
function checkExpectedMarkers(expectedMarkers) {
  if (expectedMarkers.length === 0)
    return;
  expectedMarkers.forEach(marker => marker.assertValid());
  expectedMarkers.sort(markerComparison);
  expectedMarkers.reduce((lastMarker, currentMarker) => {
    assert_less_than(
        lastMarker.location + lastMarker.length, currentMarker.location,
        'Marker ranges should be disjoint.');
    return currentMarker;
  });
}

/**
 * @param {!Node} node
 * @param {string} type
 * @param {!Array<!Marker>} markers
 */
function extractMarkersOfType(node, type, markers) {
  /** @type {!HTMLBodyElement} */
  const body = node.ownerDocument.body;
  /** @type {number} */
  const markerCount = window.internals.markerCountForNode(node, type);
  for (let i = 0; i < markerCount; ++i) {
    /** @type {!Range} */
    const markerRange = window.internals.markerRangeForNode(node, type, i);
    /** @type {string} */
    const description = window.internals.markerDescriptionForNode(node, type, i);
    /** @type {number} */
    const location = window.internals.locationFromRange(body, markerRange);
    /** @type {number} */
    const length = window.internals.lengthFromRange(body, markerRange);

    markers.push(new Marker(location, length, type, description));
  }
}

/**
 * @param {!Node} node
 * @param {!Array<!Marker>} markers
 */
function extractAllMarkersRecursivelyTo(node, markers) {
  extractMarkersOfType(node, kSpelling, markers);
  extractMarkersOfType(node, kGrammar, markers);
  node.childNodes.forEach(
      child => extractAllMarkersRecursivelyTo(child, markers));
}

/**
 * @param {!Document} doc
 * @return {!Array<!Marker>}
 */
function extractAllMarkers(doc) {
  /** @type {!Array<!Marker>} */
  const markers = [];
  extractAllMarkersRecursivelyTo(doc.body, markers);
  markers.sort(markerComparison);
  return markers;
}

/**
 * @param {!Test} testObject
 * @param {!Sample} sample,
 * @param {!Array<!Marker>} expectedMarkers
 * @param {number} remainingRetry
 * @param {number} retryInterval
 */
function verifyMarkers(
    testObject, sample, expectedMarkers, remainingRetry, retryInterval) {
  assert_not_equals(
      window.internals, undefined,
      'window.internals is required for running automated spellcheck tests.');

  /** @type {!Array<!Marker>} */
  const actualMarkers = extractAllMarkers(sample.document);
  try {
    assert_equals(actualMarkers.length, expectedMarkers.length,
                  'Number of markers mismatch.');
    actualMarkers.forEach(
        (marker, index) => marker.assertMatch(expectedMarkers[index]));
    testObject.done();
    sample.remove();
  } catch (error) {
    if (remainingRetry <= 0)
      throw error;

    // Force invoking idle time spellchecker in case it has not been run yet.
    if (window.testRunner)
      window.testRunner.runIdleTasks(() => {});

    // TODO(xiaochengh): We should make SpellCheckRequester::didCheck trigger
    // something in JavaScript (e.g., a |Promise|), so that we can actively
    // know the completion of spellchecking instead of passively waiting for
    // markers to appear or disappear.
    testObject.step_timeout(
        () => verifyMarkers(testObject, sample, expectedMarkers,
                            remainingRetry - 1, retryInterval),
        retryInterval);
  }
}

// Spellchecker gets triggered not only by text and selection change, but also
// by focus change. For example, misspelling markers in <INPUT> disappear when
// the window loses focus, even though the selection does not change.
// Therefore, we disallow spellcheck tests from running simultaneously to
// prevent interference among them. If we call spellcheck_test while another
// test is running, the new test will be added into testQueue waiting for the
// completion of the previous test.

/** @type {boolean} */
var spellcheckTestRunning = false;
/** @type {!Array<!Object>} */
const testQueue = [];

/**
 * @param {string} inputText
 * @param {function(!Document)|string} tester
 * @param {!Marker|!Array<!Marker>} expectedMarkers
 * @param {string=} opt_title
 */
function invokeSpellcheckTest(inputText, tester, expectedMarkers, opt_title) {
  spellcheckTestRunning = true;

  /** @type {!Test} */
  const testObject = async_test(opt_title, {isSpellcheckTest: true});

  if (!(expectedMarkers instanceof Array))
    expectedMarkers = [expectedMarkers]
  testObject.step(() => checkExpectedMarkers(expectedMarkers));

  if (window.testRunner)
    window.testRunner.setMockSpellCheckerEnabled(true);

  // TODO(xiaochengh): Merge the following part with |assert_selection|.
  /** @type {!Sample} */
  const sample = new Sample(inputText);
  if (typeof(tester) === 'function') {
    tester.call(window, sample.document);
  } else if (typeof(tester) === 'string') {
    const strings = tester.split(/ (.+)/);
    sample.document.execCommand(strings[0], false, strings[1]);
  } else {
    testObject.step(() => assert_unreached(`Invalid tester: ${tester}`));
  }

  /** @type {number} */
  const kMaxRetry = 10;
  /** @type {number} */
  const kRetryInterval = 50;

  // TODO(xiaochengh): We should make SpellCheckRequester::didCheck trigger
  // something in JavaScript (e.g., a |Promise|), so that we can actively know
  // the completion of spellchecking instead of passively waiting for markers to
  // appear or disappear.
  testObject.step_timeout(
      () => verifyMarkers(testObject, sample, expectedMarkers,
                          kMaxRetry, kRetryInterval),
      kRetryInterval);
}

add_result_callback(testObj => {
    if (!testObj.properties.isSpellcheckTest)
      return;
    spellcheckTestRunning = false;
    /** @type {Object} */
    const args = testQueue.shift();
    if (args === undefined)
      return;
    invokeSpellcheckTest(args.inputText, args.tester,
                         args.expectedMarkers, args.opt_title);
});

/**
 * @param {string} inputText
 * @param {function(!Document)|string} tester
 * @param {!Marker|!Array<!Marker>} expectedMarkers
 * @param {string=} opt_title
 */
function spellcheckTest(inputText, tester, expectedMarkers, opt_title) {
  if (spellcheckTestRunning) {
    testQueue.push({
        inputText: inputText, tester: tester,
        expectedMarkers: expectedMarkers, opt_title: opt_title});
    return;
  }

  invokeSpellcheckTest(inputText, tester, expectedMarkers, opt_title);
}

// Export symbols
window.Marker = Marker;
window.spellingMarker = spellingMarker;
window.grammarMarker = grammarMarker;
window.spellcheck_test = spellcheckTest;
})();
