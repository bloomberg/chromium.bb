// Copyright (C) 2019 The Android Open Source Project
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

import {fromNs} from '../../common/time';
import {LIMIT} from '../../common/track_data';

import {
  TrackController,
  trackControllerRegistry
} from '../../controller/track_controller';

import {
  Config,
  GPU_FREQ_TRACK_KIND,
  Data,
} from './common';

class GpuFreqTrackController extends TrackController<Config, Data> {
  static readonly kind = GPU_FREQ_TRACK_KIND;
  private busy = false;
  private setup = false;
  private maximumValueSeen = 0;

  onBoundsChange(start: number, end: number, resolution: number): void {
    this.update(start, end, resolution);
  }

  private async update(start: number, end: number, resolution: number):
      Promise<void> {
    // TODO: we should really call TraceProcessor.Interrupt() at this point.
    if (this.busy) return;

    const startNs = Math.round(start * 1e9);
    const endNs = Math.round(end * 1e9);

    this.busy = true;
    if (!this.setup) {
      const result = await this.query(`
      select max(value) from
        counters where name = 'gpufreq'
        and ref = ${this.config.gpu}`);
      this.maximumValueSeen = +result.columns[0].doubleValues![0];

      await this.query(
        `create virtual table ${this.tableName('window')} using window;`);

      await this.query(`create view ${this.tableName('freq')}
          as select
            ts,
            lead(ts) over (order by ts) - ts as dur,
            ref as gpu,
            name as freq_name,
            value as freq_value
          from counters
          where name = 'gpufreq'
            and ref = ${this.config.gpu}
            and ref_type = 'gpu';
      `);

      await this.query(`create virtual table ${this.tableName('span_activity')}
      using span_join(${this.tableName('freq')} PARTITIONED gpu,
                      ${this.tableName('window')});`);

      await this.query(`create view ${this.tableName('activity')}
      as select
        ts,
        dur,
        quantum_ts,
        freq_value as freq
        from ${this.tableName('span_activity')};
      `);

      this.setup = true;
    }

    const isQuantized = this.shouldSummarize(resolution);
    // |resolution| is in s/px we want # ns for 10px window:
    const bucketSizeNs = Math.round(resolution * 10 * 1e9);
    let windowStartNs = startNs;
    if (isQuantized) {
      windowStartNs = Math.floor(windowStartNs / bucketSizeNs) * bucketSizeNs;
    }
    const windowDurNs = Math.max(1, endNs - windowStartNs);

    this.query(`update ${this.tableName('window')} set
      window_start = ${startNs},
      window_dur = ${windowDurNs},
      quantum = ${isQuantized ? bucketSizeNs : 0}`);

    // Cast as double to avoid problem where values are sometimes
    // doubles, sometimes longs.
    let query = `select ts, dur, freq
      from ${this.tableName('activity')} limit ${LIMIT}`;

    if (isQuantized) {
      query = `select
        min(ts) as ts,
        sum(dur) as dur,
        sum(weighted_freq)/sum(dur) as freq_avg,
        quantum_ts
        from (
          select
          ts,
          dur,
          quantum_ts,
          freq*dur as weighted_freq
          from ${this.tableName('activity')})
        group by quantum_ts limit ${LIMIT}`;
    }

    const freqResult = await this.query(query);

    const numRows = +freqResult.numRecords;
    const data: Data = {
      start,
      end,
      resolution,
      length: numRows,
      maximumValue: this.maximumValue(),
      isQuantized,
      tsStarts: new Float64Array(numRows),
      tsEnds: new Float64Array(numRows),
      freqKHz: new Uint32Array(numRows),
    };

    const cols = freqResult.columns;
    for (let row = 0; row < numRows; row++) {
      const startSec = fromNs(+cols[0].longValues![row]);
      data.tsStarts[row] = startSec;
      data.tsEnds[row] = startSec + fromNs(+cols[1].longValues![row]);
      data.freqKHz[row] = +cols[2].doubleValues![row];
    }

    this.publish(data);
    this.busy = false;
  }

  private maximumValue() {
    return Math.max(this.config.maximumValue || 0, this.maximumValueSeen);
  }

  private async query(query: string) {
    const result = await this.engine.query(query);
    if (result.error) {
      console.error(`Query error "${query}": ${result.error}`);
      throw new Error(`Query error "${query}": ${result.error}`);
    }
    return result;
  }
}


trackControllerRegistry.register(GpuFreqTrackController);
