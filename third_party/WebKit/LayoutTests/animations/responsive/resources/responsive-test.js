// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
Exported function:
assertResponsive

Call signature:
assertResponsive({
  property: <CSS Property>,
  from: ?<CSS Value>,
  to: ?<CSS Value>,
  configurations: [{
    state: {
      ?underlying: <CSS Value>,
      ?inherited: <CSS Value>,
    },
    expect: [
      { at: <Float>, is: <CSS Value> }
    ],
  }],
})

Description:
assertResponsive takes a property specific interpolation and a list of style
configurations with interpolation expectations that apply to each
configuration.
It starts the interpolation in every configuration, changes the
state to every other configuration (n * (n - 1) complexity) and asserts that
each destination configuration's expectations are met.
Each animation target can be assigned custom styles via the ".target" selector.
This test is designed to catch stale interpolation caches.
*/

(function() {
'use strict';
var sharedStyle = null;
var animationCount = 0;
var pendingResponsiveTests = [];

function assertResponsive(options) {
  pendingResponsiveTests.push(options);
}

function createStateTransitions(configurations) {
  var stateTransitions = [];
  for (var i = 0; i < configurations.length; i++) {
    var beforeConfiguration = configurations[i];
    for (var j = 0; j < configurations.length; j++) {
      var afterConfiguration = configurations[j];
      if (j != i) {
        stateTransitions.push({
          before: beforeConfiguration,
          after: afterConfiguration,
        });
      }
    }
  }
  return stateTransitions;
}

function createElement(tag, container, className) {
  var element = document.createElement(tag);
  if (container) {
    container.appendChild(element);
  }
  if (className) {
    element.classList.add(className);
  }
  return element;
}

function createTargets(n, container) {
  var targets = [];
  for (var i = 0; i < n; i++) {
    targets.push(createElement('div', container, 'target'));
  }
  return targets;
}

function setState(targets, property, state) {
  if (state.inherited) {
    var parent = targets[0].parentElement;
    console.assert(targets.every(function(target) { return target.parentElement === parent; }));
    parent.style[property] = state.inherited;
  }
  if (state.underlying) {
    for (var target of targets) {
      target.style[property] = state.underlying;
    }
  }
}

function createAnimationName() {
  return 'anim' + (animationCount++);
}

function createKeyframes(animationName, property, from, to) {
  return `
@keyframes ${animationName} {
  from { ${property}: ${from}; }
  to { ${property}: ${to}; }
}`;
}

function addGlobalStyle(styleText) {
  if (!sharedStyle) {
    sharedStyle = createElement('style', document.documentElement);
  }
  sharedStyle.textContent += styleText;
}

function startPausedAnimations(targets, animationName, fractions) {
  console.assert(targets.length == fractions.length);
  for (var i = 0; i < targets.length; i++) {
    var target = targets[i];
    var fraction = fractions[i];
    console.assert(fraction >= 0 && fraction <= 1);
    target.style.animation = `${animationName} 1s linear both paused`;
    target.style.animationDelay = `${-fraction}s`;
  }
}

function runPendingResponsiveTests() {
  var stateTransitionTests = [];
  pendingResponsiveTests.forEach(function(options) {
    var property = options.property;
    var from = options.from;
    var to = options.to;
    var animationName = createAnimationName();
    addGlobalStyle(createKeyframes(animationName, property, from, to));

    var stateTransitions = createStateTransitions(options.configurations);
    stateTransitions.forEach(function(stateTransition) {
      var before = stateTransition.before;
      var after = stateTransition.after;
      var container = createElement('div', document.body);
      var targets = createTargets(after.expect.length, container);

      setState(targets, property, before.state);
      startPausedAnimations(targets, animationName, after.expect.map(function(expectation) { return expectation.at; }));
      stateTransitionTests.push({
        applyStateTransition() {
          setState(targets, property, after.state);
        },
        assert() {
          for (var i = 0; i < targets.length; i++) {
            var target = targets[i];
            var expectation = after.expect[i];
            var actual = getComputedStyle(target)[property];
            test(function() {
              assert_equals(actual, expectation.is);
            }, `Animation on property <${property}> from [${from}] to [${to}] with ${JSON.stringify(before.state)} changed to ${JSON.stringify(after.state)} at (${expectation.at}) is [${expectation.is}]`);
          }
        },
      });
    });
  });

  // Force style recalc to instantiate animations internally.
  getComputedStyle(document.body).color;

  // Separate style modification from measurement as different phases to avoid a style recalc storm.
  for (var stateTransitionTest of stateTransitionTests) {
    stateTransitionTest.applyStateTransition();
  }
  for (var stateTransitionTest of stateTransitionTests) {
    stateTransitionTest.assert();
  }
}

function loadScript(url) {
  return new Promise(function(resolve) {
    var script = document.createElement('script');
    script.src = url;
    script.onload = resolve;
    document.head.appendChild(script);
  });
}

loadScript('../../resources/testharness.js').then(function() {
  return loadScript('../../resources/testharnessreport.js');
}).then(function() {
  var asyncHandle = async_test('This test uses responsive-test.js.')
  requestAnimationFrame(function() {
    runPendingResponsiveTests();
    asyncHandle.done()
  });
});


window.assertResponsive = assertResponsive;

})();
