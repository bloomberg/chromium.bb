// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * CWSWidgetContainer contains a Chrome Web Store widget that displays list of
 * apps that satisfy certain constraints (e.g. fileHandler apps that can handle
 * files with specific file extension or MIME type) and enables the user to
 * install apps directly from it.
 * CWSWidgetContainer implements client side of the widget, which handles
 * widget loading and app installation.
 */

/**
 * The width of the widget (in pixels)
 * @type {number}
 * @const
 */
var WEBVIEW_WIDTH = 735;

/**
 * The height of the widget (in pixels).
 * @type {number}
 * @const
 */
var WEBVIEW_HEIGHT = 480;

/**
 * The URL of the widget showing suggested apps.
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
 * Creates the widget container element in DOM tree.
 *
 * @param {!HTMLDocument} document The document to contain this container.
 * @param {!HTMLElement} parentNode Node to be parent for this container.
 * @param {!SuggestAppDialogState} state Static state of suggest app dialog.
 * @constructor
 */
function CWSWidgetContainer(document, parentNode, state) {
  /**
   * The document that will contain the container.
   * @const {!HTMLDocument}
   * @private
   */
  this.document_ = document;

  /**
   * The element containing the widget webview.
   * @type {!Element}
   * @private
   */
  this.webviewContainer_ = document.createElement('div');
  this.webviewContainer_.id = 'webview-container';
  this.webviewContainer_.style.width = WEBVIEW_WIDTH + 'px';
  this.webviewContainer_.style.height = WEBVIEW_HEIGHT + 'px';
  parentNode.appendChild(this.webviewContainer_);

  /**
   * Element showing spinner layout in place of Web Store widget.
   * @type {!Element}
   */
  var spinnerLayer = document.createElement('div');
  spinnerLayer.className = 'spinner-layer';
  this.webviewContainer_.appendChild(spinnerLayer);

  /**
   * The widget container's button strip.
   * @type {!Element}
   */
  var buttons = document.createElement('div');
  buttons.id = 'buttons';
  parentNode.appendChild(buttons);

  /**
   * Button that opens the Webstore URL.
   * @const {!Element}
   * @private
   */
  this.webstoreButton_ = document.createElement('div');
  this.webstoreButton_.hidden = true;
  this.webstoreButton_.id = 'webstore-button';
  this.webstoreButton_.setAttribute('role', 'button');
  this.webstoreButton_.tabIndex = 0;

  /**
   * Icon for the Webstore button.
   * @type {!Element}
   */
  var webstoreButtonIcon = this.document_.createElement('span');
  webstoreButtonIcon.id = 'webstore-button-icon';
  this.webstoreButton_.appendChild(webstoreButtonIcon);

  /**
   * The label for the Webstore button.
   * @type {!Element}
   */
  var webstoreButtonLabel = this.document_.createElement('span');
  webstoreButtonLabel.id = 'webstore-button-label';
  webstoreButtonLabel.textContent = str('SUGGEST_DIALOG_LINK_TO_WEBSTORE');
  this.webstoreButton_.appendChild(webstoreButtonLabel);

  this.webstoreButton_.addEventListener(
      'click', this.onWebstoreLinkActivated_.bind(this));
  this.webstoreButton_.addEventListener(
      'keydown', this.onWebstoreLinkKeyDown_.bind(this));

  buttons.appendChild(this.webstoreButton_);

  /**
   * The webview element containing the Chrome Web Store widget.
   * @type {?WebView}
   * @private
   */
  this.webview_ = null;

  /**
   * The Chrome Web Store widget URL.
   * @const {string}
   * @private
   */
  this.widgetUrl_ = state.overrideCwsContainerUrlForTest || CWS_WIDGET_URL;

  /**
   * The Chrome Web Store widget origin.
   * @const {string}
   * @private
   */
  this.widgetOrigin_ = state.overrideCwsContainerOriginForTest ||
      CWS_WIDGET_ORIGIN;

  /**
   * Map of options for the widget.
   * @type {?Object.<string, *>}
   * @private
   */
  this.options_ = null;

  /**
   * The ID of the item being installed. Null if no items are being installed.
   * @type {?string}
   * @private
   */
  this.installingItemId_ = null;

  /**
   * The ID of the the installed item. Null if no item was installed.
   * @type {?string}
   * @private
   */
  this.installedItemId_ = null;

  /**
   * The current widget state.
   * @type {CWSWidgetContainer.State}
   * @private
   */
  this.state_ = CWSWidgetContainer.State.UNINITIALIZED;

  /**
   * The Chrome Web Store access token to be used when communicating with the
   * Chrome Web Store widget.
   * @type {?string}
   * @private
   */
  this.accessToken_ = null;

  /**
   * Called when the Chrome Web Store widget is done. It resolves the promise
   * returned by {@code this.start()}.
   * @type {?function(CWSWidgetContainer.ResolveReason)}
   * @private
   */
  this.resolveStart_ = null;

  /**
   * Promise for retriving {@code this.accessToken_}.
   * @type {Promise.<string>}
   * @private
   */
  this.tokenGetter_ = this.createTokenGetter_();
}

/**
 * @enum {string}
 * @private
 */
CWSWidgetContainer.State = {
  UNINITIALIZED: 'CWSWidgetContainer.State.UNINITIALIZED',
  GETTING_ACCESS_TOKEN: 'CWSWidgetContainer.State.GETTING_ACCESS_TOKEN',
  ACCESS_TOKEN_READY: 'CWSWidgetContainer.State.ACCESS_TOKEN_READY',
  INITIALIZING: 'CWSWidgetContainer.State.INITIALIZING',
  INITIALIZE_FAILED_CLOSING:
      'CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING',
  INITIALIZED: 'CWSWidgetContainer.State.INITIALIZED',
  INSTALLING: 'CWSWidgetContainer.State.INSTALLING',
  INSTALLED_CLOSING: 'CWSWidgetContainer.State.INSTALLED_CLOSING',
  OPENING_WEBSTORE_CLOSING: 'CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING',
  CANCELED_CLOSING: 'CWSWidgetContainer.State.CANCELED_CLOSING'
};
Object.freeze(CWSWidgetContainer.State);

/**
 * @enum {string}
 * @const
 */
CWSWidgetContainer.Result = {
  /** Install is done. The install app should be opened. */
  INSTALL_SUCCESSFUL: 'CWSWidgetContainer.Result.INSTALL_SUCCESSFUL',
  /** User cancelled the suggest app dialog. No message should be shown. */
  USER_CANCEL: 'CWSWidgetContainer.Result.USER_CANCEL',
  /** User clicked the link to web store so the dialog is closed. */
  WEBSTORE_LINK_OPENED: 'CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED',
  /** Failed to load the widget. Error message should be shown. */
  FAILED: 'CWSWidgetContainer.Result.FAILED'
};
Object.freeze(CWSWidgetContainer.Result);

/**
 * The reason due to which the container is resolving {@code this.start}
 * promise.
 * @enum {string}
 */
CWSWidgetContainer.ResolveReason = {
  /** The widget container ended up in its final state. */
  DONE: 'CWSWidgetContainer.ResolveReason.DONE',
  /** The widget container is being reset. */
  RESET: 'CWSWidgetContainer.CloserReason.RESET'
};
Object.freeze(CWSWidgetContainer.ResolveReason);

/**
 * @return {!Element} The element that should be focused initially.
 */
CWSWidgetContainer.prototype.getInitiallyFocusedElement = function() {
  return this.webviewContainer_;
};

/**
 * Injects headers into the passed request.
 *
 * @param {!Object} e Request event.
 * @return {!BlockingResponse} Modified headers.
 * @private
 */
CWSWidgetContainer.prototype.authorizeRequest_ = function(e) {
  e.requestHeaders.push({
    name: 'Authorization',
    value: 'Bearer ' + this.accessToken_
  });
  return /** @type {!BlockingResponse}*/ ({requestHeaders: e.requestHeaders});
};

/**
 * Retrieves the authorize token.
 * @return {Promise.<string>} The promise with the retrived access token.
 * @private
 */
CWSWidgetContainer.prototype.createTokenGetter_ = function() {
  return new Promise(function(resolve, reject) {
    if (window.IN_TEST) {
      // In test, use a dummy string as token. This must be a non-empty string.
      resolve('DUMMY_ACCESS_TOKEN_FOR_TEST');
      return;
    }

    // Fetch or update the access token.
    chrome.fileManagerPrivate.requestWebStoreAccessToken(
        function(accessToken) {
          if (chrome.runtime.lastError) {
            reject('Error retriveing Web Store access token: ' +
                           chrome.runtime.lastError.message);
          }
          resolve(accessToken)
        });
  });
};

/**
 * Ensures that the widget container is in the state where it can properly
 * handle showing the Chrome Web Store webview.
 * @return {Promise} Resolved when the container is ready to be used.
 */
CWSWidgetContainer.prototype.ready = function() {
  return new Promise(function(resolve, reject) {
    if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED) {
      reject('Invalid state.');
      return;
    }

    CWSWidgetContainer.Metrics.recordShowDialog();
    CWSWidgetContainer.Metrics.startLoad();

    this.state_ = CWSWidgetContainer.State.GETTING_ACCESS_TOKEN;

    this.tokenGetter_.then(function(accessToken) {
      this.state_ = CWSWidgetContainer.State.ACCESS_TOKEN_READY;
      this.accessToken_ = accessToken;
      resolve();
    }.bind(this), function(error) {
      this.state_ = CWSWidgetContainer.State.UNINITIALIZED;
      reject('Failed to get Web Store access token: ' + error);
    }.bind(this));
  }.bind(this));
};

/**
 * Initializes and starts loading the Chrome Web Store widget webview.
 * Must not be called before {@code this.ready()} is resolved.
 *
 * @param {!Object<string, *>} options Map of options for the dialog.
 * @param {?string} webStoreUrl Url for more results. Null if not supported.
 * @return {Promise.<CWSWidgetContainer.ResolveReason>} Resolved when app
 *     installation is done, or the installation is cancelled.
 */
CWSWidgetContainer.prototype.start = function(options,  webStoreUrl) {
  return new Promise(function(resolve, reject) {
    if (this.state_ !== CWSWidgetContainer.State.ACCESS_TOKEN_READY) {
      this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
      reject('Invalid state in |start|.');
      return;
    }

    if (!this.accessToken_) {
      this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
      reject('No access token.');
      return;
    }

    this.resolveStart_ = resolve;

    this.state_ = CWSWidgetContainer.State.INITIALIZING;

    this.webStoreUrl_ = webStoreUrl;
    this.options_ = options;

    this.webstoreButton_.hidden = (webStoreUrl === null);

    this.webview_ =
        /** @type {!WebView} */(this.document_.createElement('webview'));
    this.webview_.id = 'cws-widget';
    this.webview_.partition = 'persist:cwswidgets';
    this.webview_.style.width = WEBVIEW_WIDTH + 'px';
    this.webview_.style.height = WEBVIEW_HEIGHT + 'px';
    this.webview_.request.onBeforeSendHeaders.addListener(
        this.authorizeRequest_.bind(this),
        /** @type {!RequestFilter}*/ ({urls: [this.widgetOrigin_ + '/*']}),
        ['blocking', 'requestHeaders']);
    this.webview_.addEventListener('newwindow', function(event) {
      event = /** @type {NewWindowEvent} */ (event);
      // Discard the window object and reopen in an external window.
      event.window.discard();
      window.open(event.targetUrl);
      event.preventDefault();
    });
    this.webviewContainer_.appendChild(this.webview_);

    this.webviewContainer_.classList.add('show-spinner');

    this.webviewClient_ = new CWSContainerClient(
        this.webview_,
        WEBVIEW_WIDTH,
        WEBVIEW_HEIGHT,
        this.widgetUrl_,
        this.widgetOrigin_,
        this.options_);
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
 * Called when the 'See more...' button is activated. It opens
 * {@code this.webstoreUrl_}.
 * @param {Event} e The event that activated the link. Either mouse click or
 *     key down event.
 * @private
 */
CWSWidgetContainer.prototype.onWebstoreLinkActivated_ = function(e) {
  if (!this.webStoreUrl_)
    return;
  util.visitURL(this.webStoreUrl_);
  this.state_ = CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING;
  this.reportDone_();
};

/**
 * Key down event handler for webstore button element. If the key is enter, it
 * activates the button.
 * @param {Event} e The event
 * @private
 */
CWSWidgetContainer.prototype.onWebstoreLinkKeyDown_ = function(e) {
  if (e.keyCode !== 13 /* Enter */)
    return;
  this.onWebstoreLinkActivated_(e);
};

/**
 * Called when the widget is loaded successfully.
 * @param {Event} event Event.
 * @private
 */
CWSWidgetContainer.prototype.onWidgetLoaded_ = function(event) {
  CWSWidgetContainer.Metrics.finishLoad();
  CWSWidgetContainer.Metrics.recordLoad(
      CWSWidgetContainer.Metrics.LOAD.SUCCEEDED);

  this.webviewContainer_.classList.remove('show-spinner');
  this.state_ = CWSWidgetContainer.State.INITIALIZED;

  this.webview_.focus();
};

/**
 * Called when the widget is failed to load.
 * @param {Event} event Event.
 * @private
 */
CWSWidgetContainer.prototype.onWidgetLoadFailed_ = function(event) {
  CWSWidgetContainer.Metrics.recordLoad(CWSWidgetContainer.Metrics.LOAD.FAILED);

  this.webviewContainer_.classList.remove('show-spinner');
  this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
  this.reportDone_();
};

/**
 * Called when the connection status is changed to offline.
 */
CWSWidgetContainer.prototype.onConnectionLost = function() {
  if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED) {
    this.state_ = CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING;
    this.reportDone_();
  }
};

/**
 * Called when receiving the install request from the webview client.
 * @param {Event} e Event.
 * @private
 */
CWSWidgetContainer.prototype.onInstallRequest_ = function(e) {
  var itemId = e.itemId;
  this.installingItemId_ = itemId;

  this.appInstaller_ = new AppInstaller(itemId);
  this.appInstaller_.install(this.onInstallCompleted_.bind(this));

  this.webviewContainer_.classList.add('show-spinner');
  this.state_ = CWSWidgetContainer.State.INSTALLING;
};

/**
 * Called when the installation is completed from the app installer.
 * @param {AppInstaller.Result} result Result of the installation.
 * @param {string} error Detail of the error.
 * @private
 */
CWSWidgetContainer.prototype.onInstallCompleted_ = function(result, error) {
  var success = (result === AppInstaller.Result.SUCCESS);

  this.webviewContainer_.classList.remove('show-spinner');
  this.state_ = success ?
                CWSWidgetContainer.State.INSTALLED_CLOSING :
                CWSWidgetContainer.State.INITIALIZED;  // Back to normal state.
  this.webviewClient_.onInstallCompleted(success, this.installingItemId_);
  this.installedItemId_ = this.installingItemId_;
  this.installingItemId_ = null;

  switch (result) {
    case AppInstaller.Result.SUCCESS:
      CWSWidgetContainer.Metrics.recordInstall(
          CWSWidgetContainer.Metrics.INSTALL.SUCCEEDED);
      this.reportDone_();
      break;
    case AppInstaller.Result.CANCELLED:
      CWSWidgetContainer.Metrics.recordInstall(
          CWSWidgetContainer.Metrics.INSTALL.CANCELLED);
      // User cancelled the installation. Do nothing.
      break;
    case AppInstaller.Result.ERROR:
      CWSWidgetContainer.Metrics.recordInstall(
          CWSWidgetContainer.Metrics.INSTALL.FAILED);
      // TODO(tbarzic): Remove dialog showing call from this class.
      fileManager.ui.errorDialog.show(
          str('SUGGEST_DIALOG_INSTALLATION_FAILED'),
          null,
          null,
          null);
      break;
  }
};

/**
 * Resolves the promise returned by {@code this.start} when widget is done with
 * installing apps.
 * @private
 */
CWSWidgetContainer.prototype.reportDone_ = function() {
  if (this.resolveStart_)
    this.resolveStart_(CWSWidgetContainer.ResolveReason.DONE);
  this.resolveStart_ = null;
};

/**
 * Finalizes the widget container state and returns the final app instalation
 * result. The widget should not be used after calling this. If called before
 * promise returned by {@code this.start} is resolved, the reported result will
 * be as if the widget was cancelled.
 * @return {{result: CWSWidgetContainer.Result, installedItemId: ?string}}
 */
CWSWidgetContainer.prototype.finalizeAndGetResult = function() {
  switch (this.state_) {
    case CWSWidgetContainer.State.INSTALLING:
      // Install is being aborted. Send the failure result.
      // Cancels the install.
      if (this.webviewClient_)
        this.webviewClient_.onInstallCompleted(false, this.installingItemId_);
      this.installingItemId_ = null;

      // Assumes closing the dialog as canceling the install.
      this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
      break;
    case CWSWidgetContainer.State.GETTING_ACCESS_TOKEN:
    case CWSWidgetContainer.State.ACCESS_TOKEN_READY:
    case CWSWidgetContainer.State.INITIALIZING:
      CWSWidgetContainer.Metrics.recordLoad(
          CWSWidgetContainer.Metrics.LOAD.CANCELLED);
      this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
      break;
    case CWSWidgetContainer.State.INSTALLED_CLOSING:
    case CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING:
    case CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING:
      // Do nothing.
      break;
    case CWSWidgetContainer.State.INITIALIZED:
      this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
      break;
    default:
      this.state_ = CWSWidgetContainer.State.CANCELED_CLOSING;
      console.error('Invalid state.');
  }

  var result;
  switch (this.state_) {
    case CWSWidgetContainer.State.INSTALLED_CLOSING:
      result = CWSWidgetContainer.Result.INSTALL_SUCCESSFUL;
      CWSWidgetContainer.Metrics.recordCloseDialog(
          CWSWidgetContainer.Metrics.CLOSE_DIALOG.ITEM_INSTALLED);
      break;
    case CWSWidgetContainer.State.INITIALIZE_FAILED_CLOSING:
      result = CWSWidgetContainer.Result.FAILED;
      break;
    case CWSWidgetContainer.State.CANCELED_CLOSING:
      result = CWSWidgetContainer.Result.USER_CANCEL;
      CWSWidgetContainer.Metrics.recordCloseDialog(
          CWSWidgetContainer.Metrics.CLOSE_DIALOG.USER_CANCELLED);
      break;
    case CWSWidgetContainer.State.OPENING_WEBSTORE_CLOSING:
      result = CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED;
      CWSWidgetContainer.Metrics.recordCloseDialog(
          CWSWidgetContainer.Metrics.CLOSE_DIALOG.WEBSTORE_LINK_OPENED);
      break;
    default:
      result = CWSWidgetContainer.Result.USER_CANCEL;
      CWSWidgetContainer.Metrics.recordCloseDialog(
          CWSWidgetContainer.Metrics.CLOSE_DIALOG.UNKNOWN_ERROR);
  }

  this.state_ = CWSWidgetContainer.State.UNINITIALIZED;

  this.reset_();

  return {result: result, installedItemId: this.installedItemId_};
};

/**
 * Resets the widget.
 * @private
 */
CWSWidgetContainer.prototype.reset_ = function () {
  if (this.state_ !== CWSWidgetContainer.State.UNINITIALIZED)
    console.error('Widget reset before its state was finalized.');

  if (this.resolveStart_) {
    this.resolveStart_(CWSWidgetContainer.ResolveReason.RESET);
    this.resolveStart_ = null;
  }

  if (this.webviewClient_) {
    this.webviewClient_.dispose();
    this.webviewClient_ = null;
  }

  if (this.webview_)
    this.webviewContainer_.removeChild(this.webview_);
  this.options_ = null;
};

/**
 * Utility methods and constants to record histograms.
 */
CWSWidgetContainer.Metrics = {};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.Metrics.LOAD = {
  SUCCEEDED: 0,
  CANCELLED: 1,
  FAILED: 2,
};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.Metrics.CLOSE_DIALOG = {
  UNKNOWN_ERROR: 0,
  ITEM_INSTALLED: 1,
  USER_CANCELLED: 2,
  WEBSTORE_LINK_OPENED: 3,
};

/**
 * @enum {number}
 * @const
 */
CWSWidgetContainer.Metrics.INSTALL = {
  SUCCEEDED: 0,
  CANCELLED: 1,
  FAILED: 2,
};

/**
 * @param {number} result Result of load, which must be defined in
 *     CWSWidgetContainer.Metrics.LOAD.
 */
CWSWidgetContainer.Metrics.recordLoad = function(result) {
  if (0 <= result && result < 3)
    metrics.recordEnum('SuggestApps.Load', result, 3);
};

/**
 * @param {number} reason Reason of closing dialog, which must be defined in
 *     CWSWidgetContainer.Metrics.CLOSE_DIALOG.
 */
CWSWidgetContainer.Metrics.recordCloseDialog = function(reason) {
  if (0 <= reason && reason < 4)
    metrics.recordEnum('SuggestApps.CloseDialog', reason, 4);
};

/**
 * @param {number} result Result of installation, which must be defined in
 *     CWSWidgetContainer.Metrics.INSTALL.
 */
CWSWidgetContainer.Metrics.recordInstall = function(result) {
  if (0 <= result && result < 3)
    metrics.recordEnum('SuggestApps.Install', result, 3);
};

CWSWidgetContainer.Metrics.recordShowDialog = function() {
  metrics.recordUserAction('SuggestApps.ShowDialog');
};

CWSWidgetContainer.Metrics.startLoad = function() {
  metrics.startInterval('SuggestApps.LoadTime');
};

CWSWidgetContainer.Metrics.finishLoad = function() {
  metrics.recordInterval('SuggestApps.LoadTime');
};
