// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @typedef {{
 *   sitelist: Array<string>,
 *   greylist: Array<string>,
 * }}
 */
let RuleSet;

/**
 * @typedef {{
 *   gpo: RuleSet,
 *   ieem: (RuleSet|undefined),
 *   external: (RuleSet|undefined),
 * }}
 */
let RuleSetList;

/**
 * Clears the table, and inserts a header row.
 * @param {HTMLTableElement} table
 */
function clearTable(table) {
  table.innerHTML = '';
  const headerRow = document.importNode($('header-row-template').content, true);
  table.appendChild(headerRow);
}

/**
 * @param {string} rule
 * @return {string} String describing the rule type.
 */
function getRuleType(rule) {
  if (rule == '*') {
    return 'wildcard';
  }
  if (rule.includes('/')) {
    return 'prefix';
  }
  return 'hostname';
}

/**
 * Creates and returns a <tr> element for the given rule.
 * @param {string} rule
 * @param {string} rulesetName
 * @return {HTMLTableRowElement}
 */
function createRowForRule(rule, rulesetName) {
  const row = document.importNode($('rule-row-template').content, true);
  const cells = row.querySelectorAll('td');
  cells[0].innerText = rule;
  cells[0].className = 'url';
  cells[1].innerText = rulesetName;
  cells[2].innerText = getRuleType(rule);
  cells[3].innerText = /^!/.test(rule) ? 'yes' : 'no';
  return /** @type {HTMLTableRowElement} */ (row);
}

/**
 * Updates the content of all tables after receiving data from the backend.
 * @param {RuleSetList} rulesets
 */
function updateTables(rulesets) {
  clearTable(/** @type {HTMLTableElement} */ ($('sitelist')));
  clearTable(/** @type {HTMLTableElement} */ ($('greylist')));

  for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
    for (const [listName, rules] of Object.entries(ruleset)) {
      const table = $(listName);
      for (const rule of rules) {
        table.appendChild(createRowForRule(rule, rulesetName));
      }
    }
  }
}

cr.sendWithPromise('getAllRulesets').then(updateTables);

function checkUrl() {
  const url = $('url-checker-input').value;
  if (!url) {
    $('output').innerText = '';
    return;
  }
  cr.sendWithPromise('getDecision', url)
      .then(decision => {
        // URL is valid.
        $('output').innerText = JSON.stringify(decision, null, 2);
      })
      .catch(err => {
        // URL is invalid.
        $('output').innerText =
            'Invalid URL. Make sure it is formatted properly.';
      });
}

$('url-checker-input').addEventListener('input', checkUrl);
checkUrl();
