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

import {QueryResponse} from '../common/queries';
import {TimeSpan} from '../common/time';

import {copyToClipboard} from './clipboard';
import {globals} from './globals';
import {HeaderPanel} from './header_panel';
import {OverviewTimelinePanel} from './overview_timeline_panel';
import {createPage} from './pages';
import {PanAndZoomHandler} from './pan_and_zoom_handler';
import {Panel} from './panel';
import {AnyAttrsVnode, PanelContainer} from './panel_container';
import {TimeAxisPanel} from './time_axis_panel';
import {TrackGroupPanel} from './track_group_panel';
import {TRACK_SHELL_WIDTH} from './track_panel';
import {TrackPanel} from './track_panel';


const MAX_ZOOM_SPAN_SEC = 1e-4;  // 0.1 ms.

class QueryTable extends Panel {
  view() {
    const resp = globals.queryResults.get('command') as QueryResponse;
    if (resp === undefined) {
      return m('');
    }
    const cols = [];
    for (const col of resp.columns) {
      cols.push(m('td', col));
    }
    const header = m('tr', cols);

    const rows = [];
    for (let i = 0; i < resp.rows.length; i++) {
      const cells = [];
      for (const col of resp.columns) {
        cells.push(m('td', resp.rows[i][col]));
      }
      rows.push(m('tr', cells));
    }
    return m(
        'div',
        m('header.overview',
          m('span',
            `Query result - ${Math.round(resp.durationMs)} ms`,
            m('span.code', resp.query)),
          resp.error ? null :
                       m('button.query-copy',
                         {
                           onclick: () => {
                             const lines: string[][] = [];
                             lines.push(resp.columns);
                             for (const row of resp.rows) {
                               const line = [];
                               for (const col of resp.columns) {
                                 line.push(row[col].toString());
                               }
                               lines.push(line);
                             }
                             copyToClipboard(
                                 lines.map(line => line.join('\t')).join('\n'));
                           },
                         },
                         'Copy as .tsv')),
        resp.error ?
            m('.query-error', `SQL error: ${resp.error}`) :
            m('table.query-table', m('thead', header), m('tbody', rows)));
  }

  renderCanvas() {}
}

/**
 * Top-most level component for the viewer page. Holds tracks, brush timeline,
 * panels, and everything else that's part of the main trace viewer page.
 */
class TraceViewer implements m.ClassComponent {
  private onResize: () => void = () => {};
  private zoomContent?: PanAndZoomHandler;

  oncreate(vnode: m.CVnodeDOM) {
    const frontendLocalState = globals.frontendLocalState;
    const updateDimensions = () => {
      const rect = vnode.dom.getBoundingClientRect();
      frontendLocalState.timeScale.setLimitsPx(
          0, rect.width - TRACK_SHELL_WIDTH);
    };

    updateDimensions();

    // TODO: Do resize handling better.
    this.onResize = () => {
      updateDimensions();
      globals.rafScheduler.scheduleFullRedraw();
    };

    // Once ResizeObservers are out, we can stop accessing the window here.
    window.addEventListener('resize', this.onResize);

    const panZoomEl =
        vnode.dom.querySelector('.pan-and-zoom-content') as HTMLElement;

    this.zoomContent = new PanAndZoomHandler({
      element: panZoomEl,
      contentOffsetX: TRACK_SHELL_WIDTH,
      onPanned: (pannedPx: number) => {
        const traceTime = globals.state.traceTime;
        const vizTime = globals.frontendLocalState.visibleWindowTime;
        const origDelta = vizTime.duration;
        const tDelta = frontendLocalState.timeScale.deltaPxToDuration(pannedPx);
        let tStart = vizTime.start + tDelta;
        let tEnd = vizTime.end + tDelta;
        if (tStart < traceTime.startSec) {
          tStart = traceTime.startSec;
          tEnd = tStart + origDelta;
        } else if (tEnd > traceTime.endSec) {
          tEnd = traceTime.endSec;
          tStart = tEnd - origDelta;
        }
        frontendLocalState.updateVisibleTime(new TimeSpan(tStart, tEnd));
        globals.rafScheduler.scheduleRedraw();
      },
      onZoomed: (_: number, zoomRatio: number) => {
        const vizTime = frontendLocalState.visibleWindowTime;
        const curSpanSec = vizTime.duration;
        const newSpanSec =
            Math.max(curSpanSec - curSpanSec * zoomRatio, MAX_ZOOM_SPAN_SEC);
        const deltaSec = (curSpanSec - newSpanSec) / 2;
        const newStartSec = vizTime.start + deltaSec;
        const newEndSec = vizTime.end - deltaSec;
        frontendLocalState.updateVisibleTime(
            new TimeSpan(newStartSec, newEndSec));
      }
    });
  }

  onremove() {
    window.removeEventListener('resize', this.onResize);
    if (this.zoomContent) this.zoomContent.shutdown();
  }

  view() {
    const scrollingPanels: AnyAttrsVnode[] =
        globals.state.scrollingTracks.length > 0 ?
        [
          m(HeaderPanel, {title: 'Tracks', key: 'tracksheader'}),
          ...globals.state.scrollingTracks.map(
              id => m(TrackPanel, {key: id, id})),
        ] :
        [];

    for (const group of Object.values(globals.state.trackGroups)) {
      scrollingPanels.push(m(TrackGroupPanel, {
        trackGroupId: group.id,
        key: `trackgroup-${group.id}`,
      }));
      if (group.collapsed) continue;
      for (const trackId of group.tracks) {
        scrollingPanels.push(m(TrackPanel, {
          key: `track-${group.id}-${trackId}`,
          id: trackId,
        }));
      }
    }
    scrollingPanels.unshift(m(QueryTable));

    return m(
        '.page',
        m('.pan-and-zoom-content',
          m('.pinned-panel-container', m(PanelContainer, {
              doesScroll: false,
              panels: [
                m(OverviewTimelinePanel, {key: 'overview'}),
                m(TimeAxisPanel, {key: 'timeaxis'}),
                ...globals.state.pinnedTracks.map(
                    id => m(TrackPanel, {key: id, id})),
              ],
            })),
          m('.scrolling-panel-container', m(PanelContainer, {
              doesScroll: true,
              panels: scrollingPanels,
            }))));
  }
}

export const ViewerPage = createPage({
  view() {
    return m(TraceViewer);
  }
});
