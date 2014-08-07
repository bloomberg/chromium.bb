// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

/**
 * Extension ID of the testing extension.
 * @type {string}
 * @const
 */
test.util.TESTING_EXTENSION_ID = 'oobinhbdbiehknkpbpejbbpdbkdjmoco';

/**
 * Opens the main Files.app's window and waits until it is ready.
 *
 * @param {Object} appState App state.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openMainWindow = function(appState, callback) {
  launchFileManager(appState,
                    undefined,  // opt_type
                    undefined,  // opt_id
                    callback);
};

/**
 * Obtains window information.
 *
 * @return {Object.<string, {innerWidth:number, innerHeight:number}>} Map window
 *     ID and window information.
 */
test.util.sync.getWindows = function() {
  var windows = {};
  for (var id in background.appWindows) {
    var windowWrapper = background.appWindows[id];
    windows[id] = {
      outerWidth: windowWrapper.contentWindow.outerWidth,
      outerHeight: windowWrapper.contentWindow.outerHeight
    };
  }
  for (var id in background.dialogs) {
    windows[id] = {
      outerWidth: background.dialogs[id].outerWidth,
      outerHeight: background.dialogs[id].outerHeight
    };
  }
  return windows;
};

/**
 * Closes the specified window.
 *
 * @param {string} appId AppId of window to be closed.
 * @return {boolean} Result: True if success, false otherwise.
 */
test.util.sync.closeWindow = function(appId) {
  if (appId in background.appWindows &&
      background.appWindows[appId].contentWindow) {
    background.appWindows[appId].close();
    return true;
  }
  return false;
};

/**
 * Gets a document in the Files.app's window, including iframes.
 *
 * @param {Window} contentWindow Window to be used.
 * @param {string=} opt_iframeQuery Query for the iframe.
 * @return {Document=} Returns the found document or undefined if not found.
 * @private
 */
test.util.sync.getDocument_ = function(contentWindow, opt_iframeQuery) {
  if (opt_iframeQuery) {
    var iframe = contentWindow.document.querySelector(opt_iframeQuery);
    return iframe && iframe.contentWindow && iframe.contentWindow.document;
  }

  return contentWindow.document;
};

/**
 * Gets total Javascript error count from background page and each app window.
 * @return {number} Error count.
 */
test.util.sync.getErrorCount = function() {
  var totalCount = JSErrorCount;
  for (var appId in background.appWindows) {
    var contentWindow = background.appWindows[appId].contentWindow;
    if (contentWindow.JSErrorCount)
      totalCount += contentWindow.JSErrorCount;
  }
  return totalCount;
};

/**
 * Resizes the window to the specified dimensions.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} width Window width.
 * @param {number} height Window height.
 * @return {boolean} True for success.
 */
test.util.sync.resizeWindow = function(contentWindow, width, height) {
  background.appWindows[contentWindow.appID].resizeTo(width, height);
  return true;
};

/**
 * Returns an array with the files currently selected in the file manager.
 * TODO(hirono): Integrate the method into getFileList method.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} Array of selected files.
 */
test.util.sync.getSelectedFiles = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var selected = [];
  for (var i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected')) {
      selected.push(
          rows[i].querySelector('.filename-label').textContent);
    }
  }
  return selected;
};

/**
 * Returns an array with the files on the file manager's file list.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<Array.<string>>} Array of rows.
 */
test.util.sync.getFileList = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var fileList = [];
  for (var j = 0; j < rows.length; ++j) {
    var row = rows[j];
    fileList.push([
      row.querySelector('.filename-label').textContent,
      row.querySelector('.size').textContent,
      row.querySelector('.type').textContent,
      row.querySelector('.date').textContent
    ]);
  }
  return fileList;
};

/**
 * Queries all elements.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {?string} iframeQuery Iframe selector or null if no iframe.
 * @param {Array.<string>=} opt_styleNames List of CSS property name to be
 *     obtained.
 * @return {Array.<{attributes:Object.<string, string>, text:string,
 *                  styles:Object.<string, string>, hidden:boolean}>} Element
 *     information that contains contentText, attribute names and
 *     values, hidden attribute, and style names and values.
 */
test.util.sync.queryAllElements = function(
    contentWindow, targetQuery, iframeQuery, opt_styleNames) {
  var doc = test.util.sync.getDocument_(contentWindow, iframeQuery);
  if (!doc)
    return [];
  // The return value of querySelectorAll is not an array.
  return Array.prototype.map.call(
      doc.querySelectorAll(targetQuery),
      function(element) {
        var attributes = {};
        for (var i = 0; i < element.attributes.length; i++) {
          attributes[element.attributes[i].nodeName] =
              element.attributes[i].nodeValue;
        }
        var styles = {};
        var styleNames = opt_styleNames || [];
        var computedStyles = contentWindow.getComputedStyle(element);
        for (var i = 0; i < styleNames.length; i++) {
          styles[styleNames[i]] = computedStyles[styleNames[i]];
        }
        var text = element.textContent;
        return {
          attributes: attributes,
          text: text,
          styles: styles,
          // The hidden attribute is not in the element.attributes even if
          // element.hasAttribute('hidden') is true.
          hidden: !!element.hidden
        };
      });
};

/**
 * Assigns the text to the input element.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query for the input element.
 * @param {string} text Text to be assigned.
 */
test.util.sync.inputText = function(contentWindow, query, text) {
  var input = contentWindow.document.querySelector(query);
  input.value = text;
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.sync.selectFile = function(contentWindow, filename) {
  var rows = contentWindow.document.querySelectorAll('#detail-table li');
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'Home', false);
  for (var index = 0; index < rows.length; ++index) {
    var selection = test.util.sync.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename)
      return true;
    test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'Down', false);
  }
  console.error('Failed to select file "' + filename + '"');
  return false;
};

/**
 * Open the file by selectFile and fakeMouseDoubleClick.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be opened.
 * @return {boolean} True if file got selected and a double click message is
 *     sent, false otherwise.
 */
test.util.sync.openFile = function(contentWindow, filename) {
  var query = '#file-list li.table-row[selected] .filename-label span';
  return test.util.sync.selectFile(contentWindow, filename) &&
         test.util.sync.fakeMouseDoubleClick(contentWindow, query);
};

/**
 * Selects a volume specified by its icon name
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} iconName Name of the volume icon.
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether the target is found and mousedown and click events are sent.
 */
test.util.async.selectVolume = function(contentWindow, iconName, callback) {
  var query = '[volume-type-icon=' + iconName + ']';
  var driveQuery = '[volume-type-icon=drive]';
  var isDriveSubVolume = iconName == 'drive_recent' ||
                         iconName == 'drive_shared_with_me' ||
                         iconName == 'drive_offline';
  var preSelection = false;
  var steps = {
    checkQuery: function() {
      if (contentWindow.document.querySelector(query)) {
        steps.sendEvents();
        return;
      }
      // If the target volume is sub-volume of drive, we must click 'drive'
      // before clicking the sub-item.
      if (!preSelection) {
        if (!isDriveSubVolume) {
          callback(false);
          return;
        }
        if (!(test.util.sync.fakeMouseDown(contentWindow, driveQuery) &&
              test.util.sync.fakeMouseClick(contentWindow, driveQuery))) {
          callback(false);
          return;
        }
        preSelection = true;
      }
      setTimeout(steps.checkQuery, 50);
    },
    sendEvents: function() {
      // To change the selected volume, we have to send both events 'mousedown'
      // and 'click' to the navigation list.
      callback(test.util.sync.fakeMouseDown(contentWindow, query) &&
               test.util.sync.fakeMouseClick(contentWindow, query));
    }
  };
  steps.checkQuery();
};

/**
 * Executes Javascript code on a webview and returns the result.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} webViewQuery Selector for the web view.
 * @param {string} code Javascript code to be executed within the web view.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.executeScriptInWebView = function(
    contentWindow, webViewQuery, code, callback) {
  var webView = contentWindow.document.querySelector(webViewQuery);
  webView.executeScript({code: code}, callback);
};

/**
 * Sends an event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {Event} event Event to be sent.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.sendEvent = function(
    contentWindow, targetQuery, event, opt_iframeQuery) {
  var doc = test.util.sync.getDocument_(contentWindow, opt_iframeQuery);
  if (doc) {
    var target = doc.querySelector(targetQuery);
    if (target) {
      target.dispatchEvent(event);
      return true;
    }
  }
  console.error('Target element for ' + targetQuery + ' not found.');
  return false;
};

/**
 * Sends an fake event having the specified type to the target query.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} eventType Type of event.
 * @param {Object=} opt_additionalProperties Object contaning additional
 *     properties.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeEvent = function(contentWindow,
                                    targetQuery,
                                    eventType,
                                    opt_additionalProperties) {
  var event = new Event(eventType, opt_additionalProperties || {});
  if (opt_additionalProperties) {
    for (var name in opt_additionalProperties) {
      event[name] = opt_additionalProperties[name];
    }
  }
  return test.util.sync.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Sends a fake key event to the element specified by |targetQuery| with the
 * given |keyIdentifier| and optional |ctrl| modifier to the file manager.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} keyIdentifier Identifier of the emulated key.
 * @param {boolean} ctrl Whether CTRL should be pressed, or not.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeKeyDown = function(
    contentWindow, targetQuery, keyIdentifier, ctrl, opt_iframeQuery) {
  var event = new KeyboardEvent(
      'keydown',
      { bubbles: true, keyIdentifier: keyIdentifier, ctrlKey: ctrl });
  return test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Simulates a fake mouse click (left button, single click) on the element
 * specified by |targetQuery|. If the element has the click method, just calls
 * it. Otherwise, this sends 'mouseover', 'mousedown', 'mouseup' and 'click'
 * events in turns.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the all events are sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var mouseOverEvent = new MouseEvent('mouseover', {bubbles: true, detail: 1});
  var resultMouseOver = test.util.sync.sendEvent(
      contentWindow, targetQuery, mouseOverEvent, opt_iframeQuery);
  var mouseDownEvent = new MouseEvent('mousedown', {bubbles: true, detail: 1});
  var resultMouseDown = test.util.sync.sendEvent(
      contentWindow, targetQuery, mouseDownEvent, opt_iframeQuery);
  var mouseUpEvent = new MouseEvent('mouseup', {bubbles: true, detail: 1});
  var resultMouseUp = test.util.sync.sendEvent(
      contentWindow, targetQuery, mouseUpEvent, opt_iframeQuery);
  var clickEvent = new MouseEvent('click', {bubbles: true, detail: 1});
  var resultClick = test.util.sync.sendEvent(
      contentWindow, targetQuery, clickEvent, opt_iframeQuery);
  return resultMouseOver && resultMouseDown && resultMouseUp && resultClick;
};

/**
 * Simulates a fake mouse click (right button, single click) on the element
 * specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false
 *     otherwise.
 */
test.util.sync.fakeMouseRightClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var contextMenuEvent = new MouseEvent('contextmenu', {bubbles: true});
  var result = test.util.sync.sendEvent(
      contentWindow, targetQuery, contextMenuEvent, opt_iframeQuery);
  return result;
};

/**
 * Simulates a fake double click event (left button) to the element specified by
 * |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDoubleClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  // Double click is always preceded with a single click.
  if (!test.util.sync.fakeMouseClick(
      contentWindow, targetQuery, opt_iframeQuery)) {
    return false;
  }

  // Send the second click event, but with detail equal to 2 (number of clicks)
  // in a row.
  var event = new MouseEvent('click', { bubbles: true, detail: 2 });
  if (!test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery)) {
    return false;
  }

  // Send the double click event.
  var event = new MouseEvent('dblclick', { bubbles: true });
  if (!test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery)) {
    return false;
  }

  return true;
};

/**
 * Sends a fake mouse down event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseDown = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('mousedown', { bubbles: true });
  return test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Sends a fake mouse up event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseUp = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('mouseup', { bubbles: true });
  return test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Selects |filename| and fakes pressing Ctrl+C, Ctrl+V (copy, paste).
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be copied.
 * @return {boolean} True if copying got simulated successfully. It does not
 *     say if the file got copied, or not.
 */
test.util.sync.copyFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Ctrl+C and Ctrl+V
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+0043', true);
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+0056', true);
  return true;
};

/**
 * Selects |filename| and fakes pressing the Delete key.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be deleted.
 * @return {boolean} True if deleting got simulated successfully. It does not
 *     say if the file got deleted, or not.
 */
test.util.sync.deleteFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Delete
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+007F', false);
  return true;
};

/**
 * Execute a command on the document in the specified window.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} command Command name.
 * @return {boolean} True if the command is executed successfully.
 */
test.util.sync.execCommand = function(contentWindow, command) {
  return contentWindow.document.execCommand(command);
};

/**
 * Override the installWebstoreItem method in private api for test.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} expectedItemId Item ID to be called this method with.
 * @param {?string} intendedError Error message to be returned when the item id
 *     matches. 'null' represents no error.
 * @return {boolean} Always return true.
 */
test.util.sync.overrideInstallWebstoreItemApi =
    function(contentWindow, expectedItemId, intendedError) {
  var setLastError = function(message) {
    contentWindow.chrome.runtime.lastError =
        message ? {message: message} : null;
  };

  var installWebstoreItem = function(itemId, silentInstallation, callback) {
    setTimeout(function() {
      if (itemId !== expectedItemId) {
        setLastError('Invalid Chrome Web Store item ID');
        callback();
        return;
      }

      setLastError(intendedError);
      callback();
    });
  };

  test.util.executedTasks_ = [];
  contentWindow.chrome.fileBrowserPrivate.installWebstoreItem =
      installWebstoreItem;
  return true;
};

/**
 * Override the task-related methods in private api for test.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array.<Object>} taskList List of tasks to be returned in
 *     fileBrowserPrivate.getFileTasks().
 * @return {boolean} Always return true.
 */
test.util.sync.overrideTasks = function(contentWindow, taskList) {
  var getFileTasks = function(urls, onTasks) {
    // Call onTask asynchronously (same with original getFileTasks).
    setTimeout(function() {
      onTasks(taskList);
    });
  };

  var executeTask = function(taskId, url) {
    test.util.executedTasks_.push(taskId);
  };

  var setDefaultTask = function(taskId) {
    for (var i = 0; i < taskList.length; i++) {
      taskList[i].isDefault = taskList[i].taskId === taskId;
    }
  };

  test.util.executedTasks_ = [];
  contentWindow.chrome.fileBrowserPrivate.getFileTasks = getFileTasks;
  contentWindow.chrome.fileBrowserPrivate.executeTask = executeTask;
  contentWindow.chrome.fileBrowserPrivate.setDefaultTask = setDefaultTask;
  return true;
};

/**
 * Obtains the list of executed tasks.
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} List of executed task ID.
 */
test.util.sync.getExecutedTasks = function(contentWindow) {
  if (!test.util.executedTasks_) {
    console.error('Please call overrideTasks() first.');
    return null;
  }
  return test.util.executedTasks_;
};

/**
 * Invoke chrome.fileBrowserPrivate.visitDesktop(profileId) to cause window
 * teleportation.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} profileId Destination profile's ID.
 * @return {boolean} Always return true.
 */
test.util.sync.visitDesktop = function(contentWindow, profileId) {
  contentWindow.chrome.fileBrowserPrivate.visitDesktop(profileId);
  return true;
};

/**
 * Runs the 'Move to profileId' menu.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} profileId Destination profile's ID.
 * @return {boolean} True if the menu is found and run.
 */
test.util.sync.runVisitDesktopMenu = function(contentWindow, profileId) {
  var list = contentWindow.document.querySelectorAll('.visit-desktop');
  for (var i = 0; i < list.length; ++i) {
    if (list[i].label.indexOf(profileId) != -1) {
      var activateEvent = contentWindow.document.createEvent('Event');
      activateEvent.initEvent('activate');
      list[i].dispatchEvent(activateEvent);
      return true;
    }
  }
  return false;
};

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  // Register the message listener.
  var onMessage = chrome.runtime ? chrome.runtime.onMessageExternal :
      chrome.extension.onMessageExternal;
  // Return true for asynchronous functions and false for synchronous.
  onMessage.addListener(function(request, sender, sendResponse) {
    // Check the sender.
    if (sender.id != test.util.TESTING_EXTENSION_ID) {
      console.error('The testing extension must be white-listed.');
      return false;
    }
    // Set a global flag that we are in tests, so other components are aware
    // of it.
    window.IN_TEST = true;
    // Check the function name.
    if (!request.func || request.func[request.func.length - 1] == '_') {
      request.func = '';
    }
    // Prepare arguments.
    var args = request.args.slice();  // shallow copy
    if (request.appId) {
      if (background.appWindows[request.appId]) {
        args.unshift(background.appWindows[request.appId].contentWindow);
      } else if (background.dialogs[request.appId]) {
        args.unshift(background.dialogs[request.appId]);
      } else {
        console.error('Specified window not found: ' + request.appId);
        return false;
      }
    }
    // Call the test utility function and respond the result.
    if (test.util.async[request.func]) {
      args[test.util.async[request.func].length - 1] = function() {
        console.debug('Received the result of ' + request.func);
        sendResponse.apply(null, arguments);
      };
      console.debug('Waiting for the result of ' + request.func);
      test.util.async[request.func].apply(null, args);
      return true;
    } else if (test.util.sync[request.func]) {
      sendResponse(test.util.sync[request.func].apply(null, args));
      return false;
    } else {
      console.error('Invalid function name.');
      return false;
    }
  });
};

// Register the test utils.
test.util.registerRemoteTestUtils();
