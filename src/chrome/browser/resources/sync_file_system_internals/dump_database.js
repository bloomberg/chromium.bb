// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles DumpDatabase tab for syncfs-internals.
 */
const DumpDatabase = (function() {
  'use strict';

  const DumpDatabase = {};

  /**
   * Get the database dump.
   */
  function getDatabaseDump() {
    chrome.send('getDatabaseDump');
  }

  /**
   * Creates an element named |elementName| containing the content |text|.
   * @param {string} elementName Name of the new element to be created.
   * @param {string} text Text to be contained in the new element.
   * @return {HTMLElement} The newly created HTML element.
   */
  function createElementFromText(elementName, text) {
    const element = document.createElement(elementName);
    element.appendChild(document.createTextNode(text));
    return element;
  }

  /**
   * Creates a table by filling |header| and |body|.
   * @param {HTMLElement} div The outer container of the table to be renderered.
   * @param {HTMLElement} header The table header element to be fillied by
   *     this function.
   * @param {HTMLElement} body The table body element to be filled by this
   *     function.
   * @param {Array} databaseDump List of dictionaries for the database dump.
   *     The first element must have metadata of the entry.
   *     The remaining elements must be dictionaries for the database dump,
   *     which can be iterated using the 'keys' fields given by the first
   *     element.
   */
  function createDatabaseDumpTable(div, header, body, databaseDump) {
    const metadata = databaseDump.shift();
    div.appendChild(createElementFromText('h3', metadata['title']));

    let tr = document.createElement('tr');
    for (let i = 0; i < metadata.keys.length; ++i) {
      tr.appendChild(createElementFromText('td', metadata.keys[i]));
    }
    header.appendChild(tr);

    for (let i = 0; i < databaseDump.length; i++) {
      const entry = databaseDump[i];
      tr = document.createElement('tr');
      for (let k = 0; k < metadata.keys.length; ++k) {
        tr.appendChild(createElementFromText('td', entry[metadata.keys[k]]));
      }
      body.appendChild(tr);
    }
  }

  /**
   * Handles callback from onGetDatabaseDump.
   * @param {Array} databaseDump List of lists for the database dump.
   */
  DumpDatabase.onGetDatabaseDump = function(databaseDump) {
    const placeholder = $('dump-database-placeholder');
    placeholder.innerHTML = '';
    for (let i = 0; i < databaseDump.length; ++i) {
      const div = document.createElement('div');
      const table = document.createElement('table');
      const header = document.createElement('thead');
      const body = document.createElement('tbody');
      createDatabaseDumpTable(div, header, body, databaseDump[i]);
      table.appendChild(header);
      table.appendChild(body);
      div.appendChild(table);
      placeholder.appendChild(div);
    }
  };

  function main() {
    getDatabaseDump();
    $('refresh-database-dump').addEventListener('click', getDatabaseDump);
  }

  document.addEventListener('DOMContentLoaded', main);
  return DumpDatabase;
})();
