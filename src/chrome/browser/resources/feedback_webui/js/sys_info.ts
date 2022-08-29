// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * A queue of a sequence of closures that will incrementally build the sys info
 * html table.
 */
const tableCreationClosuresQueue: (() => void)[] = [];

/**
 * The time used to post delayed tasks in MS. Currently set to be enough for two
 * frames.
 */
const STANDARD_DELAY_MS: number = 32;

function getValueDivForButton(button: HTMLElement) {
  return $(button.id.substr(0, button.id.length - 4));
}

function getButtonForValueDiv(valueDiv: HTMLElement) {
  return $(valueDiv.id + '-btn');
}

function getSystemInformation():
    Promise<chrome.feedbackPrivate.SystemInformation[]> {
  return new Promise(
      resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
}

/**
 * Expands the multiline table cell that contains the given valueDiv.
 * @param button The expand button.
 * @param valueDiv The div that contains the multiline logs.
 * @param delayFactor A value used for increasing the delay after which the cell
 *     will be expanded. Useful for expandAll() since it expands the multiline
 *     cells one after another with each expension done slightly after the
 *     previous one.
 */
function expand(
    button: HTMLElement, valueDiv: HTMLElement, delayFactor: number) {
  button.textContent = loadTimeData.getString('sysinfoPageCollapseBtn');
  // Show the spinner container.
  const valueCell = valueDiv.parentNode as HTMLElement;
  valueCell.removeAttribute('aria-hidden');
  (valueCell.firstChild as HTMLElement).hidden = false;
  // Expanding huge logs can take a very long time, so we do it after a delay
  // to have a chance to render the spinner.
  setTimeout(function() {
    valueCell.className = 'number-expanded';
    // Hide the spinner container.
    (valueCell.firstChild as HTMLElement).hidden = true;
  }, STANDARD_DELAY_MS * delayFactor);
}

/**
 * Collapses the multiline table cell that contains the given valueDiv.
 * @param button The expand button.
 * @param valueDiv The div that contains the multiline logs.
 */
function collapse(button: HTMLElement, valueDiv: HTMLElement) {
  button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
  (valueDiv.parentNode as HTMLElement).className = 'number-collapsed';
  // Don't have screen readers announce the empty cell.
  const valueCell = valueDiv.parentNode as HTMLElement;
  valueCell.setAttribute('aria-hidden', 'true');
}

/**
 * Toggles whether an item is collapsed or expanded.
 */
function changeCollapsedStatus(e: Event) {
  const button = e.target as HTMLElement;
  const valueDiv = getValueDivForButton(button);
  if ((valueDiv.parentNode as HTMLElement).className === 'number-collapsed') {
    expand(button, valueDiv, 1);
  } else {
    collapse(button, valueDiv);
  }
}

/**
 * Collapses all log items.
 */
function collapseAll() {
  const valueDivs = document.body.querySelectorAll<HTMLElement>('.stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if ((valueDivs[i]!.parentNode as HTMLElement).className !==
        'number-expanded') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]!);
    if (button) {
      collapse(button, valueDivs[i]!);
    }
  }
}

/**
 * Expands all log items.
 */
function expandAll() {
  const valueDivs = document.body.querySelectorAll<HTMLElement>('.stat-value');
  for (let i = 0; i < valueDivs.length; ++i) {
    if ((valueDivs[i]!.parentNode as HTMLElement).className !==
        'number-collapsed') {
      continue;
    }
    const button = getButtonForValueDiv(valueDivs[i]!);
    if (button) {
      expand(button, valueDivs[i]!, i + 1);
    }
  }
}

function createNameCell(key: string): HTMLElement {
  const nameCell = document.createElement('td');
  nameCell.setAttribute('class', 'name');
  const nameDiv = document.createElement('div');
  nameDiv.setAttribute('class', 'stat-name');
  nameDiv.appendChild(document.createTextNode(key));
  nameCell.appendChild(nameDiv);
  return nameCell;
}

function createButtonCell(key: string, isMultiLine: boolean): HTMLElement {
  const buttonCell = document.createElement('td');
  buttonCell.setAttribute('class', 'button-cell');

  if (isMultiLine) {
    const button = document.createElement('button');
    button.setAttribute('id', '' + key + '-value-btn');
    button.setAttribute('aria-controls', '' + key + '-value');
    button.onclick = changeCollapsedStatus;
    button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
    buttonCell.appendChild(button);
  } else {
    // Don't have screen reader read the empty cell.
    buttonCell.setAttribute('aria-hidden', 'true');
  }

  return buttonCell;
}

function createValueCell(
    key: string, value: string, isMultiLine: boolean): HTMLElement {
  const valueCell = document.createElement('td');
  const valueDiv = document.createElement('div');
  valueDiv.setAttribute('class', 'stat-value');
  valueDiv.setAttribute('id', '' + key + '-value');
  valueDiv.appendChild(document.createTextNode(value));

  if (isMultiLine) {
    valueCell.className = 'number-collapsed';
    const loadingContainer =
        $('spinner-container').cloneNode(true) as HTMLElement;
    loadingContainer.setAttribute('id', '' + key + '-value-loading');
    loadingContainer.hidden = true;
    valueCell.appendChild(loadingContainer);
    // Don't have screen readers read the empty cell.
    valueCell.setAttribute('aria-hidden', 'true');
  } else {
    valueCell.className = 'number';
  }

  valueCell.appendChild(valueDiv);
  return valueCell;
}

function createTableRow(key: string, value: string): HTMLElement {
  const row = document.createElement('tr');

  // Avoid using element.scrollHeight as it's very slow. crbug.com/653968.
  const isMultiLine = value.split('\n').length > 2 || value.length > 1000;

  row.appendChild(createNameCell(key));
  row.appendChild(createButtonCell(key, isMultiLine));
  row.appendChild(createValueCell(key, value, isMultiLine));

  return row;
}

/**
 * Finalize the page after the content has been loaded.
 */
function finishPageLoading() {
  $('collapseAllBtn').onclick = collapseAll;
  $('expandAllBtn').onclick = expandAll;

  $('spinner-container').hidden = true;
}

/**
 * Pops a closure from the front of the queue and executes it.
 */
function processQueue() {
  const closure = tableCreationClosuresQueue.shift();
  if (closure) {
    closure();
  }

  if (tableCreationClosuresQueue.length > 0) {
    // Post a task to process the next item in the queue.
    setTimeout(processQueue, STANDARD_DELAY_MS);
  }
}

/**
 * Creates a closure that creates a table row for the given key and value.
 * @param key The name of the log.
 * @param value The contents of the log.
 * @return A closure that creates a row for the given log.
 */
function createTableRowWrapper(key: string, value: string): () => void {
  return function() {
    $('detailsTable').appendChild(createTableRow(key, value));
  };
}

/**
 * Creates closures to build the system information table row by row
 * incrementally.
 * @param systemInfo The system information that will be used to fill the table.
 */
function createTable(systemInfo: chrome.feedbackPrivate.SystemInformation[]) {
  for (const key in systemInfo) {
    const item = systemInfo[key]!;
    tableCreationClosuresQueue.push(
        createTableRowWrapper(item['key'], item['value']));
  }

  tableCreationClosuresQueue.push(finishPageLoading);

  processQueue();
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  getSystemInformation().then(createTable);
};
