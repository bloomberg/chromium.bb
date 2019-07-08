import { ParamsSpec } from './params/index.js';
import { getStackTrace, now } from './util/index.js';
import { version } from './version.js';
import { TestSpecID } from './id.js';
import { makeQueryString } from './url_query.js';

type Status = 'running' | 'pass' | 'warn' | 'fail';
export interface LiveTestRunResult {
  spec: string;
  cases: LiveTestCaseResult[];
}

export interface LiveTestCaseResult {
  name: string;
  params: ParamsSpec | null;
  status: Status;
  logs?: string[];
  timems: number;
}

export class Logger {
  readonly results: LiveTestRunResult[] = [];

  constructor() {}

  record(spec: TestSpecID): [GroupRecorder, LiveTestRunResult] {
    const cases: LiveTestCaseResult[] = [];
    const result: LiveTestRunResult = { spec: makeQueryString(spec), cases };
    this.results.push(result);
    return [new GroupRecorder(result), result];
  }

  asJSON(space?: number): string {
    return JSON.stringify({ version, results: this.results }, undefined, space);
  }
}

export class GroupRecorder {
  private test: LiveTestRunResult;

  constructor(test: LiveTestRunResult) {
    this.test = test;
  }

  record(name: string, params: ParamsSpec | null): [CaseRecorder, LiveTestCaseResult] {
    const result: LiveTestCaseResult = { name, params, status: 'running', timems: -1 };
    this.test.cases.push(result);
    return [new CaseRecorder(result), result];
  }
}

export class CaseRecorder {
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
    this.result.timems = endTime - this.startTime;
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
    let m = 'EXCEPTION';
    m += ' ' + getStackTrace(e);
    this.log(m);
  }
}
