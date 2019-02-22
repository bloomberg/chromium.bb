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

import {globals} from './globals';
import {Sidebar} from './sidebar';
import {Topbar} from './topbar';

function renderPermalink(): m.Children {
  if (!globals.state.permalink.requestId) return null;
  const hash = globals.state.permalink.hash;
  const url = `${self.location.origin}#!/?s=${hash}`;
  return m(
      '.alert-permalink',
      hash ? ['Permalink: ', m(`a[href=${url}]`, url)] : 'Uploading...');
}

class Alerts implements m.ClassComponent {
  view() {
    return m('.alerts', renderPermalink());
  }
}

const TogglePerfDebugButton = {
  view() {
    return m(
        '.perf-monitor-button',
        m('button',
          {
            onclick: () => globals.frontendLocalState.togglePerfDebug(),
          },
          m('i.material-icons',
            {
              title: 'Toggle Perf Debug Mode',
            },
            'assessment')));
  }
};

const PerfStats: m.Component = {
  view() {
    const perfDebug = globals.frontendLocalState.perfDebug;
    const children = [m(TogglePerfDebugButton)];
    if (perfDebug) {
      children.unshift(m('.perf-stats-content'));
    }
    return m(`.perf-stats[expanded=${perfDebug}]`, children);
  }
};

/**
 * Wrap component with common UI elements (nav bar etc).
 */
export function createPage(component: m.Component): m.Component {
  const pageComponent = {
    view() {
      return [
        m(Sidebar),
        m(Topbar),
        m(component),
        m(Alerts),
        m(PerfStats),
      ];
    },
  };

  return pageComponent;
}
