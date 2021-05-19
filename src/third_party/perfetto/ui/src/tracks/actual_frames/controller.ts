// Copyright (C) 2021 The Android Open Source Project
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
import {slowlyCountRows} from '../../common/query_iterator';
import {fromNs, toNs} from '../../common/time';
import {
  TrackController,
  trackControllerRegistry,
} from '../../controller/track_controller';

import {ACTUAL_FRAMES_SLICE_TRACK_KIND, Config, Data} from './common';

const BLUE_COLOR = '#03A9F4';    // Blue 500
const GREEN_COLOR = '#4CAF50';     // Green 500
const YELLOW_COLOR = '#FFEB3B';  // Yellow 500
const RED_COLOR = '#FF5722';      // Red 500
const LIGHT_GREEN_COLOR = '#C0D588'; // Light Green 500
const PINK_COLOR = '#F515E0';        // Pink 500

class ActualFramesSliceTrackController extends TrackController<Config, Data> {
  static readonly kind = ACTUAL_FRAMES_SLICE_TRACK_KIND;
  private maxDurNs = 0;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);

    const pxSize = this.pxSize();

    // ns per quantization bucket (i.e. ns per pixel). /2 * 2 is to force it to
    // be an even number, so we can snap in the middle.
    const bucketNs = Math.max(Math.round(resolution * 1e9 * pxSize / 2) * 2, 1);

    if (this.maxDurNs === 0) {
      const maxDurResult = await this.query(`
        select max(dur)
        from experimental_slice_layout
        where filter_track_ids = '${this.config.trackIds.join(',')}'
      `);
      if (slowlyCountRows(maxDurResult) === 1) {
        this.maxDurNs = maxDurResult.columns[0].longValues![0];
      }
    }

    const rawResult = await this.query(`
      SELECT
        (s.ts + ${bucketNs / 2}) / ${bucketNs} * ${bucketNs} as tsq,
        s.ts,
        max(s.dur) as dur,
        s.layout_depth,
        s.name,
        s.id,
        s.dur = 0 as is_instant,
        s.dur = -1 as is_incomplete,
        CASE afs.jank_tag
          WHEN 'Self Jank' THEN '${RED_COLOR}'
          WHEN 'Other Jank' THEN '${YELLOW_COLOR}'
          WHEN 'Dropped Frame' THEN '${BLUE_COLOR}'
          WHEN 'Buffer Stuffing' THEN '${LIGHT_GREEN_COLOR}'
          WHEN 'No Jank' THEN '${GREEN_COLOR}'
          ELSE '${PINK_COLOR}'
        END as color
      from experimental_slice_layout s
      join actual_frame_timeline_slice afs using(id)
      where
        filter_track_ids = '${this.config.trackIds.join(',')}' and
        s.ts >= ${startNs - this.maxDurNs} and
        s.ts <= ${endNs}
      group by tsq, s.layout_depth
      order by tsq, s.layout_depth
    `);

    const numRows = slowlyCountRows(rawResult);
    const slices: Data = {
      start,
      end,
      resolution,
      length: numRows,
      strings: [],
      sliceIds: new Float64Array(numRows),
      starts: new Float64Array(numRows),
      ends: new Float64Array(numRows),
      depths: new Uint16Array(numRows),
      titles: new Uint16Array(numRows),
      colors: new Uint16Array(numRows),
      isInstant: new Uint16Array(numRows),
      isIncomplete: new Uint16Array(numRows),
    };

    const stringIndexes = new Map<string, number>();
    function internString(str: string) {
      let idx = stringIndexes.get(str);
      if (idx !== undefined) return idx;
      idx = slices.strings.length;
      slices.strings.push(str);
      stringIndexes.set(str, idx);
      return idx;
    }

    const cols = rawResult.columns;
    for (let row = 0; row < numRows; row++) {
      const startNsQ = +cols[0].longValues![row];
      const startNs = +cols[1].longValues![row];
      const durNs = +cols[2].longValues![row];
      const endNs = startNs + durNs;

      let endNsQ = Math.floor((endNs + bucketNs / 2 - 1) / bucketNs) * bucketNs;
      endNsQ = Math.max(endNsQ, startNsQ + bucketNs);

      assertTrue(startNsQ !== endNsQ);

      slices.starts[row] = fromNs(startNsQ);
      slices.ends[row] = fromNs(endNsQ);
      slices.depths[row] = +cols[3].longValues![row];
      slices.titles[row] = internString(cols[4].stringValues![row]);
      slices.colors![row] = internString(cols[8].stringValues![row]);
      slices.sliceIds[row] = +cols[5].longValues![row];
      slices.isInstant[row] = +cols[6].longValues![row];
      slices.isIncomplete[row] = +cols[7].longValues![row];
    }
    return slices;
  }
}


trackControllerRegistry.register(ActualFramesSliceTrackController);
