// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   overrideCwsContainerUrlForTest: (string|undefined),
 *   overrideCwsContainerOriginForTest: (string|undefined)
 * }}
 */
var SuggestAppDialogState;

/**
 * @param {!Object} unformatted Unformatted option.
 * @constructor
 * @struct
 */
function LaunchParam(unformatted) {
  /**
   * @type {DialogType}
   * @const
   */
  this.type = unformatted['type'] || DialogType.FULL_PAGE;

  /**
   * @type {string}
   * @const
   */
  this.action = unformatted['action'] ? unformatted['action'] : '';

  /**
   * @type {string}
   * @const
   */
  this.currentDirectoryURL = unformatted['currentDirectoryURL'] ?
      unformatted['currentDirectoryURL'] : '';

  /**
   * @type {string}
   * @const
   */
  this.selectionURL =
      unformatted['selectionURL'] ? unformatted['selectionURL'] : '';

  /**
   * @type {string}
   * @const
   */
  this.targetName = unformatted['targetName'] ? unformatted['targetName'] : '';

  /**
   * @type {!Array<!Object>}
   * @const
   */
  this.typeList = unformatted['typeList'] ? unformatted['typeList'] : [];

  /**
   * @type {boolean}
   * @const
   */
  this.includeAllFiles = !!unformatted['includeAllFiles'];

  /**
   * @type {boolean}
   * @const
   */
  this.shouldReturnLocalPath = !!unformatted['shouldReturnLocalPath'];

  /**
   * @type {!SuggestAppDialogState}
   * @const
   */
  this.suggestAppsDialogState =
      unformatted['suggestAppsDialogState'] ?
      unformatted['suggestAppsDialogState'] :
      {
        overrideCwsContainerUrlForTest: '',
        overrideCwsContainerOriginForTest: ''
      };
}
