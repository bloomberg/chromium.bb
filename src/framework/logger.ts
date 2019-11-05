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
  logs?: LogMessageWithStack[];
  timems: number;
}

class LogMessageWithStack extends Error {
  constructor(name: string, ex: Error) {
    super(ex.message);

    this.name = name;
    this.stack = ex.stack;
  }

  toJSON(): string {
    let m = this.name;
    if (this.message) {
      m += ': ' + this.message;
    }
    m += '\n' + getStackTrace(this);
    return m;
  }
}

class LogMessageWithoutStack extends LogMessageWithStack {
  toJSON(): string {
    return this.message;
  }
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
  private logs: LogMessageWithStack[] = [];
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

  debug(ex: Error): void {
    if (!this.debugging) {
      return;
    }
    this.logs.push(new LogMessageWithoutStack('DEBUG', ex));
  }

  warn(ex: Error): void {
    this.setState(PassState.warn);
    this.logs.push(new LogMessageWithStack('WARN', ex));
  }

  fail(ex: Error): void {
    this.setState(PassState.fail);
    this.logs.push(new LogMessageWithStack('FAIL', ex));
  }

  skipped(ex: SkipTestCase): void {
    this.setState(PassState.skip);
    this.logs.push(new LogMessageWithStack('SKIP', ex));
  }

  threw(ex: Error): void {
    if (ex instanceof SkipTestCase) {
      this.skipped(ex);
      return;
    }

    this.setState(PassState.fail);
    this.logs.push(new LogMessageWithStack('EXCEPTION', ex));
  }

  private setState(state: PassState): void {
    this.state = Math.max(this.state, state);
  }
}
