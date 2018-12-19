// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const menu = {};

menu.testToggleToolbarMenu = async function(done) {
  const hello = '#file-list [file-name="hello.txt"]';
  const helloCheckmark = hello + ' .detail-checkmark';
  const selectionMenuButton = 'body.check-select #selection-menu-button';
  const menuToolbar = '#file-context-menu:not([hidden]).toolbar-menu';
  const menuNotToolbar = '#file-context-menu:not([hidden]):not(.toolbar-menu)';

  await test.setupAndWaitUntilReady();

  // Right-click hello.txt, menu is shown, not toolbar-menu.
  assertTrue(test.fakeMouseRightClick(hello));
  await test.waitForElement(menuNotToolbar);

  // Click on hello.txt checkmark, selection-menu-button replaces gear-button.
  assertTrue(test.fakeMouseClick(helloCheckmark));
  await test.waitForElement(selectionMenuButton);

  // Click selection-menu-button, verify toolbar-menu.
  assertTrue(test.fakeMouseClick(selectionMenuButton));
  await test.waitForElement(menuToolbar);

  // Right-click hello.txt, verify not toolbar-menu.
  assertTrue(test.fakeMouseRightClick(hello));
  await test.waitForElement(menuNotToolbar);

  done();
};
