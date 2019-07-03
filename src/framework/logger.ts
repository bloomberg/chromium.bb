import { ParamsSpec } from './params/index.js';
import { getStackTrace, now } from './util.js';
import { version } from './version.js';
import { TestSpecID } from './id.js';
import { makeQueryString } from './url_query.js';

type Status = 'running' | 'pass' | 'warn' | 'fail';
interface TestLiveResult {
  spec: string;
  cases: TestCaseLiveResult[];
}

export interface TestCaseLiveResult {
  name: string;
  params: ParamsSpec | null;
  status: Status;
  logs?: string[];
  timems: number;
}

export class Logger {
  readonly results: TestLiveResult[] = [];

  constructor() {}

  record(spec: TestSpecID): [GroupRecorder, TestLiveResult] {
    const cases: TestCaseLiveResult[] = [];
    const result: TestLiveResult = { spec: makeQueryString(spec), cases };
    this.results.push(result);
    return [new GroupRecorder(result), result];
  }

  asJSON(space?: number): string {
    return JSON.stringify({ version, results: this.results }, undefined, space);
  }
}

export class GroupRecorder {
  private test: TestLiveResult;

  constructor(test: TestLiveResult) {
    this.test = test;
  }

  record(name: string, params: ParamsSpec | null): [CaseRecorder, TestCaseLiveResult] {
    const result: TestCaseLiveResult = { name, params, status: 'running', timems: -1 };
    this.test.cases.push(result);
    return [new CaseRecorder(result), result];
  }
}

export class CaseRecorder {
  private result: TestCaseLiveResult;
  private failed = false;
  private warned = false;
  private startTime = -1;
  private logs: string[] = [];

  constructor(result: TestCaseLiveResult) {
    this.result = result;
  }

  start() {
    this.startTime = now();
    this.logs = [];
    this.failed = false;
    this.warned = false;
  }

  finish() {
    if (this.startTime < 0) {
      throw new Error('finish() before start()');
    }
    const endTime = now();
    this.result.timems = endTime - this.startTime;
    this.result.status = this.failed ? 'fail' : this.warned ? 'warn' : 'pass';

    this.result.logs = this.logs;
  }

  log(msg: string) {
    this.logs.push(msg);
  }

  warn(msg?: string) {
    this.warned = true;
    let m = 'WARN';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  fail(msg?: string) {
    this.failed = true;
    let m = 'FAIL';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  threw(e: Error) {
    this.failed = true;
    let m = 'EXCEPTION';
    m += ' ' + getStackTrace(e);
    this.log(m);
  }
}
