// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js'
]);

/**
 * Test fixture for ISearch.
 * @constructor
 * @extends {ChromeVoxNextE2ETest}
 */
function ChromeVoxISearchTest() {
  ChromeVoxNextE2ETest.call(this);
}

ChromeVoxISearchTest.prototype = {
  __proto__: ChromeVoxNextE2ETest.prototype,

  /** @override */
  runtimeDeps: ['ISearch', 'ISearchHandler'],

  linksAndHeadingsDoc: `
    <p>start</p>
    <a href='#a'>Home</a>
    <a href='#b'>About US</a>
    <p>
      <h1>Latest Breaking News</h1>
      <a href='foo'>See more...</a>
    </p>
    <a href='#bar'>Questions?</a>
    <h2>Privacy Policy</h2>
    <p>end<span>of test</span></p>
  `
};

/**
 * @implements {ISearchHandler}
 * @constructor
 */
function FakeISearchHandler(testObj) {
  this.test = testObj;
  this.expect_ = [];
}

FakeISearchHandler.prototype = {
  /** @override */
  onSearchReachedBoundary: function(boundaryNode) {
    this.expect_.shift()({node: boundaryNode, isBoundary: true});
  },

  /** @override */
  onSearchResultChanged: function(node, start, end) {
    this.expect_.shift()({node: node, start: start, end: end});
  },

  expect: function(str, opt_callback) {
    this.expect_.push(this.test.newCallback(function(args) {
      var node = args.node;
      var actual = node.name || node.role;
      if (args.start && args.end) {
        actual = 'start=' + args.start + ' end=' + args.end + ' text=' + actual;
      }
      if (args.isBoundary) {
        actual = 'boundary=' + actual;
      }
      assertEquals(str, actual);
      opt_callback && opt_callback();
    }));
  }
};

TEST_F('ChromeVoxISearchTest', 'Simple', function() {
  this.runWithLoadedTree(this.linksAndHeadingsDoc, function(rootNode) {
    var handler = new FakeISearchHandler(this);
    var search = new ISearch(new cursors.Cursor(rootNode, 0));
    search.handler = handler;

    // Simple forward search.
    search.search('US', 'forward');
    handler.expect(
        'start=6 end=8 text=About US',
        search.search.bind(search, 'start', 'backward'));

    handler.expect(
        'start',
        // Boundary (beginning).
        search.search.bind(search, 'foo', 'backward'));

    handler.expect(
        'boundary=start',
        // Boundary (end).
        search.search.bind(search, 'foo', 'forward'));

    // Search "focus" doesn't move.
    handler.expect(
        'boundary=start',
        // Mixed case substring.
        search.search.bind(search, 'bReak', 'forward'));

    handler.expect(
        'start=7 end=12 text=Latest Breaking News',
        search.search.bind(search, 'bReaki', 'forward'));

    // Incremental search stays on the current node.
    handler.expect(
        'start=7 end=13 text=Latest Breaking News',
        search.search.bind(search, 'bReakio', 'forward'));

    // No results for the search.
    handler.expect('boundary=Latest Breaking News');
  });
});
