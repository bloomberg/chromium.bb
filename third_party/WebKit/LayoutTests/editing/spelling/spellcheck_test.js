// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file provides
// |spellcheck_test(sample, tester, expectedText, opt_title)| asynchronous test
// to W3C test harness for easier writing of spellchecker test cases.
//
// |sample| is an HTML fragment text which is inserted as |innerHTML|. It should
// have at least one focus boundary point marker "|" and at most one anchor
// boundary point marker "^" indicating the initial selection.
// TODO(editing-dev): Make initial selection work with TEXTAREA and INPUT.
//
// |tester| is either name with parameter of execCommand or function taking
// one parameter |Document|.
//
// |expectedText| is an HTML fragment indicating the expected result, where text
// with spelling marker is surrounded by '_', and text with grammar marker is
// surrounded by '~'.
//
// |opt_title| is an optional string giving the title of the test case.
//
// See spellcheck_test.html for sample usage.

(function() {
const Sample = window.Sample;

// TODO(editing-dev): Once we can import JavaScript file from scripts, we should
// import "imported/wpt/html/resources/common.js", since |HTML5_VOID_ELEMENTS|
// is defined in there.
/**
 * @const @type {!Set<string>}
 * only void (without end tag) HTML5 elements
 */
const HTML5_VOID_ELEMENTS = new Set([
  'area', 'base', 'br', 'col', 'command', 'embed', 'hr', 'img', 'input',
  'keygen', 'link', 'meta', 'param', 'source','track', 'wbr' ]);

// TODO(editing-dev): Reduce code duplication with assert_selection's Serializer
// once we can import and export Javascript modules.

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isCharacterData(node) {
  return node.nodeType === Node.TEXT_NODE ||
      node.nodeType === Node.COMMENT_NODE;
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isElement(node) {
  return node.nodeType === Node.ELEMENT_NODE;
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isHTMLInputElement(node) {
  return node.nodeName === 'INPUT';
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isHTMLTextAreaElement(node) {
  return node.nodeName === 'TEXTAREA';
}

/**
 * @param {?Range} range
 * @param {!Node} node
 * @param {number} offset
 */
function isAtRangeEnd(range, node, offset) {
  return range && node === range.endContainer && offset === range.endOffset;
}

class MarkerSerializer {
  /**
   * @public
   * @param {!Object} markerTypes
   */
  constructor(markerTypes) {
    /** @type {!Array<string>} */
    this.strings_ = [];
    /** @type {!Object} */
    this.markerTypes_ = markerTypes;
    /** @type {!Object} */
    this.activeMarkerRanges_ = {};
    for (let type in markerTypes)
      this.activeMarkerRanges_[type] = null;
  }

  /**
   * @private
   * @param {string} string
   */
  emit(string) { this.strings_.push(string); }

  /**
   * @private
   * @param {!Node} node
   * @param {number} offset
   */
  advancedTo(node, offset) {
    for (let type in this.markerTypes_) {
      // Handle the ending of the current active marker.
      if (isAtRangeEnd(this.activeMarkerRanges_[type], node, offset)) {
        this.activeMarkerRanges_[type] = null;
        this.emit(this.markerTypes_[type]);
      }

      // Handle the starting of the next active marker.
      if (this.activeMarkerRanges_[type])
        return;
      /** @type {number} */
      const markerCount = window.internals.markerCountForNode(node, type);
      for (let i = 0; i < markerCount; ++i) {
        const marker = window.internals.markerRangeForNode(node, type, i);
        assert_equals(
            marker.startContainer, node,
            'Internal error: marker range not starting in the annotated node.');
        assert_equals(
            marker.endContainer, node,
            'Internal error: marker range not ending in the annotated node.');
        if (marker.startOffset === offset) {
          assert_greater_than(marker.endOffset, offset,
                              'Internal error: marker range is collapsed.');
          this.activeMarkerRanges_[type] = marker;
          this.emit(this.markerTypes_[type]);
          break;
        }
      }
    }
  }

  /**
   * @private
   * @param {!CharacterData} node
   */
  handleCharacterData(node) {
    /** @type {string} */
    const text = node.nodeValue;
    /** @type {number} */
    const length = text.length;
    for (let offset = 0; offset < length; ++offset) {
      this.advancedTo(node, offset);
      this.emit(text[offset]);
    }
    this.advancedTo(node, length);
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  handleInnerEditorOf(element) {
    /** @type {!ShadowRoot} */
    const shadowRoot = window.internals.shadowRoot(element);
    assert_not_equals(
        shadowRoot, undefined,
        'Internal error: text form control element not having shadow tree as ' +
        'inner editor.');
    /** @type {!HTMLDivElement} */
    const innerEditor = shadowRoot.firstChild;
    assert_equals(innerEditor.tagName, 'DIV',
                  'Internal error: inner editor is not DIV');
    innerEditor.childNodes.forEach(child => {
      assert_true(isCharacterData(child),
                  'Internal error: inner editor having child node that is ' +
                  'not CharacterData.');
      this.handleCharacterData(child);
    });
  }

  /**
   * @private
   * @param {!HTMLInputElement} element
   */
  handleInputNode(element) {
    this.emit(' value="');
    this.handleInnerEditorOf(element);
    this.emit('"');
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  handleElementNode(element) {
    /** @type {string} */
    const tagName = element.tagName.toLowerCase();
    this.emit(`<${tagName}`);
    Array.from(element.attributes)
        .sort((attr1, attr2) => attr1.name.localeCompare(attr2.name))
        .forEach(attr => {
          if (attr.value === '')
            return this.emit(` ${attr.name}`);
          const value = attr.value.replace(/&/g, '&amp;')
                            .replace(/\u0022/g, '&quot;')
                            .replace(/\u0027/g, '&apos;');
          this.emit(` ${attr.name}="${value}"`);
        });
    if (isHTMLInputElement(element) && element.value)
      this.handleInputNode(element);
    this.emit('>');
    if (HTML5_VOID_ELEMENTS.has(tagName))
      return;
    this.serializeChildren(element);
    this.emit(`</${tagName}>`);
  }

  /**
   * @public
   * @param {!HTMLDocument} document
   */
  serialize(document) {
    if (document.body)
        this.serializeChildren(document.body);
    else
        this.serializeInternal(document.documentElement);
    return this.strings_.join('');
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  serializeChildren(element) {
    // For TEXTAREA, handle its inner editor instead of its children.
    if (isHTMLTextAreaElement(element) && element.value) {
      this.handleInnerEditorOf(element);
      return;
    }

    element.childNodes.forEach(child => this.serializeInternal(child));
  }

  /**
   * @private
   * @param {!Node} node
   */
  serializeInternal(node) {
    if (isElement(node))
      return this.handleElementNode(node);
    if (isCharacterData(node))
      return this.handleCharacterData(node);
    throw new Error(`Unexpected node ${node}`);
  }
}

/**
 * @param {!Test} testObject
 * @param {!Sample} sample
 * @param {string} expectedText
 * @param {number} remainingRetry
 * @param {number} retryInterval
 */
function verifyMarkers(
    testObject, sample, expectedText, remainingRetry, retryInterval) {
  assert_not_equals(
      window.internals, undefined,
      'window.internals is required for running automated spellcheck tests.');

  /** @type {!MarkerSerializer} */
  const serializer = new MarkerSerializer({
    spelling: '_',
    grammar: '~'});

  try {
    assert_equals(serializer.serialize(sample.document), expectedText);
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
        () => verifyMarkers(testObject, sample, expectedText,
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
 * @param {string} expectedText
 * @param {string=} opt_title
 */
function invokeSpellcheckTest(inputText, tester, expectedText, opt_title) {
  spellcheckTestRunning = true;

  async_test(testObject => {
    // TODO(xiaochengh): Merge the following part with |assert_selection|.
    /** @type {!Sample} */
    const sample = new Sample(inputText);
    if (typeof(tester) === 'function') {
      tester.call(window, sample.document);
    } else if (typeof(tester) === 'string') {
      const strings = tester.split(/ (.+)/);
      sample.document.execCommand(strings[0], false, strings[1]);
    } else {
      assert_unreached(`Invalid tester: ${tester}`);
    }

    /** @type {number} */
    const kMaxRetry = 10;
    /** @type {number} */
    const kRetryInterval = 50;

    // TODO(xiaochengh): We should make SpellCheckRequester::didCheck trigger
    // something in JavaScript (e.g., a |Promise|), so that we can actively know
    // the completion of spellchecking instead of passively waiting for markers
    // to appear or disappear.
    testObject.step_timeout(
        () => verifyMarkers(testObject, sample, expectedText,
                            kMaxRetry, kRetryInterval),
        kRetryInterval);
  }, opt_title, {isSpellcheckTest: true});
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
                         args.expectedText, args.opt_title);
});

/**
 * @param {string} inputText
 * @param {function(!Document)|string} tester
 * @param {string} expectedText
 * @param {string=} opt_title
 */
function spellcheckTest(inputText, tester, expectedText, opt_title) {
  if (window.testRunner)
    window.testRunner.setMockSpellCheckerEnabled(true);

  if (spellcheckTestRunning) {
    testQueue.push({
        inputText: inputText, tester: tester,
        expectedText: expectedText, opt_title: opt_title});
    return;
  }

  invokeSpellcheckTest(inputText, tester, expectedText, opt_title);
}

// Export symbols
window.spellcheck_test = spellcheckTest;
})();
