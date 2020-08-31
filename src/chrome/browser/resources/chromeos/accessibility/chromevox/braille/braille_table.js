// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Holds information about a braille table.
 */

goog.provide('BrailleTable');


/**
 * @typedef {{
 *   locale:string,
 *   dots:string,
 *   id:string,
 *   grade:(string|undefined),
 *   variant:(string|undefined),
 *   fileNames:string
 * }}
 */
BrailleTable.Table;


/**
 * @const {string}
 */
BrailleTable.TABLE_PATH = 'chromevox/braille/tables.json';


/**
 * @const {string}
 * @private
 */
BrailleTable.COMMON_DEFS_FILENAME_ = 'cvox-common.cti';


/**
 * Retrieves a list of all available braille tables.
 * @param {function(!Array<BrailleTable.Table>)} callback
 *     Called asynchronously with an array of tables.
 */
BrailleTable.getAll = function(callback) {
  function appendCommonFilename(tables) {
    // Append the common definitions to all table filenames.
    tables.forEach(function(table) {
      table.fileNames += (',' + BrailleTable.COMMON_DEFS_FILENAME_);
    });
    return tables;
  }
  const url = chrome.extension.getURL(BrailleTable.TABLE_PATH);
  if (!url) {
    throw 'Invalid path: ' + BrailleTable.TABLE_PATH;
  }

  const xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        callback(appendCommonFilename(
            /** @type {!Array<BrailleTable.Table>} */ (
                JSON.parse(xhr.responseText))));
      }
    }
  };
  xhr.send();
};


/**
 * Finds a table in a list of tables by id.
 * @param {!Array<BrailleTable.Table>} tables tables to search in.
 * @param {string} id id of table to find.
 * @return {BrailleTable.Table} The found table, or null if not found.
 */
BrailleTable.forId = function(tables, id) {
  return tables.filter(function(table) {
    return table.id === id;
  })[0] ||
      null;
};


/**
 * Returns an uncontracted braille table corresponding to another, possibly
 * contracted, table.  If {@code table} is the lowest-grade table for its
 * locale and dot count, {@code table} itself is returned.
 * @param {!Array<BrailleTable.Table>} tables tables to search in.
 * @param {!BrailleTable.Table} table Table to match.
 * @return {!BrailleTable.Table} Corresponding uncontracted table,
 *     or {@code table} if it is uncontracted.
 */
BrailleTable.getUncontracted = function(tables, table) {
  function mostUncontractedOf(current, candidate) {
    // An 8 dot table for the same language is prefered over a 6 dot table
    // even if the locales differ by region.
    if (current.dots === '6' && candidate.dots === '8' &&
        current.locale.lastIndexOf(candidate.locale, 0) == 0) {
      return candidate;
    }
    if (current.locale === candidate.locale &&
        current.dots === candidate.dots && goog.isDef(current.grade) &&
        goog.isDef(candidate.grade) && candidate.grade < current.grade) {
      return candidate;
    }
    return current;
  }
  return tables.reduce(mostUncontractedOf, table);
};


/**
 * @param {!BrailleTable.Table} table Table to get name for.
 * @return {string} Localized display name.
 */
BrailleTable.getDisplayName = function(table) {
  const localeName = chrome.accessibilityPrivate.getDisplayNameForLocale(
      table.locale /* locale to be displayed */,
      chrome.i18n.getUILanguage().toLowerCase() /* locale to localize into */);
  if (!table.grade && !table.variant) {
    return localeName;
  } else if (table.grade && !table.variant) {
    return Msgs.getMsg(
        'braille_table_name_with_grade', [localeName, table.grade]);
  } else if (!table.grade && table.variant) {
    return Msgs.getMsg(
        'braille_table_name_with_variant', [localeName, table.variant]);
  } else {
    return Msgs.getMsg(
        'braille_table_name_with_variant_and_grade',
        [localeName, table.variant, table.grade]);
  }
};
