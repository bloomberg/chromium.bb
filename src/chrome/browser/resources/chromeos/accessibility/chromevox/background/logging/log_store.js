// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 */

goog.provide('LogStore');

goog.require('TreeDumper');
goog.require('BaseLog');
goog.require('EventLog');
goog.require('LogType');
goog.require('SpeechLog');
goog.require('TextLog');
goog.require('TreeLog');

LogStore = class {
  constructor() {
    /**
     * Ring buffer of size this.LOG_LIMIT
     * @private {!Array<BaseLog>}
     */
    this.logs_ = Array(LogStore.LOG_LIMIT);

    /*
     * this.logs_ is implemented as a ring buffer which starts
     * from this.startIndex_ and ends at this.startIndex_-1
     * In the initial state, this array is filled by undefined.
     * @type {number}
     * @private
     */
    this.startIndex_ = 0;
  }

  /**
   * Creates logs of type |type| in order.
   * This is not the best way to create logs fast but
   * getLogsOfType() is not called often.
   * @param {!LogType} logType
   * @return {!Array<BaseLog>}
   */
  getLogsOfType(logType) {
    const returnLogs = [];
    for (let i = 0; i < LogStore.LOG_LIMIT; i++) {
      const index = (this.startIndex_ + i) % LogStore.LOG_LIMIT;
      if (!this.logs_[index]) {
        continue;
      }
      if (this.logs_[index].logType === logType) {
        returnLogs.push(this.logs_[index]);
      }
    }
    return returnLogs;
  }

  /**
   * Create logs in order.
   * This is not the best way to create logs fast but
   * getLogs() is not called often.
   * @return {!Array<BaseLog>}
   */
  getLogs() {
    const returnLogs = [];
    for (let i = 0; i < LogStore.LOG_LIMIT; i++) {
      const index = (this.startIndex_ + i) % LogStore.LOG_LIMIT;
      if (!this.logs_[index]) {
        continue;
      }
      returnLogs.push(this.logs_[index]);
    }
    return returnLogs;
  }

  /**
   * Write a text log to this.logs_.
   * To add a message to logs, this function should be called.
   * @param {string} logContent
   * @param {!LogType} logType
   */
  writeTextLog(logContent, logType) {
    if (this.shouldSkipOutput_()) {
      return;
    }

    this.writeLog(new TextLog(logContent, logType));
  }

  /**
   * Write a tree log to this.logs_.
   * To add a message to logs, this function should be called.
   * @param {!TreeDumper} logContent
   */
  writeTreeLog(logContent) {
    if (this.shouldSkipOutput_()) {
      return;
    }

    this.writeLog(new TreeLog(logContent));
  }

  /**
   * Write a log to this.logs_.
   * To add a message to logs, this function should be called.
   * @param {!BaseLog} log
   */
  writeLog(log) {
    if (this.shouldSkipOutput_()) {
      return;
    }

    this.logs_[this.startIndex_] = log;
    this.startIndex_ += 1;
    if (this.startIndex_ === LogStore.LOG_LIMIT) {
      this.startIndex_ = 0;
    }
  }

  /**
   * Clear this.logs_.
   * Set to initial states.
   */
  clearLog() {
    this.logs_ = Array(LogStore.LOG_LIMIT);
    this.startIndex_ = 0;
  }

  /** @private @return {boolean} */
  shouldSkipOutput_() {
    if (ChromeVoxState.instance && ChromeVoxState.instance.currentRange &&
        ChromeVoxState.instance.currentRange.start &&
        ChromeVoxState.instance.currentRange.start.node &&
        ChromeVoxState.instance.currentRange.start.node.root) {
      return ChromeVoxState.instance.currentRange.start.node.root.docUrl
                 .indexOf(chrome.extension.getURL(
                     'chromevox/log_page/log.html')) === 0;
    }
    return false;
  }

  /**
   * @return {LogStore}
   */
  static getInstance() {
    if (!LogStore.instance) {
      LogStore.instance = new LogStore();
    }
    return LogStore.instance;
  }
};

/**
 * @const
 * @private
 */
LogStore.LOG_LIMIT = 3000;

/**
 * Global instance.
 * @type {LogStore}
 */
LogStore.instance;

BridgeHelper.registerHandler(
    BridgeTarget.LOG_STORE, BridgeAction.CLEAR_LOG,
    () => LogStore.instance.clearLog());
BridgeHelper.registerHandler(
    BridgeTarget.LOG_STORE, BridgeAction.GET_LOGS,
    () => LogStore.instance.getLogs());
