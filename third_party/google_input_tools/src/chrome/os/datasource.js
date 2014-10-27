// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.DataSource');

goog.require('goog.events');
goog.require('goog.events.Event');
goog.require('goog.events.EventTarget');
goog.require('goog.functions');


/**
 * The data source.
 *
 * @param {number} numOfCanddiate The number of canddiate to fetch.
 * @param {function(string, !Array.<!Object>)} callback .
 * @constructor
 * @extends {goog.events.EventTarget}
 */
i18n.input.chrome.DataSource = function(numOfCanddiate, callback) {
  goog.base(this);

  /**
   * The number of candidates to fetch.
   *
   * @type {number}
   */
  this.numOfCandidate = numOfCanddiate;

  /** @protected {function(string, !Array.<!Object>)} */
  this.callback = callback;
};
goog.inherits(i18n.input.chrome.DataSource, goog.events.EventTarget);


/** @type {boolean} */
i18n.input.chrome.DataSource.prototype.ready = false;


/**
 * The correction level.
 *
 * @protected {number}
 */
i18n.input.chrome.DataSource.prototype.correctionLevel = 0;


/**
 * The language code.
 *
 * @type {string}
 * @protected
 */
i18n.input.chrome.DataSource.prototype.language;


/**
 * Sets the langauge code.
 *
 * @param {string} language The language code.
 */
i18n.input.chrome.DataSource.prototype.setLanguage = function(
    language) {
  this.language = language;
};


/**
 * True if the datasource is ready.
 *
 * @return {boolean} .
 */
i18n.input.chrome.DataSource.prototype.isReady = function() {
  return this.ready;
};


/**
 * Creates the common payload for completion or prediction request.
 *
 * @return {!Object} The payload.
 * @protected
 */
i18n.input.chrome.DataSource.prototype.createCommonPayload =
    function() {
  return {
    'itc': this.getInputToolCode(),
    'num': this.numOfCandidate
  };
};


/**
 * Gets the input tool code.
 *
 * @return {string} .
 */
i18n.input.chrome.DataSource.prototype.getInputToolCode = function() {
  return this.language + '-t-i0-und';
};


/**
 * Sends completion request.
 *
 * @param {string} query The query .
 * @param {string} context The context .
 * @param {!Object=} opt_spatialData .
 */
i18n.input.chrome.DataSource.prototype.sendCompletionRequest =
    goog.functions.NULL;


/**
 * Sends prediciton request.
 *
 * @param {string} context The context.
 */
i18n.input.chrome.DataSource.prototype.sendPredictionRequest =
    goog.functions.NULL;


/**
 * Sets the correction level.
 *
 * @param {number} level .
 */
i18n.input.chrome.DataSource.prototype.setCorrectionLevel = function(level) {
  this.correctionLevel = level;
};


/**
 * Clears the data source.
 */
i18n.input.chrome.DataSource.prototype.clear = goog.functions.NULL;


/**
 * The event type.
 *
 * @enum {string}
 */
i18n.input.chrome.DataSource.EventType = {
  CANDIDATES_BACK: goog.events.getUniqueId('cb'),
  READY: goog.events.getUniqueId('r')
};



/**
 * The candidates is fetched back.
 *
 * @param {string} source The source.
 * @param {!Array.<!Object>} candidates The candidates.
 * @constructor
 * @extends {goog.events.Event}
 */
i18n.input.chrome.DataSource.CandidatesBackEvent =
    function(source, candidates) {
  goog.base(this, i18n.input.chrome.DataSource.EventType.CANDIDATES_BACK);

  /**
   * The source.
   *
   * @type {string}
   */
  this.source = source;

  /**
   * The candidate list.
   *
   * @type {!Array.<!Object>}
   */
  this.candidates = candidates;
};
goog.inherits(i18n.input.chrome.DataSource.CandidatesBackEvent,
    goog.events.Event);

