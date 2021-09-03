// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions of all types related to output.
 */

goog.provide('OutputAction');
goog.provide('OutputContextOrder');
goog.provide('OutputEarconAction');
goog.provide('OutputEventType');
goog.provide('OutputNodeSpan');
goog.provide('OutputSelectionSpan');
goog.provide('OutputSpeechProperties');

/**
 * The ordering of contextual output.
 * @enum {string}
 */
OutputContextOrder = {
  // The (ancestor) context comes before the node output.
  FIRST: 'first',
  // The (ancestor) context comes before the node output when moving forward,
  // after when moving backward.
  DIRECTED: 'directed',
  // The (ancestor) context comes after the node output.
  LAST: 'last'
};

/**
 * Used to annotate utterances with speech properties.
 */
OutputSpeechProperties = class {
  constructor() {
    /** @private {!Object} */
    this.properties_ = {};
  }

  /** @return {!Object} */
  get properties() {
    return this.properties_;
  }

  /** @override */
  toJSON() {
    // Make a copy of our properties since the caller really shouldn't be
    // modifying our local state.
    const clone = {};
    for (const key in this.properties_) {
      clone[key] = this.properties_[key];
    }
    return clone;
  }
};

/**
 * Custom actions performed while rendering an output string.
 */
OutputAction = class {
  constructor() {}

  run() {}
};

/**
 * Action to play an earcon.
 */
OutputEarconAction = class extends OutputAction {
  /**
   * @param {string} earconId
   * @param {chrome.automation.Rect=} opt_location
   */
  constructor(earconId, opt_location) {
    super();

    /** @type {string} */
    this.earconId = earconId;
    /** @type {chrome.automation.Rect|undefined} */
    this.location = opt_location;
  }

  /** @override */
  run() {
    ChromeVox.earcons.playEarcon(Earcon[this.earconId], this.location);
  }

  /** @override */
  toJSON() {
    return {earconId: this.earconId};
  }
};

/**
 * Annotation for text with a selection inside it.
 */
OutputSelectionSpan = class {
  /**
   * @param {number} startIndex
   * @param {number} endIndex
   */
  constructor(startIndex, endIndex) {
    // TODO(dtseng): Direction lost below; should preserve for braille panning.
    this.startIndex = startIndex < endIndex ? startIndex : endIndex;
    this.endIndex = endIndex > startIndex ? endIndex : startIndex;
  }
};

/**
 * Wrapper for automation nodes as annotations.  Since the
 * {@code AutomationNode} constructor isn't exposed in the API, this class is
 * used to allow instanceof checks on these annotations.
 */
OutputNodeSpan = class {
  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_offset Offsets into the node's text. Defaults to 0.
   */
  constructor(node, opt_offset) {
    this.node = node;
    this.offset = opt_offset ? opt_offset : 0;
  }
};

/**
 * Possible events handled by ChromeVox internally.
 * @enum {string}
 */
OutputEventType = {
  NAVIGATE: 'navigate'
};
