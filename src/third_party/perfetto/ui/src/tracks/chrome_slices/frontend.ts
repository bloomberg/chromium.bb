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

import {hsluvToHex} from 'hsluv';

import {Actions} from '../../common/actions';
import {cropText, drawIncompleteSlice} from '../../common/canvas_utils';
import {hslForSlice} from '../../common/colorizer';
import {TRACE_MARGIN_TIME_S} from '../../common/constants';
import {TrackState} from '../../common/state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {globals} from '../../frontend/globals';
import {SliceRect, Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {Config, Data, SLICE_TRACK_KIND} from './common';

const SLICE_HEIGHT = 18;
const TRACK_PADDING = 4;
const CHEVRON_WIDTH_PX = 10;
const HALF_CHEVRON_WIDTH_PX = CHEVRON_WIDTH_PX / 2;
const INNER_CHEVRON_OFFSET = -3;
const INNER_CHEVRON_SCALE =
    (SLICE_HEIGHT - 2 * INNER_CHEVRON_OFFSET) / SLICE_HEIGHT;

export class ChromeSliceTrack extends Track<Config, Data> {
  static readonly kind: string = SLICE_TRACK_KIND;
  static create(trackState: TrackState): Track {
    return new ChromeSliceTrack(trackState);
  }

  private hoveredTitleId = -1;

  constructor(trackState: TrackState) {
    super(trackState);
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    // TODO: fonts and colors should come from the CSS and not hardcoded here.

    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();

    if (data === undefined) return;  // Can't possibly draw anything.

    // If the cached trace slices don't fully cover the visible time range,
    // show a gray rectangle with a "Loading..." label.
    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end),
    );

    ctx.font = '12px Roboto Condensed';
    ctx.textAlign = 'center';

    // measuretext is expensive so we only use it once.
    const charWidth = ctx.measureText('ACBDLqsdfg').width / 10;

    // The draw of the rect on the selected slice must happen after the other
    // drawings, otherwise it would result under another rect.
    let drawRectOnSelected = () => {};

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      let tEnd = data.ends[i];
      const depth = data.depths[i];
      const titleId = data.titles[i];
      const sliceId = data.sliceIds[i];
      const isInstant = data.isInstant[i];
      const isIncomplete = data.isIncomplete[i];
      const title = data.strings[titleId];
      const colorOverride = data.colors && data.strings[data.colors[i]];
      if (isIncomplete) {  // incomplete slice
        tEnd = visibleWindowTime.end;
      }

      const rect = this.getSliceRect(tStart, tEnd, depth);
      if (!rect || !rect.visible) {
        continue;
      }

      const currentSelection = globals.state.currentSelection;
      const isSelected = currentSelection &&
          currentSelection.kind === 'CHROME_SLICE' &&
          currentSelection.id !== undefined && currentSelection.id === sliceId;

      const name = title.replace(/( )?\d+/g, '');
      const highlighted = titleId === this.hoveredTitleId ||
          globals.frontendLocalState.highlightedSliceId === sliceId;

      const [hue, saturation, lightness] =
          hslForSlice(name, highlighted || isSelected);

      let color: string;
      if (colorOverride === undefined) {
        color = hsluvToHex([hue, saturation, lightness]);
      } else {
        color = colorOverride;
      }
      ctx.fillStyle = color;

      // We draw instant events as upward facing chevrons starting at A:
      //     A
      //    ###
      //   ##C##
      //  ##   ##
      // D       B
      // Then B, C, D and back to A:
      if (isInstant) {
        if (isSelected) {
          drawRectOnSelected = () => {
            ctx.save();
            ctx.translate(rect.left, rect.top);

            // Draw outer chevron as dark border
            ctx.save();
            ctx.translate(0, INNER_CHEVRON_OFFSET);
            ctx.scale(INNER_CHEVRON_SCALE, INNER_CHEVRON_SCALE);
            ctx.fillStyle = hsluvToHex([hue, 100, 10]);

            this.drawChevron(ctx);
            ctx.restore();

            // Draw inner chevron as interior
            ctx.fillStyle = color;
            this.drawChevron(ctx);

            ctx.restore();
          };
        } else {
          ctx.save();
          ctx.translate(rect.left, rect.top);
          this.drawChevron(ctx);
          ctx.restore();
        }
        continue;
      }
      if (isIncomplete && rect.width > SLICE_HEIGHT / 4) {
        drawIncompleteSlice(
            ctx, rect.left, rect.top, rect.width, SLICE_HEIGHT, color);
      } else {
        ctx.fillRect(rect.left, rect.top, rect.width, SLICE_HEIGHT);
      }
      // Selected case
      if (isSelected) {
        drawRectOnSelected = () => {
          ctx.strokeStyle = hsluvToHex([hue, 100, 10]);
          ctx.beginPath();
          ctx.lineWidth = 3;
          ctx.strokeRect(
              rect.left, rect.top - 1.5, rect.width, SLICE_HEIGHT + 3);
          ctx.closePath();
        };
      }

      ctx.fillStyle = lightness > 65 ? '#404040' : 'white';
      const displayText = cropText(title, charWidth, rect.width);
      const rectXCenter = rect.left + rect.width / 2;
      ctx.textBaseline = "middle";
      ctx.fillText(displayText, rectXCenter, rect.top + SLICE_HEIGHT / 2);
    }
    drawRectOnSelected();
  }

  drawChevron(ctx: CanvasRenderingContext2D) {
    // Draw a chevron at a fixed location and size. Should be used with
    // ctx.translate and ctx.scale to alter location and size.
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(HALF_CHEVRON_WIDTH_PX, SLICE_HEIGHT);
    ctx.lineTo(0, SLICE_HEIGHT - HALF_CHEVRON_WIDTH_PX);
    ctx.lineTo(-HALF_CHEVRON_WIDTH_PX, SLICE_HEIGHT);
    ctx.lineTo(0, 0);
    ctx.fill();
  }

  getSliceIndex({x, y}: {x: number, y: number}): number|void {
    const data = this.data();
    if (data === undefined) return;
    const {timeScale} = globals.frontendLocalState;
    if (y < TRACK_PADDING) return;
    const instantWidthTime = timeScale.deltaPxToDuration(HALF_CHEVRON_WIDTH_PX);
    const t = timeScale.pxToTime(x);
    const depth = Math.floor((y - TRACK_PADDING) / SLICE_HEIGHT);
    for (let i = 0; i < data.starts.length; i++) {
      if (depth !== data.depths[i]) {
        continue;
      }
      const tStart = data.starts[i];
      if (data.isInstant[i]) {
        if (Math.abs(tStart - t) < instantWidthTime) {
          return i;
        }
      } else {
        let tEnd = data.ends[i];
        if (data.isIncomplete[i]) {
          tEnd = globals.frontendLocalState.visibleWindowTime.end;
        }
        if (tStart <= t && t <= tEnd) {
          return i;
        }
      }
    }
  }

  onMouseMove({x, y}: {x: number, y: number}) {
    this.hoveredTitleId = -1;
    globals.frontendLocalState.setHighlightedSliceId(-1);
    const sliceIndex = this.getSliceIndex({x, y});
    if (sliceIndex === undefined) return;
    const data = this.data();
    if (data === undefined) return;
    this.hoveredTitleId = data.titles[sliceIndex];
    globals.frontendLocalState.setHighlightedSliceId(data.sliceIds[sliceIndex]);
  }

  onMouseOut() {
    this.hoveredTitleId = -1;
    globals.frontendLocalState.setHighlightedSliceId(-1);
  }

  onMouseClick({x, y}: {x: number, y: number}): boolean {
    const sliceIndex = this.getSliceIndex({x, y});
    if (sliceIndex === undefined) return false;
    const data = this.data();
    if (data === undefined) return false;
    const sliceId = data.sliceIds[sliceIndex];
    if (sliceId !== undefined && sliceId !== -1) {
      globals.makeSelection(Actions.selectChromeSlice({
        id: sliceId,
        trackId: this.trackState.id,
        table: this.config.namespace
      }));
      return true;
    }
    return false;
  }

  getHeight() {
    return SLICE_HEIGHT * (this.config.maxDepth + 1) + 2 * TRACK_PADDING;
  }

  getSliceRect(tStart: number, tEnd: number, depth: number): SliceRect
      |undefined {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const pxEnd = timeScale.timeToPx(visibleWindowTime.end);
    const left = Math.max(timeScale.timeToPx(tStart), -TRACE_MARGIN_TIME_S);
    const right = Math.min(timeScale.timeToPx(tEnd), pxEnd);
    return {
      left,
      width: Math.max(right - left, 1),
      top: TRACK_PADDING + depth * SLICE_HEIGHT,
      height: SLICE_HEIGHT,
      visible:
          !(tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end)
    };
  }
}

trackRegistry.register(ChromeSliceTrack);
