// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * SuggestAppsDialog contains a list box to select an app to be opened the file
 * with. This dialog should be used as action picker for file operations.
 */

/**
 * The width of the widget (in pixel).
 * @type {number}
 * @const
 */
var WEBVIEW_WIDTH = 735;
/**
 * The height of the widget (in pixel).
 * @type {number}
 * @const
 */
var WEBVIEW_HEIGHT = 480;

/**
 * The URL of the widget.
 * @type {string}
 * @const
 */
var CWS_WIDGET_URL =
    'https://clients5.google.com/webstore/wall/cros-widget-container';
/**
 * The origin of the widget.
 * @type {string}
 * @const
 */
var CWS_WIDGET_ORIGIN = 'https://clients5.google.com';

/**
 * Creates dialog in DOM tree.
 *
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @param {Object} state Static state of suggest app dialog.
 * @constructor
 * @extends {FileManagerDialogBase}
 */
function SuggestAppsDialog(parentNode, state) {
  FileManagerDialogBase.call(this, parentNode);

  this.frame_.id = 'suggest-app-dialog';

  this.webviewContainer_ = this.document_.createElement('div');
  this.webviewContainer_.id = 'webview-container';
  this.webviewContainer_.style.width = WEBVIEW_WIDTH + 'px';
  this.webviewContainer_.style.height = WEBVIEW_HEIGHT + 'px';
  this.frame_.insertBefore(this.webviewContainer_, this.text_.nextSibling);

  var spinnerLayer = this.document_.createElement('div');
  spinnerLayer.className = 'spinner-layer';
  this.webviewContainer_.appendChild(spinnerLayer);

  this.buttons_ = this.document_.createElement('div');
  this.buttons_.id = 'buttons';
  this.frame_.appendChild(this.buttons_);

  this.webstoreButton_ = this.document_.createElement('div');
  this.webstoreButton_.id = 'webstore-button';
  this.webstoreButton_.innerHTML = str('SUGGEST_DIALOG_LINK_TO_WEBSTORE');
  this.webstoreButton_.addEventListener(
      'click', this.onWebstoreLinkClicked_.bind(this));
  this.buttons_.appendChild(this.webstoreButton_);

  this.initialFocusElement_ = this.webviewContainer_;

  this.webview_ = null;
  this.accessToken_ = null;
  this.widgetUrl_ =
      state.overrideCwsContainerUrlForTest || CWS_WIDGET_URL;
  this.widgetOrigin_ =
      state.overrideCwsContainerOriginForTest || CWS_WIDGET_ORIGIN;

  this.extension_ = null;
  this.mime_ = null;
  this.installingItemId_ = null;
  this.state_ = SuggestAppsDialog.State.UNINITIALIZED;

  this.initializationTask_ = new AsyncUtil.Group();
  this.initializationTask_.add(this.retrieveAuthorizeToken_.bind(this));
  this.initializationTask_.run();
}

SuggestAppsDialog.prototype = {
  __proto__: FileManagerDialogBase.prototype
};

/**
 * @enum {string}
 * @const
 */
SuggestAppsDialog.State = {
  UNINITIALIZED: 'SuggestAppsDialog.State.UNINITIALIZED',
  INITIALIZING: 'SuggestAppsDialog.State.INITIALIZING',
  INITIALIZE_FAILED_CLOSING:
      'SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING',
  INITIALIZED: 'SuggestAppsDialog.State.INITIALIZED',
  INSTALLING: 'SuggestAppsDialog.State.INSTALLING',
  INSTALLED_CLOSING: 'SuggestAppsDialog.State.INSTALLED_CLOSING',
  OPENING_WEBSTORE_CLOSING: 'SuggestAppsDialog.State.OPENING_WEBSTORE_CLOSING',
  CANCELED_CLOSING: 'SuggestAppsDialog.State.CANCELED_CLOSING'
};
Object.freeze(SuggestAppsDialog.State);

/**
 * @enum {string}
 * @const
 */
SuggestAppsDialog.Result = {
  // Install is done. The install app should be opened.
  INSTALL_SUCCESSFUL: 'SuggestAppsDialog.Result.INSTALL_SUCCESSFUL',
  // User cancelled the suggest app dialog. No message should be shown.
  USER_CANCELL: 'SuggestAppsDialog.Result.USER_CANCELL',
  // User clicked the link to web store so the dialog is closed.
  WEBSTORE_LINK_OPENED: 'SuggestAppsDialog.Result.WEBSTORE_LINK_OPENED',
  // Failed to load the widget. Error message should be shown.
  FAILED: 'SuggestAppsDialog.Result.FAILED'
};
Object.freeze(SuggestAppsDialog.Result);

/**
 * @override
 */
SuggestAppsDialog.prototype.onInputFocus = function() {
  this.webviewContainer_.select();
};

/**
 * Injects headers into the passed request.
 *
 * @param {Event} e Request event.
 * @return {{requestHeaders: HttpHeaders}} Modified headers.
 * @private
 */
SuggestAppsDialog.prototype.authorizeRequest_ = function(e) {
  e.requestHeaders.push({
    name: 'Authorization',
    value: 'Bearer ' + this.accessToken_
  });
  return {requestHeaders: e.requestHeaders};
};

/**
 * Retrieves the authorize token. This method should be called in
 * initialization of the dialog.
 *
 * @param {function()} callback Called when the token is retrieved.
 * @private
 */
SuggestAppsDialog.prototype.retrieveAuthorizeToken_ = function(callback) {
  if (window.IN_TEST) {
    // In test, use a dummy string as token. This must be a non-empty string.
    this.accessToken_ = 'DUMMY_ACCESS_TOKEN_FOR_TEST';
  }

  if (this.accessToken_) {
    callback();
    return;
  }

  // Fetch or update the access token.
  chrome.fileManagerPrivate.requestWebStoreAccessToken(
      function(accessToken) {
        // In case of error, this.accessToken_ will be set to null.
        this.accessToken_ = accessToken;
        callback();
      }.bind(this));
};

/**
 * Dummy function for SuggestAppsDialog.show() not to be called unintentionally.
 */
SuggestAppsDialog.prototype.show = function() {
  console.error('SuggestAppsDialog.show() shouldn\'t be called directly.');
};

/**
 * Shows suggest-apps dialog by file extension and mime.
 *
 * @param {string} extension Extension of the file.
 * @param {string} mime Mime of the file.
 * @param {function(boolean)} onDialogClosed Called when the dialog is closed.
 *     The argument is the result of installation: true if an app is installed,
 *     false otherwise.
 */
SuggestAppsDialog.prototype.showByExtensionAndMime =
    function(extension, mime, onDialogClosed) {
  this.text_.hidden = true;
  this.dialogText_ = '';
  this.showInternal_(null, extension, mime, onDialogClosed);
};

/**
 * Shows suggest-apps dialog by the filename.
 *
 * @param {string} filename Filename (without extension) of the file.
 * @param {function(boolean)} onDialogClosed Called when the dialog is closed.
 *     The argument is the result of installation: true if an app is installed,
 *     false otherwise.
 */
SuggestAppsDialog.prototype.showByFilename =
    function(filename, onDialogClosed) {
  this.text_.hidden = false;
  this.dialogText_ = str('SUGGEST_DIALOG_MESSAGE_FOR_EXECUTABLE');
  this.showInternal_(filename, null, null, onDialogClosed);
};

/**
 * Internal method to show a dialog. This should be called only from 'Suggest.
 * appDialog.showXxxx()' functions.
 *
 * @param {string} filename Filename (without extension) of the file.
 * @param {string} extension Extension of the file.
 * @param {string} mime Mime of the file.
 * @param {function(boolean)} onDialogClosed Called when the dialog is closed.
 *     The argument is the result of installation: true if an app is installed,
 *     false otherwise.
 *     @private
 */
SuggestAppsDialog.prototype.showInternal_ =
    function(filename, extension, mime, onDialogClosed) {
  if (this.state_ != SuggestAppsDialog.State.UNINITIALIZED) {
    console.error('Invalid state.');
    return;
  }

  this.extension_ = extension;
  this.mimeType_ = mime;
  this.onDialogClosed_ = onDialogClosed;
  this.state_ = SuggestAppsDialog.State.INITIALIZING;

  SuggestAppsDialog.Metrics.recordShowDialog();
  SuggestAppsDialog.Metrics.startLoad();

  // Makes it sure that the initialization is completed.
  this.initializationTask_.run(function() {
    if (!this.accessToken_) {
      this.state_ = SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING;
      this.onHide_();
      return;
    }

    var title = str('SUGGEST_DIALOG_TITLE');
    var show = this.dialogText_ ?
        FileManagerDialogBase.prototype.showTitleAndTextDialog.call(
            this, title, this.dialogText_) :
        FileManagerDialogBase.prototype.showTitleOnlyDialog.call(
            this, title);
    if (!show) {
      console.error('SuggestAppsDialog can\'t be shown');
      this.state_ = SuggestAppsDialog.State.UNINITIALIZED;
      this.onHide();
      return;
    }

    this.webview_ = this.document_.createElement('webview');
    this.webview_.id = 'cws-widget';
    this.webview_.partition = 'persist:cwswidgets';
    this.webview_.style.width = WEBVIEW_WIDTH + 'px';
    this.webview_.style.height = WEBVIEW_HEIGHT + 'px';
    this.webview_.request.onBeforeSendHeaders.addListener(
        this.authorizeRequest_.bind(this),
        {urls: [this.widgetOrigin_ + '/*']},
        ['blocking', 'requestHeaders']);
    this.webview_.addEventListener('newwindow', function(event) {
      // Discard the window object and reopen in an external window.
      event.window.discard();
      util.visitURL(event.targetUrl);
      event.preventDefault();
    });
    this.webviewContainer_.appendChild(this.webview_);

    this.frame_.classList.add('show-spinner');

    this.webviewClient_ = new CWSContainerClient(
        this.webview_,
        extension, mime, filename,
        WEBVIEW_WIDTH, WEBVIEW_HEIGHT,
        this.widgetUrl_, this.widgetOrigin_);
    this.webviewClient_.addEventListener(CWSContainerClient.Events.LOADED,
                                         this.onWidgetLoaded_.bind(this));
    this.webviewClient_.addEventListener(CWSContainerClient.Events.LOAD_FAILED,
                                         this.onWidgetLoadFailed_.bind(this));
    this.webviewClient_.addEventListener(
        CWSContainerClient.Events.REQUEST_INSTALL,
        this.onInstallRequest_.bind(this));
    this.webviewClient_.load();
  }.bind(this));
};

/**
 * Called when the 'See more...' link is clicked to be navigated to Webstore.
 * @param {Event} e Event.
 * @private
 */
SuggestAppsDialog.prototype.onWebstoreLinkClicked_ = function(e) {
  var webStoreUrl =
      FileTasks.createWebStoreLink(this.extension_, this.mimeType_);
  util.visitURL(webStoreUrl);
  this.state_ = SuggestAppsDialog.State.OPENING_WEBSTORE_CLOSING;
  this.hide();
};

/**
 * Called when the widget is loaded successfully.
 * @param {Event} event Event.
 * @private
 */
SuggestAppsDialog.prototype.onWidgetLoaded_ = function(event) {
  SuggestAppsDialog.Metrics.finishLoad();
  SuggestAppsDialog.Metrics.recordLoad(
      SuggestAppsDialog.Metrics.LOAD.SUCCEEDED);

  this.frame_.classList.remove('show-spinner');
  this.state_ = SuggestAppsDialog.State.INITIALIZED;

  this.webview_.focus();
};

/**
 * Called when the widget is failed to load.
 * @param {Event} event Event.
 * @private
 */
SuggestAppsDialog.prototype.onWidgetLoadFailed_ = function(event) {
  SuggestAppsDialog.Metrics.recordLoad(SuggestAppsDialog.Metrics.LOAD.FAILURE);

  this.frame_.classList.remove('show-spinner');
  this.state_ = SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING;

  this.hide();
};

/**
 * Called when the connection status is changed.
 * @param {VolumeManagerCommon.DriveConnectionType} connectionType Current
 *     connection type.
 */
SuggestAppsDialog.prototype.onDriveConnectionChanged =
    function(connectionType) {
  if (this.state_ !== SuggestAppsDialog.State.UNINITIALIZED &&
      connectionType === VolumeManagerCommon.DriveConnectionType.OFFLINE) {
    this.state_ = SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING;
    this.hide();
  }
};

/**
 * Called when receiving the install request from the webview client.
 * @param {Event} e Event.
 * @private
 */
SuggestAppsDialog.prototype.onInstallRequest_ = function(e) {
  var itemId = e.itemId;
  this.installingItemId_ = itemId;

  this.appInstaller_ = new AppInstaller(itemId);
  this.appInstaller_.install(this.onInstallCompleted_.bind(this));

  this.frame_.classList.add('show-spinner');
  this.state_ = SuggestAppsDialog.State.INSTALLING;
};

/**
 * Called when the installation is completed from the app installer.
 * @param {AppInstaller.Result} result Result of the installation.
 * @param {string} error Detail of the error.
 * @private
 */
SuggestAppsDialog.prototype.onInstallCompleted_ = function(result, error) {
  var success = (result === AppInstaller.Result.SUCCESS);

  this.frame_.classList.remove('show-spinner');
  this.state_ = success ?
                SuggestAppsDialog.State.INSTALLED_CLOSING :
                SuggestAppsDialog.State.INITIALIZED;  // Back to normal state.
  this.webviewClient_.onInstallCompleted(success, this.installingItemId_);
  this.installingItemId_ = null;

  switch (result) {
    case AppInstaller.Result.SUCCESS:
      SuggestAppsDialog.Metrics.recordInstall(
          SuggestAppsDialog.Metrics.INSTALL.SUCCESS);
      this.hide();
      break;
    case AppInstaller.Result.CANCELLED:
      SuggestAppsDialog.Metrics.recordInstall(
          SuggestAppsDialog.Metrics.INSTALL.CANCELLED);
      // User cancelled the installation. Do nothing.
      break;
    case AppInstaller.Result.ERROR:
      SuggestAppsDialog.Metrics.recordInstall(
          SuggestAppsDialog.Metrics.INSTALL.FAILED);
      fileManager.error.show(str('SUGGEST_DIALOG_INSTALLATION_FAILED'));
      break;
  }
};

/**
 * @override
 */
SuggestAppsDialog.prototype.hide = function(opt_originalOnHide) {
  switch (this.state_) {
    case SuggestAppsDialog.State.INSTALLING:
      // Install is being aborted. Send the failure result.
      // Cancels the install.
      if (this.webviewClient_)
        this.webviewClient_.onInstallCompleted(false, this.installingItemId_);
      this.installingItemId_ = null;

      // Assumes closing the dialog as canceling the install.
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      break;
    case SuggestAppsDialog.State.INITIALIZING:
      SuggestAppsDialog.Metrics.recordLoad(
          SuggestAppsDialog.Metrics.LOAD.CANCELLED);
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      break;
    case SuggestAppsDialog.State.INSTALLED_CLOSING:
    case SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING:
    case SuggestAppsDialog.State.OPENING_WEBSTORE_CLOSING:
      // Do nothing.
      break;
    case SuggestAppsDialog.State.INITIALIZED:
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      break;
    default:
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      console.error('Invalid state.');
  }

  if (this.webviewClient_) {
    this.webviewClient_.dispose();
    this.webviewClient_ = null;
  }

  this.webviewContainer_.removeChild(this.webview_);
  this.webview_ = null;
  this.extension_ = null;
  this.mime_ = null;

  FileManagerDialogBase.prototype.hide.call(
      this,
      this.onHide_.bind(this, opt_originalOnHide));
};

/**
 * @param {function()=} opt_originalOnHide Original onHide function passed to
 *     SuggestAppsDialog.hide().
 * @private
 */
SuggestAppsDialog.prototype.onHide_ = function(opt_originalOnHide) {
  // Calls the callback after the dialog hides.
  if (opt_originalOnHide)
    opt_originalOnHide();

  var result;
  switch (this.state_) {
    case SuggestAppsDialog.State.INSTALLED_CLOSING:
      result = SuggestAppsDialog.Result.INSTALL_SUCCESSFUL;
      SuggestAppsDialog.Metrics.recordCloseDialog(
          SuggestAppsDialog.Metrics.CLOSE_DIALOG.ITEM_INSTALLED);
      break;
    case SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING:
      result = SuggestAppsDialog.Result.FAILED;
      break;
    case SuggestAppsDialog.State.CANCELED_CLOSING:
      result = SuggestAppsDialog.Result.USER_CANCELL;
      SuggestAppsDialog.Metrics.recordCloseDialog(
          SuggestAppsDialog.Metrics.CLOSE_DIALOG.USER_CANCELL);
      break;
    case SuggestAppsDialog.State.OPENING_WEBSTORE_CLOSING:
      result = SuggestAppsDialog.Result.WEBSTORE_LINK_OPENED;
      SuggestAppsDialog.Metrics.recordCloseDialog(
          SuggestAppsDialog.Metrics.CLOSE_DIALOG.WEB_STORE_LINK);
      break;
    default:
      result = SuggestAppsDialog.Result.USER_CANCELL;
      SuggestAppsDialog.Metrics.recordCloseDialog(
          SuggestAppsDialog.Metrics.CLOSE_DIALOG.UNKNOWN_ERROR);
      console.error('Invalid state.');
  }
  this.state_ = SuggestAppsDialog.State.UNINITIALIZED;

  this.onDialogClosed_(result);
};

/**
 * Utility methods and constants to record histograms.
 */
SuggestAppsDialog.Metrics = Object.freeze({
  LOAD: Object.freeze({
    SUCCEEDED: 0,
    CANCELLED: 1,
    FAILED: 2,
  }),

  /**
   * @param {SuggestAppsDialog.Metrics.LOAD} result Result of load.
   */
  recordLoad: function(result) {
    if (0 <= result && result < 3)
      metrics.recordEnum('SuggestApps.Load', result, 3);
  },

  CLOSE_DIALOG: Object.freeze({
    UNKOWN_ERROR: 0,
    ITEM_INSTALLED: 1,
    USER_CANCELLED: 2,
    WEBSTORE_LINK_OPENED: 3,
  }),

  /**
   * @param {SuggestAppsDialog.Metrics.CLOSE_DIALOG} reason Reason of closing
   * dialog.
   */
  recordCloseDialog: function(reason) {
    if (0 <= reason && reason < 4)
      metrics.recordEnum('SuggestApps.CloseDialog', reason, 4);
  },

  INSTALL: Object.freeze({
    SUCCEEDED: 0,
    CANCELLED: 1,
    FAILED: 2,
  }),

  /**
   * @param {SuggestAppsDialog.Metrics.INSTALL} result Result of installation.
   */
  recordInstall: function(result) {
    if (0 <= result && result < 3)
      metrics.recordEnum('SuggestApps.Install', result, 3);
  },

  recordShowDialog: function() {
    metrics.recordUserAction('SuggestApps.ShowDialog');
  },

  startLoad: function() {
    metrics.startInterval('SuggestApps.LoadTime');
  },

  finishLoad: function() {
    metrics.recordInterval('SuggestApps.LoadTime');
  },
});
