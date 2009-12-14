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
  allTests = null;
  expectationsByTest = {};
  resultsByBuilder = {};
  builders = {};
}

/**
 * Processes the expectations for a test and asserts that the final expectations
 * and modifiers we apply to a test match what we expect.
 *
 * @param {string} builder Builder the test is run on.
 * @param {string} test The test name.
 * @param {string} expectations Sorted string of what the expectations for this
 *    test ought to be for this builder.
 * @param {string} modifiers Sorted string of what the modifiers for this
 *    test ought to be for this builder.
 */
function runExpectationsTest(builder, test, expectations, modifiers) {
  builders[builder] = true;

  // Put in some dummy results. processExpectations expects the test to be
  // there.
  var tests = {};
  tests[test] = {'results': [[100, 'F']], 'times': [[100, 0]]};
  resultsByBuilder[builder] = {'tests': tests};

  processExpectations();
  var resultsForTest = createResultsObjectForTest(test, builder);
  populateExpectationsData(resultsForTest);

  assertEquals(resultsForTest, resultsForTest.expectations, expectations);
  assertEquals(resultsForTest, resultsForTest.modifiers, modifiers);
}

function assertEquals(resultsForTest, actual, expected) {
  if (expected !== actual) {
    throw Error('Builder: ' + resultsForTest.builder + ' test: ' +
        resultsForTest.test + ' got: ' + actual + ' expected: ' + expected);
  }
}

function throwError(resultsForTests, actual, expected) {
}

function testReleaseFail() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'}
  ];
  expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashReleaseBuilder() {
  var builder = 'Webkit';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'FAIL', 'RELEASE');
}

function testReleaseFailDebugCrashDebugBuilder() {
  var builder = 'Webkit(dbg)';
  var test = 'foo/1.html';
  var expectationsArray = [
    {'modifiers': 'RELEASE', 'expectations': 'FAIL'},
    {'modifiers': 'DEBUG', 'expectations': 'CRASH'}
  ];
  expectationsByTest[test] = expectationsArray;
  runExpectationsTest(builder, test, 'CRASH', 'DEBUG');
}

function testOverrideJustBuildType() {
  var test = 'bar/1.html';
  expectationsByTest['bar'] = [
    {'modifiers': 'WONTFIX', 'expectations': 'FAIL PASS TIMEOUT'}
  ];
  expectationsByTest[test] = [
    {'modifiers': 'WONTFIX MAC', 'expectations': 'FAIL'},
    {'modifiers': 'LINUX DEBUG', 'expectations': 'CRASH'},
  ];
  runExpectationsTest('Webkit', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit (dbg)(3)', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit Linux', test, 'FAIL PASS TIMEOUT', 'WONTFIX');
  runExpectationsTest('Webkit Linux (dbg)(3)', test, 'CRASH', 'LINUX DEBUG');
  runExpectationsTest('Webkit Mac10.5', test, 'FAIL', 'WONTFIX MAC');
  runExpectationsTest('Webkit Mac10.5 (dbg)(3)', test, 'FAIL', 'WONTFIX MAC');
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
