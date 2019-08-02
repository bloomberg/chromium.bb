import { TestCaseRecorder } from './logger.js';
import { ParamsAny } from './params/index.js';

// A Fixture is a class used to instantiate each test case at run time.
// A new instance of the Fixture is created for every single test case
// (i.e. every time the test function is run).
export class Fixture {
  params: ParamsAny;
  protected rec: TestCaseRecorder;
  private numOutstandingAsyncExpectations = 0;

  constructor(rec: TestCaseRecorder, params: ParamsAny) {
    this.rec = rec;
    this.params = params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  async init(): Promise<void> {}

  log(msg: string): void {
    this.rec.log(msg);
  }

  finalize(): void {
    if (this.numOutstandingAsyncExpectations !== 0) {
      throw new Error(
        'there were outstanding asynchronous expectations (e.g. shouldReject) at the end of the test'
      );
    }
  }

  warn(msg?: string): void {
    this.rec.warn(msg);
  }

  fail(msg?: string): void {
    this.rec.fail(msg);
  }

  ok(msg?: string): void {
    const m = msg ? ': ' + msg : '';
    this.log('OK' + m);
  }

  protected async asyncExpectation(fn: () => Promise<void>): Promise<void> {
    this.numOutstandingAsyncExpectations++;
    await fn();
    this.numOutstandingAsyncExpectations--;
  }

  async shouldReject(p: Promise<unknown>, exceptionType?: string, msg?: string): Promise<void> {
    this.asyncExpectation(async () => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        this.fail('DID NOT THROW' + m);
      } catch (ex) {
        const actualType = typeof ex;
        if (exceptionType !== undefined && actualType === exceptionType) {
          this.fail(`THREW ${actualType} INSTEAD OF ${exceptionType}${m}`);
        } else {
          this.ok(`threw ${actualType}{m}`);
        }
      }
    });
  }

  shouldThrow(fn: () => void, exceptionType?: string, msg?: string): void {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      const actualType = typeof ex;
      if (exceptionType !== undefined && actualType === exceptionType) {
        this.fail(`THREW ${actualType} INSTEAD OF ${exceptionType}${m}`);
      } else {
        this.ok(`threw ${actualType}{m}`);
      }
    }
  }

  expect(cond: boolean, msg?: string): boolean {
    if (cond) {
      this.ok(msg);
    } else {
      this.rec.fail(msg);
    }
    return cond;
  }
}
