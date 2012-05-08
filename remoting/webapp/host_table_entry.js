// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class representing an entry in the host-list portion of the home screen.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * The deserialized form of the chromoting host as returned by Apiary.
 * Note that the object has more fields than are detailed below--these
 * are just the ones that we refer to directly.
 * @constructor
 */
remoting.Host = function() {
  /** @type {string} */
  this.hostName = '';
  /** @type {string} */
  this.hostId = '';
  /** @type {string} */
  this.status = '';
  /** @type {string} */
  this.jabberId = '';
  /** @type {string} */
  this.publicKey = '';
};

/**
 * An entry in the host table.
 * @constructor
 */
remoting.HostTableEntry = function() {
  /** @type {remoting.Host} */
  this.host = null;
  /** @type {HTMLElement} */
  this.tableRow = null;
  /** @type {HTMLElement} @private */
  this.hostNameCell_ = null;
  /** @type {function(remoting.HostTableEntry):void} @private */
  this.onRename_ = function(host) {};
  /** @type {undefined|function(remoting.HostTableEntry):void} @private */
  this.onDelete_ = function(host) {};
  // References to event handlers so that they can be removed.
  /** @type {function():void} @private */
  this.onBlurReference_ = function() {};
  /** @type {function():void} @private */
  this.onConfirmDeleteReference_ = function() {};
  /** @type {function():void} @private */
  this.onCancelDeleteReference_ = function() {};
  /** @type {function():void?} @private */
  this.onConnectReference_ = null;
};

/**
 * Create the HTML elements for this entry and set up event handlers.
 * @param {remoting.Host} host The host, as obtained from Apiary.
 * @param {function(remoting.HostTableEntry):void} onRename Callback for
 *     rename operations.
 * @param {function(remoting.HostTableEntry):void} onDelete Callback for
 *     delete operations.
 * @return {void} Nothing.
 */
remoting.HostTableEntry.prototype.create = function(host, onRename, onDelete) {
  // Create the top-level <div>
  var tableRow = /** @type {HTMLElement} */ document.createElement('div');
  tableRow.classList.add('section-row');
  // Create the host icon cell.
  var hostIcon = /** @type {HTMLElement} */ document.createElement('img');
  hostIcon.src = 'icon_host.png';
  hostIcon.classList.add('host-list-main-icon');
  tableRow.appendChild(hostIcon);
  // Create the host name cell.
  var hostNameCell = /** @type {HTMLElement} */ document.createElement('div');
  hostNameCell.classList.add('box-spacer');
  tableRow.appendChild(hostNameCell);
  // Create the host rename cell.
  var editButton = /** @type {HTMLElement} */ document.createElement('img');
  editButton.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_RENAME');
  editButton.src = 'icon_pencil.png';
  editButton.tabIndex = 0;
  editButton.classList.add('clickable');
  editButton.classList.add('host-list-edit');
  editButton.classList.add('host-list-rename-icon');
  tableRow.appendChild(editButton);
  // Create the host delete cell.
  var deleteButton = /** @type {HTMLElement} */ document.createElement('img');
  deleteButton.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_DELETE');
  deleteButton.src = 'icon_cross.png';
  deleteButton.tabIndex = 0;
  deleteButton.classList.add('clickable');
  deleteButton.classList.add('host-list-edit');
  deleteButton.classList.add('host-list-remove-icon');
  tableRow.appendChild(deleteButton);

  this.init(host, tableRow, hostNameCell, editButton, onRename,
            deleteButton, onDelete);
};


/**
 * Associate the table row with the specified elements and callbacks, and set
 * up event handlers.
 *
 * @param {remoting.Host} host The host, as obtained from Apiary.
 * @param {HTMLElement} tableRow The top-level <div> for the table entry.
 * @param {HTMLElement} hostNameCell The element containing the host name.
 * @param {HTMLElement} editButton The <img> containing the pencil icon for
 *     editing the host name.
 * @param {function(remoting.HostTableEntry):void} onRename Callback for
 *     rename operations.
 * @param {HTMLElement=} opt_deleteButton The <img> containing the cross icon
 *     for deleting the host, if present.
 * @param {function(remoting.HostTableEntry):void=} opt_onDelete Callback for
 *     delete operations.
 * @return {void} Nothing.
 */
remoting.HostTableEntry.prototype.init = function(
    host, tableRow, hostNameCell, editButton, onRename,
    opt_deleteButton, opt_onDelete) {
  this.host = host;
  this.onRename_ = onRename;
  this.onDelete_ = opt_onDelete;
  this.tableRow = tableRow;
  this.hostNameCell_ = hostNameCell;

  this.setHostName_();

  /** @type {remoting.HostTableEntry} */
  var that = this;

  /** @param {Event} event The click event. */
  var beginRename = function(event) {
    that.beginRename_();
    event.stopPropagation();
  };
  /** @param {Event} event The keyup event. */
  var beginRenameKeyboard = function(event) {
    if (event.which == 13 || event.which == 32) {
      that.beginRename_();
      event.stopPropagation();
    }
  };
  editButton.addEventListener('click', beginRename, true);
  editButton.addEventListener('keyup', beginRenameKeyboard, true);
  this.registerFocusHandlers_(editButton);

  if (opt_deleteButton) {
    /** @param {Event} event The click event. */
    var confirmDelete = function(event) {
      that.showDeleteConfirmation_();
      event.stopPropagation();
    };
    /** @param {Event} event The keyup event. */
    var confirmDeleteKeyboard = function(event) {
      if (event.which == 13 || event.which == 32) {
        that.showDeleteConfirmation_();
      }
    };
    opt_deleteButton.addEventListener('click', confirmDelete, false);
    opt_deleteButton.addEventListener('keyup', confirmDeleteKeyboard, false);
    this.registerFocusHandlers_(opt_deleteButton);
  }
  this.updateStatus();
};

/**
 * Update the row to reflect the current status of the host (online/offline and
 * clickable/unclickable).
 *
 * @param {boolean=} opt_forEdit True if the status is being updated in order
 *     to allow the host name to be edited.
 * @return {void} Nothing.
 */
remoting.HostTableEntry.prototype.updateStatus = function(opt_forEdit) {
  var clickToConnect = this.host.status == 'ONLINE' && !opt_forEdit;
  if (clickToConnect) {
    if (!this.onConnectReference_) {
      /** @type {remoting.HostTableEntry} */
      var that = this;
      this.onConnectReference_ = function() {
        var hostUrl = chrome.extension.getURL('main.html') +
            '?mode=me2me&hostId=' + encodeURIComponent(that.host.hostId);
        window.location.assign(hostUrl);
      };
      this.tableRow.addEventListener('click', this.onConnectReference_, false);
    }
    this.tableRow.classList.add('clickable');
    this.tableRow.title = chrome.i18n.getMessage(
        /*i18n-content*/'TOOLTIP_CONNECT', this.host.hostName);
  } else {
    if (this.onConnectReference_) {
      this.tableRow.removeEventListener('click', this.onConnectReference_,
                                        false);
      this.onConnectReference_ = null;
    }
    this.tableRow.classList.remove('clickable');
    this.tableRow.title = '';
  }
  var showOffline = this.host.status != 'ONLINE';
  if (showOffline) {
    this.tableRow.classList.remove('host-online');
    this.tableRow.classList.add('host-offline');
  } else {
    this.tableRow.classList.add('host-online');
    this.tableRow.classList.remove('host-offline');
  }
};

/**
 * Prepare the host for renaming by replacing its name with an edit box.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.beginRename_ = function() {
  var editBox = /** @type {HTMLInputElement} */ document.createElement('input');
  editBox.type = 'text';
  editBox.value = this.host.hostName;
  this.hostNameCell_.innerHTML = '';
  this.hostNameCell_.appendChild(editBox);
  editBox.select();

  /** @type {remoting.HostTableEntry} */
  var that = this;
  this.onBlurReference_ = function() { that.commitRename_(); };
  editBox.addEventListener('blur', this.onBlurReference_, false);

  /** @param {Event} event The keydown event. */
  var onKeydown = function(event) { that.onKeydown_(event); }
  editBox.addEventListener('keydown', onKeydown, false);
  this.updateStatus(true);
};

/**
 * Accept the hostname entered by the user.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.commitRename_ = function() {
  var editBox = this.hostNameCell_.querySelector('input');
  if (editBox) {
    if (this.host.hostName != editBox.value) {
      this.host.hostName = editBox.value;
      this.onRename_(this);
    }
    // Update the tool-top and event handler.
    this.updateStatus();
    this.removeEditBox_();
  }
};

/**
 * Prompt the user to confirm or cancel deletion of a host.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.showDeleteConfirmation_ = function() {
  var message = document.getElementById('confirm-host-delete-message');
  l10n.localizeElement(message, this.host.hostName);
  /** @type {remoting.HostTableEntry} */
  var that = this;
  var confirm = document.getElementById('confirm-host-delete');
  var cancel = document.getElementById('cancel-host-delete');
  this.onConfirmDeleteReference_ = function() { that.confirmDelete_(); };
  this.onCancelDeleteReference_ = function() { that.cancelDelete_(); };
  confirm.addEventListener('click', this.onConfirmDeleteReference_, false);
  cancel.addEventListener('click', this.onCancelDeleteReference_, false);
  remoting.setMode(remoting.AppMode.CONFIRM_HOST_DELETE);
};

/**
 * Confirm deletion of a host.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.confirmDelete_ = function() {
  this.onDelete_(this);
  this.cleanUpConfirmationEventListeners_();
  remoting.setMode(remoting.AppMode.HOME);
};

/**
 * Cancel deletion of a host.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.cancelDelete_ = function() {
  this.cleanUpConfirmationEventListeners_();
  remoting.setMode(remoting.AppMode.HOME);
};

/**
 * Remove the confirm and cancel event handlers, which refer to this object.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.cleanUpConfirmationEventListeners_ =
    function() {
  var confirm = document.getElementById('confirm-host-delete');
  var cancel = document.getElementById('cancel-host-delete');
  confirm.removeEventListener('click', this.onConfirmDeleteReference_, false);
  cancel.removeEventListener('click', this.onCancelDeleteReference_, false);
  this.onCancelDeleteReference_ = function() {};
  this.onConfirmDeleteReference_ = function() {};
};

/**
 * Remove the edit box corresponding to the specified host, and reset its name.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.removeEditBox_ = function() {
  var editBox = this.hostNameCell_.querySelector('input');
  if (editBox) {
    // onblur will fire when the edit box is removed, so remove the hook.
    editBox.removeEventListener('blur', this.onBlurReference_, false);
  }
  this.setHostName_();
};

/**
 * Create the DOM nodes and event handlers for the hostname cell.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.setHostName_ = function() {
  var hostNameNode = /** @type {HTMLElement} */ document.createElement('span');
  if (this.host.status == 'ONLINE') {
    hostNameNode.innerText = this.host.hostName;
    hostNameNode.tabIndex = 0;
    this.registerFocusHandlers_(hostNameNode);
    /** @type {remoting.HostTableEntry} */
    var that = this;
    /** @param {Event} event */
    var onKeyDown = function(event) {
      if (that.onConnectReference_ &&
          (event.which == 13 || event.which == 32)) {
        that.onConnectReference_();
      }
    };
    hostNameNode.addEventListener('keydown', onKeyDown, false);
  } else {
    hostNameNode.innerText = chrome.i18n.getMessage(/*i18n-content*/'OFFLINE',
                                                    this.host.hostName);
  }
  hostNameNode.classList.add('host-list-label');
  this.hostNameCell_.innerHTML = '';  // Remove previous contents (if any).
  this.hostNameCell_.appendChild(hostNameNode);
};

/**
 * Handle a key event while the user is typing a host name
 * @param {Event} event The keyboard event.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.onKeydown_ = function(event) {
  if (event.which == 27) {  // Escape
    this.removeEditBox_();
  } else if (event.which == 13) {  // Enter
    this.commitRename_();
  }
};

/**
 * Register focus and blur handlers to cause the parent node to be highlighted
 * whenever a child link has keyboard focus. Note that this is only necessary
 * because Chrome does not yet support the draft CSS Selectors 4 specification
 * (http://www.w3.org/TR/selectors4/#subject), which provides a more elegant
 * solution to this problem.
 *
 * @param {HTMLElement} e The element on which to register the event handlers.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.registerFocusHandlers_ = function(e) {
  /** @type {remoting.HostTableEntry} */
  var that = this;
  e.addEventListener('focus', function() { that.onFocusChange_(); }, false);
  e.addEventListener('blur', function() { that.onFocusChange_(); }, false);
};

/**
 * Handle a focus change event within this table row.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.onFocusChange_ = function() {
  var element = document.activeElement;
  while (element) {
    if (element == this.tableRow) {
      this.tableRow.classList.add('child-focused');
      return;
    }
    element = element.parentNode;
  }
  this.tableRow.classList.remove('child-focused');
};
