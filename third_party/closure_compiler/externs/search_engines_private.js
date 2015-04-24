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
 * @typedef {{
 *   guid: string,
 *   name: string,
 *   isSelected: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#type-SearchEngine
 */
var SearchEngine;

/**
 * Gets a list of the "default” search engines. Exactly one of the values
 * should have default == true.
 * @param {function(!Array<SearchEngine>):void} callback
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-getDefaultSearchEngines
 */
chrome.searchEnginesPrivate.getDefaultSearchEngines = function(callback) {};

/**
 * Sets the search engine with the given GUID as the selected default.
 * @param {string} guid
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#method-setSelectedSearchEngine
 */
chrome.searchEnginesPrivate.setSelectedSearchEngine = function(guid) {};

/**
 * Fires when the list of default search engines changes or when the user
 * selects a preferred default search engine.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/searchEnginesPrivate#event-onDefaultSearchEnginesChanged
 */
chrome.searchEnginesPrivate.onDefaultSearchEnginesChanged;


