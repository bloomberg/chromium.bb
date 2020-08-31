// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output logger.
 */

goog.provide('OutputRulesStr');

OutputRulesStr = class {
  /**
   * @param {string} enableKey The key to enable logging in localStorage
   */
  constructor(enableKey) {
    /** @type {string} */
    this.str = '';
    /** @type {function(): boolean} */
    this.disableLogging = function() {
      return localStorage[enableKey] != 'true';
    };
  }

  /** @param {string} str */
  write(str) {
    if (this.disableLogging()) {
      return;
    }
    this.str += str;
    }

    /**
     * @param {string} token
     * @param {string|undefined} value
     */
    writeTokenWithValue(token, value) {
      if (this.disableLogging()) {
        return;
      }
      this.writeToken(token);
      if (value) {
        this.str += value;
      } else {
        this.str += 'EMPTY';
      }
      this.str += '\n';
    }

  /** @param {string} token */
  writeToken(token) {
    if (this.disableLogging()) {
      return;
    }
    this.str += '$' + token + ': ';
    }

    /**
     * @param {{event: string,
     *          role: string,
     *          navigation: (string|undefined),
     *          output: (string|undefined)}} rule
     */
    writeRule(rule) {
      if (this.disableLogging()) {
        return;
      }
      this.str += 'RULE: ';
      this.str += rule.event + ' ' + rule.role;
      if (rule.navigation) {
        this.str += ' ' + rule.navigation;
      }
      if (rule.output) {
        this.str += ' ' + rule.output;
      }
      this.str += '\n';
    }

  bufferClear() {
    if (this.disableLogging()) {
      return;
    }
    this.str += '\nBuffer is cleared.\n';
  }

  /** @param {string} errorMsg */
  writeError(errorMsg) {
    if (this.disableLogging()) {
      return;
    }
    this.str += 'ERROR with message: ';
    this.str += errorMsg;
    this.str += '\n';
  }
};
