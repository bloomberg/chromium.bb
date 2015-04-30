search_engines_private_externs.js

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: searchEnginesPrivate */

/**
 * @const
 */
chrome.searchEnginesPrivate = {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#type-HotwordFeature
 */
chrome.searchEnginesPrivate.HotwordFeature = {
  SEARCH: 'SEARCH',
  ALWAYS_ON: 'ALWAYS_ON',
  RETRAIN_LINK: 'RETRAIN_LINK',
  AUDIO_HISTORY: 'AUDIO_HISTORY',
};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#type-SearchEngineType
 */
chrome.searchEnginesPrivate.SearchEngineType = {
  DEFAULT: 'DEFAULT',
  OTHER: 'OTHER',
};

/**
 * @typedef {{
 *   availability: !Array<!chrome.searchEnginesPrivate.HotwordFeature>,
 *   audioHistoryState: (string|undefined),
 *   errorMsg: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#type-HotwordState
 */
var HotwordState;

/**
 * @typedef {{
 *   guid: string,
 *   name: string,
 *   keyword: string,
 *   url: string,
 *   type: !chrome.searchEnginesPrivate.SearchEngineType,
 *   isSelected: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#type-SearchEngine
 */
var SearchEngine;

/**
 * Gets a list of the search engines. Exactly one of the values should have
 * default == true.
 * @param {function(!Array<SearchEngine>):void} callback
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-getSearchEngines
 */
chrome.searchEnginesPrivate.getSearchEngines = function(callback) {};

/**
 * Sets the search engine with the given GUID as the selected default.
 * @param {string} guid
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-setSelectedSearchEngine
 */
chrome.searchEnginesPrivate.setSelectedSearchEngine = function(guid) {};

/**
 * Adds a new "other" (non-default) search engine with the given name, keyword,
 * and URL.
 * @param {string} name
 * @param {string} keyword
 * @param {string} url
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-addOtherSearchEngine
 */
chrome.searchEnginesPrivate.addOtherSearchEngine = function(name, keyword, url) {};

/**
 * Updates the search engine that has the given GUID, with the given name,
 * keyword, and URL.
 * @param {string} guid
 * @param {string} name
 * @param {string} keyword
 * @param {string} url
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-updateSearchEngine
 */
chrome.searchEnginesPrivate.updateSearchEngine = function(guid, name, keyword, url) {};

/**
 * Removes the search engine with the given GUID.
 * @param {string} guid
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-removeSearchEngine
 */
chrome.searchEnginesPrivate.removeSearchEngine = function(guid) {};

/**
 * Gets the hotword state.
 * @param {function(HotwordState):void} callback
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-getHotwordState
 */
chrome.searchEnginesPrivate.getHotwordState = function(callback) {};

/**
 * Opts in to hotwording; |retrain| indicates whether the user wants to retrain
 * the hotword system with their voice by launching the audio verification app.
 * @param {boolean} retrain
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-optIntoHotwording
 */
chrome.searchEnginesPrivate.optIntoHotwording = function(retrain) {};

/**
 * Fires when the list of search engines changes or when the user selects a
 * preferred default search engine. The new list of engines is passed along.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#event-onSearchEnginesChanged
 */
chrome.searchEnginesPrivate.onSearchEnginesChanged;


