// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarksEditDialogElement, normalizeNode} from 'chrome://bookmarks/bookmarks.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createFolder, createItem, replaceBody} from './test_util.js';

suite('<bookmarks-edit-dialog>', function() {
  let dialog: BookmarksEditDialogElement;
  let lastUpdate: {id: string, edit: {url?: string, title?: string}};
  let lastCreation: chrome.bookmarks.CreateDetails;

  suiteSetup(function() {
    chrome.bookmarks.update = function(id, edit) {
      lastUpdate.id = id;
      lastUpdate.edit = edit;
    };
    chrome.bookmarks.create = function(node) {
      lastCreation = node;
    };
  });

  setup(function() {
    lastUpdate = {id: '', edit: {}};
    lastCreation = {};
    dialog = document.createElement('bookmarks-edit-dialog');
    replaceBody(dialog);
  });

  test('editing an item shows the url field', function() {
    const item = normalizeNode(createItem('0'));
    dialog.showEditDialog(item);

    assertFalse(dialog.$.url.hidden);
  });

  test('editing a folder hides the url field', function() {
    const folder = normalizeNode(createFolder('0', []));
    dialog.showEditDialog(folder);

    assertTrue(dialog.$.url.hidden);
  });

  test('adding a folder hides the url field', function() {
    dialog.showAddDialog(true, '1');
    assertTrue(dialog.$.url.hidden);
  });

  test('editing passes the correct details to the update', function() {
    // Editing an item without changing anything.
    const item = normalizeNode(
        createItem('1', {url: 'http://website.com', title: 'website'}));
    dialog.showEditDialog(item);

    dialog.$.saveButton.click();

    assertEquals(item.id, lastUpdate.id);
    assertEquals(item.url, lastUpdate.edit.url);
    assertEquals(item.title, lastUpdate.edit.title);

    // Editing a folder, changing the title.
    const folder = normalizeNode(createFolder('2', [], {title: 'Cool Sites'}));
    dialog.showEditDialog(folder);
    dialog.$.name.value = 'Awesome websites';

    dialog.$.saveButton.click();

    assertEquals(folder.id, lastUpdate.id);
    assertEquals(undefined, lastUpdate.edit.url);
    assertEquals('Awesome websites', lastUpdate.edit.title);
  });

  test('add passes the correct details to the backend', function() {
    dialog.showAddDialog(false, '1');

    dialog.$.name.value = 'Permission Site';
    dialog.$.url.value = 'permission.site';
    flush();

    dialog.$.saveButton.click();

    assertEquals('1', lastCreation.parentId);
    assertEquals('http://permission.site', lastCreation.url);
    assertEquals('Permission Site', lastCreation.title);
  });

  test('validates urls correctly', function() {
    dialog.$.url.value = 'http://www.example.com';
    assertTrue(dialog.validateUrl());

    dialog.$.url.value = 'https://a@example.com:8080';
    assertTrue(dialog.validateUrl());

    dialog.$.url.value = 'example.com';
    flush();
    assertTrue(dialog.validateUrl());
    flush();
    assertEquals('http://example.com', dialog.$.url.value);

    dialog.$.url.value = '';
    assertFalse(dialog.validateUrl());

    dialog.$.url.value = '~~~example.com~~~';
    assertFalse(dialog.validateUrl());
  });

  test('doesn\'t save when URL is invalid', function() {
    const item = normalizeNode(createItem('0'));
    dialog.showEditDialog(item);

    dialog.$.url.value = '';

    flush();
    dialog.$.saveButton.click();
    flush();

    assertTrue(dialog.$.url.invalid);
    assertTrue(dialog.$.dialog.open);
  });
});
