import { TestCaseRecorder } from './logger.js';
import { ParamSpec } from './params/index.js';

// A Fixture is a class used to instantiate each test case at run time.
// A new instance of the Fixture is created for every single test case
// (i.e. every time the test function is run).
export class Fixture {
  params: ParamSpec;
  protected rec: TestCaseRecorder;
  private eventualExpectations: Array<Promise<unknown>> = [];
  private numOutstandingAsyncExpectations = 0;

  constructor(rec: TestCaseRecorder, params: ParamSpec) {
    this.rec = rec;
    this.params = params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  async init(): Promise<void> {}

  debug(msg: string): void {
    this.rec.debug(msg);
  }

  log(msg: string): void {
    this.rec.log(msg);
  }

  async finalize(): Promise<void> {
    if (this.numOutstandingAsyncExpectations !== 0) {
      throw new Error(
        'there were outstanding asynchronous expectations (e.g. shouldReject) at the end of the test'
      );
    }

    await Promise.all(this.eventualExpectations);
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

  protected async immediateAsyncExpectation<T>(fn: () => Promise<T>): Promise<T> {
    this.numOutstandingAsyncExpectations++;
    const ret = await fn();
    this.numOutstandingAsyncExpectations--;
    return ret;
  }

  protected eventualAsyncExpectation<T>(fn: () => Promise<T>): Promise<T> {
    const promise = fn();
    this.eventualExpectations.push(promise);
    return promise;
  }

  private expectErrorValue(expectedName: string, ex: unknown, m: string): void {
    if (!(ex instanceof Error)) {
      this.fail('THREW NON-ERROR');
      return;
    }
    const actualName = ex.name;
    if (actualName !== expectedName) {
      this.fail(`THREW ${actualName} INSTEAD OF ${expectedName}${m}`);
    } else {
      this.ok(`threw ${actualName}${m}`);
    }
  }

  shouldReject(expectedName: string, p: Promise<unknown>, msg?: string): void {
    this.eventualAsyncExpectation(async () => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        this.fail('DID NOT THROW' + m);
      } catch (ex) {
        this.expectErrorValue(expectedName, ex, m);
      }
    });
  }

  shouldThrow(expectedName: string, fn: () => void, msg?: string): void {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      this.fail('DID NOT THROW' + m);
    } catch (ex) {
      this.expectErrorValue(expectedName, ex, m);
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
