import { TestSpecID } from './id.js';
import { ParamsSpec } from './params/index.js';
import { makeQueryString } from './url_query.js';
import { getStackTrace, now } from './util/index.js';
import { version } from './version.js';

type Status = 'running' | 'pass' | 'warn' | 'fail';
export interface LiveTestSpecResult {
  readonly spec: string;
  readonly cases: LiveTestCaseResult[];
}

export interface LiveTestCaseResult {
  readonly test: string;
  readonly params: ParamsSpec | null;
  status: Status;
  logs?: string[];
  timems: number;
}

export class Logger {
  readonly results: LiveTestSpecResult[] = [];

  constructor() {}

  record(spec: TestSpecID): [TestSpecRecorder, LiveTestSpecResult] {
    const result: LiveTestSpecResult = { spec: makeQueryString(spec), cases: [] };
    this.results.push(result);
    return [new TestSpecRecorder(result), result];
  }

  asJSON(space?: number): string {
    return JSON.stringify({ version, results: this.results }, undefined, space);
  }
}

export class TestSpecRecorder {
  private result: LiveTestSpecResult;

  constructor(result: LiveTestSpecResult) {
    this.result = result;
  }

  record(test: string, params: ParamsSpec | null): [TestCaseRecorder, LiveTestCaseResult] {
    const result: LiveTestCaseResult = { test, params, status: 'running', timems: -1 };
    this.result.cases.push(result);
    return [new TestCaseRecorder(result), result];
  }
}

export class TestCaseRecorder {
  private result: LiveTestCaseResult;
  private failed = false;
  private warned = false;
  private startTime = -1;
  private logs: string[] = [];

  constructor(result: LiveTestCaseResult) {
    this.result = result;
  }

  start(): void {
    this.startTime = now();
    this.logs = [];
    this.failed = false;
    this.warned = false;
  }

  finish(): void {
    if (this.startTime < 0) {
      throw new Error('finish() before start()');
    }
    const endTime = now();
    // Round to next microsecond to avoid storing useless .xxxx00000000000002 in results.
    this.result.timems = Math.ceil((endTime - this.startTime) * 1000) / 1000;
    this.result.status = this.failed ? 'fail' : this.warned ? 'warn' : 'pass';

    this.result.logs = this.logs;
  }

  log(msg: string): void {
    this.logs.push(msg);
  }

  warn(msg?: string): void {
    this.warned = true;
    let m = 'WARN';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  fail(msg?: string): void {
    this.failed = true;
    let m = 'FAIL';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  threw(e: Error): void {
    this.failed = true;
    this.log('EXCEPTION: ' + e.name + ': ' + e.message + '\n' + getStackTrace(e));
  }
}
