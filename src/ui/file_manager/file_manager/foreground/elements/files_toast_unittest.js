// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setUpPage() {
  const importElements = (src) => {
    const link = document.createElement('link');
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
    document.body.innerHTML += '<files-toast></files-toast>';
    window.waitUser = false;
  };

  importElements(
      'third_party/polymer/v1_0/components-chromium/polymer/polymer.html');
  importElements(
      'ui/file_manager/file_manager/foreground/elements/files_toast.html');

  // Make the test harness pause until the test page is fully loaded.
  window.waitUser = true;
}

async function testToast(done) {
  /** @type {FilesToast|Element} */
  const toast = document.querySelector('files-toast');
  const text = document.querySelector('files-toast #text');
  const action = document.querySelector('files-toast #action');
  const waitFor = async f => {
    while (!f()) {
      await new Promise(r => setTimeout(r, 0));
    }
  };

  // Toast is hidden to start.
  assertFalse(toast.visible);

  // Show toast1, verify visible, text and action text.
  let a1Called = false;
  toast.show('t1', {
    text: 'a1',
    callback: () => {
      a1Called = true;
    }
  });
  assertTrue(toast.visible);
  assertEquals('t1', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a1', action.innerText);

  // Queue up toast2 and toast3, should still be showing toast1.
  let a2Called = false;
  toast.show('t2', {
    text: 'a2',
    callback: () => {
      a2Called = true;
    }
  });
  toast.show('t3');
  assertEquals('t1', text.innerText);

  // Invoke toast1 action, callback will be called,
  // and toast2 will show after animation.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a1Called);
  await waitFor(() => text.innerText === 't2');
  assertTrue(toast.visible);
  assertEquals('t2', text.innerText);
  assertFalse(action.hidden);
  assertEquals('a2', action.innerText);

  // Invoke toast2 action, callback will be called,
  // and toast3 will show after animation with no action.
  action.dispatchEvent(new MouseEvent('click'));
  assertTrue(a2Called);
  await waitFor(() => text.innerText === 't3');
  assertTrue(toast.visible);
  assertEquals('t3', text.innerText);
  assertTrue(action.hidden);

  // Call hide(), toast should no longer be visible, no more toasts shown.
  toast.hide();
  await waitFor(() => !toast.visible);

  done();
}
