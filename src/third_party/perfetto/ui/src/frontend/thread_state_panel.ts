// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
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

import {translateState} from '../common/thread_state';
import {timeToCode} from '../common/time';

import {globals} from './globals';
import {Panel, PanelSize} from './panel';

interface ThreadStateDetailsAttr {
  utid: number;
  ts: number;
  dur: number;
  state: string;
  cpu: number;
}

export class ThreadStatePanel extends Panel<ThreadStateDetailsAttr> {
  view({attrs}: m.CVnode<ThreadStateDetailsAttr>) {
    const threadInfo = globals.threads.get(attrs.utid);
    if (threadInfo) {
      return m(
          '.details-panel',
          m('.details-panel-heading', 'Thread State'),
          m('.details-table', [m('table', [
              m('tr',
                m('th', `Start time`),
                m('td',
                  `${
                      timeToCode(
                          attrs.ts - globals.state.traceTime.startSec)}`)),
              m('tr',
                m('th', `Duration`),
                m('td',
                  `${timeToCode(attrs.dur)} `,
                  m('a',
                    {href: 'http://b/140256335', target: '_blank'},
                    '(b/140256335)'))),
              m('tr',
                m('th', `State`),
                m('td',
                  `${translateState(attrs.state)}` +
                      `${
                          attrs.state === 'Running' ? ` on CPU ${attrs.cpu}` :
                                                      ''}`)),
              m('tr',
                m('th', `Process`),
                m('td', `${threadInfo.procName} [${threadInfo.pid}]`)),
            ])]));
    } else {
      return m('.details-panel');
    }
  }

  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}
}
