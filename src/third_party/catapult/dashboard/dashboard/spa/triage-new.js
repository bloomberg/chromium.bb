/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import './cp-checkbox.js';
import './cp-input.js';
import './cp-textarea.js';
import './raised-button.js';
import '@polymer/polymer/lib/elements/dom-repeat.js';
import {ElementBase, STORE} from './element-base.js';
import {UPDATE} from './simple-redux.js';
import {get} from '@polymer/polymer/lib/utils/path.js';
import {html} from '@polymer/polymer/polymer-element.js';
import {isElementChildOf, setImmutable} from './utils.js';

export default class TriageNew extends ElementBase {
  static get is() { return 'triage-new'; }

  static get properties() {
    return {
      statePath: String,
      cc: String,
      components: Array,
      description: String,
      isOpen: {type: Boolean, reflectToAttribute: true},
      labels: Array,
      owner: String,
      summary: String,
    };
  }

  static buildState(options = {}) {
    return {
      cc: options.cc || '',
      components: TriageNew.collectAlertProperties(
          options.alerts, 'bugComponents'),
      description: options.description || '',
      isOpen: options.isOpen || false,
      labels: TriageNew.collectAlertProperties(
          options.alerts, 'bugLabels'),
      owner: options.owner || '',
      summary: TriageNew.summarize(options.alerts),
    };
  }

  static get template() {
    return html`
      <style>
        :host {
          background: var(--background-color, white);
          box-shadow: var(--elevation-2);
          display: none;
          flex-direction: column;
          min-width: 500px;
          outline: none;
          padding: 16px;
          position: absolute;
          right: 0;
          z-index: var(--layer-menu, 100);
        }
        :host([is-open]) {
          display: flex;
        }
        *:not(:nth-child(2)) {
          margin-top: 12px;
        }
      </style>

      <cp-input
          id="summary"
          label="Summary"
          tabindex="0"
          value="[[summary]]"
          on-change="onSummary_">
      </cp-input>

      <cp-input
          id="owner"
          label="Owner"
          tabindex="0"
          value="[[owner]]"
          on-change="onOwner_">
      </cp-input>

      <cp-input
          id="cc"
          label="CC"
          tabindex="0"
          value="[[cc]]"
          on-change="onCC_">
      </cp-input>

      <cp-textarea
          autofocus
          id="description"
          label="Description"
          tabindex="0"
          value="[[description]]"
          on-keyup="onDescription_">
      </cp-textarea>

      <template is="dom-repeat" items="[[labels]]" as="label">
        <cp-checkbox
            checked="[[label.isEnabled]]"
            tabindex="0"
            on-change="onLabel_">
          [[label.name]]
        </cp-checkbox>
      </template>

      <template is="dom-repeat" items="[[components]]" as="component">
        <cp-checkbox
            checked="[[component.isEnabled]]"
            tabindex="0"
            on-change="onComponent_">
          [[component.name]]
        </cp-checkbox>
      </template>

      <raised-button
          id="submit"
          on-click="onSubmit_"
          tabindex="0">
        Submit
      </raised-button>
    `;
  }

  stateChanged(rootState) {
    const oldIsOpen = this.isOpen;
    super.stateChanged(rootState);

    if (this.isOpen && !oldIsOpen) {
      this.$.description.focus();
    }
  }

  ready() {
    super.ready();
    this.addEventListener('blur', this.onBlur_.bind(this));
    this.addEventListener('keyup', this.onKeyup_.bind(this));
  }

  async onKeyup_(event) {
    if (event.key === 'Escape') {
      await STORE.dispatch(UPDATE(this.statePath, {isOpen: false}));
    }
  }

  async onBlur_(event) {
    if (event.relatedTarget === this ||
        isElementChildOf(event.relatedTarget, this)) {
      return;
    }
    await STORE.dispatch(UPDATE(this.statePath, {isOpen: false}));
  }

  async onSummary_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {summary: event.target.value}));
  }

  async onDescription_(event) {
    if (event.ctrlKey && (event.key === 'Enter')) {
      await this.onSubmit_(event);
      return;
    }
    await STORE.dispatch(UPDATE(this.statePath, {
      description: event.target.value,
    }));
  }

  async onLabel_(event) {
    await STORE.dispatch({
      type: TriageNew.reducers.toggleLabel.name,
      statePath: this.statePath,
      name: event.model.label.name,
    });
  }

  async onComponent_(event) {
    await STORE.dispatch({
      type: TriageNew.reducers.toggleComponent.name,
      statePath: this.statePath,
      name: event.model.component.name,
    });
  }

  async onOwner_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {owner: event.target.value}));
  }

  async onCC_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {cc: event.target.value}));
  }

  async onSubmit_(event) {
    await STORE.dispatch(UPDATE(this.statePath, {isOpen: false}));
    this.dispatchEvent(new CustomEvent('submit', {
      bubbles: true,
      composed: true,
    }));
  }
}

TriageNew.reducers = {
  toggleLabel: (state, action, rootState) => {
    for (let i = 0; i < state.labels.length; ++i) {
      if (state.labels[i].name === action.name) {
        return setImmutable(
            state, `labels.${i}.isEnabled`, e => !e);
      }
    }
    return state;
  },

  toggleComponent: (state, action, rootState) => {
    for (let i = 0; i < state.components.length; ++i) {
      if (state.components[i].name === action.name) {
        return setImmutable(
            state, `components.${i}.isEnabled`, e => !e);
      }
    }
    return state;
  },
};

TriageNew.summarize = alerts => {
  if (!alerts) return '';
  const pctDeltaRange = new tr.b.math.Range();
  const revisionRange = new tr.b.math.Range();
  let measurements = new Set();
  for (const alert of alerts) {
    if (!alert.improvement) {
      pctDeltaRange.addValue(Math.abs(100 * alert.percentDeltaValue));
    }
    // TODO handle non-numeric revisions
    revisionRange.addValue(alert.startRevision);
    revisionRange.addValue(alert.endRevision);
    measurements.add(alert.measurement);
  }
  measurements = Array.from(measurements);
  measurements.sort((x, y) => x.localeCompare(y));
  measurements = measurements.join(',');

  let pctDeltaString = pctDeltaRange.min.toLocaleString(undefined, {
    maximumFractionDigits: 1,
  }) + '%';
  if (pctDeltaRange.min !== pctDeltaRange.max) {
    pctDeltaString += '-' + pctDeltaRange.max.toLocaleString(undefined, {
      maximumFractionDigits: 1,
    }) + '%';
  }

  let revisionString = revisionRange.min;
  if (revisionRange.min !== revisionRange.max) {
    revisionString += ':' + revisionRange.max;
  }

  return (
    `${pctDeltaString} regression in ${measurements} at ${revisionString}`
  );
};

TriageNew.collectAlertProperties = (alerts, property) => {
  if (!alerts) return [];
  let labels = new Set();
  if (property === 'bugLabels') {
    labels.add('Pri-2');
    labels.add('Type-Bug-Regression');
  }
  for (const alert of alerts) {
    if (!alert[property]) continue;
    for (const label of alert[property]) {
      labels.add(label);
    }
  }
  labels = Array.from(labels);
  labels.sort((x, y) => x.localeCompare(y));
  return labels.map(name => {
    return {name, isEnabled: true};
  });
};

ElementBase.register(TriageNew);
