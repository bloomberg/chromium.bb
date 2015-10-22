// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(function() {
  var testCount = 0;
  var animationEventCount = 0;

  var durationSeconds = 0.001;
  var iterationCount = 0.5;
  var delaySeconds = 0;
  var fragment = document.createDocumentFragment();
  var fragmentAttachedListeners = [];
  var style = document.createElement('style');
  fragment.appendChild(style);

  var smilTestsDiv = document.createElement('div');
  smilTestsDiv.id = 'smil-tests';
  smilTestsDiv.textContent = 'SVG SMIL:';
  fragment.appendChild(smilTestsDiv);

  var waTestsDiv = document.createElement('div');
  waTestsDiv.id = 'web-animations-tests';
  waTestsDiv.textContent = 'Web Animations API:';
  fragment.appendChild(waTestsDiv);

  var updateScheduled = false;
  function maybeScheduleUpdate() {
    if (updateScheduled) {
      return;
    }
    updateScheduled = true;
    setTimeout(function() {
      updateScheduled = false;
      document.body.appendChild(fragment);
      requestAnimationFrame(function () {
        fragmentAttachedListeners.forEach(function(listener) {listener();});
      });
    }, 0);
  }

  function smilResults() {
    var result = '\nSVG SMIL:\n';
    var targets = document.querySelectorAll('.target.smil');
    for (var i = 0; i < targets.length; i++) {
      result += targets[i].getResultString() + '\n';
    }
    return result;
  }

  function waResults() {
    var result = '\nWeb Animations API:\n';
    var targets = document.querySelectorAll('.target.web-animations');
    for (var i = 0; i < targets.length; i++) {
      result += targets[i].getResultString() + '\n';
    }
    return result;
  }

  function dumpResults() {
    var results = document.createElement('pre');
    results.id = 'results';
    results.textContent = smilResults() + waResults();
    document.body.appendChild(results);
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

  function createTestContainer(description, className) {
    var testContainer = document.createElement('div');
    testContainer.setAttribute('description', description);
    testContainer.classList.add('test');
    if (className) {
      testContainer.classList.add(className);
    }
    return testContainer;
  }

  function convertPropertyToCamelCase(property) {
    return property.replace(/^-/, '').replace(/-\w/g, function(m) {return m[1].toUpperCase();});
  }

  function describeSMILTest(params) {
    return '<animate /> ' + convertPropertyToCamelCase(params.property) + ': from [' + params.from + '] to [' + params.to + ']';
  }

  function describeWATest(params) {
    return 'element.animate() ' + convertPropertyToCamelCase(params.property) + ': from [' + params.from + '] to [' + params.to + ']';
  }

  var nextTestId = 0;
  function assertAttributeInterpolation(params, expectations) {
    var testId = 'test-' + ++nextTestId;
    var nextCaseId = 0;

    var smilTestContainer = createTestContainer(describeSMILTest(params), testId);
    smilTestsDiv.appendChild(smilTestContainer);
    expectations.forEach(function(expectation) {
      if (expectation.at < 0 || expectation.at > 1)
        return;
      smilTestContainer.appendChild(makeInterpolationTest(
          'smil', expectation.at, testId, 'case-' + ++nextCaseId, params, expectation.is));
    });

    var waTestContainer = createTestContainer(describeWATest(params), testId);
    waTestsDiv.appendChild(waTestContainer);
    expectations.forEach(function(expectation) {
      waTestContainer.appendChild(makeInterpolationTest(
          'web-animations', expectation.at, testId, 'case-' + ++nextCaseId, params, expectation.is));
    });

    maybeScheduleUpdate();
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

  function normalizeValue(value) {
    return roundNumbers(value).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function createTargetContainer(id) {
    var targetContainer = document.createElement('div');
    var template = document.querySelector('#target-template');
    if (template) {
      targetContainer.appendChild(template.content.cloneNode(true));
      // Remove whitespace text nodes at start / end.
      while (targetContainer.firstChild.nodeType != Node.ELEMENT_NODE && !/\S/.test(targetContainer.firstChild.nodeValue)) {
        targetContainer.removeChild(targetContainer.firstChild);
      }
      while (targetContainer.lastChild.nodeType != Node.ELEMENT_NODE && !/\S/.test(targetContainer.lastChild.nodeValue)) {
        targetContainer.removeChild(targetContainer.lastChild);
      }
      // If the template contains just one element, use that rather than a wrapper div.
      if (targetContainer.children.length == 1 && targetContainer.childNodes.length == 1) {
        targetContainer = targetContainer.firstChild;
        targetContainer.remove();
      }
    }
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.classList.add('target');
    target.classList.add(id);
    return targetContainer;
  }

  function sanitizeUrls(value) {
    var matches = value.match(/url\([^\)]*\)/g);
    if (matches !== null) {
      for (var i = 0; i < matches.length; ++i) {
        var url = /url\(([^\)]*)\)/g.exec(matches[i])[1];
        var anchor = document.createElement('a');
        anchor.href = url;
        anchor.pathname = '...' + anchor.pathname.substring(anchor.pathname.lastIndexOf('/'));
        value = value.replace(matches[i], 'url(' + anchor.href + ')');
      }
    }
    return value;
  }

  function serializeSVGLengthList(numberList) {
    var elements = [];
    for (var index = 0; index < numberList.numberOfItems; ++index)
      elements.push(numberList.getItem(index).value);
    return String(elements);
  }

  function serializeSVGNumberList(numberList) {
    var elements = [];
    for (var index = 0; index < numberList.numberOfItems; ++index)
      elements.push(numberList.getItem(index).value);
    return String(elements);
  }

  function serializeSVGPointList(pointList) {
    var elements = [];
    for (var index = 0; index < pointList.numberOfItems; ++index) {
      var point = pointList.getItem(index);
      elements.push(point.x);
      elements.push(point.y);
    }
    return String(elements);
  }

  function serializeSVGPreserveAspectRatio(preserveAspectRatio) {
    return String([preserveAspectRatio.align, preserveAspectRatio.meetOrSlice]);
  }

  function serializeSVGRect(rect) {
    return String([rect.x, rect.y, rect.width, rect.height]);
  }

  function serializeSVGTransformList(transformList) {
    var elements = [];
    for (var index = 0; index < transformList.numberOfItems; ++index) {
      var transform = transformList.getItem(index);
      elements.push(transform.type);
      elements.push(transform.angle);
      elements.push(transform.matrix.a);
      elements.push(transform.matrix.b);
      elements.push(transform.matrix.c);
      elements.push(transform.matrix.d);
      elements.push(transform.matrix.e);
      elements.push(transform.matrix.f);
    }
    return String(elements);
  }

  var svgNamespace = 'http://www.w3.org/2000/svg';
  var xlinkNamespace = 'http://www.w3.org/1999/xlink';

  var animatedNumberOptionalNumberAttributes = [
    'baseFrequency',
    'kernelUnitLength',
    'order',
    'radius',
    'stdDeviation',
  ];

  function namespacedAttributeName(attributeName) {
    if (attributeName === 'href')
      return 'xlink:href';
    return attributeName;
  }

  function getAttributeValue(element, attributeName) {
    if (animatedNumberOptionalNumberAttributes.indexOf(attributeName) !== -1)
      return getAttributeValue(element, attributeName + 'X') + ', ' + getAttributeValue(element, attributeName + 'Y');

    // The attribute 'class' is exposed in IDL as 'className'
    if (attributeName === 'class')
      attributeName = 'className';

    // The attribute 'in' is exposed in IDL as 'in1'
    if (attributeName === 'in')
      attributeName = 'in1';

    // The attribute 'orient' is exposed in IDL as 'orientType' and 'orientAngle'
    if (attributeName === 'orient') {
      if (element['orientType'] && element['orientType'].animVal === SVGMarkerElement.SVG_MARKER_ORIENT_AUTO)
        return 'auto';
      attributeName = 'orientAngle';
    }

    var result;
    if (attributeName === 'd')
      result = element.getAttribute('d');
    else if (attributeName === 'points')
      result = element['animatedPoints'];
    else
      result = element[attributeName].animVal;

    if (!result) {
      if (attributeName === 'pathLength')
        return '0';
      if (attributeName === 'preserveAlpha')
        return 'false';

      console.log('Unknown attribute, cannot get ' + element.className.baseVal + ' ' + attributeName);
      return null;
    }

    if (result instanceof SVGAngle || result instanceof SVGLength)
      result = result.value;
    else if (result instanceof SVGLengthList)
      result = serializeSVGLengthList(result);
    else if (result instanceof SVGNumberList)
      result = serializeSVGNumberList(result);
    else if (result instanceof SVGPointList)
      result = serializeSVGPointList(result);
    else if (result instanceof SVGPreserveAspectRatio)
      result = serializeSVGPreserveAspectRatio(result);
    else if (result instanceof SVGRect)
      result = serializeSVGRect(result);
    else if (result instanceof SVGTransformList)
      result = serializeSVGTransformList(result);

    if (typeof result !== 'string' && typeof result !== 'number' && typeof result !== 'boolean') {
      console.log('Attribute value has unexpected type: ' + result);
    }

    return String(result);
  }

  function setAttributeValue(element, attributeName, expectation) {
    if (!element[attributeName]
        && attributeName !== 'class'
        && attributeName !== 'd'
        && (attributeName !== 'in' || !element['in1'])
        && (attributeName !== 'orient' || !element['orientType'])
        && (animatedNumberOptionalNumberAttributes.indexOf(attributeName) === -1 || !element[attributeName + 'X'])) {
      console.log('Unknown attribute, cannot set ' + element.className.baseVal + ' ' + attributeName);
      return;
    }

    if (attributeName === 'class') {
      // Preserve the existing classes as we use them to select elements.
      expectation = element['className'].baseVal + ' ' + expectation;
    }

    if (attributeName.toLowerCase().indexOf('transform') === -1) {
      var setElement = document.createElementNS(svgNamespace, 'set');
      setElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
      setElement.setAttribute('attributeType', 'XML');
      setElement.setAttribute('to', expectation);
      element.appendChild(setElement);
    } else {
      element.setAttribute(attributeName, expectation);
    }
  }

  function makeKeyframes(target, attributeName, params) {
    // Replace 'transform' with 'svgTransform', etc. This avoids collisions with CSS properties or the Web Animations API (offset).
    attributeName = 'svg' + attributeName[0].toUpperCase() + attributeName.slice(1);

    var keyframes = [{}, {}];
    if (attributeName === 'svgClass') {
      // Preserve the existing classes as we use them to select elements.
      keyframes[0][attributeName] = target['className'].baseVal + ' ' + params.from;
      keyframes[1][attributeName] = target['className'].baseVal + ' ' + params.to;
    } else {
      keyframes[0][attributeName] = params.from;
      keyframes[1][attributeName] = params.to;
    }
    return keyframes;
  }

  function appendAnimate(target, attributeName, from, to)
  {
    var animateElement = document.createElementNS(svgNamespace, 'animate');
    animateElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
    animateElement.setAttribute('attributeType', 'XML');
    if (attributeName === 'class') {
      // Preserve the existing classes as we use them to select elements.
      animateElement.setAttribute('from', target['className'].baseVal + ' ' + from);
      animateElement.setAttribute('to', target['className'].baseVal + ' ' + to);
    } else {
      animateElement.setAttribute('from', from);
      animateElement.setAttribute('to', to);
    }
    animateElement.setAttribute('begin', '0');
    animateElement.setAttribute('dur', '1');
    animateElement.setAttribute('fill', 'freeze');
    target.appendChild(animateElement);
  }

  function appendAnimateTransform(target, attributeName, from, to)
  {
    var animateElement = document.createElementNS(svgNamespace, 'animate');
    animateElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
    animateElement.setAttribute('attributeType', 'XML');
    if (attributeName === 'class') {
      // Preserve the existing classes as we use them to select elements.
      animateElement.setAttribute('from', target['className'].baseVal + ' ' + from);
      animateElement.setAttribute('to', target['className'].baseVal + ' ' + to);
    } else {
      animateElement.setAttribute('from', from);
      animateElement.setAttribute('to', to);
    }
    animateElement.setAttribute('begin', '0');
    animateElement.setAttribute('dur', '1');
    animateElement.setAttribute('fill', 'freeze');
    target.appendChild(animateElement);
  }

  // Append animateTransform elements to parents of target.
  function appendAnimateTransform(target, attributeName, from, to)
  {
    var currentElement = target;
    var parents = [];
    from = from.split(')');
    to = to.split(')');
    // Discard empty string at end.
    from.pop();
    to.pop();

    // SMIL requires separate animateTransform elements for each transform in the list.
    if (from.length !== 1 || to.length !== 1)
      return;

    from = from[0].split('(');
    to = to[0].split('(');
    if (from[0].trim() !== to[0].trim())
        return;

    var animateTransformElement = document.createElementNS(svgNamespace, 'animateTransform');
    animateTransformElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
    animateTransformElement.setAttribute('attributeType', 'XML');
    animateTransformElement.setAttribute('type', from[0].trim());
    animateTransformElement.setAttribute('from', from[1]);
    animateTransformElement.setAttribute('to', to[1]);
    animateTransformElement.setAttribute('begin', '0');
    animateTransformElement.setAttribute('dur', '1');
    animateTransformElement.setAttribute('fill', 'freeze');
    target.appendChild(animateTransformElement);
  }

  function makeInterpolationTest(testType, fraction, testId, caseId, params, expectation) {
    var attributeName = convertPropertyToCamelCase(params.property);

    var targetContainer = createTargetContainer(caseId);
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.classList.add(testType);
    var replicaContainer, replica;
    {
      replicaContainer = createTargetContainer(caseId);
      replica = replicaContainer.querySelector('.target') || replicaContainer;
      replica.classList.add('replica');
      setAttributeValue(replica, params.property, expectation);
    }
    target.testType = testType;
    target.getResultString = function() {
      var value = getAttributeValue(this, params.property);
      var result = '';
      var reason = '';
      var property = convertPropertyToCamelCase(params.property);
      {
        var parsedExpectation = getAttributeValue(replica, params.property);
        if (attributeName === 'class') {
          parsedExpectation = parsedExpectation.replace('replica', testType);
        }
        // console.log('expected ' + parsedExpectation + ', actual ' + value);
        var pass = normalizeValue(value) === normalizeValue(parsedExpectation);
        result = pass ? 'PASS: ' : 'FAIL: ';
        reason = pass ? '' : ', expected [' + expectation + ']' +
            (expectation === parsedExpectation ? '' : ' (parsed as [' + sanitizeUrls(roundNumbers(parsedExpectation)) + '])');
        value = pass ? expectation : sanitizeUrls(value);
      }
      return result + property + ' from [' + params.from + '] to ' +
          '[' + params.to + '] was [' + value + ']' +
          ' at ' + fraction + reason;
    };
    var easing = createEasing(fraction);
    testCount++;
    {
      fragmentAttachedListeners.push(function() {
        if (testType === 'smil') {
          if (attributeName.toLowerCase().indexOf('transform') === -1)
            appendAnimate(target, attributeName, params.from, params.to);
          else
            appendAnimateTransform(target, attributeName, params.from, params.to);

          targetContainer.pauseAnimations();
          targetContainer.setCurrentTime(fraction);
        } else if (testType === 'web-animations' && target.animate) {
          target.animate(makeKeyframes(target, attributeName, params), {
              fill: 'forwards',
              duration: 1,
              easing: easing,
              delay: -0.5,
              iterations: 0.5,
            });
        }
        animationEnded();
      });
    }
    var testFragment = document.createDocumentFragment();
    testFragment.appendChild(targetContainer);
    testFragment.appendChild(replicaContainer);
    testFragment.appendChild(document.createTextNode('\n'));
    return testFragment;
  }

  var finished = false;
  function finishTest() {
    finished = true;
    dumpResults();
    if (window.testRunner) {
      var results = document.querySelector('#results');
      document.documentElement.textContent = '';
      document.documentElement.appendChild(results);
      testRunner.dumpAsText();

      testRunner.notifyDone();
    }
  }

  if (window.testRunner) {
    testRunner.waitUntilDone();
  }

  function isLastAnimationEvent() {
    return !finished && animationEventCount === testCount;
  }

  function animationEnded() {
    animationEventCount++;
    if (!isLastAnimationEvent()) {
      return;
    }
    requestAnimationFrame(finishTest);
  }

  if (!window.testRunner) {
    setTimeout(function() {
      if (finished) {
        return;
      }
      finishTest();
    }, 5000);
  }

  window.assertAttributeInterpolation = assertAttributeInterpolation;
})();
