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

import {RawQueryArgs, RawQueryResult} from './protos';
import {TimeSpan} from './time';

export interface LoadingTracker {
  beginLoading(): void;
  endLoading(): void;
}

export class NullLoadingTracker implements LoadingTracker {
  beginLoading(): void {}
  endLoading(): void {}
}

/**
 * Abstract interface of a trace proccessor.
 * This is the TypeScript equivalent of src/trace_processor/rpc.h.
 *
 * Engine also defines helpers for the most common service methods
 * (e.g. query).
 */
export abstract class Engine {
  abstract readonly id: string;
  private _cpus?: number[];
  private _numGpus?: number;
  private loadingTracker: LoadingTracker;

  constructor(tracker?: LoadingTracker) {
    this.loadingTracker = tracker ? tracker : new NullLoadingTracker();
  }

  /**
   * Push trace data into the engine. The engine is supposed to automatically
   * figure out the type of the trace (JSON vs Protobuf).
   */
  abstract parse(data: Uint8Array): Promise<void>;

  /**
   * Notify the engine no more data is coming.
   */
  abstract notifyEof(): void;

  /**
   * Resets the trace processor state by destroying any table/views created by
   * the UI after loading.
   */
  abstract restoreInitialTables(): void;

  /*
   * Performs a SQL query and retruns a proto-encoded RawQueryResult object.
   */
  abstract rawQuery(rawQueryArgs: Uint8Array): Promise<Uint8Array>;

  /**
   * Shorthand for sending a SQL query to the engine.
   * Deals with {,un}marshalling of request/response args.
   */
  async query(sqlQuery: string): Promise<RawQueryResult> {
    this.loadingTracker.beginLoading();
    try {
      const args = new RawQueryArgs();
      args.sqlQuery = sqlQuery;
      args.timeQueuedNs = Math.floor(performance.now() * 1e6);
      const argsEncoded = RawQueryArgs.encode(args).finish();
      const respEncoded = await this.rawQuery(argsEncoded);
      return RawQueryResult.decode(respEncoded);
    } finally {
      this.loadingTracker.endLoading();
    }
  }

  async queryOneRow(query: string): Promise<number[]> {
    const result = await this.query(query);
    const res: number[] = [];
    result.columns.map(c => res.push(+c.longValues![0]));
    return res;
  }

  // TODO(hjd): When streaming must invalidate this somehow.
  async getCpus(): Promise<number[]> {
    if (!this._cpus) {
      const result =
          await this.query('select distinct(cpu) from sched order by cpu;');
      this._cpus = result.columns[0].longValues!.map(n => +n);
    }
    return this._cpus;
  }

  async getNumberOfGpus(): Promise<number> {
    if (!this._numGpus) {
      const result = await this.query(
          'select count(distinct(arg_set_id)) as gpuCount from counters where name = "gpufreq";');
      this._numGpus = +result.columns[0].longValues![0];
    }
    return this._numGpus;
  }

  // TODO: This should live in code that's more specific to chrome, instead of
  // in engine.
  async getNumberOfProcesses(): Promise<number> {
    const result = await this.query('select count(*) from process;');
    return +result.columns[0].longValues![0];
  }

  async getTraceTimeBounds(): Promise<TimeSpan> {
    const query = `select start_ts, end_ts from trace_bounds`;
    const res = (await this.queryOneRow(query));
    return new TimeSpan(res[0] / 1e9, res[1] / 1e9);
  }
}
