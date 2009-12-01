/**
 * @fileoverview Poor-man's unittests for some of the trickier logic bits of
 * the flakiness dashboard.
 *
 * Currently only tests processExpectations and populateExpectationsData.
 * A test just consists of calling runExpectationsTest with the appropriate
 * arguments.
 */

// TODO(ojan): Add more test cases.
// TODO(ojan): Add tests for processMissingAndExtraExpectations

/**
 * Clears out the global objects modified or used by processExpectations and
 * populateExpectationsData. A bit gross since it's digging into implementation
 * details, but it's good enough for now.
 */
function setupExpectationsTest() {
  allExpectations = null;
  expectationsByTest = {};
  resultsByBuilder = {};
}

/**
 * Processes the expectations for a test and asserts that the final expectations
 * and modifiers we apply to a test match what we expect.
 *
 * @param {string} builder Builder the test is run on.
 * @param {string} test The test name.
 * @param {Array} expectationsArray Array of test expectations. This should be
 *    in the same format as in expectations.json as output by
 *    run_webkit_tests.py and on the buildbots.
 * @param {Object} results Object listing the results for this test on the bot.
 *    This should be in the same format as in expectations.json as output by
 *    run_webkit_tests.py and on the buildbots.
 * @param {string} expectations Sorted string of what the expectations for this
 *    test ought to be for this builder.
 * @param {string} modifiers Sorted string of what the modifiers for this
 *    test ought to be for this builder.
 */
function runExpectationsTest(builder, test, expectationsArray, results,
    expectations, modifiers) {
  // Setup global dashboard state.
  builders = {};
  builders[builder] = true;
  expectationsByTest[test] = expectationsArray;
  resultsByBuilder[builder] = results;

  processExpectations();
  var resultsForTest = createResultsObjectForTest(test, builder);
  populateExpectationsData(resultsForTest);

  assertEquals(resultsForTest.expectations, expectations);
  assertEquals(resultsForTest.modifiers, modifiers);
}

function assertEquals(actual, expected) {
  if (expected !== actual) {
    throw Error('Got: ' + actual + ' expected: ' + expected);
  }
}

function testReleaseFail() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectatiosnArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'}
  ];
  var results = {'tests': {
    'foo/1.html': {
      'results': [[100, 'F']],
      'times': [[100, 0]]
  }}};
  runExpectationsTest(builder, test, expectatiosnArray, results, 'FAIL',
      'RELEASE');
}

function testReleaseFailDebugCrashReleaseBuilder() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectatiosnArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  var results = {'tests': {
    'foo/1.html': {
      'results': [[100, 'F']],
      'times': [[100, 0]]
  }}};
  runExpectationsTest(builder, test, expectatiosnArray, results, 'FAIL',
      'RELEASE');
}

function testReleaseFailDebugCrashDebugBuilder() {
  var builder = 'Webkit(dbg)';
  var test = 'foo/1.html';
  var expectatiosnArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  var results = {'tests': {
    'foo/1.html': {
      'results': [[100, 'F']],
      'times': [[100, 0]]
  }}};
  runExpectationsTest(builder, test, expectatiosnArray, results,  'CRASH',
      'DEBUG');
}

function runTests() {
  document.body.innerHTML = '<pre id=unittest-results></pre>';
  for (var name in window) {
    if (typeof window[name] == 'function' && /^test/.test(name)) {
      setupExpectationsTest();

      var test = window[name];
      var error = null;

      try {
        test();
      } catch (err) {
        error = err;
      }

      var result = error ? error.toString() : 'PASSED';
      $('unittest-results').insertAdjacentHTML("beforeEnd",
          name + ': ' + result + '\n');
    }
  }
}

if (document.readyState == 'complete') {
  runTests();
} else {
  window.addEventListener('load', runTests, false);
}
