// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to track of connections to and from this computer and display them.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor */
remoting.ConnectionHistory = function() {
  /** @type {HTMLElement} @private */
  this.viewAll_ = document.getElementById('history-view-all');
  /** @type {HTMLElement} @private */
  this.viewOutgoing_ = document.getElementById('history-view-outgoing');
  /** @type {HTMLElement} @private */
  this.viewIncoming_ = document.getElementById('history-view-incoming');
  /** @type {HTMLElement} @private */
  this.clear_ = document.getElementById('clear-connection-history');
  /** @type {HTMLElement} @private */
  this.historyEntries_ = document.getElementById('connection-history-entries');
  /** @type {remoting.ConnectionHistory.Filter} @private */
  this.filter_ = remoting.ConnectionHistory.Filter.VIEW_ALL;

  /** @type {remoting.ConnectionHistory} */
  var that = this;
  var closeButton = document.getElementById('close-connection-history');
  closeButton.addEventListener('click', this.hide.bind(this), false);
  /** @param {Event} event Event identifying which button was clicked. */
  var setFilter = function(event) { that.setFilter_(event.target); };
  this.viewAll_.addEventListener('click', setFilter, false);
  this.viewOutgoing_.addEventListener('click', setFilter, false);
  this.viewIncoming_.addEventListener('click', setFilter, false);
  this.clear_.addEventListener('click', this.clearHistory_.bind(this), false);
};

/** @enum {string} */
remoting.ConnectionHistory.Filter = {
  VIEW_ALL: 'history-view-all',
  VIEW_OUTGOING: 'history-view-outgoing',
  VIEW_INCOMING: 'history-view-incoming'
};

/** Show the dialog and refresh its contents */
remoting.ConnectionHistory.prototype.show = function() {
  this.load();
  remoting.setMode(remoting.AppMode.HISTORY);
};

/** Hide the dialog */
remoting.ConnectionHistory.prototype.hide = function() {
  remoting.setMode(remoting.AppMode.HOME);
};

/**
 * A saved entry in the connection history.
 * @param {Object} object A Javascript object, which may or may not be of the
 *     correct type.
 * @constructor
 */
remoting.ConnectionHistory.Entry = function(object) {
  this.valid =
      'date' in object && typeof(object['date']) == 'number' &&
      'from' in object && typeof(object['from']) == 'string' &&
      'to' in object && typeof(object['to']) == 'string' &&
      'duration' in object && typeof(object['duration']) == 'number';
  if (this.valid) {
    /** @type {Date} */
    this.date = new Date(object['date']);
    /** @type {string} */
    this.from = object['from'];
    /** @type {string} */
    this.to = object['to'];
    /** @type {number} */
    this.duration = object['duration'];
  }
};

/**
 * @return {string} The connection duration, formatted as a string, or 'Not
 *     available' if there is no duration stored.
 * @private
 */
remoting.ConnectionHistory.Entry.prototype.durationString_ = function() {
  var secs = this.duration % 60;
  var mins = ((this.duration - secs) / 60) % 60;
  var hours = (this.duration - secs - 60 * mins) / 3600;
  if (secs < 10) {
    secs = '0' + secs;
  }
  var result = mins + ':' + secs;
  if (hours > 0) {
    if (mins < 10) {
      result = '0' + result;
    }
    result = hours + ':' + result;
  }
  return result;
};

/**
 * @return {{summary: Element, detail: Element}} Two table rows containing the
 *     summary and detail information, respectively, for the connection.
 */
remoting.ConnectionHistory.Entry.prototype.createTableRows = function() {
  var summary = /** @type {HTMLElement} */ document.createElement('tr');
  summary.classList.add('connection-history-summary');
  var zippy = /** @type {HTMLElement} */ document.createElement('td');
  zippy.classList.add('zippy');
  summary.appendChild(zippy);
  // TODO(jamiewalch): Find a way of combining date and time such both align
  // vertically without being considered separate columns, which puts too much
  // space between them.
  var date = document.createElement('td');
  // TODO(jamiewalch): Use a shorter localized version of the date.
  date.innerText = this.date.toLocaleDateString();
  summary.appendChild(date);
  var time = document.createElement('td');
  time.innerText = this.date.toLocaleTimeString();
  summary.appendChild(time);
  var from = document.createElement('td');
  from.innerText = this.from;
  summary.appendChild(from);
  var to = document.createElement('td');
  to.innerText = this.to;
  summary.appendChild(to);
  var duration = document.createElement('td');
  duration.innerText = this.durationString_();
  summary.appendChild(duration);
  // TODO(jamiewalch): Fill out the detail row correctly.
  var detail = /** @type {HTMLElement} */ document.createElement('tr');
  detail.classList.add('connection-history-detail');
  for (var i = 0; i < summary.childElementCount; ++i) {
    var td = document.createElement('td');
    if (i != 0) {
      // The inner div allows the details rows to be hidden without changing
      // the column widths.
      var div = document.createElement('div');
      div.innerText = 'Nothing to see here';
      td.appendChild(div);
    }
    detail.appendChild(td);
  }
  /** @param {HTMLElement} node The summary row. */
  var toggleDetail = function(node) {
    node.classList.toggle('expanded');
  };
  summary.addEventListener('click',
                           function() { toggleDetail(summary); },
                           false);
  detail.addEventListener('click',
                          function() { toggleDetail(summary); },
                          false);
  return { 'summary': summary, 'detail': detail };
};

/** Refresh the contents of the connection history table */
remoting.ConnectionHistory.prototype.load = function() {
  // TODO(jamiewalch): Load connection history data when it's available.
  var history = [];
  // Remove existing entries from the DOM and repopulate.
  // TODO(jamiewalch): Enforce the filter.
  this.historyEntries_.innerText = '';
  for (var i in history) {
    var connection = new remoting.ConnectionHistory.Entry(history[i]);
    if (connection.valid) {
      var rows = connection.createTableRows();
      this.historyEntries_.appendChild(rows.summary);
      this.historyEntries_.appendChild(rows.detail);
    }
  }
};

/**
 * @param {EventTarget} element The element that was clicked.
 * @private
 */
remoting.ConnectionHistory.prototype.setFilter_ = function(element) {
  for (var i in remoting.ConnectionHistory.Filter) {
    var link = document.getElementById(remoting.ConnectionHistory.Filter[i]);
    if (element == link) {
      link.classList.add('no-link');
      this.filter_ = /** @type {remoting.ConnectionHistory.Filter} */ (i);
    } else {
      link.classList.remove('no-link');
    }
  }
};

/**
 * @private
 */
remoting.ConnectionHistory.prototype.clearHistory_ = function() {
  // TODO(jamiewalch): Implement once we store users' connection histories.
};

/** @type {remoting.ConnectionHistory} */
remoting.ConnectionHistory.connectionHistory = null;
