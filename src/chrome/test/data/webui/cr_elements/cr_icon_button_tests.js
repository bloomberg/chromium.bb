// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-icon-button', function() {
  let button;

  setup(async () => {
    PolymerTest.clearBody();
    button = document.createElement('cr-icon-button');
    document.body.appendChild(button);
    await PolymerTest.flushTasks();
  });

  test('enabled/disabled', () => {
    assertEquals('0', button.getAttribute('tabindex'));
    assertEquals('false', button.getAttribute('aria-disabled'));
    button.disabled = true;
    assertEquals('-1', button.getAttribute('tabindex'));
    assertEquals('true', button.getAttribute('aria-disabled'));
  });

  test('iron-icon is created, reused and removed based on |ironIcon|', () => {
    assertFalse(!!button.$$('iron-icon'));
    button.ironIcon = 'icon-key';
    assertTrue(!!button.$$('iron-icon'));
    button.$$('iron-icon').icon = 'icon-key';
    button.ironIcon = 'another-icon-key';
    assertEquals(1, button.shadowRoot.querySelectorAll('iron-icon').length);
    button.$$('iron-icon').icon = 'another-icon-key';
    button.ironIcon = '';
    assertFalse(!!button.$$('iron-icon'));
  });

  test('enter emits click event', async () => {
    const wait = test_util.eventToPromise('click', button);
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    await wait;
  });

  test('space emits click event', async () => {
    const wait = test_util.eventToPromise('click', button);
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
    await wait;
  });

  test('disabled prevents UI and programmatic clicks', async () => {
    let clickCount = 0;
    const clickHandler = () => {
      clickCount++;
    };
    button.addEventListener('click', clickHandler);

    button.disabled = true;
    await PolymerTest.flushTasks();
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
    MockInteractions.downAndUp(button);
    button.click();
    await PolymerTest.flushTasks();
    assertEquals(0, clickCount);

    button.disabled = false;
    await PolymerTest.flushTasks();
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], 'Enter');
    MockInteractions.pressAndReleaseKeyOn(button, -1, [], ' ');
    MockInteractions.downAndUp(button);
    button.click();
    await PolymerTest.flushTasks();
    assertEquals(4, clickCount);
    button.removeEventListener('click', clickHandler);
  });

  test('when tabindex is -1, it stays -1', async () => {
    document.body.innerHTML = '<cr-icon-button tabindex="-1"></cr-icon-button>';
    await PolymerTest.flushTasks();
    button = document.body.querySelector('cr-icon-button');
    assertEquals('-1', button.getAttribute('tabindex'));
  });
});
