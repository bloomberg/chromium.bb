/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 *  - assertInterpolation({property, from, to, [method]}, [{at: fraction, is: value}])
 *        Constructs a test case for each fraction that asserts the expected value
 *        equals the value produced by interpolation between from and to using
 *        CSS Animations, CSS Transitions and Web Animations. If the method option is
 *        specified then only that interpolation method will be used.
 *  - assertNoInterpolation({property, from, to, [method]})
 *        This works in the same way as assertInterpolation with expectations auto
 *        generated according to each interpolation method's handling of values
 *        that don't interpolate.
 *  - afterTest(callback)
 *        Calls callback after all the tests have executed.
 */
'use strict';
(function() {
  var interpolationTests = [];
  var cssAnimationsData = {
    sharedStyle: null,
    nextID: 0,
  };
  var webAnimationsEnabled = typeof Element.prototype.animate === 'function';
  var expectNoInterpolation = {};
  var afterTestHook = function() {};

  var cssAnimationsInterpolation = {
    name: 'CSS Animations',
    supports: function() {return true;},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      var id = cssAnimationsData.nextID++;
      if (!cssAnimationsData.sharedStyle) {
        cssAnimationsData.sharedStyle = createElement(document.body, 'style');
      }
      cssAnimationsData.sharedStyle.textContent += '' +
        '@keyframes animation' + id + ' {' +
          'from {' + property + ': ' + from + ';}' +
          'to {' + property + ': ' + to + ';}' +
        '}';
      target.style.animationName = 'animation' + id;
      target.style.animationDuration = '2e10s';
      target.style.animationDelay = '-1e10s';
      target.style.animationTimingFunction = createEasing(at);
    },
  };

  var cssTransitionsInterpolation = {
    name: 'CSS Transitions',
    supports: function() {return true;},
    setup: function(property, from, target) {
      target.style[property] = from;
    },
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, -Infinity);
    },
    interpolate: function(property, from, to, at, target) {
      target.style.transitionDuration = '2e10s';
      target.style.transitionDelay = '-1e10s';
      target.style.transitionTimingFunction = createEasing(at);
      target.style.transitionProperty = property;
      target.style[property] = to;
    },
  };

  var webAnimationsInterpolation = {
    name: 'Web Animations',
    supports: function(property) {return property.indexOf('-webkit-') != 0;},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      var keyframes = [{}, {}];
      keyframes[0][property] = from;
      keyframes[1][property] = to;
      var player = target.animate(keyframes, {
        fill: 'forwards',
        duration: 1,
        easing: createEasing(at),
        delay: -0.5,
        iterations: 0.5,
      });
    },
  };

  function expectFlip(from, to, flipAt) {
    return [-0.3, 0, 0.3, 0.5, 0.6, 1, 1.5].map(function(at) {
      return {
        at: at,
        is: at < flipAt ? from : to
      };
    });
  }

  // Constructs a timing function which produces 'y' at x = 0.5
  function createEasing(y) {
    // FIXME: if 'y' is > 0 and < 1 use a linear timing function and allow
    // 'x' to vary. Use a bezier only for values < 0 or > 1.
    if (y == 0) {
      return 'steps(1, end)';
    }
    if (y == 1) {
      return 'steps(1, start)';
    }
    if (y == 0.5) {
      return 'steps(2, end)';
    }
    // Approximate using a bezier.
    var b = (8 * y - 1) / 6;
    return 'cubic-bezier(0, ' + b + ', 1, ' + b + ')';
  }

  function createElement(parent, tag, text) {
    var element = document.createElement(tag || 'div');
    element.textContent = text || '';
    parent.appendChild(element);
    return element;
  }

  function loadScript(url) {
    return new Promise(function(resolve) {
      var script = document.createElement('script');
      script.src = url;
      script.onload = resolve;
      document.head.appendChild(script);
    });
  }

  function createTargetContainer(parent) {
    var targetContainer = createElement(parent);
    var template = document.querySelector('#target-template');
    if (template) {
      targetContainer.appendChild(template.content.cloneNode(true));
    }
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.classList.add('target');
    targetContainer.target = target;
    return targetContainer;
  }

  function roundNumbers(value) {
    return value.
        // Round numbers to two decimal places.
        replace(/-?\d*\.\d+(e-?\d+)?/g, function(n) {
          return (parseFloat(n).toFixed(2)).
              replace(/\.\d+/, function(m) {
                return m.replace(/0+$/, '');
              }).
              replace(/\.$/, '').
              replace(/^-0$/, '0');
        });
  }

  var anchor = document.createElement('a');
  function sanitizeUrls(value) {
    var matches = value.match(/url\([^\)]*\)/g);
    if (matches !== null) {
      for (var i = 0; i < matches.length; ++i) {
        var url = /url\(([^\)]*)\)/g.exec(matches[i])[1];
        anchor.href = url;
        anchor.pathname = '...' + anchor.pathname.substring(anchor.pathname.lastIndexOf('/'));
        value = value.replace(matches[i], 'url(' + anchor.href + ')');
      }
    }
    return value;
  }

  function normalizeValue(value) {
    return roundNumbers(sanitizeUrls(value)).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function assertNoInterpolation(options) {
    assertInterpolation(options, expectNoInterpolation);
  }

  function assertInterpolation(options, expectations) {
    console.assert(CSS.supports(options.property, options.from));
    console.assert(CSS.supports(options.property, options.to));
    interpolationTests.push({
      options: options,
      expectations: expectations,
    });
  }

  function createTestTargets(interpolationMethods, interpolationTests, container) {
    var targets = [];
    interpolationMethods.forEach(function(interpolationMethod) {
      var methodContainer = createElement(container);
      interpolationTests.forEach(function(interpolationTest) {
        var property = interpolationTest.options.property;
        if (!interpolationMethod.supports(property)) {
          return;
        }
        if (interpolationTest.options.method && interpolationTest.options.method != interpolationMethod.name) {
          return;
        }
        var from = interpolationTest.options.from;
        var to = interpolationTest.options.to;
        var testText = interpolationMethod.name + ': property <' + property + '> from [' + from + '] to [' + to + ']';
        var testContainer = createElement(methodContainer, 'div', testText);
        createElement(testContainer, 'br');
        var expectations = interpolationTest.expectations;
        if (expectations === expectNoInterpolation) {
          expectations = interpolationMethod.nonInterpolationExpectations(from, to);
        }
        expectations.forEach(function(expectation) {
          var actualTargetContainer = createTargetContainer(testContainer);
          var expectedTargetContainer = createTargetContainer(testContainer);
          expectedTargetContainer.target.classList.add('expected');
          expectedTargetContainer.target.style[property] = expectation.is;
          var target = actualTargetContainer.target;
          target.classList.add('actual');
          interpolationMethod.setup(property, from, target);
          target.interpolate = function() {
            interpolationMethod.interpolate(property, from, to, expectation.at, target);
          };
          target.measure = function() {
            var actualValue = getComputedStyle(target)[property];
            test(function() {
              assert_equals(
                normalizeValue(actualValue),
                normalizeValue(getComputedStyle(expectedTargetContainer.target)[property]));
            }, testText + ' at (' + expectation.at + ') is [' + sanitizeUrls(actualValue) + ']');
          };
          targets.push(target);
        });
      });
    });
    return targets;
  }

  function runInterpolationTests() {
    var interpolationMethods = [
      cssTransitionsInterpolation,
      cssAnimationsInterpolation,
    ];
    if (webAnimationsEnabled) {
      interpolationMethods.push(webAnimationsInterpolation);
    }
    var container = createElement(document.body);
    var targets = createTestTargets(interpolationMethods, interpolationTests, container);
    getComputedStyle(document.documentElement).left; // Force a style recalc for transitions.
    // Separate interpolation and measurement into different phases to avoid (targets.length) style recalcs.
    for (var target of targets) {
      target.interpolate();
    }
    for (var target of targets) {
      target.measure();
    }
    if (window.testRunner) {
      container.remove();
    }
    afterTestHook();
  }

  function afterTest(f) {
    afterTestHook = f;
  }

  window.assertInterpolation = assertInterpolation;
  window.assertNoInterpolation = assertNoInterpolation;
  window.afterTest = afterTest;

  loadScript('../../resources/testharness.js').then(function() {
    return loadScript('../../resources/testharnessreport.js');
  }).then(function() {
    var asyncHandle = async_test('This test uses interpolation-test.js.')
    requestAnimationFrame(function() {
      runInterpolationTests();
      asyncHandle.done()
    });
  });
})();
