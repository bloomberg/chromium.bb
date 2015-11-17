// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
Exported functions:
assertCSSResponsive
assertSVGResponsive

Options format: {
  ?targetTag: <Target element tag name>,
  property: <Property/Attribute name>,
  ?getter(target): <Reads animated value from target>,
  ?from: <Value>,
  ?to: <Value>,
  configurations: [{
    state: {
      ?underlying: <Value>,
      ?inherited: <CSS Value>,
    },
    expect: [
      { at: <Float>, is: <Value> },
    ],
  }],
}

Description:
assertCSSResponsive() and assertSVGResponsive() take a property
specific interpolation and a list of style configurations with interpolation
expectations that apply to each configuration.
It starts the interpolation in every configuration, changes the
state to every other configuration (n * (n - 1) complexity) and asserts that
each destination configuration's expectations are met.
Each animation target can be assigned custom styles via the ".target" selector.
This test is designed to catch stale interpolation caches.
*/

(function() {
'use strict';
var pendingResponsiveTests = [];
var htmlNamespace = 'http://www.w3.org/1999/xhtml';
var svgNamespace = 'http://www.w3.org/2000/svg';

function assertCSSResponsive(options) {
  pendingResponsiveTests.push({
    options,
    bindings: {
      prefixProperty(property) {
        return property;
      },
      createTargetContainer(container) {
        return createElement('div', container);
      },
      createTarget(container) {
        return createElement('div', container, 'target');
      },
      setValue(target, property, value) {
        target.style[property] = value;
      },
      getAnimatedValue(target, property) {
        return getComputedStyle(target)[property];
      },
    },
  });
}

function assertSVGResponsive(options) {
  pendingResponsiveTests.push({
    options,
    bindings: {
      prefixProperty(property) {
        return 'svg' + property[0].toUpperCase() + property.slice(1);
      },
      createTargetContainer(container) {
        var svgRoot = createElement('svg', container, 'svg-root', svgNamespace);
        svgRoot.setAttribute('width', 0);
        svgRoot.setAttribute('height', 0);
        return svgRoot;
      },
      createTarget(targetContainer) {
        console.assert(options.targetTag);
        return createElement(options.targetTag, targetContainer, 'target', svgNamespace);
      },
      setValue(target, property, value) {
        target.setAttribute(property, value);
      },
      getAnimatedValue(target, property) {
        return options.getter ? options.getter(target) : target[property].animVal;
      },
    },
  });
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

function createElement(tag, container, className, namespace) {
  var element = document.createElementNS(namespace || htmlNamespace, tag);
  if (container) {
    container.appendChild(element);
  }
  if (className) {
    element.classList.add(className);
  }
  return element;
}

function createTargets(bindings, n, container) {
  var targets = [];
  for (var i = 0; i < n; i++) {
    targets.push(bindings.createTarget(container));
  }
  return targets;
}

function setState(bindings, targets, property, state) {
  if (state.inherited) {
    var parent = targets[0].parentElement;
    console.assert(targets.every(function(target) { return target.parentElement === parent; }));
    bindings.setValue(parent, property, state.inherited);
  }
  if (state.underlying) {
    for (var target of targets) {
      bindings.setValue(target, property, state.underlying);
    }
  }
}

function keyframeText(options, keyframeName) {
  return (keyframeName in options) ? `[${options[keyframeName]}]` : 'neutral';
}

function createKeyframes(prefixedProperty, options) {
  var keyframes = [];
  if ('from' in options) {
    keyframes.push({
      offset: 0,
      [prefixedProperty]: options.from,
    });
  }
  if ('to' in options) {
    keyframes.push({
      offset: 1,
      [prefixedProperty]: options.to,
    });
  }
  return keyframes;
}

function startPausedAnimations(targets, keyframes, fractions) {
  console.assert(targets.length == fractions.length);
  for (var i = 0; i < targets.length; i++) {
    var target = targets[i];
    var fraction = fractions[i];
    console.assert(fraction >= 0 && fraction < 1);
    var animation = target.animate(keyframes, 1);
    animation.currentTime = fraction;
    animation.pause();
  }
}

function runPendingResponsiveTests() {
  return new Promise(function(resolve) {
    var stateTransitionTests = [];
    pendingResponsiveTests.forEach(function(responsiveTest) {
      var options = responsiveTest.options;
      var bindings = responsiveTest.bindings;
      var property = options.property;
      var prefixedProperty = bindings.prefixProperty(property);
      var from = options.from;
      var to = options.to;
      var keyframes = createKeyframes(prefixedProperty, options);
      var fromText = keyframeText(options, 'from');
      var toText = keyframeText(options, 'to');

      var stateTransitions = createStateTransitions(options.configurations);
      stateTransitions.forEach(function(stateTransition) {
        var before = stateTransition.before;
        var after = stateTransition.after;
        var container = bindings.createTargetContainer(document.body);
        var targets = createTargets(bindings, after.expect.length, container);

        setState(bindings, targets, property, before.state);
        startPausedAnimations(targets, keyframes, after.expect.map(function(expectation) { return expectation.at; }));
        stateTransitionTests.push({
          applyStateTransition() {
            setState(bindings, targets, property, after.state);
          },
          assert() {
            for (var i = 0; i < targets.length; i++) {
              var target = targets[i];
              var expectation = after.expect[i];
              var actual = bindings.getAnimatedValue(target, property);
              test(function() {
                assert_equals(actual, expectation.is);
              }, `Animation on property <${prefixedProperty}> from ${fromText} to ${toText} with ${JSON.stringify(before.state)} changed to ${JSON.stringify(after.state)} at (${expectation.at}) is [${expectation.is}]`);
            }
          },
        });
      });
    });

    for (var stateTransitionTest of stateTransitionTests) {
      stateTransitionTest.applyStateTransition();
    }

    requestAnimationFrame(function() {
      for (var stateTransitionTest of stateTransitionTests) {
        stateTransitionTest.assert();
      }
      resolve();
    });
  });
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
  runPendingResponsiveTests().then(function() {
    asyncHandle.done();
  });
});


window.assertCSSResponsive = assertCSSResponsive;
window.assertSVGResponsive = assertSVGResponsive;

})();
