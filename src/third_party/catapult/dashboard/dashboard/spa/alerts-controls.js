/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import './cp-checkbox.js';
import './cp-input.js';
import './cp-switch.js';
import './raised-button.js';
import './recommended-options.js';
import '@polymer/polymer/lib/elements/dom-if.js';
import '@polymer/polymer/lib/elements/dom-repeat.js';
import * as PolymerAsync from '@polymer/polymer/lib/utils/async.js';
import AlertsTable from './alerts-table.js';
import MenuInput from './menu-input.js';
import OptionGroup from './option-group.js';
import ReportNamesRequest from './report-names-request.js';
import SheriffsRequest from './sheriffs-request.js';
import {ElementBase, STORE} from './element-base.js';
import {TOGGLE, UPDATE} from './simple-redux.js';
import {crbug} from './utils.js';
import {get} from '@polymer/polymer/lib/utils/path.js';
import {html} from '@polymer/polymer/polymer-element.js';

export default class AlertsControls extends ElementBase {
  static get is() { return 'alerts-controls'; }

  static get properties() {
    return {
      recentPerformanceBugs: Array,
      areAlertGroupsPlaceholders: Boolean,
      userEmail: String,

      statePath: String,
      errors: Array,
      bug: Object,
      hasTriagedNew: Boolean,
      hasTriagedExisting: Boolean,
      hasIgnored: Boolean,
      ignoredCount: Number,
      maxRevision: String,
      minRevision: String,
      recentlyModifiedBugs: Array,
      report: Object,
      sheriff: Object,
      showEmptyInputs: Boolean,
      showingTriaged: Boolean,
      showingImprovements: Boolean,
      showingRecentlyModifiedBugs: Boolean,
      triagedBugId: Number,
      alertGroups: Array,
    };
  }

  static buildState(options = {}) {
    return {
      errors: [],
      bug: MenuInput.buildState({
        label: 'Bug',
        options: [],
        selectedOptions: options.bugs,
      }),
      hasTriagedNew: false,
      hasTriagedExisting: false,
      hasIgnored: false,
      ignoredCount: 0,
      maxRevision: options.maxRevision || '',
      minRevision: options.minRevision || '',
      recentlyModifiedBugs: [],
      report: MenuInput.buildState({
        label: 'Report',
        options: [],
        selectedOptions: options.reports || [],
      }),
      sheriff: MenuInput.buildState({
        label: 'Sheriff',
        options: [],
        selectedOptions: options.sheriffs || [],
      }),
      showEmptyInputs: options.showEmptyInputs || false,
      showingTriaged: options.showingTriaged || false,
      showingImprovements: options.showingImprovements || false,
      showingRecentlyModifiedBugs: false,
      triagedBugId: 0,
      alertGroups: options.alertGroups ||
        AlertsTable.placeholderAlertGroups(),
    };
  }

  static get template() {
    return html`
      <style>
        :host {
          align-items: center;
          display: flex;
          margin-bottom: 8px;
        }

        #sheriff-container,
        #bug-container,
        #report-container {
          margin-right: 8px;
        }

        cp-input {
          margin-right: 8px;
          margin-top: 12px;
        }

        #report-container {
          display: flex;
        }

        #triaged {
          margin-left: 8px;
          margin-right: 8px;
        }

        #spacer {
          flex-grow: 1;
        }

        #recent-bugs-container {
          position: relative;
        }

        .bug_notification {
          background-color: var(--background-color, white);
          box-shadow: var(--elevation-2);
          overflow: hidden;
          padding: 8px;
          position: absolute;
          right: 0;
          white-space: nowrap;
          z-index: var(--layer-menu, 100);
        }

        #recent-bugs-table {
          margin: 0;
          padding: 0;
        }

        #filter[enabled] {
          background-color: var(--primary-color-dark, blue);
          border-radius: 50%;
          color: var(--background-color, white);
          padding: 4px;
        }

        iron-icon {
          cursor: pointer;
          flex-shrink: 0;
          height: var(--icon-size, 1em);
          width: var(--icon-size, 1em);
        }

        #close {
          align-self: flex-start;
        }

        #edit, #documentation {
          color: var(--primary-color-dark, blue);
          padding: 8px;
        }
      </style>

      <iron-collapse
          horizontal
          id="sheriff-container"
          opened="[[showMenuInput_(showEmptyInputs, sheriff, bug, report,
                                    minRevision, maxRevision)]]">
        <menu-input
            id="sheriff"
            state-path="[[statePath]].sheriff"
            on-clear="onSheriffClear_"
            on-option-select="onSheriffSelect_">
          <recommended-options slot="top" state-path="[[statePath]].sheriff">
          </recommended-options>
        </menu-input>
      </iron-collapse>

      <iron-collapse
          horizontal
          id="bug-container"
          opened="[[showMenuInput_(showEmptyInputs, bug, sheriff, report,
                                    minRevision, maxRevision)]]">
        <menu-input
            id="bug"
            state-path="[[statePath]].bug"
            on-clear="onBugClear_"
            on-input-keyup="onBugKeyup_"
            on-option-select="onBugSelect_">
          <recommended-options slot="top" state-path="[[statePath]].bug">
          </recommended-options>
        </menu-input>
      </iron-collapse>

      <iron-collapse
          horizontal
          id="report-container"
          opened="[[showMenuInput_(showEmptyInputs, report, sheriff, bug,
                                    minRevision, maxRevision)]]">
        <menu-input
            id="report"
            state-path="[[statePath]].report"
            on-clear="onReportClear_"
            on-option-select="onReportSelect_">
          <recommended-options slot="top" state-path="[[statePath]].report">
          </recommended-options>
        </menu-input>
      </iron-collapse>

      <iron-collapse
          horizontal
          id="min-container"
          opened="[[showInput_(showEmptyInputs, minRevision, maxRevision,
                                sheriff, bug, report)]]">
        <cp-input
            id="min-revision"
            value="[[minRevision]]"
            label="Min Revision"
            on-keyup="onMinRevisionKeyup_">
        </cp-input>
      </iron-collapse>

      <iron-collapse
          horizontal
          id="max-container"
          opened="[[showInput_(showEmptyInputs, minRevision, maxRevision,
                                sheriff, bug, report)]]">
        <cp-input
            id="max-revision"
            value="[[maxRevision]]"
            label="Max Revision"
            on-keyup="onMaxRevisionKeyup_">
        </cp-input>
      </iron-collapse>

      <iron-icon
          id="filter"
          icon="cp:filter"
          enabled$="[[showEmptyInputs]]"
          on-click="onFilter_">
      </iron-icon>

      <iron-collapse
          horizontal
          opened="[[isEmpty_(bug.selectedOptions)]]">
        <cp-switch
            id="improvements"
            disabled="[[!isEmpty_(bug.selectedOptions)]]"
            title="[[getImprovementsTooltip_(showingImprovements)]]"
            checked$="[[showingImprovements]]"
            on-change="onToggleImprovements_">
          <template is="dom-if" if="[[showingImprovements]]">
            Regressions and Improvements
          </template>
          <template is="dom-if" if="[[!showingImprovements]]">
            Regressions Only
          </template>
        </cp-switch>

        <cp-switch
            id="triaged"
            disabled="[[!isEmpty_(bug.selectedOptions)]]"
            title="[[getTriagedTooltip_(showingTriaged)]]"
            checked$="[[showingTriaged]]"
            on-change="onToggleTriaged_">
          <template is="dom-if" if="[[showingTriaged]]">
            New and Triaged
          </template>
          <template is="dom-if" if="[[!showingTriaged]]">
            New Only
          </template>
        </cp-switch>
      </iron-collapse>

      <span id=spacer></span>

      <span id="recent-bugs-container">
        <raised-button
            id="recent-bugs"
            disabled$="[[isEmpty_(recentlyModifiedBugs)]]"
            on-click="onClickRecentlyModifiedBugs_">
          Recent Bugs
        </raised-button>

        <iron-collapse
            class="bug_notification"
            opened="[[hasTriagedNew]]">
          Created
          <a href="[[crbug_(triagedBugId)]]" target="_blank">
            [[triagedBugId]]
          </a>
        </iron-collapse>

        <iron-collapse
            class="bug_notification"
            opened="[[hasTriagedExisting]]">
          Updated
          <a href="[[crbug_(triagedBugId)]]" target="_blank">
            [[triagedBugId]]
          </a>
        </iron-collapse>

        <iron-collapse
            class="bug_notification"
            opened="[[hasIgnored]]">
          Ignored [[ignoredCount]] alert[[plural_(ignoredCount)]]
        </iron-collapse>

        <iron-collapse
            class="bug_notification"
            opened="[[showingRecentlyModifiedBugs]]"
            on-blur="onRecentlyModifiedBugsBlur_">
          <table id="recent-bugs-table">
            <thead>
              <tr>
                <th>Bug #</th>
                <th>Summary</th>
              </tr>
            </thead>
            <template is="dom-repeat" items="[[recentlyModifiedBugs]]"
                                      as="bug">
              <tr>
                <td>
                  <a href="[[crbug_(bug.id)]]" target="_blank">
                    [[bug.id]]
                  </a>
                </td>
                <td>[[bug.summary]]</td>
              </tr>
            </template>
          </table>
        </iron-collapse>
      </span>

      <iron-icon id="close" icon="cp:close" on-click="onClose_">
      </iron-icon>
    `;
  }

  connectedCallback() {
    super.connectedCallback();
    AlertsControls.connected(this.statePath);
    this.dispatchSources_();
  }

  static async connected(statePath) {
    AlertsControls.loadReportNames(statePath);
    AlertsControls.loadSheriffs(statePath);
    STORE.dispatch({
      type: AlertsControls.reducers.receiveRecentlyModifiedBugs.name,
      statePath,
      json: localStorage.getItem('recentlyModifiedBugs'),
    });
  }

  stateChanged(rootState) {
    if (!this.statePath) return;
    const oldUserEmail = this.userEmail;
    this.set('userEmail', rootState.userEmail);
    const oldRecentPerformanceBugs = this.recentPerformanceBugs;
    this.set('recentPerformanceBugs', rootState.recentPerformanceBugs);
    this.setProperties(get(rootState, this.statePath));
    this.set('areAlertGroupsPlaceholders', this.alertGroups ===
      AlertsTable.placeholderAlertGroups());
    if (this.hasTriagedNew || this.hasTriagedExisting || this.hasIgnored) {
      this.$['recent-bugs'].scrollIntoView(true);
    }
    if (this.recentPerformanceBugs !== oldRecentPerformanceBugs) {
      STORE.dispatch({
        type: AlertsControls.reducers.receiveRecentPerformanceBugs.name,
        statePath: this.statePath,
      });
    }
    if (this.userEmail !== oldUserEmail) {
      AlertsControls.loadReportNames(this.statePath);
      AlertsControls.loadSheriffs(this.statePath);
    }
  }

  static async loadReportNames(statePath) {
    let infos;
    let error;
    try {
      infos = await new ReportNamesRequest().response;
    } catch (err) {
      error = err;
    }
    STORE.dispatch({
      type: AlertsControls.reducers.receiveReportNames.name,
      statePath, infos, error,
    });
  }

  static async loadSheriffs(statePath) {
    let sheriffs;
    let error;
    try {
      sheriffs = await new SheriffsRequest().response;
    } catch (err) {
      error = err;
    }
    STORE.dispatch({
      type: AlertsControls.reducers.receiveSheriffs.name,
      statePath, sheriffs, error,
    });

    const state = get(STORE.getState(), statePath);
    if (state.sheriff.selectedOptions.length === 0) {
      MenuInput.focus(statePath + '.sheriff');
    }
  }

  async onFilter_() {
    await STORE.dispatch(TOGGLE(this.statePath + '.showEmptyInputs'));
  }

  showMenuInput_(showEmptyInputs, thisInput, thatInput, otherInput,
      minRevision, maxRevision) {
    if (showEmptyInputs) return true;
    if (thisInput && thisInput.selectedOptions &&
        thisInput.selectedOptions.length) {
      return true;
    }
    if (!thatInput || !otherInput) return true;
    if (thatInput.selectedOptions.length === 0 &&
        otherInput.selectedOptions.length === 0 &&
        !minRevision && !maxRevision) {
      // Show all inputs when they're all empty.
      return true;
    }
    return false;
  }

  showInput_(showEmptyInputs, thisInput, thatInput, menuA, menuB, menuC) {
    if (showEmptyInputs) return true;
    if (thisInput) return true;
    if (!menuA || !menuB || !menuC) return true;
    if (menuA.selectedOptions.length === 0 &&
        menuB.selectedOptions.length === 0 &&
        menuC.selectedOptions.length === 0 &&
        !thatInput) {
      // Show all inputs when they're all empty.
      return true;
    }
    return false;
  }

  crbug_(bugId) {
    return crbug(bugId);
  }

  async dispatchSources_() {
    if (!this.sheriff || !this.bug || !this.report) return;
    const sources = await AlertsControls.compileSources(
        this.sheriff.selectedOptions,
        this.bug.selectedOptions,
        this.report.selectedOptions,
        this.minRevision, this.maxRevision,
        this.showingImprovements, this.showingTriaged);
    this.dispatchEvent(new CustomEvent('sources', {
      bubbles: true,
      composed: true,
      detail: {sources},
    }));
  }

  async onSheriffClear_(event) {
    MenuInput.focus(this.statePath + '.sheriff');
    this.dispatchSources_();
  }

  async onSheriffSelect_(event) {
    this.dispatchSources_();
  }

  async onBugClear_(event) {
    MenuInput.focus(this.statePath + '.bug');
    this.dispatchSources_();
  }

  async onBugKeyup_(event) {
    STORE.dispatch({
      type: AlertsControls.reducers.onBugKeyup.name,
      statePath: this.statePath,
      bugId: event.detail.value,
    });
  }

  async onBugSelect_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {
      showingTriaged: true,
      showingImprovements: true,
    }));
    this.dispatchSources_();
  }

  async onReportClear_(event) {
    MenuInput.focus(this.statePath + '.report');
    this.dispatchSources_();
  }

  async onReportSelect_(event) {
    this.dispatchSources_();
  }

  async onMinRevisionKeyup_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {
      minRevision: event.target.value,
    }));
    this.debounce('dispatchSources', () => {
      this.dispatchSources_();
    }, PolymerAsync.timeOut.after(AlertsControls.TYPING_DEBOUNCE_MS));
  }

  async onMaxRevisionKeyup_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {
      maxRevision: event.target.value,
    }));
    this.debounce('dispatchSources', () => {
      this.dispatchSources_();
    }, PolymerAsync.timeOut.after(AlertsControls.TYPING_DEBOUNCE_MS));
  }

  getImprovementsTooltip_(showingImprovements) {
    if (showingImprovements) {
      return 'Now showing both regressions and improvements. ' +
        'Click to toggle to show only regressions.';
    }
    return 'Now showing regressions but not improvements. ' +
      'Click to toggle to show both regressions and improvements.';
  }

  getTriagedTooltip_(showingTriaged) {
    if (showingTriaged) {
      return 'Now showing both triaged and untriaged alerts. ' +
        'Click to toggle to show only untriaged alerts.';
    }
    return 'Now showing only untriaged alerts. ' +
      'Click to toggle to show both triaged and untriaged alerts.';
  }

  async onToggleImprovements_(event) {
    STORE.dispatch(TOGGLE(this.statePath + '.showingImprovements'));
    this.dispatchSources_();
  }

  async onToggleTriaged_(event) {
    STORE.dispatch(TOGGLE(this.statePath + '.showingTriaged'));
  }

  async onClickRecentlyModifiedBugs_(event) {
    STORE.dispatch(TOGGLE(this.statePath + '.showingRecentlyModifiedBugs'));
  }

  async onRecentlyModifiedBugsBlur_(event) {
    STORE.dispatch(TOGGLE(this.statePath + '.showingRecentlyModifiedBugs'));
  }

  async onClose_(event) {
    this.dispatchEvent(new CustomEvent('close-section', {
      bubbles: true,
      composed: true,
      detail: {sectionId: this.sectionId},
    }));
  }
}

AlertsControls.TYPING_DEBOUNCE_MS = 300;

AlertsControls.reducers = {
  receiveReportNames: (state, {infos, error}, rootState) => {
    if (error) {
      const errors = [...new Set([error.message, ...state.errors])];
      return {...state, errors};
    }

    const reportNames = infos.map(t => t.name);
    const report = {
      ...state.report,
      options: OptionGroup.groupValues(reportNames),
      label: `Reports (${reportNames.length})`,
    };
    return {...state, report};
  },

  receiveSheriffs: (state, {sheriffs, error}, rootState) => {
    if (error) {
      const errors = [...new Set([error.message, ...state.errors])];
      return {...state, errors};
    }

    const sheriff = MenuInput.buildState({
      label: `Sheriffs (${sheriffs.length})`,
      options: sheriffs,
      selectedOptions: state.sheriff ? state.sheriff.selectedOptions : [],
    });
    return {...state, sheriff};
  },

  onBugKeyup: (state, action, rootState) => {
    const options = state.bug.options.filter(option => !option.manual);
    const bugIds = options.map(option => option.value);
    if (action.bugId.match(/^\d+$/) &&
        !bugIds.includes(action.bugId)) {
      options.unshift({
        value: action.bugId,
        label: action.bugId,
        manual: true,
      });
    }
    return {
      ...state,
      bug: {
        ...state.bug,
        options,
      },
    };
  },

  receiveRecentPerformanceBugs: (state, action, rootState) => {
    const options = rootState.recentPerformanceBugs.map(bug => {
      return {
        label: bug.id + ' ' + bug.summary,
        value: bug.id,
      };
    });
    return {...state, bug: {...state.bug, options}};
  },

  receiveRecentlyModifiedBugs: (state, {json}, rootState) => {
    if (!json) return state;
    return {...state, recentlyModifiedBugs: JSON.parse(json)};
  },
};

function maybeInt(x) {
  x = parseInt(x);
  return isNaN(x) ? undefined : x;
}

// Maximum number of alerts to count (not return) when fetching alerts for a
// sheriff rotation. When non-zero, /api/alerts returns the number of alerts
// that would match the datastore query (up to COUNT_LIMIT) as response.count.
// The maximum number of alerts to return from /api/alerts is given by `limit`
// in AlertsRequest.
// See count_limit in Anomaly.QueryAsync() and totalCount in AlertsSection.
const COUNT_LIMIT = 5000;

AlertsControls.compileSources = async(
  sheriffs, bugs, reports, minRevision, maxRevision, improvements,
  showingTriaged) => {
  // Returns a list of AlertsRequest bodies. See ../api/alerts.py for
  // request body parameters.
  const revisions = {};
  minRevision = maybeInt(minRevision);
  maxRevision = maybeInt(maxRevision);
  if (minRevision !== undefined) revisions.min_end_revision = minRevision;
  if (maxRevision !== undefined) revisions.max_start_revision = maxRevision;
  const sources = [];
  for (const sheriff of sheriffs) {
    const source = {sheriff, recovered: false, ...revisions};
    source.count_limit = COUNT_LIMIT;
    source.is_improvement = improvements;
    if (!showingTriaged) source.bug_id = '';
    sources.push(source);
  }
  for (const bug of bugs) {
    sources.push({bug_id: bug, ...revisions});
  }
  if (reports.length) {
    const reportTemplateInfos = await new ReportNamesRequest().response;
    for (const name of reports) {
      for (const reportId of reportTemplateInfos) {
        if (reportId.name === name) {
          sources.push({report: reportId.id, ...revisions});
          break;
        }
      }
    }
  }
  if (sources.length === 0 && (minRevision || maxRevision)) {
    sources.push(revisions);
  }
  return sources;
};

ElementBase.register(AlertsControls);
