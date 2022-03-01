// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates a ICE candidate grid.
 * @param {Element} peerConnectionElement
 */

import {$} from 'chrome://resources/js/util.m.js';
/**
 * A helper function for appending a child element to |parent|.
 * Copied from webrtc_internals.js
 *
 * @param {!Element} parent The parent element.
 * @param {string} tag The child element tag.
 * @param {string} text The textContent of the new DIV.
 * @return {!Element} the new DIV element.
 */
function appendChildWithText(parent, tag, text) {
  const child = document.createElement(tag);
  child.textContent = text;
  parent.appendChild(child);
  return child;
}

export function createIceCandidateGrid(peerConnectionElement) {
  const container = document.createElement('details');
  appendChildWithText(container, 'summary', 'ICE candidate grid');

  const table = document.createElement('table');
  table.id = 'grid-' + peerConnectionElement.id;
  table.className = 'candidategrid';
  container.appendChild(table);

  const tableHeader = document.createElement('tr');
  table.append(tableHeader);

  // For candidate pairs.
  appendChildWithText(tableHeader, 'th', 'Candidate (pair) id');
  // [1] is used for both candidate pairs and individual candidates.
  appendChildWithText(tableHeader, 'th', 'State / Candidate type');
  // For individual candidates.
  appendChildWithText(tableHeader, 'th', 'Network type / address');
  appendChildWithText(tableHeader, 'th', 'Port');
  appendChildWithText(tableHeader, 'th', 'Protocol / candidate type');
  appendChildWithText(tableHeader, 'th', '(Pair) Priority');

  // For candidate pairs.
  appendChildWithText(tableHeader, 'th', 'Bytes sent / received');
  appendChildWithText(tableHeader, 'th',
      'STUN requests sent / responses received');
  appendChildWithText(tableHeader, 'th',
      'STUN requests received / responses sent');
  appendChildWithText(tableHeader, 'th', 'RTT');
  appendChildWithText(tableHeader, 'th', 'Last update');

  peerConnectionElement.appendChild(container);
}

/**
 * Creates or returns a table row in the ICE candidate grid.
 * @param {string} peerConnectionElement id
 * @param {string} stat object id
 * @param {type} type of the row
 */
function findOrCreateGridRow(peerConnectionElementId, statId, type) {
  const elementId = 'grid-' + peerConnectionElementId
      + '-' + statId + '-' + type;
  let row = document.getElementById(elementId);
  if (!row) {
    row = document.createElement('tr');
    row.id = elementId;
    for (let i = 0; i < 11; i++) {
      row.appendChild(document.createElement('td'));
    }
    $('grid-' + peerConnectionElementId).appendChild(row);
  }
  return row;
}

/**
 * Updates a table row in the ICE candidate grid.
 * @param {string} peerConnectionElement id
 * @param {boolean} whether the pair is the selected pair of a transport
 *   (displayed bold)
 * @param {object} candidate pair stats report
 * @param {Map} full map of stats
 */
function appendRow(peerConnectionElement, active, candidatePair, stats) {
  const pairRow = findOrCreateGridRow(peerConnectionElement.id,
      candidatePair.id, 'candidatepair');
  pairRow.classList.add('candidategrid-candidatepair')
  if (active) {
    pairRow.classList.add('candidategrid-active');
  }
  // Set transport-specific fields.
  pairRow.children[0].innerText = candidatePair.id;
  pairRow.children[1].innerText = candidatePair.state;
  // Show (pair) priority as hex.
  pairRow.children[5].innerText =
      '0x' + parseInt(candidatePair.priority, 10).toString(16);
  pairRow.children[6].innerText =
      candidatePair.bytesSent + ' / ' + candidatePair.bytesReceived;
  pairRow.children[7].innerText =
    (parseInt(candidatePair.requestsSent, 10) +
      parseInt(candidatePair.consentRequestsSent, 10)) + ' / ' +
    candidatePair.responsesReceived;
  pairRow.children[8].innerText = candidatePair.requestsReceived + ' / ' +
    candidatePair.responsesSent;
  pairRow.children[9].innerText =
    candidatePair.currentRoundTripTime !== undefined ?
        candidatePair.currentRoundTripTime + 's' : '';
  pairRow.children[10].innerText = (new Date()).toLocaleTimeString();

  // Local candidate.
  const localRow = findOrCreateGridRow(peerConnectionElement.id,
      candidatePair.id, 'local');
  localRow.className = 'candidategrid-candidate'
  const localCandidate = stats.get(candidatePair.localCandidateId);
  ['id', 'type', 'address', 'port', 'candidateType',
      'priority'].forEach((stat, index) => {
    // Relay protocol is only set for local relay candidates.
    if (stat == 'candidateType' && localCandidate.relayProtocol) {
      localRow.children[index].innerText = localCandidate[stat] +
          '(' + localCandidate.relayProtocol + ')';
    } else if (stat === 'priority') {
      localRow.children[index].innerText = '0x' +
          parseInt(localCandidate[stat], 10).toString(16);
    } else {
      localRow.children[index].innerText = localCandidate[stat];
    }
  });
  // Network type is only for the local candidate
  // so put it into the pair row above the address.
  pairRow.children[2].innerText = localCandidate.networkType;
  // protocol must always be the same for the pair
  // so put it into the pair row above the candidate type.
  pairRow.children[4].innerText = localCandidate.protocol;

  // Remote candidate.
  const remoteRow = findOrCreateGridRow(peerConnectionElement.id,
      candidatePair.id, 'remote');
  remoteRow.className = 'candidategrid-candidate'
  const remoteCandidate = stats.get(candidatePair.remoteCandidateId);
  ['id', 'type', 'address', 'port', 'candidateType',
      'priority'].forEach((stat, index) => {
    if (stat === 'priority') {
      remoteRow.children[index].innerText = '0x' +
          parseInt(remoteCandidate[stat], 10).toString(16);
    } else {
      remoteRow.children[index].innerText = remoteCandidate[stat];
    }
  });
  return pairRow;
}

/**
 * Updates the (spec) ICE candidate grid.
 * @param {Element} peerConnectionElement
 * @param {Map} stats reconstructed stats object.
 */
export function updateIceCandidateGrid(peerConnectionElement, stats) {
  const container = $('grid-' + peerConnectionElement.id);
  // Remove the active/bold marker from all rows.
  container.childNodes.forEach(row => {
    row.classList.remove('candidategrid-active');
  });
  let activePairIds = [];
  // Find the active transport(s), then find its candidate pair
  // and display it first. Note that previously selected pairs continue to be
  // shown since rows are not removed.
  stats.forEach(transportReport => {
    if (transportReport.type !== 'transport') {
      return;
    }
    if (!transportReport.selectedCandidatePairId) {
      return;
    }
    activePairIds.push(transportReport.selectedCandidatePairId);
    appendRow(peerConnectionElement, true,
        stats.get(transportReport.selectedCandidatePairId), stats);
  });

  // Then iterate over the other candidate pairs.
  stats.forEach(report => {
    if (report.type !== 'candidate-pair' || activePairIds.includes(report.id)) {
      return;
    }
    appendRow(peerConnectionElement, false, report, stats);
  });
}

