/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import '@chopsui/chops-button';
import '@chopsui/chops-checkbox';
import '@chopsui/chops-input';
import '@chopsui/chops-switch';
import '@chopsui/chops-textarea';
import {ElementBase, STORE} from './element-base.js';
import {TOGGLE, UPDATE} from './simple-redux.js';
import {get, set} from 'dot-prop-immutable';
import {html, css} from 'lit-element';
import {isElementChildOf} from './utils.js';

export class TriageNew extends ElementBase {
  static get is() { return 'triage-new'; }

  static get properties() {
    return {
      statePath: String,
      cc: String,
      components: Array,
      description: String,
      isOpen: {type: Boolean, reflect: true},
      labels: Array,
      owner: String,
      summary: String,
      startBisect: Boolean,
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
      startBisect: options.startBisect !== false,
    };
  }

  static get styles() {
    return css`
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
      :host([isopen]) {
        display: flex;
      }
      *:not(:nth-child(1)) {
        margin-top: 12px;
      }
    `;
  }

  render() {
    return html`
      <chops-input
          id="summary"
          label="Summary"
          tabindex="0"
          value="${this.summary}"
          @change="${this.onSummary_}">
      </chops-input>

      <chops-input
          id="owner"
          label="Owner"
          tabindex="0"
          value="${this.owner}"
          @change="${this.onOwner_}">
      </chops-input>

      <chops-input
          id="cc"
          label="CC"
          tabindex="0"
          value="${this.cc}"
          @change="${this.onCC_}">
      </chops-input>

      <chops-textarea
          autofocus
          id="description"
          label="Description"
          tabindex="0"
          value="${this.description}"
          @keyup="${this.onDescription_}">
      </chops-textarea>

      ${(this.labels || []).map(label => html`
        <chops-checkbox
            .checked="${label.isEnabled}"
            tabindex="0"
            @change="${event => this.onLabel_(label.name)}">
          ${label.name}
        </chops-checkbox>
      `)}

      ${(this.components || []).map(component => html`
        <chops-checkbox
            .checked="${component.isEnabled}"
            tabindex="0"
            @change="${event => this.onComponent_(component.name)}">
          ${component.name}
        </chops-checkbox>
      `)}

      <chops-switch
          id="pinpoint"
          .checked="${this.startBisect}"
          @click="${this.onTogglePinpoint_}"
          tabindex="0">
        Start Pinpoint Bisect
      </chops-switch>

      <chops-button
          id="submit"
          @click="${this.onSubmit_}"
          tabindex="0">
        Submit
      </chops-button>
    `;
  }

  stateChanged(rootState) {
    const oldIsOpen = this.isOpen;
    super.stateChanged(rootState);

    if (this.isOpen && !oldIsOpen && this.descriptionInput) {
      this.descriptionInput.focus();
    }
  }

  get descriptionInput() {
    return this.shadowRoot.querySelector('#description');
  }

  constructor() {
    super();
    this.addEventListener('blur', this.onBlur_.bind(this));
    this.addEventListener('keyup', this.onKeyup_.bind(this));
  }

  onTogglePinpoint_(event) {
    STORE.dispatch(TOGGLE(this.statePath + '.startBisect'));
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

  async onLabel_(name) {
    await STORE.dispatch({
      type: TriageNew.reducers.toggleLabel.name,
      statePath: this.statePath,
      name,
    });
  }

  async onComponent_(name) {
    await STORE.dispatch({
      type: TriageNew.reducers.toggleComponent.name,
      statePath: this.statePath,
      name,
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
        return set(state, `labels.${i}.isEnabled`, e => !e);
      }
    }
    return state;
  },

  toggleComponent: (state, action, rootState) => {
    for (let i = 0; i < state.components.length; ++i) {
      if (state.components[i].name === action.name) {
        return set(state, `components.${i}.isEnabled`, e => !e);
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
