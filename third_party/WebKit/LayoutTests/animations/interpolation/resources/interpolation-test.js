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
 *    value y to value z at each of the times in timeFractions. For each
 *    time fraction a div is constructed with the 'target' class and an
 *    animation which freezes at the time fraction is scheduled.
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

  function dumpResults() {
    var targets = document.querySelectorAll('.target');
    if (isRefTest) {
      // Convert back to reference to avoid cases where the computed style is
      // out of sync with the compositor.
      for (var i = 0; i < targets.length; i++) {
        targets[i].convertToReference();
      }
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
    var html = document.documentElement.outerHTML;
    document.documentElement.style.whiteSpace = 'pre';
    document.documentElement.textContent = html;
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
    fractions.map(function(fraction) {
      makeInterpolationTest(fraction, params.property, params.from, params.to);
    });
  }

  var keyframeId = 0;
  function makeInterpolationTest(fraction, property, from, to) {
    var easing = createEasing(fraction);
    var id = 'test' + keyframeId++;
    var target = document.createElement('div');
    var template = document.querySelector('#target-template');
    if (template) {
      target.appendChild(template.content.cloneNode(true));
    }
    var style = document.createElement('style');
    target.classList.add('target');
    target.getResultString = function() {
        return property + ' from [' + from + '] to ' +
            '[' + to + '] was [' + getComputedStyle(this)[property] + ']' +
            ' at ' + fraction;
    };
    target.convertToReference = function() {
        this.style[property] = getComputedStyle(this)[property];
        this.removeChild(style);
    };
    target.id = id;
    style.innerHTML = '@' + prefix + 'keyframes ' + id + '{\n' +
        '  0% { ' + property + ': ' + from + '; }\n' +
        '  100% { ' + property + ': ' + to + '; }\n' +
        '}\n' +
        '#' + id + ' {\n' +
        '  ' + prefix + 'animation: ' + id + ' ' + durationSeconds + 's forwards;\n' +
        '  ' + prefix + 'animation-timing-function: ' + easing + ';\n' +
        '  ' + prefix + 'animation-iteration-count: ' + iterationCount + ';\n' +
        '}';
    target.appendChild(style);
    testCount++;
    document.body.appendChild(target);
    document.body.appendChild(document.createTextNode('\n'));
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
      if (++startedCount != testCount) {
        return;
      }
      internals.pauseAnimations(durationSeconds / 2);
      dumpResults();
      if (window.testRunner) {
        if (!isRefTest) {
          testRunner.dumpAsText();
        }
        testRunner.notifyDone();
      }
    });
  } else {
    if (window.internals && internals.runtimeFlags.webAnimationsCSSEnabled) {
      durationSeconds = 0;
    }
    document.documentElement.addEventListener(endEvent, function() {
      if (++finishedCount != testCount) {
        return;
      }
      dumpResults();
      if (window.testRunner) {
        if (!isRefTest) {
          testRunner.dumpAsText();
        }
        testRunner.notifyDone();
      }
    });
  }

  window.runAsRefTest = runAsRefTest;
  window.testInterpolationAt = testInterpolationAt;
  window.convertToReference = convertToReference;
})();
