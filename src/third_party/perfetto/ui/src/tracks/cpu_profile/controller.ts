// Copyright (C) 2020 The Android Open Source Project
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

import {NUM} from '../../common/query_result';
import {
  TrackController,
  trackControllerRegistry
} from '../../controller/track_controller';

import {
  Config,
  CPU_PROFILE_TRACK_KIND,
  Data,
} from './common';

class CpuProfileTrackController extends TrackController<Config, Data> {
  static readonly kind = CPU_PROFILE_TRACK_KIND;
  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const query = `select
        id,
        ts,
        callsite_id as callsiteId
      from cpu_profile_stack_sample
      where utid = ${this.config.utid}
      order by ts`;

    const result = await this.queryV2(query);
    const numRows = result.numRows();
    const data: Data = {
      start,
      end,
      resolution,
      length: numRows,
      ids: new Float64Array(numRows),
      tsStarts: new Float64Array(numRows),
      callsiteId: new Uint32Array(numRows),
    };

    const it = result.iter({id: NUM, ts: NUM, callsiteId: NUM});
    for (let row = 0; it.valid(); it.next(), ++row) {
      data.ids[row] = it.id;
      data.tsStarts[row] = it.ts;
      data.callsiteId[row] = it.callsiteId;
    }

    return data;
  }
}

trackControllerRegistry.register(CpuProfileTrackController);
