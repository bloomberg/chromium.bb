/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This script is intended to be used for constructing layout tests which
 * exercise the interpolation functionaltiy of the animation system.
 * Tests which run using this script should be portable across browsers.
 *
 * The following functions are exported:
 *  * runAsRefTest - indicates that the test is a ref test and disables
 *    dumping of textual output.
 *  * testInterpolationAt([timeFractions], {property: x, from: y, to: z})
 *    Constructs a test case for the interpolation of property x from
 *    value y to value z at each of the times in timeFractions.
 *  * assertInterpolation({property: x, from: y, to: z}, [{at: fraction, is: value}])
 *    Constructs a test case which for each fraction will output a PASS
 *    or FAIL depending on whether the interpolated result matches
 *    'value'. Replica elements are constructed to aid eyeballing test
 *    results. This function may not be used in a ref test.
 *  * convertToReference - This is intended to be used interactively to
 *    construct a reference given the results of a test. To build a
 *    reference, run the test, open the inspector and trigger this
 *    function, then copy/paste the results.
 */
'use strict';
(function() {
  var prefix = 'webkitAnimation' in document.createElement('div').style ? '-webkit-' : '';
  var isRefTest = false;
  var startEvent = prefix ? 'webkitAnimationStart' : 'animationstart';
  var endEvent = prefix ? 'webkitAnimationEnd' : 'animationend';
  var testCount = 0;
  var finishedCount = 0;
  var startedCount = 0;
  // FIXME: This should be 0, but 0 duration animations are broken in at least
  // pre-Web-Animations Blink, WebKit and Gecko.
  var durationSeconds = 0.1;
  var iterationCount = 0.5;
  var cssText = '';
  var fragment = document.createDocumentFragment();
  var style = document.createElement('style');
  var afterTestCallback = null;
  fragment.appendChild(style);

  var updateScheduled = false;
  function maybeScheduleUpdate() {
    if (updateScheduled) {
      return;
    }
    updateScheduled = true;
    setTimeout(function() {
      updateScheduled = false;
      style.innerHTML = cssText;
      document.body.appendChild(fragment);
    }, 0);
  }

  function dumpResults() {
    var targets = document.querySelectorAll('.target.active');
    if (isRefTest) {
      // Convert back to reference to avoid cases where the computed style is
      // out of sync with the compositor.
      for (var i = 0; i < targets.length; i++) {
        targets[i].convertToReference();
      }
      style.parentNode.removeChild(style);
    } else {
      var resultString = '';
      for (var i = 0; i < targets.length; i++) {
        resultString += targets[i].getResultString() + '\n';
      }
      var results = document.createElement('div');
      results.style.whiteSpace = 'pre';
      results.textContent = resultString;
      document.body.appendChild(results);
    }
  }

  function convertToReference() {
    console.assert(isRefTest);
    var scripts = document.querySelectorAll('script');
    for (var i = 0; i < scripts.length; i++) {
      scripts[i].parentNode.removeChild(scripts[i]);
    }
    style.parentNode.removeChild(style);
    var html = document.documentElement.outerHTML;
    document.documentElement.style.whiteSpace = 'pre';
    document.documentElement.textContent = html;
  }

  function afterTest(callback) {
    afterTestCallback = callback;
  }

  function runAsRefTest() {
    console.assert(!isRefTest);
    isRefTest = true;
  }

  // Constructs a cubic-bezier timing function which produces 'y' at x = 0.5
  function createEasing(y) {
      var b = (8 * y - 1) / 6;
      return 'cubic-bezier(0, ' + b + ', 1, ' + b + ')';
  }

  function testInterpolationAt(fractions, params) {
    if (!Array.isArray(fractions)) {
      fractions = [fractions];
    }
    assertInterpolation(params, fractions.map(function(fraction) {
      return {at: fraction};
    }));
  }

  function assertInterpolation(params, expectations) {
    var keyframeId = defineKeyframes(params);
    var nextTestId = 0;
    expectations.forEach(function(expectation) {
      makeInterpolationTest(
          expectation.at, keyframeId, ++nextTestId, params, expectation.is);
    });
    maybeScheduleUpdate();
  }

  var nextKeyframeId = 0;
  function defineKeyframes(params) {
    var id = 'test-' + ++nextKeyframeId;
    cssText += '@' + prefix + 'keyframes ' + id + ' { \n' +
        '  0% { ' + params.property + ': ' + params.from + '; }\n' +
        '  100% { ' + params.property + ': ' + params.to + '; }\n' +
        '}\n';
    return id;
  }

  function normalizeValue(value) {
    return value.
        // Round numbers to two decimal places.
        replace(/\.\d+/g, function(n) {
          return ('.' + Math.round(parseFloat(n, 10) * 100)).
              replace(/\.?0*$/, '');
        }).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function createTarget(id) {
    var target = document.createElement('div');
    target.classList.add(id);
    var template = document.querySelector('#target-template');
    if (template) {
      target.appendChild(template.content.cloneNode(true));
    }
    target.classList.add('target');
    return target;
  }

  function makeInterpolationTest(fraction, keyframeId, testId, params, expectation) {
    console.assert(expectation === undefined || !isRefTest);
    // If the prefixed property is not supported, try to unprefix it.
    if (/^-[^-]+-/.test(params.property) && !CSS.supports(params.property, expectation)) {
      var unprefixed = params.property.replace(/^-[^-]+-/, '');
      if (CSS.supports(unprefixed, expectation)) {
        params.property = unprefixed;
      }
    }
    var id = keyframeId + '-' + testId;
    var target = createTarget(id);
    target.classList.add('active');
    var replica;
    if (expectation !== undefined) {
      replica = createTarget(id);
      replica.classList.add('replica');
      replica.style.setProperty(params.property, expectation);
    }
    target.getResultString = function() {
      if (!CSS.supports(params.property, expectation)) {
        return 'FAIL: [' + params.property + ': ' + expectation + '] is not supported';
      }
      var value = getComputedStyle(this).getPropertyValue(params.property);
      var result = '';
      var reason = '';
      if (expectation !== undefined) {
        var parsedExpectation = getComputedStyle(replica).getPropertyValue(params.property);
        var pass = normalizeValue(value) === normalizeValue(parsedExpectation);
        result = pass ? 'PASS: ' : 'FAIL: ';
        reason = pass ? '' : ', expected [' + expectation + ']';
        value = pass ? expectation : value;
      }
      return result + params.property + ' from [' + params.from + '] to ' +
          '[' + params.to + '] was [' + value + ']' +
          ' at ' + fraction + reason;
    };
    target.convertToReference = function() {
      this.style[params.property] = getComputedStyle(this).getPropertyValue(params.property);
    };
    var easing = createEasing(fraction);
    cssText += '.' + id + '.active {\n' +
        '  ' + prefix + 'animation: ' + keyframeId + ' ' + durationSeconds + 's forwards;\n' +
        '  ' + prefix + 'animation-timing-function: ' + easing + ';\n' +
        '  ' + prefix + 'animation-iteration-count: ' + iterationCount + ';\n' +
        '}\n';
    testCount++;
    fragment.appendChild(target);
    replica && fragment.appendChild(replica);
    fragment.appendChild(document.createTextNode('\n'));
  }

  var finished = false;
  function finishTest() {
    finished = true;
    dumpResults();
    if (afterTestCallback) {
      afterTestCallback();
    }
    if (window.testRunner) {
      if (!isRefTest) {
        testRunner.dumpAsText();
      }
      testRunner.notifyDone();
    }
  }

  if (window.testRunner) {
    testRunner.waitUntilDone();
  }

  if (window.internals && !internals.runtimeFlags.webAnimationsCSSEnabled) {
    // FIXME: Once http://crbug.com/279039 is fixed we can use the logic below
    // that just waits for all of the animations to finish.
    durationSeconds = 1000;
    iterationCount = 1;
    document.documentElement.addEventListener(startEvent, function() {
      if (finished || ++startedCount != testCount) {
        return;
      }
      internals.pauseAnimations(durationSeconds / 2);
      finishTest();
    });
  } else {
    if (window.internals && internals.runtimeFlags.webAnimationsCSSEnabled) {
      durationSeconds = 0;
    }
    document.documentElement.addEventListener(endEvent, function() {
      if (finished || ++finishedCount != testCount) {
        return;
      }
      finishTest();
    });
  }

  if (!window.testRunner) {
    setTimeout(function() {
      if (finished) {
        return;
      }
      finishTest();
    }, 10000);
  }

  window.runAsRefTest = runAsRefTest;
  window.testInterpolationAt = testInterpolationAt;
  window.assertInterpolation = assertInterpolation;
  window.convertToReference = convertToReference;
  window.afterTest = afterTest;
})();
