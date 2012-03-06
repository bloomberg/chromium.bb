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
  /** @type {Element} */
  this.tableRow = null;
  /** @type {Element} @private */
  this.hostNameCell_ = null;
  /** @type {function(remoting.HostTableEntry):void} @private */
  this.onRename_ = function(hostId) {};
  /** @type {function():void} @private */
  this.onBlurReference_ = function() {};
};

/**
 * Create the HTML elements for this entry.
 * @param {remoting.Host} host The host, as obtained from Apiary.
 * @param {function(remoting.HostTableEntry):void} onRename Callback for
 *     rename operations.
 * @param {function(remoting.HostTableEntry):void} onDelete Callback for
 *     delete operations.
 */
remoting.HostTableEntry.prototype.init = function(host, onRename, onDelete) {
  this.host = host;
  this.onRename_ = onRename;

  /** @type {remoting.HostTableEntry} */
  var that = this;

  this.tableRow = /** @type {HTMLElement} */ document.createElement('div');
  this.tableRow.classList.add('section-row');

  // Create the host icon cell.
  var hostIcon = /** @type {HTMLElement} */ document.createElement('img');
  hostIcon.src = 'icon_host.png';
  hostIcon.classList.add('host-list-main-icon');
  this.tableRow.appendChild(hostIcon);

  // Create the host name cell.
  this.hostNameCell_ = /** @type {HTMLElement} */ document.createElement('div');
  this.hostNameCell_.classList.add('box-spacer');
  this.setHostName_();
  this.tableRow.appendChild(this.hostNameCell_);

  // Create the host status cell.
  if (host.status == 'ONLINE') {
    var hostUrl = chrome.extension.getURL('main.html') +
        '?mode=me2me&hostId=' + encodeURIComponent(host.hostId);
    var startMe2Me = function() { window.location.replace(hostUrl); };
    this.hostNameCell_.addEventListener('click', startMe2Me, false);
    hostIcon.addEventListener('click', startMe2Me, false);
    this.tableRow.classList.add('clickable');
    this.tableRow.classList.add('host-online');
    this.tableRow.title = chrome.i18n.getMessage(
        /*i18n-content*/'TOOLTIP_CONNECT', host.hostName);
  } else {
    this.tableRow.classList.add('host-offline');
  }

  // Create the host rename cell.
  var editButton = /** @type {HTMLElement} */ document.createElement('img');
  var beginRename = function() { that.beginRename_(); };
  editButton.addEventListener('click', beginRename, true);
  editButton.classList.add('clickable');
  editButton.classList.add('host-list-edit');
  editButton.src = 'icon_pencil.png';
  editButton.classList.add('host-list-rename-icon');
  editButton.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_RENAME');
  this.tableRow.appendChild(editButton);

  // Create the host delete cell.
  var deleteButton = /** @type {HTMLElement} */ document.createElement('div');
  deleteButton.addEventListener('click', function() { onDelete(that); }, false);
  deleteButton.classList.add('clickable');
  deleteButton.classList.add('host-list-edit');
  var crossImage = /** @type {HTMLElement} */ document.createElement('img');
  crossImage.src = 'icon_cross.png';
  crossImage.classList.add('host-list-remove-icon');
  deleteButton.appendChild(crossImage);
  deleteButton.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_DELETE');
  this.tableRow.appendChild(deleteButton);
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
  // Keep a reference to the blur event handler so that we can remove it later.
  this.onBlurReference_ = function() { that.commitRename_(); };
  editBox.addEventListener('blur', this.onBlurReference_, false);

  /** @param {Event} event The keydown event. */
  var onKeydown = function(event) { that.onKeydown_(event); }
  editBox.addEventListener('keydown', onKeydown, false);
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
  this.hostNameCell_.innerHTML = '';  // Remove the edit box.
  this.setHostName_();
};

remoting.HostTableEntry.prototype.setHostName_ = function() {
  var hostNameNode = /** @type {HTMLElement} */ document.createElement('span');
  if (this.host.status == 'ONLINE') {
    hostNameNode.innerText = this.host.hostName;
  } else {
    hostNameNode.innerText = chrome.i18n.getMessage(/*i18n-content*/'OFFLINE',
                                                    this.host.hostName);
  }
  hostNameNode.classList.add('host-list-label');
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
