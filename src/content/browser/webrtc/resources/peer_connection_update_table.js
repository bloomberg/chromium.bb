// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

const MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED = 10;
/**
 * The data of a peer connection update.
 * @param {number} pid The id of the renderer.
 * @param {number} lid The id of the peer conneciton inside a renderer.
 * @param {string} type The type of the update.
 * @param {string} value The details of the update.
 */
class PeerConnectionUpdateEntry {
  constructor(pid, lid, type, value) {
    /**
     * @type {number}
     */
    this.pid = pid;

    /**
     * @type {number}
     */
    this.lid = lid;

    /**
     * @type {string}
     */
    this.type = type;

    /**
     * @type {string}
     */
    this.value = value;
  }
}

/**
 * Maintains the peer connection update log table.
 */
export class PeerConnectionUpdateTable {
  constructor() {
    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_ID_SUFFIX_ = '-update-log';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_CONTAINER_CLASS_ = 'update-log-container';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_TABLE_CLASS = 'update-log-table';
  }

  /**
   * Adds the update to the update table as a new row. The type of the update
   * is set to the summary of the cell; clicking the cell will reveal or hide
   * the details as the content of a TextArea element.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @param {!PeerConnectionUpdateEntry} update The update to add.
   */
  addPeerConnectionUpdate(peerConnectionElement, update) {
    const tableElement = this.ensureUpdateContainer_(peerConnectionElement);

    const row = document.createElement('tr');
    tableElement.firstChild.appendChild(row);

    const time = new Date(parseFloat(update.time));
    const timeItem = document.createElement('td');
    timeItem.textContent = time.toLocaleString();
    row.appendChild(timeItem);

    // `type` is a display variant of update.type which does not get serialized
    // into chrome://webrtc-internals.
    let type = update.type;

    if (update.value.length === 0) {
      const typeItem = document.createElement('td');
      typeItem.textContent = type;
      row.appendChild(typeItem);
      return;
    }

    if (update.type === 'icecandidate' || update.type === 'addIceCandidate') {
      // extract ICE candidate type from the field following typ.
      const candidateType = update.value.match(/(?: typ )(host|srflx|relay)/);
      if (candidateType) {
        type += ' (' + candidateType[1] + ')';
      }
    } else if (
        update.type === 'createOfferOnSuccess' ||
        update.type === 'createAnswerOnSuccess') {
      this.setLastOfferAnswer_(tableElement, update);
    } else if (update.type === 'setLocalDescription') {
      if (update.value !== this.getLastOfferAnswer_(tableElement)) {
        type += ' (munged)';
      }
    } else if (update.type === 'setConfiguration') {
      // Update the configuration that is displayed at the top.
      peerConnectionElement.firstChild.children[2].textContent = update.value;
    } else if (['iceconnectionstatechange', 'connectionstatechange',
        'signalingstatechange'].includes(update.type)) {
      const fieldName = {
        'iceconnectionstatechange' : 'iceconnectionstate',
        'connectionstatechange' : 'connectionstate',
        'signalingstatechange' : 'signalingstate',
      }[update.type];
      const el = peerConnectionElement.getElementsByClassName(fieldName)[0];
      const numberOfEvents = el.textContent.split(' => ').length;
      if (numberOfEvents < MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED) {
        el.textContent += ' => ' + update.value;
      } else if (numberOfEvents === MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED) {
        el.textContent += ' ...';
      }
    }

    const summaryItem = $('summary-template').content.cloneNode(true);
    const summary = summaryItem.querySelector('summary');
    summary.textContent = type;
    row.appendChild(summaryItem);

    const valueContainer = document.createElement('pre');
    const details = row.cells[1].childNodes[0];
    details.appendChild(valueContainer);

    // Highlight ICE failures and failure callbacks.
    if ((update.type === 'iceconnectionstatechange' &&
         update.value === 'failed') ||
        (update.type === 'iceconnectionstatechange (legacy)' &&
         update.value === 'failed') ||
        update.type.indexOf('OnFailure') !== -1 ||
        update.type === 'addIceCandidateFailed') {
      valueContainer.parentElement.classList.add('update-log-failure');
    }

    // RTCSessionDescription is serialized as 'type: <type>, sdp:'
    if (update.value.indexOf(', sdp:') !== -1) {
      const [type, sdp] = update.value.substr(6).split(', sdp: ');

      // Create a copy-to-clipboard button.
      const copyBtn = document.createElement('button');
      copyBtn.textContent = 'Copy description to clipboard';
      copyBtn.onclick = () => {
        navigator.clipboard.writeText(JSON.stringify({type, sdp}));
      };
      valueContainer.appendChild(copyBtn);

      // Fold the SDP sections.
      const sections = sdp.split('\nm=')
        .map((part, index) => (index > 0 ?
          'm=' + part : part).trim() + '\r\n');
      summary.textContent +=
        ' (type: "' + type + '", ' + sections.length + ' sections)';
      sections.forEach(section => {
        const lines = section.trim().split('\n');
        const sectionDetails = document.createElement('details');
        sectionDetails.open = true;
        sectionDetails.textContent = lines.slice(1).join('\n');

        const sectionSummary = document.createElement('summary');
        sectionSummary.textContent =
          lines[0].trim() + ' (' + (lines.length - 1) + ' more lines)';
        sectionDetails.appendChild(sectionSummary);

        valueContainer.appendChild(sectionDetails);
      });
    } else {
      valueContainer.textContent = update.value;
    }
  }

  /**
   * Makes sure the update log table of the peer connection is created.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @return {!Element} The log table element.
   * @private
   */
  ensureUpdateContainer_(peerConnectionElement) {
    const tableId = peerConnectionElement.id + this.UPDATE_LOG_ID_SUFFIX_;
    let tableElement = $(tableId);
    if (!tableElement) {
      const tableContainer = document.createElement('div');
      tableContainer.className = this.UPDATE_LOG_CONTAINER_CLASS_;
      peerConnectionElement.appendChild(tableContainer);

      tableElement = document.createElement('table');
      tableElement.className = this.UPDATE_LOG_TABLE_CLASS;
      tableElement.id = tableId;
      tableElement.border = 1;
      tableContainer.appendChild(tableElement);
      tableElement.appendChild(
          $('time-event-template').content.cloneNode(true));
    }
    return tableElement;
  }

  /**
   * Store the last createOfferOnSuccess/createAnswerOnSuccess to compare to
   * setLocalDescription and visualize SDP munging.
   *
   * @param {!Element} tableElement The peerconnection update element.
   * @param {!PeerConnectionUpdateEntry} update The update to add.
   * @private
   */
  setLastOfferAnswer_(tableElement, update) {
    tableElement['data-lastofferanswer'] = update.value;
  }

  /**
   * Retrieves the last createOfferOnSuccess/createAnswerOnSuccess to compare
   * to setLocalDescription and visualize SDP munging.
   *
   * @param {!Element} tableElement The peerconnection update element.
   * @private
   */
  getLastOfferAnswer_(tableElement) {
    return tableElement['data-lastofferanswer'];
  }
}
