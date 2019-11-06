// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {cr.ui.Menu} */
let contextMenu;

/** @type {!ListContainer} */
let listContainer;

/** @type {!FileTable} */
let table;

/** @type {!FileGrid} */
let grid;

// Set up test components.
function setUp() {
  // Install cr.ui <command> elements and <cr-menu>s on the page.
  document.body.innerHTML = [
    '<style>',
    '  .hide {',
    '    display: none;',
    '  }',
    '</style>',
    '<command id="default-task">',
    '<cr-menu id="file-context-menu" hidden>',
    '  <cr-menu-item id="item" command="#default-task"></cr-menu-item>',
    '</cr-menu>',
    '<div id="list-container">',
    '  <div id="detail-table">',
    '    <list id="file-list" contextmenu="#file-context-menu">',
    '    </list>',
    '  </div>',
    '  <grid id="file-grid" contextmenu="#file-context-menu" hidden>',
    '  </grid>',
    '  <paper-progress class="loading-indicator" hidden></paper-progress>',
    '</div>',
  ].join('');

  // Initialize cr.ui.Command with the <command>s.
  cr.ui.decorate('command', cr.ui.Command);
  // Setup the listContainer and its dependencies
  contextMenu = util.queryDecoratedElement('#file-context-menu', cr.ui.Menu);
  table = /** @type {!FileTable} */
      (queryRequiredElement('#detail-table', undefined));
  table.list = document.querySelector('#file-list');
  grid = /** @type {!FileGrid} */
      (queryRequiredElement('#file-grid', undefined));
  listContainer = new ListContainer(
      queryRequiredElement('#list-container', undefined), table, grid);
  // Hook up enough ListContainer internals to handle touch tests
  table.startBatchUpdates = table.endBatchUpdates = () => {};
  table.setListThumbnailLoader = () => {};
  grid.startBatchUpdates = grid.endBatchUpdates = () => {};
  grid.setListThumbnailLoader = () => {};
  listContainer.dataModel = /** @type {!FileListModel} */ ({});
  listContainer.selectionModel = new cr.ui.ListSelectionModel();
  listContainer.setCurrentListType(ListContainer.ListType.DETAIL);

  cr.ui.contextMenuHandler.setContextMenu(table, contextMenu);
}

/**
 * Send a 'contextmenu' event to the element target of a query.
 * @param {string} targetQuery Query to specify the element.
 */
function sendContextMenu(targetQuery) {
  const event = new MouseEvent('contextmenu', {
    bubbles: true,
    composed: true,  // Allow the event to bubble past shadow DOM root.
  });
  const target = document.querySelector(targetQuery);
  assertTrue(!!target);
  return target.dispatchEvent(event);
}

/**
 *  Tests that sending a 'contextmenu' event will show the menu
 *  if it contains cr-menu-item(s) that are actionable.
 */
function testShowMenuWithActionsOpensContextMenu() {
  sendContextMenu('#file-list');
  assertFalse(contextMenu.hasAttribute('hidden'));
}

/**
 *  Tests that sending a 'contextmenu' event will hide the menu
 *  if it doesn't contain cr-menu-item(s) that are actionable.
 */
function testShowMenuWithNoActionsHidesContextMenu() {
  const menuItem = document.querySelector('#item');

  menuItem.setAttribute('hidden', '');
  sendContextMenu('#file-list');
  assertTrue(contextMenu.hasAttribute('hidden'));
  menuItem.removeAttribute('hidden');

  menuItem.setAttribute('disabled', 'disabled');
  sendContextMenu('#file-list');
  assertTrue(contextMenu.hasAttribute('hidden'));
  menuItem.removeAttribute('disabled');

  menuItem.setAttribute('class', 'hide');
  sendContextMenu('#file-list');
  assertTrue(contextMenu.hasAttribute('hidden'));
}
