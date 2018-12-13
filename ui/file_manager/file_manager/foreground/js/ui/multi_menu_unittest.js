// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var menubutton;

// Set up test components.
function setUp() {
  // Install cr.ui <command> elements and <cr-menu>s on the page.
  document.body.innerHTML = [
    '<style>',
    '  cr-menu {',
    '    position: fixed;',
    '  }',
    '  cr-menu-item {',
    '    width: 10px;',
    '    height: 10px',
    '    display: block;',
    '  }',
    '</style>',
    '<command id="default-task">',
    '<command id="more-actions">',
    '<command id="show-submenu">',
    '<button id="test-menu-button" menu="#menu"></button>',
    '<cr-menu id="menu" hidden>',
    '  <cr-menu-item command="#default-task"></cr-menu-item>',
    '  <cr-menu-item command="#more-actions"></cr-menu-item>',
    '  <cr-menu-item command="#show-submenu"',
    'visibleif="full-page" class="hide-on-toolbar"',
    'sub-menu="#sub-menu" hidden></cr-menu-item>',
    '</cr-menu>',
    '<cr-menu id="sub-menu" hidden>',
    '  <cr-menu-item class="custom-appearance"></cr-menu-item>',
    '</cr-menu>',
  ].join('');

  // Initialize cr.ui.Command with the <command>s.
  cr.ui.decorate('command', cr.ui.Command);
  menubutton =
      util.queryDecoratedElement('#test-menu-button', cr.ui.MultiMenuButton);
}

/**
 *  Tests that making the top level menu visible doesn't
 *  cause the sub-menu to become visible.
 */
function testShowMenuDoesntShowSubMenu() {
  menubutton.showMenu(true);
  const subMenu = document.querySelector('#sub-menu');
  assertTrue(subMenu.hasAttribute('hidden'));
}
