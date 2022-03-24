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

import {
  NamedSliceTrack,
  NamedSliceTrackTypes
} from '../../frontend/named_slice_track';
import {NewTrackArgs} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

export interface GenericSliceTrackConfig {
  sqlTrackId: number;
}

export interface GenericSliceTrackTypes extends NamedSliceTrackTypes {
  config: GenericSliceTrackConfig;
}

export class GenericSliceTrack extends NamedSliceTrack<GenericSliceTrackTypes> {
  static readonly kind = 'GenericSliceTrack';
  static create(args: NewTrackArgs) {
    return new GenericSliceTrack(args);
  }

  constructor(args: NewTrackArgs) {
    super(args);
  }

  async initSqlTable(tableName: string): Promise<void> {
    const sql = `create view ${tableName} as
    select ts, dur, id, depth, ifnull(name, '') as name
    from slice where track_id = ${this.config.sqlTrackId}`;
    await this.engine.query(sql);
  }
}

trackRegistry.register(GenericSliceTrack);
