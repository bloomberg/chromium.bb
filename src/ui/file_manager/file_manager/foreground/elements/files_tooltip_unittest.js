// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {Element} */
let chocolateButton;

/** @type {Element} */
let cherriesButton;

/** @type {Element} */
let cheeseButton;

/** @type {Element} */
let otherButton;

/** @type {FilesTooltip|Element} */
let tooltip;

const bodyContent = `
  <style type="text/css">
   button {
     display: flex;
     height: 32px;
     margin: 30px;
     width: 32px;
   }

   #container {
     display: flex;
     justify-content: space-between;
   }

   files-tooltip {
     background: yellow;
     box-sizing: border-box;
     position: absolute;
     text-align: center;
     width: 100px;
   }
  </style>

  <!-- Targets for tooltip testing. -->
  <div id="container">
    <button id="chocolate" aria-label="Chocolate!"></button>
    <button id="cherries" aria-label="Cherries!"></button>
  </div>

  <button id="cheese" aria-label="Cheese!" show-card-tooltip></button>

  <!-- Button without a tooltip. -->
  <button id="other"></button>

  <!-- Polymer files tooltip element. -->
  <files-tooltip></files-tooltip>
`;

function setUp() {
  /** @const {boolean} Assume files-ng in unittest. */
  const enableFilesNg = true;

  // Mock LoadTimeData strings for files-ng case. These tests check top and
  // left position values, which can differ in files-ng vs not-files-ng.
  window.loadTimeData.data = {
    FILES_NG_ENABLED: enableFilesNg,
  };

  window.loadTimeData.getString = id => {
    return window.loadTimeData.data_[id] || id;
  };

  /** @return {boolean} */
  window.isFilesNg = () => {
    return enableFilesNg;
  };

  document.body.innerHTML = bodyContent;
  chocolateButton = document.querySelector('#chocolate');
  cherriesButton = document.querySelector('#cherries');
  cheeseButton = document.querySelector('#cheese');
  otherButton = document.querySelector('#other');

  tooltip = document.querySelector('files-tooltip');
  assertNotEqual('none', window.getComputedStyle(tooltip).display);
  assertEquals('0', window.getComputedStyle(tooltip).opacity);
  assertEquals(enableFilesNg, tooltip.hasAttribute('files-ng'));

  tooltip.addTargets([chocolateButton, cherriesButton, cheeseButton]);
}

function waitForMutation(target) {
  return new Promise((fulfill, reject) => {
    const observer = new MutationObserver(mutations => {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

function testFocus(callback) {
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            assertEquals('4px', tooltip.style.left);

            if (window.isFilesNg()) {
              assertEquals('78px', tooltip.style.top);
            } else {
              assertEquals('70px', tooltip.style.top);
            }

            cherriesButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cherries!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft =
                document.body.offsetWidth - tooltip.offsetWidth + 'px';
            assertEquals(expectedLeft, tooltip.style.left);

            if (window.isFilesNg()) {
              assertEquals('78px', tooltip.style.top);
            } else {
              assertEquals('70px', tooltip.style.top);
            }

            otherButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

function testHover(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('4px', tooltip.style.left);
            if (window.isFilesNg()) {
              assertEquals('78px', tooltip.style.top);
            } else {
              assertEquals('70px', tooltip.style.top);
            }

            chocolateButton.dispatchEvent(new MouseEvent('mouseout'));
            cherriesButton.dispatchEvent(new MouseEvent('mouseover'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cherries!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft =
                document.body.offsetWidth - tooltip.offsetWidth + 'px';
            assertEquals(expectedLeft, tooltip.style.left);

            if (window.isFilesNg()) {
              assertEquals('78px', tooltip.style.top);
            } else {
              assertEquals('70px', tooltip.style.top);
            }

            cherriesButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

function testClickHides(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            // Hiding here is synchronous. Dispatch the event asynchronously,
            // so the mutation observer is started before hiding.
            setTimeout(() => {
              document.body.dispatchEvent(new MouseEvent('mousedown'));
            });
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'true');
          }),
      callback);
}

function testCardTooltipHover(callback) {
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
            assertEquals('card-label', label.className);

            assertEquals('38px', tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

function testCardtooltipRTL(callback) {
  document.documentElement.setAttribute('dir', 'rtl');
  document.body.setAttribute('dir', 'rtl');

  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
            assertEquals('card-label', label.className);

            assertEquals('962px', tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            // revert back document direction to not impact other tests.
            document.documentElement.setAttribute('dir', 'ltr');
            document.body.setAttribute('dir', 'ltr');
          }),
      callback);
}