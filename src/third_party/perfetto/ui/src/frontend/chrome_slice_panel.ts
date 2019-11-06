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

import {timeToCode} from '../common/time';

import {globals} from './globals';
import {Panel, PanelSize} from './panel';

export class ChromeSliceDetailsPanel extends Panel {
  view() {
    const sliceInfo = globals.sliceDetails;
    if (sliceInfo.ts && sliceInfo.dur && sliceInfo.name) {
      return m(
          '.details-panel',
          m('.details-panel-heading', `Slice Details:`),
          m(
              '.details-table',
              [m('table',
                 [
                   m('tr', m('th', `Name`), m('td', `${sliceInfo.name}`)),
                   m('tr',
                     m('th', `Category`),
                     m('td', `${sliceInfo.category}`)),
                   m('tr',
                     m('th', `Start time`),
                     m('td', `${timeToCode(sliceInfo.ts)}`)),
                   m('tr',
                     m('th', `Duration`),
                     m('td', `${timeToCode(sliceInfo.dur)}`))
                 ])],
              ));
    } else {
      return m(
          '.details-panel',
          m(
              '.details-panel-heading',
              `Slice Details:`,
              ));
    }
  }
  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}
}
