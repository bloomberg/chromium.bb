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

import {assertTrue} from '../../base/logging';
import {Actions} from '../../common/actions';
import {TrackState} from '../../common/state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  CPU_SLICE_TRACK_KIND,
  Data,
  SliceData,
  SummaryData
} from './common';

const MARGIN_TOP = 5;
const RECT_HEIGHT = 30;

interface Color {
  c: string;
  h: number;
  s: number;
  l: number;
}
const MD_PALETTE: Color[] = [
  {c: 'red', h: 4, s: 90, l: 58},
  {c: 'pink', h: 340, s: 82, l: 52},
  {c: 'purple', h: 291, s: 64, l: 42},
  {c: 'deep purple', h: 262, s: 52, l: 47},
  {c: 'indigo', h: 231, s: 48, l: 48},
  {c: 'blue', h: 207, s: 90, l: 54},
  {c: 'light blue', h: 199, s: 98, l: 48},
  {c: 'cyan', h: 187, s: 100, l: 42},
  {c: 'teal', h: 174, s: 100, l: 29},
  {c: 'green', h: 122, s: 39, l: 49},
  {c: 'light green', h: 88, s: 50, l: 53},
  {c: 'lime', h: 66, s: 70, l: 54},
  {c: 'yellow', h: 54, s: 100, l: 62},
  {c: 'amber', h: 45, s: 100, l: 51},
  {c: 'orange', h: 36, s: 100, l: 50},
  {c: 'deep organge', h: 14, s: 100, l: 57},
  {c: 'brown', h: 16, s: 25, l: 38},
  {c: 'grey', h: 0, s: 0, l: 62},
  {c: 'blue gray', h: 200, s: 18, l: 46},
];

function hash(s: string, max: number): number {
  let hash = 0x811c9dc5 & 0xfffffff;
  for (let i = 0; i < s.length; i++) {
    hash ^= s.charCodeAt(i);
    hash = (hash * 16777619) & 0xffffffff;
  }
  return Math.abs(hash) % max;
}

function cropText(str: string, charWidth: number, rectWidth: number) {
  const maxTextWidth = rectWidth - 4;
  let displayText = '';
  const nameLength = str.length * charWidth;
  if (nameLength < maxTextWidth) {
    displayText = str;
  } else {
    // -3 for the 3 ellipsis.
    const displayedChars = Math.floor(maxTextWidth / charWidth) - 3;
    if (displayedChars > 3) {
      displayText = str.substring(0, displayedChars) + '...';
    }
  }
  return displayText;
}

function getCurResolution() {
  // Truncate the resolution to the closest power of 10.
  const resolution = globals.frontendLocalState.timeScale.deltaPxToDuration(1);
  return Math.pow(10, Math.floor(Math.log10(resolution)));
}

class CpuSliceTrack extends Track<Config, Data> {
  static readonly kind = CPU_SLICE_TRACK_KIND;
  static create(trackState: TrackState): CpuSliceTrack {
    return new CpuSliceTrack(trackState);
  }

  private mouseXpos?: number;
  private reqPending = false;
  private hue: number;
  private utidHoveredInThisTrack = -1;

  constructor(trackState: TrackState) {
    super(trackState);
    // TODO: this needs to be kept in sync with the hue generation algorithm
    // of overview_timeline_panel.ts
    this.hue = (128 + (32 * this.config.cpu)) % 256;
  }

  reqDataDeferred() {
    const {visibleWindowTime} = globals.frontendLocalState;
    const reqStart = visibleWindowTime.start - visibleWindowTime.duration;
    const reqEnd = visibleWindowTime.end + visibleWindowTime.duration;
    const reqRes = getCurResolution();
    this.reqPending = false;
    globals.dispatch(Actions.reqTrackData({
      trackId: this.trackState.id,
      start: reqStart,
      end: reqEnd,
      resolution: reqRes
    }));
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    // TODO: fonts and colors should come from the CSS and not hardcoded here.
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();

    // If there aren't enough cached slices data in |data| request more to
    // the controller.
    const inRange = data !== undefined &&
        (visibleWindowTime.start >= data.start &&
         visibleWindowTime.end <= data.end);
    if (!inRange || data === undefined ||
        data.resolution !== getCurResolution()) {
      if (!this.reqPending) {
        this.reqPending = true;
        setTimeout(() => this.reqDataDeferred(), 50);
      }
    }
    if (data === undefined) return;  // Can't possibly draw anything.

    // If the cached trace slices don't fully cover the visible time range,
    // show a gray rectangle with a "Loading..." label.
    checkerboardExcept(
        ctx,
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end));

    if (data.kind === 'summary') {
      this.renderSummary(ctx, data);
    } else if (data.kind === 'slice') {
      this.renderSlices(ctx, data);
    }
  }

  renderSummary(ctx: CanvasRenderingContext2D, data: SummaryData): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const startPx = Math.floor(timeScale.timeToPx(visibleWindowTime.start));
    const bottomY = MARGIN_TOP + RECT_HEIGHT;

    let lastX = startPx;
    let lastY = bottomY;

    ctx.fillStyle = `hsl(${this.hue}, 50%, 60%)`;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    for (let i = 0; i < data.utilizations.length; i++) {
      const utilization = data.utilizations[i];
      const startTime = i * data.bucketSizeSeconds + data.start;

      lastX = Math.floor(timeScale.timeToPx(startTime));

      ctx.lineTo(lastX, lastY);
      lastY = MARGIN_TOP + Math.round(RECT_HEIGHT * (1 - utilization));
      ctx.lineTo(lastX, lastY);
    }
    ctx.lineTo(lastX, bottomY);
    ctx.closePath();
    ctx.fill();
  }

  renderSlices(ctx: CanvasRenderingContext2D, data: SliceData): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    assertTrue(data.starts.length === data.ends.length);
    assertTrue(data.starts.length === data.utids.length);

    ctx.textAlign = 'center';
    ctx.font = '12px Google Sans';
    const charWidth = ctx.measureText('dbpqaouk').width / 8;

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const utid = data.utids[i];
      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }
      const rectStart = timeScale.timeToPx(tStart);
      const rectEnd = timeScale.timeToPx(tEnd);
      const rectWidth = rectEnd - rectStart;
      if (rectWidth < 0.1) continue;


      // TODO: consider de-duplicating this code with the copied one from
      // chrome_slices/frontend.ts.
      let title = `[utid:${utid}]`;
      let subTitle = '';
      const color = Object.assign({}, MD_PALETTE[14]);

      const threadInfo = globals.threads.get(utid);
      if (threadInfo !== undefined) {
        const hasProc = !!threadInfo.pid;
        const procName = threadInfo.procName || '';
        let hashKey = threadInfo.tid;
        if (hasProc) {
          title = `${procName} [${threadInfo.pid}]`;
          subTitle = `${threadInfo.threadName} [${threadInfo.tid}]`;
          hashKey = threadInfo.pid!;
        } else {
          title = `${threadInfo.threadName} [${threadInfo.tid}]`;
        }
        const colorIdx = hash(hashKey.toString(), 16);
        Object.assign(color, MD_PALETTE[colorIdx]);
      }

      const hovered = globals.frontendLocalState.highlightedUtid === utid;
      color.l =
          hovered ? Math.max(color.l - 40, 20) : Math.min(color.l + 10, 80);
      color.s -= 20;
      ctx.fillStyle = `hsl(${color.h}, ${color.s}%, ${color.l}%)`;
      ctx.fillRect(rectStart, MARGIN_TOP, rectEnd - rectStart, RECT_HEIGHT);

      // Don't render text when we have less than 5px to play with.
      if (rectWidth < 5) continue;

      title = cropText(title, charWidth, rectWidth);
      subTitle = cropText(subTitle, charWidth, rectWidth);
      const rectXCenter = rectStart + rectWidth / 2;
      ctx.fillStyle = '#fff';
      ctx.font = '12px Google Sans';
      ctx.fillText(title, rectXCenter, MARGIN_TOP + RECT_HEIGHT / 2 - 3);
      ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
      ctx.font = '10px Google Sans';
      ctx.fillText(subTitle, rectXCenter, MARGIN_TOP + RECT_HEIGHT / 2 + 11);
    }

    const hoveredThread = globals.threads.get(this.utidHoveredInThisTrack);
    if (hoveredThread !== undefined) {
      let line1 = '';
      let line2 = '';
      if (hoveredThread.pid) {
        line1 = `P: ${hoveredThread.procName} [${hoveredThread.pid}]`;
        line2 = `T: ${hoveredThread.threadName} [${hoveredThread.tid}]`;
      } else {
        line1 = `T: ${hoveredThread.threadName} [${hoveredThread.tid}]`;
      }

      ctx.font = '10px Google Sans';
      const line1Width = ctx.measureText(line1).width;
      const line2Width = ctx.measureText(line2).width;
      const width = Math.max(line1Width, line2Width);

      ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
      ctx.fillRect(this.mouseXpos!, MARGIN_TOP, width + 16, RECT_HEIGHT);
      ctx.fillStyle = 'hsl(200, 50%, 40%)';
      ctx.textAlign = 'left';
      ctx.fillText(line1, this.mouseXpos! + 8, 18);
      ctx.fillText(line2, this.mouseXpos! + 8, 28);
    }
  }

  onMouseMove({x, y}: {x: number, y: number}) {
    const data = this.data();
    this.mouseXpos = x;
    if (data === undefined || data.kind === 'summary') return;
    const {timeScale} = globals.frontendLocalState;
    if (y < MARGIN_TOP || y > MARGIN_TOP + RECT_HEIGHT) {
      this.utidHoveredInThisTrack = -1;
      globals.frontendLocalState.setHighlightedUtid(-1);
      return;
    }
    const t = timeScale.pxToTime(x);
    let hoveredUtid = -1;

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const utid = data.utids[i];
      if (tStart <= t && t <= tEnd) {
        hoveredUtid = utid;
        break;
      }
    }
    this.utidHoveredInThisTrack = hoveredUtid;
    globals.frontendLocalState.setHighlightedUtid(hoveredUtid);
  }

  onMouseOut() {
    this.utidHoveredInThisTrack = -1;
    globals.frontendLocalState.setHighlightedUtid(-1);
    this.mouseXpos = 0;
  }
}

trackRegistry.register(CpuSliceTrack);
