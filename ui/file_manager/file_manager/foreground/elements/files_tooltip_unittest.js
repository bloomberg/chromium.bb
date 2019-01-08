// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {Array<string>} */
const testPage = [
  '<style type="text/css">',
  ' button {',
  '   display: flex;',
  '   height: 32px;',
  '   margin: 30px;',
  '   width: 32px;',
  ' }',
  '',
  ' #container {',
  '   display: flex;',
  '   justify-content: space-between;',
  ' }',
  '',
  ' files-tooltip {',
  '   background: yellow;',
  '   box-sizing: border-box;',
  '   position: absolute;',
  '   text-align: center;',
  '   width: 100px;',
  ' }',
  '</style>',
  '',
  '<!-- Targets for tooltip testing. -->',
  '<div id="container">',
  '  <button id="chocolate" aria-label="Chocolate!"></button>',
  '  <button id="cherries" aria-label="Cherries!"></button>',
  '</div>',
  '',
  '<!-- Button without a tooltip. -->',
  '<button id="other"></button>',
  '',
  '<!-- Polymer files tooltip element. -->',
  '<files-tooltip></files-tooltip>',
  '',
];

/** @type {Element} */
var chocolateButton;

/** @type {Element} */
var cherriesButton;

/** @type {Element} */
var otherButton;

/** @type {FilesTooltip|Element} */
var tooltip;

function setUpPage() {
  console.log('setUpPage');

  const importElements = (src) => {
    var link = document.createElement('link');
    link.rel = 'import';
    link.onload = onLinkLoaded;
    document.head.appendChild(link);
    const sourceRoot = '../../../../../../../../';
    link.href = sourceRoot + src;
  };

  let linksLoaded = 0;

  const onLinkLoaded = () => {
    if (++linksLoaded < 2) {
      return;
    }
    document.body.innerHTML += testPage.join('\n');
    window.waitUser = false;
  };

  const polymer =
      'third_party/polymer/v1_0/components-chromium/polymer/polymer.html';
  importElements(polymer);

  const filesTooltip =
      'ui/file_manager/file_manager/foreground/elements/files_tooltip.html';
  importElements(filesTooltip);

  // Make the test harness pause until out test page is fully loaded.
  window.waitUser = true;
}

function setUp() {
  chocolateButton = document.querySelector('#chocolate');
  cherriesButton = document.querySelector('#cherries');
  otherButton = document.querySelector('#other');

  tooltip = document.querySelector('files-tooltip');
  tooltip.addTargets([chocolateButton, cherriesButton]);
}

function waitForMutation(target) {
  return new Promise(function(fulfill, reject) {
    var observer = new MutationObserver(function(mutations) {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

function testFocus(callback) {
  chocolateButton.focus();

  return reportPromise(
    waitForMutation(tooltip).then(function() {
      assertEquals('Chocolate!', tooltip.textContent.trim());
      assertTrue(!!tooltip.getAttribute('visible'));
      assertEquals('4px', tooltip.style.left);
      assertEquals('70px', tooltip.style.top);

      cherriesButton.focus();
      return waitForMutation(tooltip);
    }).then(function() {
      assertEquals('Cherries!', tooltip.textContent.trim());
      assertTrue(!!tooltip.getAttribute('visible'));
      var expectedLeft = document.body.offsetWidth - tooltip.offsetWidth + 'px';
      assertEquals(expectedLeft, tooltip.style.left);
      assertEquals('70px', tooltip.style.top);

      otherButton.focus();
      return waitForMutation(tooltip);
    }).then(function() {
      assertFalse(!!tooltip.getAttribute('visible'));
    }), callback);
}

function testHover(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
    waitForMutation(tooltip).then(function() {
      assertEquals('Chocolate!', tooltip.textContent.trim());
      assertTrue(!!tooltip.getAttribute('visible'));
      assertEquals('4px', tooltip.style.left);
      assertEquals('70px', tooltip.style.top);

      chocolateButton.dispatchEvent(new MouseEvent('mouseout'));
      cherriesButton.dispatchEvent(new MouseEvent('mouseover'));
      return waitForMutation(tooltip);
    }).then(function() {
      assertEquals('Cherries!', tooltip.textContent.trim());
      assertTrue(!!tooltip.getAttribute('visible'));
      var expectedLeft = document.body.offsetWidth - tooltip.offsetWidth + 'px';
      assertEquals(expectedLeft, tooltip.style.left);
      assertEquals('70px', tooltip.style.top);

      cherriesButton.dispatchEvent(new MouseEvent('mouseout'));
      return waitForMutation(tooltip);
    }).then(function() {
      assertFalse(!!tooltip.getAttribute('visible'));
    }), callback);
}

function testClickHides(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

  return reportPromise(
    waitForMutation(tooltip).then(function() {
      assertEquals('Chocolate!', tooltip.textContent.trim());
      assertTrue(!!tooltip.getAttribute('visible'));

      // Hiding here is synchronous. Dispatch the event asynchronously, so the
      // mutation observer is started before hiding.
      setTimeout(function() {
        document.body.dispatchEvent(new MouseEvent('mousedown'));
      });
      return waitForMutation(tooltip);
    }).then(function() {
      assertFalse(!!tooltip.getAttribute('visible'));
    }), callback);
}
