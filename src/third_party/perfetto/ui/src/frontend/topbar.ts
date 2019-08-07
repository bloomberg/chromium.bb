// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {Actions} from '../common/actions';
import {QueryResponse} from '../common/queries';
import {EngineConfig} from '../common/state';

import {globals} from './globals';

const QUERY_ID = 'quicksearch';

let selResult = 0;
let numResults = 0;
let mode: 'search'|'command' = 'search';
let omniboxValue = '';

function enterCommandMode() {
  if (mode === 'search') {
    mode = 'command';
    globals.rafScheduler.scheduleFullRedraw();
  }
}

function clearOmniboxResults(e: Event) {
  globals.queryResults.delete(QUERY_ID);
  globals.dispatch(Actions.deleteQuery({queryId: QUERY_ID}));
  const txt = (e.target as HTMLInputElement);
  if (txt.value.length <= 0) {
    mode = 'search';
    globals.rafScheduler.scheduleFullRedraw();
  }
}

function onKeyDown(e: Event) {
  e.stopPropagation();
  const key = (e as KeyboardEvent).key;

  // Avoid that the global 'a', 'd', 'w', 's' handler sees these keystrokes.
  // TODO: this seems a bug in the pan_and_zoom_handler.ts.
  if (key === 'ArrowUp' || key === 'ArrowDown') {
    e.preventDefault();
    return;
  }
  const txt = (e.target as HTMLInputElement);
  omniboxValue = txt.value;
}

function onKeyUp(e: Event) {
  e.stopPropagation();
  const key = (e as KeyboardEvent).key;
  const txt = e.target as HTMLInputElement;
  omniboxValue = txt.value;
  if (key === 'ArrowUp' || key === 'ArrowDown') {
    selResult += (key === 'ArrowUp') ? -1 : 1;
    selResult = Math.max(selResult, 0);
    selResult = Math.min(selResult, numResults - 1);
    e.preventDefault();
    globals.rafScheduler.scheduleFullRedraw();
    return;
  }
  if (txt.value.length <= 0 || key === 'Escape') {
    globals.queryResults.delete(QUERY_ID);
    globals.dispatch(Actions.deleteQuery({queryId: QUERY_ID}));
    mode = 'search';
    txt.value = '';
    omniboxValue = txt.value;
    txt.blur();
    globals.rafScheduler.scheduleFullRedraw();
    return;
  }
  if (mode === 'command' && key === 'Enter') {
    globals.dispatch(Actions.executeQuery(
        {engineId: '0', queryId: 'command', query: txt.value}));
  }
}

class Omnibox implements m.ClassComponent {
  oncreate(vnode: m.VnodeDOM) {
    const txt = vnode.dom.querySelector('input') as HTMLInputElement;
    txt.addEventListener('focus', enterCommandMode);
    txt.addEventListener('click', enterCommandMode);
    txt.addEventListener('blur', clearOmniboxResults);
    txt.addEventListener('keydown', onKeyDown);
    txt.addEventListener('keyup', onKeyUp);
  }

  view() {
    const msgTTL = globals.state.status.timestamp + 1 - Date.now() / 1e3;
    let enginesAreBusy = false;
    for (const engine of Object.values(globals.state.engines)) {
      enginesAreBusy = enginesAreBusy || !engine.ready;
    }

    if (msgTTL > 0 || enginesAreBusy) {
      setTimeout(
          () => globals.rafScheduler.scheduleFullRedraw(), msgTTL * 1000);
      return m(
          `.omnibox.message-mode`,
          m(`input[placeholder=${globals.state.status.msg}][readonly]`, {
            value: '',
          }));
    }

    // TODO(primiano): handle query results here.
    const results = [];
    const resp = globals.queryResults.get(QUERY_ID) as QueryResponse;
    if (resp !== undefined) {
      numResults = resp.rows ? resp.rows.length : 0;
      for (let i = 0; i < resp.rows.length; i++) {
        const clazz = (i === selResult) ? '.selected' : '';
        results.push(m(`div${clazz}`, resp.rows[i][resp.columns[0]]));
      }
    }
    const placeholder = {
      search: 'Click to enter command mode',
      command: 'e.g. select * from sched left join thread using(utid) limit 10'
    };

    const commandMode = mode === 'command';
    return m(
        `.omnibox${commandMode ? '.command-mode' : ''}`,
        m(`input[placeholder=${placeholder[mode]}]`, {
          onchange: m.withAttr('value', v => omniboxValue = v),
          value: omniboxValue,
        }),
        m('.omnibox-results', results));
  }
}

export class Topbar implements m.ClassComponent {
  view() {
    const progBar = [];
    const engine: EngineConfig = globals.state.engines['0'];
    if (globals.state.queries[QUERY_ID] !== undefined ||
        (engine !== undefined && !engine.ready)) {
      progBar.push(m('.progress'));
    }
    return m('.topbar', m(Omnibox), ...progBar);
  }
}
