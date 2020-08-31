// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of the current braille translators.
 */

goog.provide('BrailleTranslatorManager');

goog.require('BrailleTable');
goog.require('ExpandingBrailleTranslator');
goog.require('LibLouis');

BrailleTranslatorManager = class {
  /**
   * @param {LibLouis=} opt_liblouisForTest Liblouis instance to use
   *     for testing.
   */
  constructor(opt_liblouisForTest) {
    /**
     * @type {!LibLouis}
     * @private
     */
    this.liblouis_ =
        opt_liblouisForTest ||
        new LibLouis(
            chrome.extension.getURL('chromevox/braille/liblouis_wrapper.js'),
            chrome.extension.getURL('chromevox/braille/tables'),
            this.loadLiblouis_.bind(this));

    /**
     * @type {!Array<function()>}
     * @private
     */
    this.changeListeners_ = [];
    /**
     * @type {!Array<BrailleTable.Table>}
     * @private
     */
    this.tables_ = [];
    /**
     * @type {ExpandingBrailleTranslator}
     * @private
     */
    this.expandingTranslator_ = null;
    /**
     * @type {LibLouis.Translator}
     * @private
     */
    this.defaultTranslator_ = null;
    /**
     * @type {string?}
     * @private
     */
    this.defaultTableId_ = null;
    /**
     * @type {LibLouis.Translator}
     * @private
     */
    this.uncontractedTranslator_ = null;
    /**
     * @type {string?}
     * @private
     */
    this.uncontractedTableId_ = null;
  }

  /**
   * Adds a listener to be called whenever there is a change in the
   * translator(s) returned by other methods of this instance.
   * @param {function()} listener The listener.
   */
  addChangeListener(listener) {
    this.changeListeners_.push(listener);
  }

  /**
   * Refreshes the braille translator(s) used for input and output.  This
   * should be called when something has changed (such as a preference) to
   * make sure that the correct translator is used.
   * @param {string} brailleTable The table for this translator to use.
   * @param {string=} opt_brailleTable8 Optionally specify an uncontracted
   * table.
   */
  refresh(brailleTable, opt_brailleTable8) {
    if (brailleTable && brailleTable === this.defaultTableId_) {
      return;
    }

    const tables = this.tables_;
    if (tables.length == 0) {
      return;
    }

    // Look for the table requested.
    let table = BrailleTable.forId(tables, brailleTable);
    if (!table) {
      // Match table against current locale.
      const currentLocale = chrome.i18n.getMessage('@@ui_locale').split(/[_-]/);
      const major = currentLocale[0];
      const minor = currentLocale[1];
      const firstPass = tables.filter(function(table) {
        return table.locale.split(/[_-]/)[0] == major;
      });
      if (firstPass.length > 0) {
        table = firstPass[0];
        if (minor) {
          const secondPass = firstPass.filter(function(table) {
            return table.locale.split(/[_-]/)[1] == minor;
          });
          if (secondPass.length > 0) {
            table = secondPass[0];
          }
        }
      }
    }
    if (!table) {
      table = BrailleTable.forId(tables, 'en-US-comp8');
    }

    // If the user explicitly set an 8 dot table, use that when looking
    // for an uncontracted table.  Otherwise, use the current table and let
    // getUncontracted find an appropriate corresponding table.
    const table8Dot = opt_brailleTable8 ?
        BrailleTable.forId(tables, opt_brailleTable8) :
        null;
    const uncontractedTable =
        BrailleTable.getUncontracted(tables, table8Dot || table);
    const newDefaultTableId = table.id;
    const newUncontractedTableId =
        table.id === uncontractedTable.id ? null : uncontractedTable.id;
    if (newDefaultTableId === this.defaultTableId_ &&
        newUncontractedTableId === this.uncontractedTableId_) {
      return;
    }

    const finishRefresh = function(defaultTranslator, uncontractedTranslator) {
      this.defaultTableId_ = newDefaultTableId;
      this.uncontractedTableId_ = newUncontractedTableId;
      this.expandingTranslator_ = new ExpandingBrailleTranslator(
          defaultTranslator, uncontractedTranslator);
      this.defaultTranslator_ = defaultTranslator;
      this.uncontractedTranslator_ = uncontractedTranslator;
      this.changeListeners_.forEach(function(listener) {
        listener();
      });
    }.bind(this);

    this.liblouis_.getTranslator(table.fileNames, function(translator) {
      if (!newUncontractedTableId) {
        finishRefresh(translator, null);
      } else {
        this.liblouis_.getTranslator(
            uncontractedTable.fileNames, function(uncontractedTranslator) {
              finishRefresh(translator, uncontractedTranslator);
            });
      }
    }.bind(this));
  }

  /**
   * @return {ExpandingBrailleTranslator} The current expanding braille
   *     translator, or {@code null} if none is available.
   */
  getExpandingTranslator() {
    return this.expandingTranslator_;
  }

  /**
   * @return {LibLouis.Translator} The current braille translator to use
   *     by default, or {@code null} if none is available.
   */
  getDefaultTranslator() {
    return this.defaultTranslator_;
  }

  /**
   * @return {LibLouis.Translator} The current uncontracted braille
   *     translator, or {@code null} if it is the same as the default
   *     translator.
   */
  getUncontractedTranslator() {
    return this.uncontractedTranslator_;
  }

  /**
   * Asynchronously fetches the list of braille tables and refreshes the
   * translators when done.
   * @private
   */
  fetchTables_() {
    BrailleTable.getAll(function(tables) {
      this.tables_ = tables;

      // Initial refresh; set options from user preferences.
      this.refresh(localStorage['brailleTable']);
    }.bind(this));
  }

  /**
   * Loads the liblouis instance by attaching it to the document.
   * @private
   */
  loadLiblouis_() {
    this.fetchTables_();
  }

  /**
   * @return {!LibLouis} The liblouis instance used by this object.
   */
  getLibLouisForTest() {
    return this.liblouis_;
  }

  /**
   * @return {!Array<BrailleTable.Table>} The currently loaded braille
   *     tables, or an empty array if they are not yet loaded.
   */
  getTablesForTest() {
    return this.tables_;
  }
};
