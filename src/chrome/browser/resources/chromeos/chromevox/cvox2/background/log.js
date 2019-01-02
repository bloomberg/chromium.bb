// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox log page.
 *
 */

goog.provide('LogPage');

goog.require('LogStore');
goog.require('Msgs');

/**
 * Class to manage the log page.
 * @constructor
 */
LogPage = function() {};

/**
 * The Background object.
 * @type {Window}
 */
LogPage.backgroundWindow;

/**
 * The LogStore object.
 * @type {LogStore}
 */
LogPage.LogStore;

LogPage.init = function() {
  LogPage.backgroundWindow = chrome.extension.getBackgroundPage();
  LogPage.LogStore = LogPage.backgroundWindow.LogStore.getInstance();

  var clearLogButton = document.getElementById('clearLog');
  clearLogButton.onclick = function(event) {
    LogPage.LogStore.clearLog();
    location.reload();
  };

  var checkboxes = document.getElementsByClassName('log-filter');
  var filterEventListener = function(event) {
    var target = event.target;
    sessionStorage.setItem(target.name, target.checked);
    location.reload();
  };
  for (var i = 0; i < checkboxes.length; i++)
    checkboxes[i].onclick = filterEventListener;

  LogPage.update();
};

/**
 * Update the states of checkboxes and
 * update logs.
 */
LogPage.update = function() {
  for (var type in LogStore.LogType) {
    var typeFilter = LogStore.LogType[type] + 'Filter';
    var element = document.getElementById(typeFilter);
    /** If sessionStorage is null, set true. */
    if (!sessionStorage.getItem(typeFilter))
      sessionStorage.setItem(typeFilter, true);
    element.checked = (sessionStorage.getItem(typeFilter) == 'true');
  }

  var log = LogPage.LogStore.getLogs();
  LogPage.updateLog(log, document.getElementById('logList'));
};

/**
 * Updates the log section.
 * @param {!Array<Log>} log Array of speech.
 * @param {Element} div
 */
LogPage.updateLog = function(log, div) {
  for (var i = 0; i < log.length; i++) {
    if (sessionStorage.getItem(log[i].logType + 'Filter') != 'true')
      continue;

    var p = document.createElement('p');
    var typeName = document.createElement('span');
    typeName.textContent = log[i].logType;
    typeName.className = 'log-type-tag';
    var timeStamp = document.createElement('span');
    timeStamp.textContent = LogPage.formatTimeStamp(log[i].date);
    timeStamp.className = 'log-time-tag';
    var textWrapper = document.createElement('span');
    textWrapper.textContent = log[i].logStr;

    p.appendChild(typeName);
    p.appendChild(timeStamp);
    p.appendChild(textWrapper);
    div.appendChild(p);
  }
};

/**
 * Format time stamp.
 * In this log, events are dispatched many times in a short time, so
 * milliseconds order time stamp is required.
 * @param {!Date} date
 * @return {!string}
 */
LogPage.formatTimeStamp = function(date) {
  var time = date.getTime();
  time -= date.getTimezoneOffset() * 1000 * 60;
  var timeStr = ('00' + Math.floor(time / 1000 / 60 / 60) % 24).slice(-2) + ':';
  timeStr += ('00' + Math.floor(time / 1000 / 60) % 60).slice(-2) + ':';
  timeStr += ('00' + Math.floor(time / 1000) % 60).slice(-2) + '.';
  timeStr += ('000' + time % 1000).slice(-3);
  return timeStr;
};

document.addEventListener('DOMContentLoaded', function() {
  LogPage.init();
}, false);
