// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ElementObject} from './element_object.js';
import {KeyModifiers} from './key_modifiers.js';
import {getCaller, pending, repeatUntil, sendTestMessage} from './test_util.js';
import {VolumeManagerCommonVolumeType} from './volume_manager_common_volume_type.js';

/**
 * When step by step tests are enabled, turns on automatic step() calls. Note
 * that if step() is defined at the time of this call, invoke it to start the
 * test auto-stepping ball rolling.
 */
window.autoStep = () => {
  window.autostep = window.autostep || false;
  if (!window.autostep) {
    window.autostep = true;
  }
  if (window.autostep && typeof window.step === 'function') {
    window.step();
  }
};

/**
 * This error type is thrown by executeJsInPreviewTagSwa_ if the script to
 * execute in the untrusted context produces an error.
 */
export class ExecuteScriptError extends Error {
  constructor(message) {
    super(message);
    this.name = 'ExecuteScriptError';
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
export class RemoteCall {
  /**
   * @param {string} origin ID of the app to be manipulated.
   */
  constructor(origin) {
    this.origin_ = origin;

    /**
     * Tristate holding the cached result of isStepByStepEnabled_().
     * @type {?boolean}
     */
    this.cachedStepByStepEnabled_ = null;
  }

  /**
   * Checks whether step by step tests are enabled or not.
   * @private
   * @return {!Promise<boolean>}
   */
  async isStepByStepEnabled_() {
    if (this.cachedStepByStepEnabled_ === null) {
      this.cachedStepByStepEnabled_ = await new Promise((fulfill) => {
        chrome.commandLinePrivate.hasSwitch(
            'enable-file-manager-step-by-step-tests', fulfill);
      });
    }
    return this.cachedStepByStepEnabled_;
  }

  /**
   * Sends a test |message| to the test code running in the File Manager.
   * @param {!Object} message
   * @return {!Promise<*>} A promise which when fulfilled returns the
   *     result of executing test code with the given message.
   */
  sendMessage(message) {
    return new Promise((fulfill) => {
      chrome.runtime.sendMessage(this.origin_, message, {}, fulfill);
    });
  }

  /**
   * Calls a remote test util in the Files app's extension. See:
   * registerRemoteTestUtils in test_util_base.js.
   *
   * @param {string} func Function name.
   * @param {?string} appId App window Id or null for functions not requiring a
   *     window.
   * @param {?Array<*>=} args Array of arguments.
   * @param {function(*)=} opt_callback Callback handling the function's result.
   * @return {!Promise} Promise to be fulfilled with the result of the remote
   *     utility.
   */
  async callRemoteTestUtil(func, appId, args, opt_callback) {
    const stepByStep = await this.isStepByStepEnabled_();
    let finishCurrentStep;
    if (stepByStep) {
      while (window.currentStep) {
        await window.currentStep;
      }
      window.currentStep = new Promise(resolve => {
        finishCurrentStep = () => {
          window.currentStep = null;
          resolve();
        };
      });
      console.info('Executing: ' + func + ' on ' + appId + ' with args: ');
      console.info(args);
      if (window.autostep !== true) {
        await new Promise((onFulfilled) => {
          console.info('Type step() to continue...');
          /** @type {?function()} */
          window.step = function() {
            window.step = null;
            onFulfilled();
          };
        });
      } else {
        console.info('Auto calling step() ...');
      }
    }
    const response = await this.sendMessage({func, appId, args});

    if (stepByStep) {
      console.info('Returned value:');
      console.info(JSON.stringify(response));
      finishCurrentStep();
    }
    if (opt_callback) {
      opt_callback(response);
    }
    return response;
  }

  /**
   * Waits until a window having the given ID prefix appears.
   * @param {string} windowIdPrefix ID prefix of the requested window.
   * @return {!Promise<string>} promise Promise to be fulfilled with a found
   *     window's ID.
   */
  waitForWindow(windowIdPrefix) {
    const caller = getCaller();
    const windowIdRegex = new RegExp(windowIdPrefix);
    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      for (const id in windows) {
        if (id.indexOf(windowIdPrefix) === 0 || windowIdRegex.test(id)) {
          return id;
        }
      }
      return pending(
          caller, 'Window with the prefix %s is not found.', windowIdPrefix);
    });
  }

  /**
   * Closes a window and waits until the window is closed.
   *
   * @param {string} appId App window Id.
   * @return {Promise} promise Promise to be fulfilled with the result (true:
   *     success, false: failed).
   */
  async closeWindowAndWait(appId) {
    const caller = getCaller();

    // Closes the window.
    if (!await this.callRemoteTestUtil('closeWindow', null, [appId])) {
      // Returns false when the closing is failed.
      return false;
    }

    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      for (const id in windows) {
        if (id === appId) {
          // Window is still available. Continues waiting.
          return pending(
              caller, 'Window with the prefix %s is not found.', appId);
        }
      }
      // Window is not available. Closing is done successfully.
      return true;
    });
  }

  /**
   * Waits until the window turns to the given size.
   * @param {string} appId App window Id.
   * @param {number} width Requested width in pixels.
   * @param {number} height Requested height in pixels.
   */
  waitForWindowGeometry(appId, width, height) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const windows = await this.callRemoteTestUtil('getWindows', null, []);
      if (!windows[appId]) {
        return pending(caller, 'Window %s is not found.', appId);
      }
      if (windows[appId].outerWidth !== width ||
          windows[appId].outerHeight !== height) {
        return pending(
            caller, 'Expected window size is %j, but it is %j',
            {width: width, height: height}, windows[appId]);
      }
    });
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @return {Promise<ElementObject>} Promise to be fulfilled when the element
   *     appears.
   */
  waitForElement(appId, query) {
    return this.waitForElementStyles(appId, query, []);
  }

  /**
   * Waits for the specified element appearing in the DOM.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {!Array<string>} styleNames List of CSS property name to be
   *     obtained. NOTE: Causes element style re-calculation.
   * @return {Promise<ElementObject>} Promise to be fulfilled when the element
   *     appears.
   */
  waitForElementStyles(appId, query, styleNames) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements = await this.callRemoteTestUtil(
          'deepQueryAllElements', appId, [query, styleNames]);
      if (elements && elements.length > 0) {
        return /** @type {ElementObject} */ (elements[0]);
      }
      return pending(caller, 'Element %s is not found.', query);
    });
  }

  /**
   * Waits for a remote test function to return a specific result.
   *
   * @param {string} funcName Name of remote test function to be executed.
   * @param {?string} appId App window Id.
   * @param {function(Object):boolean|boolean|Object} expectedResult An value to
   *     be checked against the return value of |funcName| or a callback that
   *     receives the return value of |funcName| and returns true if the result
   *     is the expected value.
   * @param {?Array<*>=} args Arguments to be provided to |funcName| when
   *     executing it.
   * @return {Promise} Promise to be fulfilled when the |expectedResult| is
   *     returned from |funcName| execution.
   */
  waitFor(funcName, appId, expectedResult, args) {
    const caller = getCaller();
    args = args || [];
    return repeatUntil(async () => {
      const result = await this.callRemoteTestUtil(funcName, appId, args);
      if (typeof expectedResult === 'function' && expectedResult(result)) {
        return result;
      }
      if (expectedResult === result) {
        return result;
      }
      const msg = 'waitFor: Waiting for ' +
          `${funcName} to return ${expectedResult}, ` +
          `but got ${JSON.stringify(result)}.`;
      return pending(caller, msg);
    });
  }

  /**
   * Waits for the specified element leaving from the DOM.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @return {Promise} Promise to be fulfilled when the element is lost.
   */
  waitForElementLost(appId, query) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const elements =
          await this.callRemoteTestUtil('deepQueryAllElements', appId, [query]);
      if (elements.length > 0) {
        return pending(caller, 'Elements %j still exists.', elements);
      }
      return true;
    });
  }

  /**
   * Wait for the |query| to match |count| elements.
   *
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {number} count The expected element match count.
   * @return {Promise} Promise to be fulfilled on success.
   */
  waitForElementsCount(appId, query, count) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const expect = `Waiting for [${query}] to match ${count} elements`;
      const result =
          await this.callRemoteTestUtil('countElements', appId, [query, count]);
      return !result ? pending(caller, expect) : true;
    });
  }

  /**
   * Sends a fake key down event.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {string} key DOM UI Events Key value.
   * @param {boolean} ctrlKey Control key flag.
   * @param {boolean} shiftKey Shift key flag.
   * @param {boolean} altKey Alt key flag.
   * @return {Promise} Promise to be fulfilled or rejected depending on the
   *     result.
   */
  async fakeKeyDown(appId, query, key, ctrlKey, shiftKey, altKey) {
    const result = await this.callRemoteTestUtil(
        'fakeKeyDown', appId, [query, key, ctrlKey, shiftKey, altKey]);
    if (result) {
      return true;
    } else {
      throw new Error('Fail to fake key down.');
    }
  }

  /**
   * Sets the given input text on the element identified by the query.
   * @param {string} appId App window ID.
   * @param {string|!Array<string>} selector The query selector to locate
   *     the element
   * @param {string} text The text to be set on the element.
   */
  async inputText(appId, selector, text) {
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('inputText', appId, [selector, text]));
  }

  /**
   * Gets file entries just under the volume.
   *
   * @param {VolumeManagerCommonVolumeType} volumeType Volume type.
   * @param {Array<string>} names File name list.
   * @return {Promise} Promise to be fulfilled with file entries or rejected
   *     depending on the result.
   */
  getFilesUnderVolume(volumeType, names) {
    return this.callRemoteTestUtil(
        'getFilesUnderVolume', null, [volumeType, names]);
  }

  /**
   * Waits for a single file.
   * @param {VolumeManagerCommonVolumeType} volumeType Volume type.
   * @param {string} name File name.
   * @return {!Promise} Promise to be fulfilled when the file had found.
   */
  waitForAFile(volumeType, name) {
    const caller = getCaller();
    return repeatUntil(async () => {
      if ((await this.getFilesUnderVolume(volumeType, [name])).length === 1) {
        return true;
      }
      return pending(caller, '"' + name + '" is not found.');
    });
  }

  /**
   * Shorthand for clicking an element.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {KeyModifiers=} opt_keyModifiers Object
   * @return {Promise} Promise to be fulfilled with the clicked element.
   */
  async waitAndClickElement(appId, query, opt_keyModifiers) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil(
        'fakeMouseClick', appId, [query, opt_keyModifiers]);
    chrome.test.assertTrue(result, 'mouse click failed.');
    return element;
  }

  /**
   * Shorthand for right-clicking an element.
   * @param {string} appId App window Id.
   * @param {string|!Array<string>} query Query to specify the element.
   *     If query is an array, |query[0]| specifies the first
   *     element(s), |query[1]| specifies elements inside the shadow DOM of
   *     the first element, and so on.
   * @param {KeyModifiers=} opt_keyModifiers Object
   * @return {Promise} Promise to be fulfilled with the clicked element.
   */
  async waitAndRightClick(appId, query, opt_keyModifiers) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil(
        'fakeMouseRightClick', appId, [query, opt_keyModifiers]);
    chrome.test.assertTrue(result, 'mouse right-click failed.');
    return element;
  }

  /**
   * Shorthand for focusing an element.
   * @param {string} appId App window Id.
   * @param {!Array<string>} query Query to specify the element to be focused.
   * @return {Promise} Promise to be fulfilled with the focused element.
   */
  async focus(appId, query) {
    const element = await this.waitForElement(appId, query);
    const result = await this.callRemoteTestUtil('focus', appId, query);
    chrome.test.assertTrue(result, 'focus failed.');
    return element;
  }

  /**
   * Simulate Click in the UI in the middle of the element.
   * @param{string} appId App window ID contains the element. NOTE: The click is
   * simulated on most recent window in the window system.
   * @param {string|!Array<string>} query Query to the element to be clicked.
   * @return {!Promise} A promise fulfilled after the click event.
   */
  async simulateUiClick(appId, query) {
    const element = /* @type {!Object} */ (
        await this.waitForElementStyles(appId, query, ['display']));
    chrome.test.assertTrue(!!element, 'element for simulateUiClick not found');

    // Find the middle of the element.
    const x =
        Math.floor(element['renderedLeft'] + (element['renderedWidth'] / 2));
    const y =
        Math.floor(element['renderedTop'] + (element['renderedHeight'] / 2));

    return sendTestMessage(
        {appId, name: 'simulateClick', 'clickX': x, 'clickY': y});
  }
}

/**
 * Class to manipulate the window in the remote extension.
 */
export class RemoteCallFilesApp extends RemoteCall {
  /**
   * @return {boolean} Returns whether the code is running in SWA mode.
   */
  isSwaMode() {
    return this.origin_.startsWith('chrome://');
  }

  /**
   * Sends a test |message| to the test code running in the File Manager.
   * @param {!Object} message
   * @return {!Promise<*>}
   * @override
   */
  sendMessage(message) {
    if (!this.isSwaMode()) {
      return super.sendMessage(message);
    }

    const command = {
      name: 'callSwaTestMessageListener',
      appId: message.appId,
      data: JSON.stringify(message),
    };

    return new Promise((fulfill) => {
      chrome.test.sendMessage(JSON.stringify(command), (response) => {
        if (response === '"@undefined@"') {
          fulfill(undefined);
        } else {
          try {
            fulfill(response == '' ? true : JSON.parse(response));
          } catch (e) {
            console.error(`Failed to parse "${response}" due to ${e}`);
            fulfill(false);
          }
        }
      });
    });
  }

  /** @override */
  async waitForWindow(windowIdPrefix) {
    if (!this.isSwaMode()) {
      return super.waitForWindow(windowIdPrefix);
    }

    return this.waitForSwaWindow();
  }

  async getWindows() {
    if (!this.isSwaMode()) {
      return this.callRemoteTestUtil('getWindows', null, []);
    }

    return JSON.parse(
        await sendTestMessage({name: 'getWindowsSWA', isSWA: true}));
  }

  /**
   * Wait for a SWA window to be open.
   * @return {!Promise<string>}
   */
  async waitForSwaWindow() {
    const caller = getCaller();
    const appId = await repeatUntil(async () => {
      const ret = await sendTestMessage({name: 'findSwaWindow'});
      if (ret === 'none') {
        return pending(caller, 'Wait for SWA window');
      }
      return ret;
    });

    return appId;
  }

  /**
   * Executes a script in the context of a <preview-tag> element contained in
   * the window.
   * For SWA: It's the first chrome-untrusted://file-manager <iframe>.
   * For legacy: It's the first elements based on the `query`.
   * Responds with its output.
   *
   * @param {string} appId App window Id.
   * @param {!Array<string>} query Query to the <preview-tag> element (this is
   *     ignored for SWA).
   * @param {string} statement Javascript statement to be executed within the
   *     <preview-tag>.
   * @return {!Promise<*>} resolved with the return value of the `statement`.
   */
  async executeJsInPreviewTag(appId, query, statement) {
    if (this.isSwaMode()) {
      return this.executeJsInPreviewTagSwa_(statement);
    }

    return this.callRemoteTestUtil(
        'deepExecuteScriptInWebView', appId, [query, statement]);
  }

  /**
   * Inject javascript statemenent in the first chrome-untrusted://file-manager
   * page found and respond with its output.
   * @private
   * @param {string} statement
   * @return {!Promise}
   */
  async executeJsInPreviewTagSwa_(statement) {
    const script = `try {
          let result = ${statement};
          result = result === undefined ? '@undefined@' : [result];
          window.domAutomationController.send(JSON.stringify(result));
        } catch (error) {
          const errorInfo = {'@error@':  error.message, '@stack@': error.stack};
          window.domAutomationController.send(JSON.stringify(errorInfo));
        }`;

    const command = {
      name: 'executeScriptInChromeUntrusted',
      data: script,
    };

    const response = await sendTestMessage(command);
    if (response === '"@undefined@"') {
      return undefined;
    }
    const output = JSON.parse(response);
    if ('@error@' in output) {
      console.error(output['@error@']);
      console.error('Original StackTrace:\n' + output['@stack@']);
      throw new ExecuteScriptError(
          'Error executing JS in Preview: ' + output['@error@']);
    } else {
      return output;
    }
  }

  /**
   * Waits until the browser is opened and shows the expected URL.
   * @param {string} expectedURL
   * @return {!Promise} Promise to be fulfilled when the expected URL is shown
   *     in a browser window.
   */
  async waitForActiveBrowserTabUrl(expectedURL) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const command = {name: 'getActiveTabURL'};
      const activeWindowURL = await sendTestMessage(command);
      if (activeWindowURL !== expectedURL) {
        return pending(
            caller, 'waitForActiveBrowserTabUrl: expected %j actual %j.',
            expectedURL, activeWindowURL);
      }
    });
  }

  /**
   * Waits for the file list turns to the given contents.
   * @param {string} appId App window Id.
   * @param {Array<Array<string>>} expected Expected contents of file list.
   * @param {{orderCheck:(?boolean|undefined),
   *     ignoreLastModifiedTime:(?boolean|undefined)}=} opt_options Options of
   *     the comparison. If orderCheck is true, it also compares the order of
   *     files. If ignoreLastModifiedTime is true, it compares the file without
   *     its last modified time.
   * @return {Promise} Promise to be fulfilled when the file list turns to the
   *     given contents.
   */
  waitForFiles(appId, expected, opt_options) {
    const options = opt_options || {};
    const caller = getCaller();
    return repeatUntil(async () => {
      const files = await this.callRemoteTestUtil('getFileList', appId, []);
      if (!options.orderCheck) {
        files.sort();
        expected.sort();
      }
      for (let i = 0; i < Math.min(files.length, expected.length); i++) {
        // Change the value received from the UI to match when comparing.
        if (options.ignoreFileSize) {
          files[i][1] = expected[i][1];
        }
        if (options.ignoreLastModifiedTime) {
          if (expected[i].length < 4) {
            // expected sometimes doesn't include the modified time at all, so
            // just remove from the data from UI.
            files[i].splice(3, 1);
          } else {
            files[i][3] = expected[i][3];
          }
        }
      }
      if (!chrome.test.checkDeepEq(expected, files)) {
        return pending(
            caller, 'waitForFiles: expected: %j actual %j.', expected, files);
      }
    });
  }

  /**
   * Waits until the number of files in the file list is changed from the given
   * number.
   * TODO(hirono): Remove the function.
   *
   * @param {string} appId App window Id.
   * @param {number} lengthBefore Number of items visible before.
   * @return {Promise} Promise to be fulfilled with the contents of files.
   */
  waitForFileListChange(appId, lengthBefore) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const files = await this.callRemoteTestUtil('getFileList', appId, []);
      files.sort();

      const notReadyRows =
          files.filter((row) => row.filter((cell) => cell == '...').length);

      if (notReadyRows.length === 0 && files.length !== lengthBefore &&
          files.length !== 0) {
        return files;
      } else {
        return pending(
            caller, 'The number of file is %d. Not changed.', lengthBefore);
      }
    });
  }

  /**
   * Waits until the given taskId appears in the executed task list.
   * @param {string} appId App window Id.
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor Task to
   *     watch.
   * @param {Array<Object>=} opt_replyArgs arguments to reply to executed task.
   * @return {Promise} Promise to be fulfilled when the task appears in the
   *     executed task list.
   */
  waitUntilTaskExecutes(appId, descriptor, opt_replyArgs) {
    const caller = getCaller();
    return repeatUntil(async () => {
      if (!await this.callRemoteTestUtil(
              'taskWasExecuted', appId, [descriptor])) {
        const executedTasks =
            (await this.callRemoteTestUtil('getExecutedTasks', appId, []))
                .map(
                    ({appId, taskType, actionId}) =>
                        `${appId}|${taskType}|${actionId}`);
        return pending(caller, 'Executed task is %j', executedTasks);
      }
      if (opt_replyArgs) {
        await this.callRemoteTestUtil(
            'replyExecutedTask', appId, [descriptor, opt_replyArgs]);
      }
    });
  }

  /**
   * Check if the next tabforcus'd element has the given ID or not.
   * @param {string} appId App window Id.
   * @param {string} elementId String of 'id' attribute which the next
   *     tabfocus'd element should have.
   * @return {Promise} Promise to be fulfilled with the result.
   */
  async checkNextTabFocus(appId, elementId) {
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'Tab key dispatch failure');

    const caller = getCaller();
    return repeatUntil(async () => {
      let element =
          await this.callRemoteTestUtil('getActiveElement', appId, []);
      if (element && element.attributes['id'] === elementId) {
        return true;
      }
      // Try to check the shadow root.
      element =
          await this.callRemoteTestUtil('deepGetActiveElement', appId, []);
      if (element && element.attributes['id'] === elementId) {
        return true;
      }
      return pending(
          caller,
          'Waiting for active element with id: "' + elementId +
              '", but current is: "' + element.attributes['id'] + '"');
    });
  }

  /**
   * Waits until the current directory is changed.
   * @param {string} appId App window Id.
   * @param {string} expectedPath Path to be changed to.
   * @return {Promise} Promise to be fulfilled when the current directory is
   *     changed to expectedPath.
   */
  waitUntilCurrentDirectoryIsChanged(appId, expectedPath) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const path =
          await this.callRemoteTestUtil('getBreadcrumbPath', appId, []);
      if (path !== expectedPath) {
        return pending(
            caller, 'Expected path is %s got %s', expectedPath, path);
      }
    });
  }

  /**
   * Expands tree item.
   * @param {string} appId App window Id.
   * @param {string} query Query to the <tree-item> element.
   */
  async expandTreeItemInDirectoryTree(appId, query) {
    await this.waitForElement(appId, query);
    const elements = await this.callRemoteTestUtil(
        'queryAllElements', appId, [`${query}[expanded]`]);
    // If it's already expanded just set the focus on directory tree.
    if (elements.length > 0) {
      return this.callRemoteTestUtil('focus', appId, ['#directory-tree']);
    }

    // We must wait until <tree-item> has attribute [has-children=true]
    // otherwise it won't expand. We must also to account for the case
    // :not([expanded]) to ensure it has NOT been expanded by some async
    // operation since the [expanded] checks above.
    const expandIcon =
        query + ':not([expanded]) > .tree-row[has-children=true] .expand-icon';
    await this.waitAndClickElement(appId, expandIcon);
    // Wait for the expansion to finish.
    await this.waitForElement(appId, query + '[expanded]');
    // Force the focus on directory tree.
    await this.callRemoteTestUtil('focus', appId, ['#directory-tree']);
  }

  /**
   * Expands directory tree for specified path.
   */
  expandDirectoryTreeFor(appId, path, volumeType = 'downloads') {
    return this.expandDirectoryTreeForInternal_(
        appId, path.split('/'), 0, volumeType);
  }

  /**
   * Internal function for expanding directory tree for specified path.
   */
  async expandDirectoryTreeForInternal_(appId, components, index, volumeType) {
    if (index >= components.length - 1) {
      return;
    }

    // First time we should expand the root/volume first.
    if (index === 0) {
      await this.expandVolumeInDirectoryTree(appId, volumeType);
      return this.expandDirectoryTreeForInternal_(
          appId, components, index + 1, volumeType);
    }
    const path = '/' + components.slice(1, index + 1).join('/');
    await this.expandTreeItemInDirectoryTree(
        appId, `[full-path-for-testing="${path}"]`);
    await this.expandDirectoryTreeForInternal_(
        appId, components, index + 1, volumeType);
  }

  /**
   * Expands download volume in directory tree.
   */
  expandDownloadVolumeInDirectoryTree(appId) {
    return this.expandVolumeInDirectoryTree(appId, 'downloads');
  }

  /**
   * Expands download volume in directory tree.
   */
  expandVolumeInDirectoryTree(appId, volumeType) {
    return this.expandTreeItemInDirectoryTree(
        appId, `[volume-type-for-testing="${volumeType}"]`);
  }

  /**
   * Navigates to specified directory on the specified volume by using directory
   * tree.
   * DEPRECATED: Use background.js:navigateWithDirectoryTree instead
   * crbug.com/996626.
   */
  async navigateWithDirectoryTree(
      appId, path, rootLabel, volumeType = 'downloads') {
    await this.expandDirectoryTreeFor(appId, path, volumeType);

    // Select target path.
    await this.callRemoteTestUtil(
        'fakeMouseClick', appId, [`[full-path-for-testing="${path}"]`]);

    // Entries within Drive starts with /root/ but it isn't displayed in the
    // breadcrubms used by waitUntilCurrentDirectoryIsChanged.
    path = path.replace(/^\/root/, '')
               .replace(/^\/team_drives/, '')
               .replace(/^\/Computers/, '');

    // TODO(lucmult): Remove this once MyFilesVolume is rolled out.
    // Remove /Downloads duplication when MyFilesVolume is enabled.
    if (volumeType == 'downloads' && path.startsWith('/Downloads') &&
        rootLabel.endsWith('/Downloads')) {
      rootLabel = rootLabel.replace('/Downloads', '');
    }

    // Wait until the Files app is navigated to the path.
    return this.waitUntilCurrentDirectoryIsChanged(
        appId, `/${rootLabel}${path}`);
  }

  /**
   * Wait until the expected number of volumes is mounted.
   * @param {number} expectedVolumesCount Expected number of mounted volumes.
   * @return {Promise} promise Promise to be fulfilled.
   */
  async waitForVolumesCount(expectedVolumesCount) {
    const caller = getCaller();
    return repeatUntil(async () => {
      const volumesCount = await sendTestMessage({name: 'getVolumesCount'});
      if (volumesCount === expectedVolumesCount.toString()) {
        return;
      }
      const msg =
          'Expected number of mounted volumes: ' + expectedVolumesCount +
          '. Actual: ' + volumesCount;
      return pending(caller, msg);
    });
  }

  /**
   * Isolates the specified banner to test. The banner is still checked against
   * it's filters, but is now the top priority banner.
   * @param {string} appId App window Id
   * @param {string} bannerTagName Banner tag name in lowercase to isolate.
   */
  async isolateBannerForTesting(appId, bannerTagName) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(await this.callRemoteTestUtil(
        'isolateBannerForTesting', appId, [bannerTagName]));
  }

  /**
   * Disables banners from attaching to the DOM.
   * @param {string} appId App window Id
   */
  async disableBannersForTesting(appId) {
    await this.waitFor('isFileManagerLoaded', appId, true);
    chrome.test.assertTrue(
        await this.callRemoteTestUtil('disableBannersForTesting', appId, []));
  }
}
