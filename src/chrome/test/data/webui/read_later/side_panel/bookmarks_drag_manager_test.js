// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {FOLDER_OPEN_CHANGED_EVENT} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksDragManager, DROP_POSITION_ATTR, DropPosition, overrideFolderOpenerTimeoutDelay} from 'chrome://read-later.top-chrome/side_panel/bookmarks_drag_manager.js';
import {BookmarksListElement, LOCAL_STORAGE_OPEN_FOLDERS_KEY} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarkDragManagerTest', () => {
  /** @type {!BookmarkDragManager} */
  let bookmarkDragManager;

  /** @type {!BookmarksListElement} */
  let delegate;

  /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
  const folders = [{
    id: '1',
    title: 'Bookmarks bar',
    parentId: '0',
    children: [
      {
        id: '2',
        title: 'Google',
        parentId: '1',
        url: 'http://google.com',
      },
      {
        id: '3',
        title: 'Google Docs',
        parentId: '1',
        url: 'http://docs.google.com',
      },
      {
        id: '4',
        title: 'My folder',
        parentId: '1',
        children: [{
          id: '5',
          title: 'My folder\'s child',
          url: 'http://google.com',
          parentId: '4',
        }],
      },
      {
        id: '5',
        title: 'Closed folder',
        parentId: '1',
        children: [{
          id: '6',
          title: 'Closed folder\'s child',
          url: 'http://google.com',
          parentId: '5',
        }],
      },
    ],
  }];

  function getDraggableElements() {
    function getDraggableElementsInner(root) {
      const draggableElements = [];
      const children =
          root.shadowRoot.querySelectorAll('bookmark-folder, .bookmark');
      children.forEach(child => {
        if (child.tagName === 'BOOKMARK-FOLDER') {
          draggableElements.push(child.shadowRoot.querySelector('#folder'));
          draggableElements.push(...getDraggableElementsInner(child));
        } else {
          draggableElements.push(child);
        }
      });
      return draggableElements;
    }

    const rootFolder = delegate.shadowRoot.querySelector('bookmark-folder');
    return getDraggableElementsInner(rootFolder);
  }

  setup(async () => {
    document.body.innerHTML = '';

    loadTimeData.overrideValues({
      bookmarksDragAndDropEnabled: true,
    });

    const bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(
        /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */ (
            JSON.parse(JSON.stringify(folders))));
    BookmarksApiProxy.setInstance(bookmarksApi);

    window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY] =
        JSON.stringify(['1', '4']);

    delegate = new BookmarksListElement();
    bookmarkDragManager = new BookmarksDragManager(delegate);
    document.body.appendChild(delegate);

    await flushTasks();
  });

  test('DragStartCallsAPI', () => {
    let calledIds, calledIndex, calledTouch, calledX, calledY;
    chrome.bookmarkManagerPrivate.startDrag = (ids, index, touch, x, y) => {
      calledIds = ids;
      calledIndex = index;
      calledTouch = touch;
      calledX = x;
      calledY = y;
    };

    const draggableBookmark = getDraggableElements()[0];
    draggableBookmark.dispatchEvent(new DragEvent(
        'dragstart',
        {bubbles: true, composed: true, clientX: 100, clientY: 200}));

    assertDeepEquals(['2'], calledIds);
    assertEquals(0, calledIndex);
    assertFalse(calledTouch);
    assertEquals(100, calledX);
    assertEquals(200, calledY);
  });

  test('DragOverUpdatesAttributes', () => {
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[0];
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    /**
     * @param {!HTMLElement} dragOverElement
     * @param {number} yRatio
     * @param {!DropPosition} dropPosition
     */
    function assertDropPosition(dragOverElement, yRatio, dropPosition) {
      const dragOverRect = dragOverElement.getBoundingClientRect();
      dragOverElement.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * yRatio),
      }));
      assertEquals(
          dropPosition, dragOverElement.getAttribute(DROP_POSITION_ATTR));
    }

    const dragOverBookmark = draggableElements[1];
    assertDropPosition(dragOverBookmark, 0.2, DropPosition.ABOVE);
    assertDropPosition(dragOverBookmark, 0.5, DropPosition.ABOVE);
    assertDropPosition(dragOverBookmark, 0.8, DropPosition.BELOW);

    const dragOverFolder = draggableElements[2];
    assertDropPosition(dragOverFolder, 0.2, DropPosition.ABOVE);
    assertDropPosition(dragOverFolder, 0.5, DropPosition.INTO);
    delegate.isFolderOpen = () => false;
    assertDropPosition(dragOverFolder, 0.8, DropPosition.BELOW);
    delegate.isFolderOpen = () => true;
    assertDropPosition(dragOverFolder, 0.8, DropPosition.INTO);
  });

  test('DragOverDescendant', async () => {
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    const draggableElements = getDraggableElements();
    const draggedFolder = draggableElements[2];
    draggedFolder.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    // Drag over self.
    let dragOverRect = draggedFolder.getBoundingClientRect();
    draggedFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top,
    }));
    assertEquals(null, draggedFolder.getAttribute(DROP_POSITION_ATTR));

    const dragOverChild = draggableElements[3];
    dragOverRect = dragOverChild.getBoundingClientRect();
    dragOverChild.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top,
    }));
    assertEquals(null, dragOverChild.getAttribute(DROP_POSITION_ATTR));
  });

  test('DropsIntoFolder', () => {
    let calledId, calledIndex;
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    chrome.bookmarkManagerPrivate.drop = (id, index) => {
      calledId = id;
      calledIndex = index;
    };

    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[0];
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    const dropFolder = draggableElements[2];
    const dragOverRect = dropFolder.getBoundingClientRect();
    dropFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    dropFolder.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));

    assertEquals('4', calledId);
    assertEquals(undefined, calledIndex);
  });

  test('DropsBookmarksToReorder', () => {
    let calledId, calledIndex;
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    chrome.bookmarkManagerPrivate.drop = (id, index) => {
      calledId = id;
      calledIndex = index;
    };

    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[2];
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    const dragAboveBookmark = draggableElements[0];
    const dragAboveRect = dragAboveBookmark.getBoundingClientRect();
    dragAboveBookmark.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragAboveRect.left,
      clientY: dragAboveRect.top + (dragAboveRect.height * .1),
    }));
    dragAboveBookmark.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));
    assertEquals('1', calledId);
    assertEquals(0, calledIndex);

    const dragBelowBookmark = draggableElements[1];
    const dragBelowRect = dragBelowBookmark.getBoundingClientRect();
    dragBelowBookmark.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragBelowRect.left,
      clientY: dragBelowRect.top + (dragBelowRect.height * .9),
    }));
    dragBelowBookmark.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));
    assertEquals('1', calledId);
    assertEquals(2, calledIndex);
  });

  test('DragOverFolderAutoOpens', async () => {
    overrideFolderOpenerTimeoutDelay(0);
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[0];
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    const folderNode = folders[0].children[3];
    const dragOverFolder = draggableElements[4];
    const dragOverRect = dragOverFolder.getBoundingClientRect();
    dragOverFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    assertFalse(delegate.isFolderOpen(folderNode));

    // Drag over a new bookmark before the timeout runs out to ensure the
    // timeout is canceled.
    const newDragOverBookmark = draggableElements[3];
    const newDragOverBookmarkRect = newDragOverBookmark.getBoundingClientRect();
    newDragOverBookmark.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: newDragOverBookmarkRect.left,
      clientY:
          newDragOverBookmarkRect.top + (newDragOverBookmarkRect.height * .5),
    }));
    await new Promise(resolve => setTimeout(resolve, 0));
    assertFalse(delegate.isFolderOpen(folderNode));

    // Drag back into closed folder and wait for the timeout to resolve.
    dragOverFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    await new Promise(resolve => setTimeout(resolve, 0));
    assertTrue(delegate.isFolderOpen(folderNode));
  });
});