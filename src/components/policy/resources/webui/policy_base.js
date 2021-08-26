// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/js/action_link.js';

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import {addSingletonGetter, addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {define as crUiDefine} from 'chrome://resources/js/cr/ui.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * @typedef {{
 *    [id: string]: {
 *      name: string,
 *      policyNames: !Array<string>,
 * }}
 */
let PolicyNamesResponse;

/**
 * @typedef {!Array<{
 *  name: string,
 *  id: ?String,
 *  policies: {[name: string]: policy.Policy}
 * }>}
 */
let PolicyValuesResponse;

/**
 * @typedef {{
 *    level: string,
 *    scope: string,
 *    source: string,
 *    value: any,
 * }}
 */
let Conflict;

/**
 * @typedef {{
 *    ignored?: boolean,
 *    name: string,
 *    level: string,
 *    link: ?string,
 *    scope: string,
 *    source: string,
 *    error: string,
 *    warning: string,
 *    info: string,
 *    value: any,
 *    deprecated: ?boolean,
 *    future: ?boolean,
 *    allSourcesMerged: ?boolean,
 *    conflicts: ?Array<!Conflict>,
 *    superseded: ?Array<!Conflict>,
 * }}
 */
let Policy;

/**
 * @typedef {{
 *     id: ?string,
 *     isExtension?: boolean,
 *     name: string,
 *     policies: !Array<!Policy>
 * }}
 */
let PolicyTableModel;

/**
 * A box that shows the status of cloud policy for a device, machine or user.
 * @constructor
 * @extends {HTMLFieldSetElement}
 */
const StatusBox = crUiDefine(function() {
  const node = $('status-box-template').cloneNode(true);
  node.removeAttribute('id');
  return node;
});

StatusBox.prototype = {
  // Set up the prototype chain.
  __proto__: HTMLFieldSetElement.prototype,

  /**
   * Initialization function for the cr.ui framework.
   */
  decorate() {},

  /**
   * Sets the text of a particular named label element in the status box
   * and updates the visibility if needed.
   * @param {string} labelName The name of the label element that is being
   *     updated.
   * @param {string} labelValue The new text content for the label.
   * @param {boolean=} needsToBeShown True if we want to show the label
   *     False otherwise.
   */
  setLabelAndShow_(labelName, labelValue, needsToBeShown = true) {
    const labelElement = this.querySelector(labelName);
    labelElement.textContent = labelValue ? ' ' + labelValue : '';
    if (needsToBeShown) {
      labelElement.parentElement.hidden = false;
    }
  },
  /**
   * Populate the box with the given cloud policy status.
   * @param {string} scope The policy scope, either "device", "machine",
   *     "user", or "updater".
   * @param {Object} status Dictionary with information about the status.
   */
  initialize(scope, status) {
    const notSpecifiedString = loadTimeData.getString('notSpecified');

    // Set appropriate box legend based on status key
    this.querySelector('.legend').textContent =
        loadTimeData.getString(status.boxLegendKey);

    if (scope === 'device') {
      // Populate the device naming information.
      // Populate the asset identifier.
      this.setLabelAndShow_('.asset-id', status.assetId || notSpecifiedString);

      // Populate the device location.
      this.setLabelAndShow_('.location', status.location || notSpecifiedString);

      // Populate the directory API ID.
      this.setLabelAndShow_(
          '.directory-api-id', status.directoryApiId || notSpecifiedString);
      this.setLabelAndShow_('.client-id', status.clientId);
      // For off-hours policy, indicate if it's active or not.
      if (status.isOffHoursActive != null) {
        this.setLabelAndShow_(
            '.is-offhours-active',
            loadTimeData.getString(
                status.isOffHoursActive ? 'offHoursActive' :
                                          'offHoursNotActive'));
      }
    } else if (scope === 'machine') {
      this.setLabelAndShow_('.machine-enrollment-device-id', status.deviceId);
      this.setLabelAndShow_(
          '.machine-enrollment-token', status.enrollmentToken);
      if (status.machine) {
        this.setLabelAndShow_('.machine-enrollment-name', status.machine);
      }
      this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
    } else if (scope === 'updater') {
      if (status.version) {
        this.setLabelAndShow_('.version', status.version);
      }
      if (status.domain) {
        this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
      }
    } else {
      // Populate the topmost item with the username.
      this.setLabelAndShow_('.username', status.username);
      // Populate the user gaia id.
      this.setLabelAndShow_('.gaia-id', status.gaiaId || notSpecifiedString);
      this.setLabelAndShow_('.client-id', status.clientId);
      if (status.isAffiliated != null) {
        this.setLabelAndShow_(
            '.is-affiliated',
            loadTimeData.getString(
                status.isAffiliated ? 'isAffiliatedYes' : 'isAffiliatedNo'));
      }
    }

    if (status.enterpriseDomainManager) {
      this.setLabelAndShow_('.managed-by', status.enterpriseDomainManager);
    }

    if (status.timeSinceLastRefresh) {
      this.setLabelAndShow_(
          '.time-since-last-refresh', status.timeSinceLastRefresh);
    }

    if (scope !== 'updater') {
      this.setLabelAndShow_('.refresh-interval', status.refreshInterval);
      this.setLabelAndShow_('.status', status.status);
      this.setLabelAndShow_(
          '.policy-push',
          loadTimeData.getString(
              status.policiesPushAvailable ? 'policiesPushOn' :
                                             'policiesPushOff'));
    }

    if (status.lastCloudReportSentTimestamp) {
      this.setLabelAndShow_(
          '.last-cloud-report-sent-timestamp',
          status.lastCloudReportSentTimestamp + ' (' +
              status.timeSinceLastCloudReportSent + ')');
    }
  },
};

/**
 * A single policy conflict's entry in the policy table.
 * @constructor
 * @extends {HTMLDivElement}
 */
const PolicyConflict = crUiDefine(function() {
  const node = $('policy-conflict-template').cloneNode(true);
  node.removeAttribute('id');
  return node;
});

PolicyConflict.prototype = {
  // Set up the prototype chain.
  __proto__: HTMLDivElement.prototype,

  decorate() {},

  /**
   * @param {Conflict} conflict
   * @param {string} row_label
   */
  initialize(conflict, row_label) {
    this.querySelector('.scope').textContent = loadTimeData.getString(
        conflict.scope === 'user' ? 'scopeUser' : 'scopeDevice');
    this.querySelector('.level').textContent = loadTimeData.getString(
        conflict.level === 'recommended' ? 'levelRecommended' :
                                           'levelMandatory');
    this.querySelector('.source').textContent =
        loadTimeData.getString(conflict.source);
    this.querySelector('.value.row .value').textContent = conflict.value;
    this.querySelector('.name').textContent = loadTimeData.getString(row_label);
  }
};

/**
 * A single policy's row entry in the policy table.
 * @constructor
 * @extends {HTMLDivElement}
 */
const PolicyRow = crUiDefine(function() {
  const node = $('policy-template').cloneNode(true);
  node.removeAttribute('id');
  return node;
});

PolicyRow.prototype = {
  // Set up the prototype chain.
  __proto__: HTMLDivElement.prototype,

  /**
   * Initialization function for the cr.ui framework.
   */
  decorate() {
    const toggle = this.querySelector('.policy.row .toggle');
    toggle.addEventListener('click', this.toggleExpanded_.bind(this));

    const copy = this.querySelector('.copy-value');
    copy.addEventListener('click', this.copyValue_.bind(this));
  },

  /** @param {Policy} policy */
  initialize(policy) {
    /** @type {Policy} */
    this.policy = policy;

    /** @private {boolean} */
    this.unset_ = policy.value === undefined;

    /** @private {boolean} */
    this.hasErrors_ = !!policy.error;

    /** @private {boolean} */
    this.hasWarnings_ = !!policy.warning;

    /** @private {boolean} */
    this.hasInfos_ = !!policy.info;

    /** @private {boolean} */
    this.hasConflicts_ = !!policy.conflicts;

    /** @private {boolean} */
    this.hasSuperseded_ = !!policy.superseded;

    /** @private {boolean} */
    this.isMergedValue_ = !!policy.allSourcesMerged;

    /** @private {boolean} */
    this.deprecated_ = !!policy.deprecated;

    /** @private {boolean} */
    this.future_ = !!policy.future;

    // Populate the name column.
    const nameDisplay = this.querySelector('.name .link span');
    nameDisplay.textContent = policy.name;
    if (policy.link) {
      const link = this.querySelector('.name .link');
      link.href = policy.link;
      link.title = loadTimeData.getStringF('policyLearnMore', policy.name);
    } else {
      this.classList.add('no-help-link');
    }

    // Populate the remaining columns with policy scope, level and value if a
    // value has been set. Otherwise, leave them blank.
    if (!this.unset_) {
      const scopeDisplay = this.querySelector('.scope');
      scopeDisplay.textContent = loadTimeData.getString(
          policy.scope === 'user' ? 'scopeUser' : 'scopeDevice');

      const levelDisplay = this.querySelector('.level');
      levelDisplay.textContent = loadTimeData.getString(
          policy.level === 'recommended' ? 'levelRecommended' :
                                           'levelMandatory');

      const sourceDisplay = this.querySelector('.source');
      sourceDisplay.textContent = loadTimeData.getString(policy.source);
      // Reduces load on the DOM for long values;
      const truncatedValue =
          (policy.value && policy.value.toString().length > 256) ?
          `${policy.value.toString().substr(0, 256)}\u2026` :
          policy.value;

      const valueDisplay = this.querySelector('.value');
      valueDisplay.textContent = truncatedValue;

      const copyLink = this.querySelector('.copy .link');
      copyLink.title = loadTimeData.getStringF('policyCopyValue', policy.name);

      const valueRowContentDisplay = this.querySelector('.value.row .value');
      valueRowContentDisplay.textContent = policy.value;

      const errorRowContentDisplay = this.querySelector('.errors.row .value');
      errorRowContentDisplay.textContent = policy.error;
      const warningRowContentDisplay =
          this.querySelector('.warnings.row .value');
      warningRowContentDisplay.textContent = policy.warning;
      const infoRowContentDisplay = this.querySelector('.infos.row .value');
      infoRowContentDisplay.textContent = policy.info;

      const messagesDisplay = this.querySelector('.messages');
      const errorsNotice =
          this.hasErrors_ ? loadTimeData.getString('error') : '';
      const deprecationNotice =
          this.deprecated_ ? loadTimeData.getString('deprecated') : '';
      const futureNotice = this.future_ ? loadTimeData.getString('future') : '';
      const warningsNotice =
          this.hasWarnings_ ? loadTimeData.getString('warning') : '';
      const conflictsNotice = this.hasConflicts_ && !this.isMergedValue_ ?
          loadTimeData.getString('conflict') :
          '';
      const ignoredNotice =
          this.policy.ignored ? loadTimeData.getString('ignored') : '';
      let notice =
          [
            errorsNotice, deprecationNotice, futureNotice, warningsNotice,
            ignoredNotice, conflictsNotice
          ].filter(x => !!x)
              .join(', ') ||
          loadTimeData.getString('ok');
      const supersededNotice = this.hasSuperseded_ && !this.isMergedValue_ ?
          loadTimeData.getString('superseding') :
          '';
      if (supersededNotice) {
        // Include superseded notice regardless of other notices
        notice += `, ${supersededNotice}`;
      }
      messagesDisplay.textContent = notice;

      if (policy.conflicts) {
        policy.conflicts.forEach(conflict => {
          const row = new PolicyConflict;
          row.initialize(conflict, 'conflictValue');
          this.appendChild(row);
        });
      }
      if (policy.superseded) {
        policy.superseded.forEach(superseded => {
          const row = new PolicyConflict;
          row.initialize(superseded, 'supersededValue');
          this.appendChild(row);
        });
      }
    } else {
      const messagesDisplay = this.querySelector('.messages');
      messagesDisplay.textContent = loadTimeData.getString('unset');
    }
  },

  /**
   * Copies the policy's value to the clipboard.
   * @private
   */
  copyValue_() {
    const policyValueDisplay = this.querySelector('.value.row .value');

    // Select the text that will be copied.
    const selection = window.getSelection();
    const range = window.document.createRange();
    range.selectNodeContents(policyValueDisplay);
    selection.removeAllRanges();
    selection.addRange(range);

    // Copy the policy value to the clipboard.
    navigator.clipboard.writeText(policyValueDisplay.innerText).catch(error => {
      console.error('Unable to copy policy value to clipboard:', error);
    });
  },

  /**
   * Toggle the visibility of an additional row containing the complete text.
   * @private
   */
  toggleExpanded_() {
    const warningRowDisplay = this.querySelector('.warnings.row');
    const errorRowDisplay = this.querySelector('.errors.row');
    const infoRowDisplay = this.querySelector('.infos.row');
    const valueRowDisplay = this.querySelector('.value.row');
    valueRowDisplay.hidden = !valueRowDisplay.hidden;
    if (valueRowDisplay.hidden) {
      this.classList.remove('expanded');
    } else {
      this.classList.add('expanded');
    }

    this.querySelector('.show-more').hidden = !valueRowDisplay.hidden;
    this.querySelector('.show-less').hidden = valueRowDisplay.hidden;
    if (this.hasWarnings_) {
      warningRowDisplay.hidden = !warningRowDisplay.hidden;
    }
    if (this.hasErrors_) {
      errorRowDisplay.hidden = !errorRowDisplay.hidden;
    }
    if (this.hasInfos_) {
      infoRowDisplay.hidden = !infoRowDisplay.hidden;
    }
    this.querySelectorAll('.policy-conflict-data')
        .forEach(row => row.hidden = !row.hidden);
    this.querySelectorAll('.policy-superseded-data')
        .forEach(row => row.hidden = !row.hidden);
  },
};

/**
 * A table of policies and their values.
 * @constructor
 * @extends {HTMLDivElement}
 */
const PolicyTable = crUiDefine(function() {
  const node = $('policy-table-template').cloneNode(true);
  node.removeAttribute('id');
  return node;
});


PolicyTable.prototype = {
  // Set up the prototype chain.
  __proto__: HTMLDivElement.prototype,

  /**
   * Initialization function for the cr.ui framework.
   */
  decorate() {
    this.policies_ = {};
    this.filterPattern_ = '';
  },

  /** @param {PolicyTableModel} dataModel */
  update(dataModel) {
    // Clear policies
    const mainContent = this.querySelector('.main');
    const policies = this.querySelectorAll('.policy-data');
    this.querySelector('.header').textContent = dataModel.name;
    this.querySelector('.id').textContent = dataModel.id;
    this.querySelector('.id').hidden = !dataModel.id;
    policies.forEach(row => mainContent.removeChild(row));

    dataModel.policies
        .sort((a, b) => {
          if ((a.value !== undefined && b.value !== undefined) ||
              a.value === b.value) {
            if (a.link !== undefined && b.link !== undefined) {
              // Sorting the policies in ascending alpha order.
              return a.name > b.name ? 1 : -1;
            }

            // Sorting so unknown policies are last.
            return a.link !== undefined ? -1 : 1;
          }

          // Sorting so unset values are last.
          return a.value !== undefined ? -1 : 1;
        })
        .forEach(policy => {
          const policyRow = new PolicyRow;
          policyRow.initialize(policy);
          mainContent.appendChild(policyRow);
        });
    this.filter();
  },

  /**
   * Set the filter pattern. Only policies whose name contains |pattern| are
   * shown in the policy table. The filter is case insensitive. It can be
   * disabled by setting |pattern| to an empty string.
   * @param {string} pattern The filter pattern.
   */
  setFilterPattern(pattern) {
    this.filterPattern_ = pattern.toLowerCase();
    this.filter();
  },

  /**
   * Filter policies. Only policies whose name contains the filter pattern are
   * shown in the table. Furthermore, policies whose value is not currently
   * set are only shown if the corresponding checkbox is checked.
   */
  filter() {
    const showUnset = $('show-unset').checked;
    const policies = this.querySelectorAll('.policy-data');
    for (let i = 0; i < policies.length; i++) {
      const policyDisplay = policies[i];
      policyDisplay.hidden =
          policyDisplay.policy.value === undefined && !showUnset ||
          policyDisplay.policy.name.toLowerCase().indexOf(
              this.filterPattern_) === -1;
    }
    this.querySelector('.no-policy').hidden =
        !!this.querySelector('.policy-data:not([hidden])');
  },
};

/**
 * A singleton object that handles communication between browser and WebUI.
 */
export class Page {
  constructor() {
    /** @type {?Element} */
    this.mainSection = null;

    /** @type {{[id: string]: PolicyTable}} */
    this.policyTables = {};
  }

  /**
   * Main initialization function. Called by the browser on page load.
   */
  initialize() {
    FocusOutlineManager.forDocument(document);

    this.mainSection = $('main-section');

    // Place the initial focus on the filter input field.
    $('filter').focus();

    $('filter').onsearch = () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable].setFilterPattern($('filter').value);
      }
    };
    $('reload-policies').onclick = () => {
      $('reload-policies').disabled = true;
      $('screen-reader-message').textContent =
          loadTimeData.getString('loadingPolicies');
      chrome.send('reloadPolicies');
    };

    const exportButton = $('export-policies');
    const hideExportButton = loadTimeData.valueExists('hideExportButton') &&
        loadTimeData.getBoolean('hideExportButton');
    if (hideExportButton) {
      exportButton.style.display = 'none';
    } else {
      exportButton.onclick = () => {
        chrome.send('exportPoliciesJSON');
      };
    }

    $('copy-policies').onclick = () => {
      chrome.send('copyPoliciesJSON');
    };

    $('show-unset').onchange = () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable].filter();
      }
    };

    chrome.send('listenPoliciesUpdates');
    addWebUIListener('status-updated', status => this.setStatus(status));
    addWebUIListener(
        'policies-updated',
        (names, values) => this.onPoliciesReceived_(names, values));
    addWebUIListener('download-json', json => this.downloadJson(json));
  }

  /**
   * @param {PolicyNamesResponse} policyNames
   * @param {PolicyValuesResponse} policyValues
   * @private
   */
  onPoliciesReceived_(policyNames, policyValues) {
    /** @type {Array<!PolicyTableModel>} */
    const policyGroups = policyValues.map(value => {
      const knownPolicyNames =
          policyNames[value.id] ? policyNames[value.id].policyNames : [];
      const knownPolicyNamesSet = new Set(knownPolicyNames);
      const receivedPolicyNames = Object.keys(value.policies);
      const allPolicyNames =
          Array.from(new Set([...knownPolicyNames, ...receivedPolicyNames]));
      const policies = allPolicyNames.map(
          name => Object.assign(
              {
                name,
                link: knownPolicyNames === policyNames.chrome.policyNames &&
                        knownPolicyNamesSet.has(name) ?
                    `https://chromeenterprise.google/policies/?policy=${name}` :
                    undefined,
              },
              value.policies[name]));

      return {
        name: value.forSigninScreen ?
            `${value.name} [${loadTimeData.getString('signinProfile')}]` :
            value.name,
        id: value.isExtension ? value.id : null,
        policies
      };
    });

    policyGroups.forEach(group => this.createOrUpdatePolicyTable(group));

    this.reloadPoliciesDone();
  }

  /**
   * Triggers the download of the policies as a JSON file.
   * @param {String} json The policies as a JSON string.
   */
  downloadJson(json) {
    const blob = new Blob([json], {type: 'application/json'});
    const blobUrl = URL.createObjectURL(blob);

    const link = document.createElement('a');
    link.href = blobUrl;
    link.download = 'policies.json';

    document.body.appendChild(link);

    link.dispatchEvent(new MouseEvent(
        'click', {bubbles: true, cancelable: true, view: window}));

    document.body.removeChild(link);
  }

  /** @param {PolicyTableModel} dataModel */
  createOrUpdatePolicyTable(dataModel) {
    const id = `${dataModel.name}-${dataModel.id}`;
    if (!this.policyTables[id]) {
      this.policyTables[id] = new PolicyTable;
      this.mainSection.appendChild(this.policyTables[id]);
    }
    this.policyTables[id].update(dataModel);
  }

  /**
   * Update the status section of the page to show the current cloud policy
   * status.
   * @param {Object} status Dictionary containing the current policy status.
   */
  setStatus(status) {
    // Remove any existing status boxes.
    const container = $('status-box-container');
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }
    // Hide the status section.
    const section = $('status-section');
    section.hidden = true;

    // Add a status box for each scope that has a cloud policy status.
    for (const scope in status) {
      const box = new StatusBox;
      box.initialize(scope, status[scope]);
      container.appendChild(box);
      // Show the status section.
      section.hidden = false;
    }
  }

  /**
   * Re-enable the reload policies button when the previous request to reload
   * policies values has completed.
   */
  reloadPoliciesDone() {
    $('reload-policies').disabled = false;
    $('screen-reader-message').textContent =
        loadTimeData.getString('loadPoliciesDone');
  }
}

// Make Page a singleton.
addSingletonGetter(Page);
