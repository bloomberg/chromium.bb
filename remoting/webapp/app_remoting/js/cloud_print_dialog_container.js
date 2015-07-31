/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @fileoverview
 * The application side of the application/cloud print dialog interface, used
 * by the application to exchange messages with the dialog.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * According to https://developer.chrome.com/apps/tags/webview, the level
 * ranges from 0 to 4. But in real life they are -1 (debug), 0 (log and info),
 * 1 (warn), and 2 (error). See crbug.com/499408
 *
 * @enum {number}
 */
remoting.ConsoleMessageLevel = {
  // console.debug
  VERBOSE: -1,
  // console.info or console.log
  INFO: 0,
  // console.warn
  WARNING: 1,
  //console.err
  ERROR: 2
};

(function() {

'use strict';

/**
 * Interval to refresh the access token used by the cloud print dialog.
 * Refreshing the token every 30 minutes should be good enough because the
 * access token will be valid for an hour.
 *
 * @const {number}
 */
var CLOUD_PRINT_DIALOG_TOKEN_REFRESH_INTERVAL_MS = 30 * 60 * 1000;

/**
 * @param {!Webview} webview The webview hosting the cloud cloud print dialog.
 * @param {remoting.WindowShape} windowShape
 * @param {base.WindowMessageDispatcher} windowMessageDispatcher
 * @param {!remoting.ConnectedView} connectedView
 * @constructor
 * @implements {remoting.WindowShape.ClientUI}
 * @implements {base.Disposable}
 */
remoting.CloudPrintDialogContainer =
    function(webview, windowShape, windowMessageDispatcher, connectedView) {
  /** @private {!Webview} */
  this.webview_ = webview;

  /** @private {remoting.WindowShape} */
  this.windowShape_ = windowShape;

  /** @private {!remoting.ConnectedView} */
  this.connectedView_ = connectedView;

  // TODO (weitaosu): This is only needed if the cloud print webview is on the
  // same page as the plugin. We should remove it if we move the webview to a
  // standalone message window.
  this.connectedView_.allowFocus(webview);

  /** @private {string} */
  this.accessToken_ = '';

  /** @private {base.WindowMessageDispatcher} */
  this.windowMessageDispatcher_ = windowMessageDispatcher;

  this.windowMessageDispatcher_.registerMessageHandler(
      'cloud-print-dialog', this.onMessage_.bind(this));

  /** @private {base.RepeatingTimer} */
  this.timer_ = new base.RepeatingTimer(
      this.cacheAccessToken_.bind(this),
      CLOUD_PRINT_DIALOG_TOKEN_REFRESH_INTERVAL_MS, true);

  // Adding a synchronous (blocking) handler so that the reqeust headers can
  // be modified on the spot.
  webview.request.onBeforeSendHeaders.addListener(
      this.setAuthHeaders_.bind(this),
      /** @type {!RequestFilter} */ ({urls: ['https://*.google.com/*']}),
      ['requestHeaders','blocking']);
};

remoting.CloudPrintDialogContainer.INJECTED_SCRIPT =
    '_modules/koejkfhmphamcgafjmkellhnekdkopod/cloud_print_dialog_injected.js';
remoting.CloudPrintDialogContainer.CLOUD_PRINT_DIALOG_URL =
    'https://www.google.com/cloudprint/dialog.html?';

remoting.CloudPrintDialogContainer.prototype.dispose = function() {
  this.windowMessageDispatcher_.unregisterMessageHandler('cloud-print-dialog');
  this.timer_.dispose();
};

/**
 * Timer callback to cache the access token.
 * @private
 */
remoting.CloudPrintDialogContainer.prototype.cacheAccessToken_ = function() {
  /** @type {remoting.CloudPrintDialogContainer} */
  var that = this;
  remoting.identity.getNewToken().then(
    function(/** string */ token){
      console.assert(token !== that.accessToken_);
      that.accessToken_ = token;
  }).catch(remoting.Error.handler(function(/** remoting.Error */ error) {
    console.log('Failed to refresh access token: ' + error.toString());
  }));
};

/**
 * @param {Array<{left: number, top: number, width: number, height: number}>}
 *     rects List of rectangles.
 */
remoting.CloudPrintDialogContainer.prototype.addToRegion = function(rects) {
  var rect =
      /** @type {ClientRect} */(this.webview_.getBoundingClientRect());
  rects.push({left: rect.left,
              top: rect.top,
              width: rect.width,
              height: rect.height});
};

/**
 * Show the cloud print dialog.
 */
remoting.CloudPrintDialogContainer.prototype.showCloudPrintUI = function() {
  // TODO (weitaosu): Considering showing the cloud print dialog in a separate
  // window or using remoting.Html5ModalDialog to show the modal dialog.
  this.webview_.hidden = false;
  this.windowShape_.registerClientUI(this);
  this.windowShape_.centerToDesktop(this.webview_);
};

/**
 * Hide the cloud print dialog.
 */
remoting.CloudPrintDialogContainer.prototype.hideCloudPrintUI = function() {
  this.webview_.hidden = true;
  this.windowShape_.unregisterClientUI(this);
  this.connectedView_.returnFocusToPlugin();
}

/**
 * Event handler to process messages from the webview.
 *
 * @param {Event} event
 * @private
 */
remoting.CloudPrintDialogContainer.prototype.onMessage_ = function(event) {
  var data = event.data;
  console.assert(typeof data === 'object' &&
                 data['source'] == 'cloud-print-dialog');

  switch (event.data['command']) {
    case 'cp-dialog-on-init::':
      // We actually never receive this message because the cloud print dialog
      // has already been initialized by the time the injected script finishes
      // executing.
      break;

    case 'cp-dialog-on-close::':
      this.hideCloudPrintUI();
      break;

    default:
      console.error('Unexpected message:', event.data['command'], event.data);
  }
};

/**
 * Retrieve the file specified by |fileName| in the app package and pass its
 * content to the onDone callback.
 *
 * @param {string} fileName Name of the file in the app package to be read.
 * @param {!function(string):void} onDone Callback to be invoked on success.
 * @param {!function(*):void} onError Callback to be invoked on failure.
 */
remoting.CloudPrintDialogContainer.readFile =
    function(fileName, onDone, onError) {
  var fileUrl = chrome.runtime.getURL(fileName);
  var xhr = new remoting.Xhr({ method: 'GET', url: fileUrl});

  xhr.start().then(function(/** !remoting.Xhr.Response */ response) {
    if (response.status == 200) {
      onDone(response.getText());
    } else {
      onError('xhr.status = ' + response.status);
    }
  });
};

/**
 * Lanunch the cloud print dialog to print the document.
 *
 * @param {string} title Title of the print job.
 * @param {string} type Type of the storage of the document (url, gdrive, etc).
 * @param {string} data Meaning of this field depends on the |type| parameter.
 */
remoting.CloudPrintDialogContainer.prototype.printDocument =
    function(title, type, data) {
  var dialogUrl = remoting.CloudPrintDialogContainer.CLOUD_PRINT_DIALOG_URL;
  dialogUrl += 'title=' + encodeURIComponent(title) + '&';

  switch (type) {
    case 'url':
      // 'data' should contain the url to the document to be printed.
      if (data.substr(0, 7) !== 'http://' && data.substr(0, 8) !== 'https://') {
        console.error('Bad URL: ' + data);
        return;
      }
      dialogUrl += 'type=url&url=' + encodeURIComponent(data);
      break;
    case 'google.drive':
      // 'data' should contain the doc id of the gdrive document to be printed.
      dialogUrl += 'type=google.drive&content=' + encodeURIComponent(data);
      break;
    default:
      console.error('Unknown content type for the printDocument command.');
      return;
  }

  // TODO (weitaosu); Consider moving the event registration to the ctor
  // and add unregistration in the dtor.
  var that = this;

  /** @param {string} script */
  var showDialog = function(script) {
    /** @type {Webview} */
    var webview = that.webview_;

    var sendHandshake = function() {
      webview.contentWindow.postMessage('app-remoting-handshake', '*');
    }

    /** @param {Event} event */
    var redirectConsoleOutput = function(event) {

      var e = /** @type {chrome.ConsoleMessageBrowserEvent} */ (event);
      var message = 'console message from webviwe: {' +
                    'level=' + e.level + ', ' +
                    'source=' + e.sourceId + ', ' +
                    'line=' + e.line + ', ' +
                    'message="' + e.message + '"}';

      switch (e['level']) {
        case remoting.ConsoleMessageLevel.VERBOSE:
          console.debug(message);
          break;
        case remoting.ConsoleMessageLevel.INFO:
          console.info(message);
          break;
        case remoting.ConsoleMessageLevel.WARNING:
          console.warn(message);
          break;
        case remoting.ConsoleMessageLevel.ERROR:
          console.error(message);
          break;
        default:
          console.error('unrecognized message level. ' + message);
          break;
      }
    }

    webview.addEventListener('consolemessage', redirectConsoleOutput);

    // Inject the script and send a handshake message to the cloud print dialog
    // after the injected script has been executed.
    webview.addEventListener("loadstart", function(event) {
        console.log('"loadstart" captured in webview containier.');
        // TODO (weitaosu): Consider switching to addContentScripts when M44
        // is released.
        webview.executeScript(
          {code: script + ' //# sourceURL=cloud_print_dialog_injected.js'},
          sendHandshake);
    });

    // We need to show the cloud print UI here because we will never receive
    // the 'cp-dialog-on-init::' message.
    webview.src = dialogUrl;
    that.showCloudPrintUI();
  }

  /** @param {*} errorMsg */
  var onError = function(errorMsg) {
    console.error('Failed to retrieve the script: ', errorMsg);
  }

  remoting.CloudPrintDialogContainer.readFile(
      remoting.CloudPrintDialogContainer.INJECTED_SCRIPT, showDialog, onError);
};

/**
 * Handler of the onBeforeSendHeaders event for the webview. It adds the auth
 * header to the request being send. Note that this handler is synchronous so
 * modifications to the requst must happen before it returns.
 *
 * @param {Object} details Details of the WebRequest.
 * @return {!BlockingResponse} The modified request headers.
 * @private
 */
remoting.CloudPrintDialogContainer.prototype.setAuthHeaders_ =
    function(details) {
  var url = /** @type {string} */ (details['url']);
  console.log('Setting auth token for request: ', url);

  var headers = /** @type {Array} */ (details['requestHeaders']) || [];
  if (this.accessToken_.length == 0) {
    console.error('No auth token available for the request: ', url);
    return /** @type {!BlockingResponse} */ ({'requestHeaders': headers});
  }

  // Make fresh copy of the headers array.
  var newHeaders = /** @type {Array} */ (headers.slice());
  newHeaders.push({
    'name': 'Authorization',
    'value': 'Bearer ' + this.accessToken_
  });

  return /** @type {!BlockingResponse} */ ({'requestHeaders': newHeaders});
};

})();
