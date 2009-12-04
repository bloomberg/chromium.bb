/**
 * @fileoverview Base JS file for pages that want to parse the results JSON
 * from the testing bots. This deals with generic utility functions, visible
 * history, popups and appending the script elements for the JSON files.
 *
 * The calling page is expected to implement three "abstract" functions/objects.
 * generatePage, validateHashParameter and defaultStateValues.
 */
var pageLoadStartTime = Date.now();

/**
 * Generates the contents of the dashboard. The page should override this with
 * a function that generates the page assuming all resources have loaded.
 */
function generatePage() {
}

/**
 * Takes a key and a value and sets the currentState[key] = value iff key is
 * a valid hash parameter and the value is a valid value for that key.
 *
 * @return {boolean} Whether the key what inserted into the currentState.
 */
function handleValidHashParameter(key, value) {
  return false;
}

/**
 * Default hash parameters for the page. The page should override this to create
 * default states.
 */
var defaultStateValues = {};


//////////////////////////////////////////////////////////////////////////////
// CONSTANTS
//////////////////////////////////////////////////////////////////////////////
var EXPECTATIONS_MAP = {
  'T': 'TIMEOUT',
  'C': 'CRASH',
  'P': 'PASS',
  'F': 'TEXT',
  'I': 'IMAGE',
  'Z': 'IMAGE+TEXT',
  // We used to glob a bunch of expectations into "O" as OTHER. Expectations
  // are more precise now though and it just means MISSING.
  'O': 'MISSING',
  'N': 'NO DATA',
  'X': 'SKIP'
};

// Keys in the JSON files.
var WONTFIX_COUNTS_KEY = 'wontfixCounts';
var FIXABLE_COUNTS_KEY = 'fixableCounts';
var DEFERRED_COUNTS_KEY = 'deferredCounts';
var WONTFIX_DESCRIPTION = 'Tests never to be fixed (WONTFIX)';
var FIXABLE_DESCRIPTION = 'All tests for this release';
var DEFERRED_DESCRIPTION = 'All deferred tests (DEFER)';
var FIXABLE_COUNT_KEY = 'fixableCount';
var ALL_FIXABLE_COUNT_KEY = 'allFixableCount';
var CHROME_REVISIONS_KEY = 'chromeRevision';
var WEBKIT_REVISIONS_KEY = 'webkitRevision';

/**
 * @return {boolean} Whether the value represents a failing result.
 */
function isFailingResult(value) {
  return 'FSTOCIZ'.indexOf(value) != -1;
}


/**
 * Takes a key and a value and sets the currentState[key] = value iff key is
 * a valid hash parameter and the value is a valid value for that key. Handles
 * cross-dashboard parameters then falls back to calling
 * handleValidHashParameter for dashboard-specific parameters.
 *
 * @return {boolean} Whether the key what inserted into the currentState.
 */
function handleValidHashParameterWrapper(key, value) {
  switch(key) {
    case 'testType':
      validateParameter(currentState, key, value,
          function() {
            return isValidName(value);
          });

      return true;

    case 'useTestData':
    case 'useV8Canary':
    case 'useWebKitCanary':
      currentState[key] = value == 'true';
      return true;

    case 'buildDir':
      currentState['testType'] = 'layout-test-results';
      if (value === 'Debug' || value == 'Release') {
        currentState[key] = value;
        return true;
      } else {
        return false;
      }

    default:
      return handleValidHashParameter(key, value);
  }
}

var defaultCrossDashboardStateValues = {
  testType: 'layout_test_results',
  useWebKitCanary: false,
  useV8Canary: false,
  buildDir : '',
}

// Generic utility functions.
function $(id) {
  return document.getElementById(id);
}

function stringContains(a, b) {
  return a.indexOf(b) != -1;
}

function caseInsensitiveContains(a, b) {
  return a.match(new RegExp(b, 'i'));
}

function startsWith(a, b) {
  return a.indexOf(b) == 0;
}

function endsWidth(a, b) {
  return a.lastIndexOf(b) == a.length - b.length;
}

function isValidName(str) {
  return str.match(/[A-Za-z0-9\-\_,]/);
}

function trimString(str) {
  return str.replace(/^\s+|\s+$/g, '');
}

function validateParameter(state, key, value, validateFn) {
  if (validateFn()) {
    state[key] = value;
  } else {
    console.log(key + ' value is not valid: ' + value);
  }
}

/**
 * Parses window.location.hash and set the currentState values appropriately.
 */
function parseParameters(parameterStr) {
  currentState = {};

  var params = window.location.hash.substring(1).split('&');
  var invalidKeys = [];
  for (var i = 0; i < params.length; i++) {
    var thisParam = params[i].split('=');
    if (thisParam.length != 2) {
      console.log('Invalid query parameter: ' + params[i]);
      continue;
    }

    var key = thisParam[0];
    var value = decodeURIComponent(thisParam[1]);
    if (!handleValidHashParameterWrapper(key, value))
      invalidKeys.push(key + '=' + value);
  }

  if (invalidKeys.length)
    console.log("Invalid query parameters: " + invalidKeys.join(','));

  fillMissingValues(currentState, defaultCrossDashboardStateValues);
  fillMissingValues(currentState, defaultStateValues);
}

function getDefaultValue(key) {
  if (key in defaultStateValues)
    return defaultStateValues[key];
  return defaultCrossDashboardStateValues[key];
}

function fillMissingValues(to, from) {
  for (var state in from) {
    if (!(state in to))
      to[state] = from[state];
  }
}

function appendScript(path) {
  var script = document.createElement('script');
  script.src = path;
  document.getElementsByTagName('head')[0].appendChild(script);
}

// Permalinkable state of the page.
var currentState;

// Parse cross-dashboard parameters before using them.
parseParameters();

var builders, builderBase;
if (currentState.buildDir) {
  // If buildDir is set, point to the results.json and expectations.json in the
  // local tree. Useful for debugging changes to the python JSON generator.
  builders = {'DUMMY_BUILDER_NAME': ''};
  var loc = document.location.toString();
  var offset = loc.indexOf('webkit/');
  builderBase = loc.substr(loc, offset + 7) + currentState.buildDir + "/";
} else {
  // Map of builderName (the name shown in the waterfall)
  // to builderPath (the path used in the builder's URL)
  // TODO(ojan): Make this switch based off of the testType.
  if (currentState.useWebKitCanary) {
    builders = {
      'Webkit (webkit.org)': 'webkit-rel-webkit-org',
      'Webkit Linux (webkit.org)': 'webkit-rel-linux-webkit-org',
      'Webkit Mac (webkit.org)': 'webkit-rel-mac-webkit-org'
    };
  } else if (currentState.useV8Canary) {
    builders = {
      'Webkit (V8-Latest)': 'webkit-rel-v8',
      'Webkit Mac (V8-Latest)': 'webkit-rel-mac-v8',
      'Webkit Linux (V8-Latest)': 'webkit-rel-linux-v8'
    }
  } else {
    builders = {
      'Webkit': 'webkit-rel',
      'Webkit (dbg)(1)': 'webkit-dbg-1',
      'Webkit (dbg)(2)': 'webkit-dbg-2',
      'Webkit (dbg)(3)': 'webkit-dbg-3',
      'Webkit Linux': 'webkit-rel-linux',
      'Webkit Linux (dbg)(1)': 'webkit-dbg-linux-1',
      'Webkit Linux (dbg)(2)': 'webkit-dbg-linux-2',
      'Webkit Linux (dbg)(3)': 'webkit-dbg-linux-3',
      'Webkit Mac10.5': 'webkit-rel-mac5',
      'Webkit Mac10.5 (dbg)(1)': 'webkit-dbg-mac5-1',
      'Webkit Mac10.5 (dbg)(2)': 'webkit-dbg-mac5-2',
      'Webkit Mac10.5 (dbg)(3)': 'webkit-dbg-mac5-3'
    };
  }
  builderBase = 'http://build.chromium.org/buildbot/';
}

// Append JSON script elements.
var resultsByBuilder = {};
var expectationsByTest = {};
function ADD_RESULTS(builds) {
  for (var builderName in builds) {
    if (builderName != 'version')
      resultsByBuilder[builderName] = builds[builderName];
  }

  handleResourceLoad();
}

function getPathToBuilderResultsFile(builderName) {
  return builderBase + currentState['testType'] + '/' +
      builders[builderName] + '/';
}

var expectationsLoaded = false;
function ADD_EXPECTATIONS(expectations) {
  expectationsLoaded = true;
  expectationsByTest = expectations;
  handleResourceLoad();
}

function appendJSONScriptElements() {
  parseParameters();

  if (currentState.useTestData) {
    appendScript('dashboards/flakiness_dashboard_tests.js');
    return;
  }

  for (var builderName in builders) {
    appendScript(getPathToBuilderResultsFile(builderName) + 'results.json');
  }

  // Grab expectations file from any builder.
  appendScript(getPathToBuilderResultsFile(builderName) + 'expectations.json');
}

var hasDoneInitialPageGeneration = false;

function handleResourceLoad() {
  // In case we load a results.json that's not in the list of builders,
  // make sure to only call handleLocationChange once from the resource loads.
  if (!hasDoneInitialPageGeneration)
    handleLocationChange();
}

function handleLocationChange() {
  if (!expectationsLoaded)
    return;

  for (var build in builders) {
    if (!resultsByBuilder[build])
      return;
  }

  hasDoneInitialPageGeneration = true;
  parseParameters();
  generatePage();
}

window.onhashchange = handleLocationChange;

/**
 * Sets the page state. Takes varargs of key, value pairs.
 */
function setQueryParameter(var_args) {
  for (var i = 0; i < arguments.length; i += 2) {
    currentState[arguments[i]] = arguments[i + 1];
  }
  window.location.replace(getPermaLinkURL());
}

function getPermaLinkURL() {
  return window.location.pathname + '#' + joinParameters(currentState);
}

function joinParameters(stateObject) {
  var state = [];
  for (var key in stateObject) {
    var value = stateObject[key];
    if (value != getDefaultValue(key))
      state.push(key + '=' + encodeURIComponent(value));
  }
  return state.join('&');
}

function logTime(msg, startTime) {
  console.log(msg + ': ' + (Date.now() - startTime));
}

function hidePopup() {
  var popup = $('popup');
  if (popup)
    popup.parentNode.removeChild(popup);
}

function showPopup(e, html) {
  var popup = $('popup');
  if (!popup) {
    popup = document.createElement('div');
    popup.id = 'popup';
    document.body.appendChild(popup);
  }

  // Set html first so that we can get accurate size metrics on the popup.
  popup.innerHTML = html;

  var targetRect = e.target.getBoundingClientRect();

  var x = Math.min(targetRect.left - 10,
      document.documentElement.clientWidth - popup.offsetWidth);
  x = Math.max(0, x);
  popup.style.left = x + document.body.scrollLeft + 'px';

  var y = targetRect.top + targetRect.height;
  if (y + popup.offsetHeight > document.documentElement.clientHeight) {
    y = targetRect.top - popup.offsetHeight;
  }
  y = Math.max(0, y);
  popup.style.top = y + document.body.scrollTop + 'px';
}

appendJSONScriptElements();

document.addEventListener('mousedown', function(e) {
      // Clear the open popup, unless the click was inside the popup.
      var popup = $('popup');
      if (popup && e.target != popup &&
          !(popup.compareDocumentPosition(e.target) & 16)) {
        hidePopup();
      }
    }, false);

window.addEventListener('load', function() {
      // This doesn't seem totally accurate as there is a race between
      // onload firing and the last script tag being executed.
      logTime('Time to load JS', pageLoadStartTime);
    }, false);
