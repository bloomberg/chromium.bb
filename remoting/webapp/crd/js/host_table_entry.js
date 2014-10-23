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
 * An entry in the host table.
 * @param {remoting.Host} host The host, as obtained from Apiary.
 * @param {number} webappMajorVersion The major version nmber of the web-app,
 *     used to identify out-of-date hosts.
 * @param {function(remoting.HostTableEntry):void} onRename Callback for
 *     rename operations.
 * @param {function(remoting.HostTableEntry):void=} opt_onDelete Callback for
 *     delete operations.
 * @constructor
 */
remoting.HostTableEntry = function(
    host, webappMajorVersion, onRename, opt_onDelete) {
  /** @type {remoting.Host} */
  this.host = host;
  /** @type {number} */
  this.webappMajorVersion_ = webappMajorVersion;
  /** @type {function(remoting.HostTableEntry):void} @private */
  this.onRename_ = onRename;
  /** @type {undefined|function(remoting.HostTableEntry):void} @private */
  this.onDelete_ = opt_onDelete;

  /** @type {HTMLElement} */
  this.tableRow = null;
  /** @type {HTMLElement} @private */
  this.hostNameCell_ = null;
  /** @type {HTMLElement} @private */
  this.warningOverlay_ = null;
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
 * @return {void} Nothing.
 */
remoting.HostTableEntry.prototype.createDom = function() {
  // Create the top-level <div>
  var tableRow = /** @type {HTMLElement} */ document.createElement('div');
  tableRow.classList.add('section-row');
  // Create the host icon cell.
  var hostIconDiv = /** @type {HTMLElement} */ document.createElement('div');
  hostIconDiv.classList.add('host-list-main-icon');
  var warningOverlay =
      /** @type {HTMLElement} */ document.createElement('span');
  hostIconDiv.appendChild(warningOverlay);
  var hostIcon = /** @type {HTMLElement} */ document.createElement('img');
  hostIcon.src = 'icon_host.webp';
  hostIconDiv.appendChild(hostIcon);
  tableRow.appendChild(hostIconDiv);
  // Create the host name cell.
  var hostNameCell = /** @type {HTMLElement} */ document.createElement('div');
  hostNameCell.classList.add('box-spacer');
  hostNameCell.id = 'host_' + this.host.hostId;
  tableRow.appendChild(hostNameCell);
  // Create the host rename cell.
  var editButton = /** @type {HTMLElement} */ document.createElement('span');
  var editButtonImg = /** @type {HTMLElement} */ document.createElement('img');
  editButtonImg.title = chrome.i18n.getMessage(
      /*i18n-content*/'TOOLTIP_RENAME');
  editButtonImg.src = 'icon_pencil.webp';
  editButton.tabIndex = 0;
  editButton.classList.add('clickable');
  editButton.classList.add('host-list-edit');
  editButtonImg.classList.add('host-list-rename-icon');
  editButton.appendChild(editButtonImg);
  tableRow.appendChild(editButton);
  // Create the host delete cell.
  var deleteButton = /** @type {HTMLElement} */ document.createElement('span');
  var deleteButtonImg =
      /** @type {HTMLElement} */ document.createElement('img');
  deleteButtonImg.title =
      chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_DELETE');
  deleteButtonImg.src = 'icon_cross.webp';
  deleteButton.tabIndex = 0;
  deleteButton.classList.add('clickable');
  deleteButton.classList.add('host-list-edit');
  deleteButtonImg.classList.add('host-list-remove-icon');
  deleteButton.appendChild(deleteButtonImg);
  tableRow.appendChild(deleteButton);

  this.init(tableRow, warningOverlay, hostNameCell, editButton, deleteButton);
};

/**
 * Associate the table row with the specified elements and callbacks, and set
 * up event handlers.
 *
 * @param {HTMLElement} tableRow The top-level <div> for the table entry.
 * @param {HTMLElement} warningOverlay The <span> element to render a warning
 *     icon on top of the host icon.
 * @param {HTMLElement} hostNameCell The element containing the host name.
 * @param {HTMLElement} editButton The <img> containing the pencil icon for
 *     editing the host name.
 * @param {HTMLElement=} opt_deleteButton The <img> containing the cross icon
 *     for deleting the host, if present.
 * @return {void} Nothing.
 */
remoting.HostTableEntry.prototype.init = function(
    tableRow, warningOverlay, hostNameCell, editButton, opt_deleteButton) {
  this.tableRow = tableRow;
  this.warningOverlay_ = warningOverlay;
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
      /** @type {string} */
      var encodedHostId = encodeURIComponent(this.host.hostId);
      this.onConnectReference_ = function() {
        remoting.connectMe2Me(encodedHostId);
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
  this.warningOverlay_.hidden = !remoting.Host.needsUpdate(
      this.host, this.webappMajorVersion_);
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
  this.hostNameCell_.innerText = '';
  this.hostNameCell_.appendChild(editBox);
  editBox.select();

  this.onBlurReference_ = this.commitRename_.bind(this);
  editBox.addEventListener('blur', this.onBlurReference_, false);

  editBox.addEventListener('keydown', this.onKeydown_.bind(this), false);
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
  var confirm = document.getElementById('confirm-host-delete');
  var cancel = document.getElementById('cancel-host-delete');
  this.onConfirmDeleteReference_ = this.confirmDelete_.bind(this);
  this.onCancelDeleteReference_ = this.cancelDelete_.bind(this);
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
  // Update the tool-top and event handler.
  this.updateStatus();
  this.setHostName_();
};

/**
 * Create the DOM nodes and event handlers for the hostname cell.
 * @return {void} Nothing.
 * @private
 */
remoting.HostTableEntry.prototype.setHostName_ = function() {
  var hostNameNode = /** @type {HTMLElement} */ document.createElement('a');
  if (this.host.status == 'ONLINE') {
    if (remoting.Host.needsUpdate(this.host, this.webappMajorVersion_)) {
      hostNameNode.innerText = chrome.i18n.getMessage(
          /*i18n-content*/'UPDATE_REQUIRED', this.host.hostName);
    } else {
      hostNameNode.innerText = this.host.hostName;
    }
    hostNameNode.href = '#';
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
    if (this.host.updatedTime) {
      var lastOnline = new Date(this.host.updatedTime);
      var now = new Date();
      var displayString = '';
      if (now.getFullYear() == lastOnline.getFullYear() &&
          now.getMonth() == lastOnline.getMonth() &&
          now.getDate() == lastOnline.getDate()) {
        displayString = lastOnline.toLocaleTimeString();
      } else {
        displayString = lastOnline.toLocaleDateString();
      }
      hostNameNode.innerText = chrome.i18n.getMessage(
          /*i18n-content*/'LAST_ONLINE', [this.host.hostName, displayString]);
    } else {
      hostNameNode.innerText = chrome.i18n.getMessage(
          /*i18n-content*/'OFFLINE', this.host.hostName);
    }
  }
  hostNameNode.classList.add('host-list-label');
  this.hostNameCell_.innerText = '';  // Remove previous contents (if any).
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
  e.addEventListener('focus', this.onFocusChange_.bind(this), false);
  e.addEventListener('blur', this.onFocusChange_.bind(this), false);
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
