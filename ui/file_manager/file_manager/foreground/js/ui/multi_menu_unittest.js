// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {cr.ui.MultiMenuButton} */
let menubutton;

/** @type {cr.ui.Menu} */
let topMenu;

/** @type {cr.ui.Menu} */
let subMenu;

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
    '    height: 10px;',
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
    '  <cr-menu-item id="host-sub-menu" command="#show-submenu"',
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
  topMenu = util.queryDecoratedElement('#menu', cr.ui.Menu);
  subMenu = util.queryDecoratedElement('#sub-menu', cr.ui.Menu);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 */
function sendMouseOver(targetQuery) {
  const event = new MouseEvent('mouseover', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Send a 'mouseover' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 * @param {number} x Position of the event in 'X'.
 * @param {number} y Position of the event in 'X'.
 */
function sendMouseOut(targetQuery, x, y) {
  const event = new MouseEvent('mouseout', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    clientX: x,
    clientY: y,
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 * Tests that making the top level menu visible doesn't
 * cause the sub-menu to become visible.
 */
function testShowMenuDoesntShowSubMenu() {
  menubutton.showMenu(true);
  // Check the top level menu is not hidden.
  assertFalse(topMenu.hasAttribute('hidden'));
  // Check the sub-menu is hidden
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that a 'mouseover' event on top of normal menu-items
 * doesn't cause the sub-menu to become visible.
 */
function testMouseOverNormalItemsDoesntShowSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#default-task');
  assertTrue(subMenu.hasAttribute('hidden'));
  sendMouseOver('#more-actions');
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseover' on a menu-item with 'show-submenu' command
 * causes the sub-menu to become visible.
 */
function testMouseOverHostMenuShowsSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the top level
 * menu causes the sub-menu to hide.
 */
function testMouseoutFromHostMenuItemToHostMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
  // Get the location of one of our menu-items to send with the event.
  const item = document.querySelector('#default-task');
  const loc = item.getBoundingClientRect();
  sendMouseOut('#host-sub-menu', loc.left, loc.top);
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that 'mouseout' with the mouse over the sub-menu
 * doesn't hide the sub-menu.
 */
function testMouseoutFromHostMenuToSubMenu() {
  menubutton.showMenu(true);
  sendMouseOver('#host-sub-menu');
  assertFalse(subMenu.hasAttribute('hidden'));
  // Get the location of our sub-menu to send with the event.
  const loc = subMenu.getBoundingClientRect();
  sendMouseOut('#host-sub-menu', loc.left, loc.top);
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that selecting a menu-item with a 'show-submenu' command
 * doesn't cause the sub-menu to become visible.
 */
function testSelectHostMenuItem() {
  menubutton.showMenu(true);
  topMenu.selectedIndex = 2;
  const hostItem = document.querySelector('#host-sub-menu');
  assert(hostItem.hasAttribute('selected'));
  assertEquals(hostItem.getAttribute('selected'), 'selected');
  assertTrue(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that selecting a menu-item with a 'show-submenu' command
 * followed by calling the showSubMenu() method causes the
 * sub-menu to become visible.
 * (Note: in an application, this would happen from a command
 * being executed rather than a direct showSubMenu() call.)
 */
function testSelectHostMenuItemAndCallShowSubMenu() {
  testSelectHostMenuItem();
  menubutton.showSubMenu();
  assertFalse(subMenu.hasAttribute('hidden'));
}

/**
 * Tests that a mouse click outside of a menu and sub-menu causes
 * both menus to hide.
 */
function testClickOutsideVisibleMenuAndSubMenu() {
  testSelectHostMenuItemAndCallShowSubMenu();
  const event = new MouseEvent('mousedown', {
    bubbles: true,
    cancelable: true,
    view: window,
    composed: true,  // Allow the event to bubble past shadow DOM root.
    clientX: 0,      // 0, 0 is in the padding area of the viewport
    clientY: 0,
  });
  menubutton.dispatchEvent(event);
  assertTrue(topMenu.hasAttribute('hidden'));
  assertTrue(subMenu.hasAttribute('hidden'));
}
