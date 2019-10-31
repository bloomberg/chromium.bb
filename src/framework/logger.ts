import { SkipTestCase } from './fixture.js';
import { TestSpecID } from './id.js';
import { ParamSpec } from './params/index.js';
import { makeQueryString } from './url_query.js';
import { extractPublicParams } from './url_query.js';
import { getStackTrace, now } from './util/index.js';
import { version } from './version.js';

type Status = 'running' | 'pass' | 'skip' | 'warn' | 'fail';
export interface LiveTestSpecResult {
  readonly spec: string;
  readonly cases: LiveTestCaseResult[];
}

export interface LiveTestCaseResult {
  readonly test: string;
  readonly params: ParamSpec | null;
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

  record(test: string, params: ParamSpec | null): [TestCaseRecorder, LiveTestCaseResult] {
    const result: LiveTestCaseResult = {
      test,
      params: params ? extractPublicParams(params) : null,
      status: 'running',
      timems: -1,
    };
    this.result.cases.push(result);
    return [new TestCaseRecorder(result), result];
  }
}

enum PassState {
  pass = 0,
  skip = 1,
  warn = 2,
  fail = 3,
}

export class TestCaseRecorder {
  private result: LiveTestCaseResult;
  private state = PassState.pass;
  private startTime = -1;
  private logs: string[] = [];
  private debugging = false;

  constructor(result: LiveTestCaseResult) {
    this.result = result;
  }

  start(debug: boolean = false): void {
    this.startTime = now();
    this.logs = [];
    this.state = PassState.pass;
    this.debugging = debug;
  }

  finish(): void {
    if (this.startTime < 0) {
      throw new Error('finish() before start()');
    }
    const endTime = now();
    // Round to next microsecond to avoid storing useless .xxxx00000000000002 in results.
    this.result.timems = Math.ceil((endTime - this.startTime) * 1000) / 1000;
    this.result.status = PassState[this.state] as Status;

    this.result.logs = this.logs;
    this.debugging = false;
  }

  debug(msg: string): void {
    if (!this.debugging) {
      return;
    }
    this.log('DEBUG: ' + msg);
  }

  log(msg: string): void {
    this.logs.push(msg);
  }

  warn(msg?: string): void {
    this.setState(PassState.warn);
    let m = 'WARN';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  fail(msg?: string): void {
    this.setState(PassState.fail);
    let m = 'FAIL';
    if (msg) {
      m += ': ' + msg;
    }
    m += ' ' + getStackTrace(new Error());
    this.log(m);
  }

  skipped(ex: SkipTestCase): void {
    this.setState(PassState.skip);
    const m = 'SKIPPED: ' + getStackTrace(ex);
    this.log(m);
  }

  threw(ex: Error): void {
    if (ex instanceof SkipTestCase) {
      this.skipped(ex);
      return;
    }
    this.setState(PassState.fail);
    this.log('EXCEPTION: ' + ex.name + ': ' + ex.message + '\n' + getStackTrace(ex));
  }

  private setState(state: PassState): void {
    this.state = Math.max(this.state, state);
  }
}
