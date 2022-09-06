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

import {Engine} from '../common/engine';
import {
  LogBounds,
  LogBoundsKey,
  LogEntries,
  LogEntriesKey,
  LogExistsKey,
} from '../common/logs';
import {NUM, STR} from '../common/query_result';
import {fromNs, TimeSpan, toNsCeil, toNsFloor} from '../common/time';
import {publishTrackData} from '../frontend/publish';

import {Controller} from './controller';
import {App} from './globals';

async function updateLogBounds(
    engine: Engine, span: TimeSpan): Promise<LogBounds> {
  const vizStartNs = toNsFloor(span.start);
  const vizEndNs = toNsCeil(span.end);

  const countResult = await engine.query(`
     select
      ifnull(min(ts), 0) as minTs,
      ifnull(max(ts), 0) as maxTs,
      count(ts) as countTs
     from android_logs where ts >= ${vizStartNs} and ts <= ${vizEndNs}`);

  const countRow = countResult.firstRow({minTs: NUM, maxTs: NUM, countTs: NUM});

  const firstRowNs = countRow.minTs;
  const lastRowNs = countRow.maxTs;
  const total = countRow.countTs;

  const minResult = await engine.query(`
     select ifnull(max(ts), 0) as maxTs from android_logs where ts < ${
      vizStartNs}`);
  const startNs = minResult.firstRow({maxTs: NUM}).maxTs;

  const maxResult = await engine.query(`
     select ifnull(min(ts), 0) as minTs from android_logs where ts > ${
      vizEndNs}`);
  const endNs = maxResult.firstRow({minTs: NUM}).minTs;

  const startTs = startNs ? fromNs(startNs) : 0;
  const endTs = endNs ? fromNs(endNs) : Number.MAX_SAFE_INTEGER;
  const firstRowTs = firstRowNs ? fromNs(firstRowNs) : endTs;
  const lastRowTs = lastRowNs ? fromNs(lastRowNs) : startTs;
  return {
    startTs,
    endTs,
    firstRowTs,
    lastRowTs,
    total,
  };
}

async function updateLogEntries(
    engine: Engine, span: TimeSpan, pagination: Pagination):
    Promise<LogEntries> {
  const vizStartNs = toNsFloor(span.start);
  const vizEndNs = toNsCeil(span.end);
  const vizSqlBounds = `ts >= ${vizStartNs} and ts <= ${vizEndNs}`;

  const rowsResult = await engine.query(`
        select
          ts,
          prio,
          ifnull(tag, '[NULL]') as tag,
          ifnull(msg, '[NULL]') as msg
        from android_logs
        where ${vizSqlBounds}
        order by ts
        limit ${pagination.start}, ${pagination.count}
    `);

  const timestamps = [];
  const priorities = [];
  const tags = [];
  const messages = [];

  const it = rowsResult.iter({ts: NUM, prio: NUM, tag: STR, msg: STR});
  for (; it.valid(); it.next()) {
    timestamps.push(it.ts);
    priorities.push(it.prio);
    tags.push(it.tag);
    messages.push(it.msg);
  }

  return {
    offset: pagination.start,
    timestamps,
    priorities,
    tags,
    messages,
  };
}

class Pagination {
  private _offset: number;
  private _count: number;

  constructor(offset: number, count: number) {
    this._offset = offset;
    this._count = count;
  }

  get start() {
    return this._offset;
  }

  get count() {
    return this._count;
  }

  get end() {
    return this._offset + this._count;
  }

  contains(other: Pagination): boolean {
    return this.start <= other.start && other.end <= this.end;
  }

  grow(n: number): Pagination {
    const newStart = Math.max(0, this.start - n / 2);
    const newCount = this.count + n;
    return new Pagination(newStart, newCount);
  }
}

export interface LogsControllerArgs {
  engine: Engine;
  app: App;
}

/**
 * LogsController looks at two parts of the state:
 * 1. The visible trace window
 * 2. The requested offset and count the log lines to display
 * And keeps two bits of published information up to date:
 * 1. The total number of log messages in visible range
 * 2. The logs lines that should be displayed
 */
export class LogsController extends Controller<'main'> {
  private app: App;
  private engine: Engine;
  private span: TimeSpan;
  private pagination: Pagination;
  private hasLogs = false;

  constructor(args: LogsControllerArgs) {
    super('main');
    this.app = args.app;
    this.engine = args.engine;
    this.span = new TimeSpan(0, 10);
    this.pagination = new Pagination(0, 0);
    this.hasAnyLogs().then((exists) => {
      this.hasLogs = exists;
      publishTrackData({
        id: LogExistsKey,
        data: {
          exists,
        },
      });
    });
  }

  async hasAnyLogs() {
    const result = await this.engine.query(`
      select count(*) as cnt from android_logs
    `);
    return result.firstRow({cnt: NUM}).cnt > 0;
  }

  run() {
    if (!this.hasLogs) return;

    const traceTime = this.app.state.frontendLocalState.visibleState;
    const newSpan = new TimeSpan(traceTime.startSec, traceTime.endSec);
    const oldSpan = this.span;

    const pagination = this.app.state.logsPagination;
    // This can occur when loading old traces.
    // TODO(hjd): Fix the problem of accessing state from a previous version of
    // the UI in a general way.
    if (pagination === undefined) {
      return;
    }

    const {offset, count} = pagination;
    const requestedPagination = new Pagination(offset, count);
    const oldPagination = this.pagination;

    const needSpanUpdate = !oldSpan.equals(newSpan);
    const needPaginationUpdate = !oldPagination.contains(requestedPagination);

    // TODO(hjd): We could waste a lot of time queueing useless updates here.
    // We should avoid enqueuing a request when one is in progress.
    if (needSpanUpdate) {
      this.span = newSpan;
      updateLogBounds(this.engine, newSpan).then((data) => {
        if (!newSpan.equals(this.span)) return;
        publishTrackData({
          id: LogBoundsKey,
          data,
        });
      });
    }

    // TODO(hjd): We could waste a lot of time queueing useless updates here.
    // We should avoid enqueuing a request when one is in progress.
    if (needSpanUpdate || needPaginationUpdate) {
      this.pagination = requestedPagination.grow(100);

      updateLogEntries(this.engine, newSpan, this.pagination).then((data) => {
        if (!this.pagination.contains(requestedPagination)) return;
        publishTrackData({
          id: LogEntriesKey,
          data,
        });
      });
    }

    return [];
  }
}
