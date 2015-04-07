// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * SuggestAppsDialog contains a list box to select an app to be opened the file
 * with. This dialog should be used as action picker for file operations.
 */


/**
 * Creates dialog in DOM tree.
 *
 * @param {!HTMLElement} parentNode Node to be parent for this dialog.
 * @param {!SuggestAppDialogState} state Static state of suggest app dialog.
 * @constructor
 * @extends {FileManagerDialogBase}
 */
function SuggestAppsDialog(parentNode, state) {
  FileManagerDialogBase.call(this, parentNode);

  this.frame_.id = 'suggest-app-dialog';

  /**
   * The root element for the Chrome Web Store widget container.
   * @const {!HTMLElement}
   */
  var widgetRoot = this.document_.createElement('div');
  this.frame_.insertBefore(widgetRoot, this.text_.nextSibling);

  /**
   * The wrapper around Chrome Web Store widget.
   * @const {!CWSWidgetContainer}
   * @private
   */
  this.widget_ = new CWSWidgetContainer(this.document_, widgetRoot, state);

  this.initialFocusElement_ = this.widget_.getInitiallyFocusedElement();

  /**
   * The reported widget result.
   * @type {SuggestAppsDialog.Result}
   * @private
   */
  this.result_ = SuggestAppsDialog.Result.FAILED;
}

SuggestAppsDialog.prototype = {
  __proto__: FileManagerDialogBase.prototype
};

/**
 * @enum {string}
 */
SuggestAppsDialog.Result = {
  // Install is done. The install app should be opened.
  SUCCESS: 'SuggestAppsDialog.Result.SUCCESS',
  // User cancelled the suggest app dialog. No message should be shown.
  CANCELLED: 'SuggestAppsDialog.Result.CANCELLED',
  // Failed to load the widget. Error message should be shown.
  FAILED: 'SuggestAppsDialog.Result.FAILED'
};
Object.freeze(SuggestAppsDialog.Result);

/**
 * Dummy function for SuggestAppsDialog.show() not to be called unintentionally.
 */
SuggestAppsDialog.prototype.show = function() {
  console.error('SuggestAppsDialog.show() shouldn\'t be called directly.');
};

/**
 * Shows suggest-apps dialog by file extension and mime.
 *
 * @param {string} extension Extension of the file with a trailing dot.
 * @param {string} mime Mime of the file.
 * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
 *     when the dialog is closed, with a result code and an optionally an
 *     extension id, if an extension was installed.
 */
SuggestAppsDialog.prototype.showByExtensionAndMime =
    function(extension, mime, onDialogClosed) {
  assert(extension && extension[0] === '.');
  this.showInternal_(
      {
        file_extension: extension.substr(1),
        mime_type: mime
      },
      str('SUGGEST_DIALOG_TITLE'),
      FileTasks.createWebStoreLink(extension, mime),
      onDialogClosed);
};

/**
 * Shows suggest-apps dialog for FSP API
 * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
 *     when the dialog is closed, with a result code and an optionally an
 *     extension id, if an extension was installed.
 */
SuggestAppsDialog.prototype.showProviders = function(onDialogClosed) {
  this.showInternal_(
      {
        file_system_provider: true
      },
      str('SUGGEST_DIALOG_FOR_PROVIDERS_TITLE'),
      null /* webStoreUrl */,
      onDialogClosed);
};

/**
 * Internal method to show a dialog. This should be called only from 'Suggest.
 * appDialog.showXxxx()' functions.
 *
 * @param {!Object<string, *>} options Map of options for the dialog.
 * @param {string} title Title of the dialog.
 * @param {?string} webStoreUrl Url for more results. Null if not supported.
 * @param {function(SuggestAppsDialog.Result, ?string)} onDialogClosed Called
 *     when the dialog is closed, with a result code and an optionally an
 *     extension id, if an extension was installed.
 * @private
 */
SuggestAppsDialog.prototype.showInternal_ =
    function(options, title, webStoreUrl, onDialogClosed) {
  this.text_.hidden = true;
  this.dialogText_ = '';

  this.onDialogClosed_ = onDialogClosed;

  var dialogShown = false;

  this.widget_.ready()
      .then(
          /** @return {!Promise} */
          function() {
            return this.showDialog_(title);
          }.bind(this))
      .then(
          /** @return {!Promise.<CWSWidgetContainer.ResolveReason>} */
          function() {
            dialogShown = true;
            return this.widget_.start(options, webStoreUrl);
          }.bind(this))
      .then(
          /** @param {CWSWidgetContainer.ResolveReason} reason */
          function(reason) {
            if (reason !== CWSWidgetContainer.ResolveReason.RESET)
              this.hide();
          }.bind(this))
      .catch(
          /** @param {string} error */
          function(error) {
            console.error('Failed to start CWS widget: ' + error);
            this.result_ = SuggestAppsDialog.Result.FAILED;
            if (dialogShown) {
              this.hide();
            } else {
              this.onHide_();
            }
          }.bind(this));
};

/**
 * Internal method for showing the dialog in the file manager window.
 * @param {string} title The dialog title.
 * @return {Promise}
 */
SuggestAppsDialog.prototype.showDialog_ = function(title) {
  return new Promise(function(resolve, reject) {
     var success = this.dialogText_ ?
         FileManagerDialogBase.prototype.showTitleAndTextDialog.call(
             this, title, this.dialogText_) :
         FileManagerDialogBase.prototype.showTitleOnlyDialog.call(
             this, title);
     if (!success) {
       reject('SuggestAppsDialog cannot be shown.');
       return;
     }
     resolve();
  }.bind(this));
};

/**
 * Called when the connection status is changed.
 * @param {VolumeManagerCommon.DriveConnectionType} connectionType Current
 *     connection type.
 */
SuggestAppsDialog.prototype.onDriveConnectionChanged =
    function(connectionType) {
  if (connectionType === VolumeManagerCommon.DriveConnectionType.OFFLINE)
    this.widget_.onConnectionLost();
};

/**
 * @param {Function=} opt_originalOnHide Called when the original dialog is
 *     hidden.
 * @override
 */
SuggestAppsDialog.prototype.hide = function(opt_originalOnHide) {
  var widgetResult = this.widget_.finalizeAndGetResult();

  switch (widgetResult.result) {
    case CWSWidgetContainer.Result.INSTALL_SUCCESSFUL:
      this.result_ = SuggestAppsDialog.Result.SUCCESS;
      break;
    case CWSWidgetContainer.Result.WEBSTORE_LINK_OPENED:
    case CWSWidgetContainer.Result.USER_CANCEL:
      this.result_ =  SuggestAppsDialog.Result.CANCELLED;
      break;
    default:
      this.result_ = SuggestAppsDialog.Result.FAILED;
  }

  this.installedItemId_ = widgetResult.installedItemId;

  FileManagerDialogBase.prototype.hide.call(
      this,
      this.onHide_.bind(this, opt_originalOnHide));
};

/**
 * @param {Function=} opt_originalOnHide Original onHide function passed to
 *     SuggestAppsDialog.hide().
 * @private
 */
SuggestAppsDialog.prototype.onHide_ = function(opt_originalOnHide) {
  // Calls the callback after the dialog hides.
  if (opt_originalOnHide)
    opt_originalOnHide();

  this.onDialogClosed_(this.result_, this.installedItemId_);
};
