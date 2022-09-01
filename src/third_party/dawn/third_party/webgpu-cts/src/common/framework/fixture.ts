import { TestCaseRecorder } from '../internal/logging/test_case_recorder.js';
import { JSONWithUndefined } from '../internal/params_utils.js';
import { assert, unreachable } from '../util/util.js';

export class SkipTestCase extends Error {}
export class UnexpectedPassError extends Error {}

export { TestCaseRecorder } from '../internal/logging/test_case_recorder.js';

/** The fully-general type for params passed to a test function invocation. */
export type TestParams = {
  readonly [k: string]: JSONWithUndefined;
};

type DestroyableObject =
  | { destroy(): void }
  | { close(): void }
  | { getExtension(extensionName: 'WEBGL_lose_context'): WEBGL_lose_context };

export class SubcaseBatchState {
  private _params: TestParams;

  constructor(params: TestParams) {
    this._params = params;
  }

  /**
   * Returns the case parameters for this test fixture shared state. Subcase params
   * are not included.
   */
  get params(): TestParams {
    return this._params;
  }

  /**
   * Runs before the `.before()` function.
   * @internal MAINTENANCE_TODO: Make this not visible to test code?
   */
  async init() {}
  /**
   * Runs between the `.before()` function and the subcases.
   * @internal MAINTENANCE_TODO: Make this not visible to test code?
   */
  async postInit() {}
  /**
   * Runs after all subcases finish.
   * @internal MAINTENANCE_TODO: Make this not visible to test code?
   */
  async finalize() {}
}

/**
 * A Fixture is a class used to instantiate each test sub/case at run time.
 * A new instance of the Fixture is created for every single test subcase
 * (i.e. every time the test function is run).
 */
export class Fixture<S extends SubcaseBatchState = SubcaseBatchState> {
  private _params: unknown;
  private _sharedState: S;
  /**
   * Interface for recording logs and test status.
   *
   * @internal
   */
  protected rec: TestCaseRecorder;
  private eventualExpectations: Array<Promise<unknown>> = [];
  private numOutstandingAsyncExpectations = 0;
  private objectsToCleanUp: DestroyableObject[] = [];

  public static MakeSharedState(params: TestParams): SubcaseBatchState {
    return new SubcaseBatchState(params);
  }

  /** @internal */
  constructor(sharedState: S, rec: TestCaseRecorder, params: TestParams) {
    this._sharedState = sharedState;
    this.rec = rec;
    this._params = params;
  }

  /**
   * Returns the (case+subcase) parameters for this test function invocation.
   */
  get params(): unknown {
    return this._params;
  }

  /**
   * Gets the test fixture's shared state. This object is shared between subcases
   * within the same testcase.
   */
  get sharedState(): S {
    return this._sharedState;
  }

  /**
   * Override this to do additional pre-test-function work in a derived fixture.
   * This has to be a member function instead of an async `createFixture` function, because
   * we need to be able to ergonomically override it in subclasses.
   *
   * @internal MAINTENANCE_TODO: Make this not visible to test code?
   */
  async init(): Promise<void> {}

  /**
   * Override this to do additional post-test-function work in a derived fixture.
   *
   * Called even if init was unsuccessful.
   *
   * @internal MAINTENANCE_TODO: Make this not visible to test code?
   */
  async finalize(): Promise<void> {
    assert(
      this.numOutstandingAsyncExpectations === 0,
      'there were outstanding immediateAsyncExpectations (e.g. expectUncapturedError) at the end of the test'
    );

    // Loop to exhaust the eventualExpectations in case they chain off each other.
    while (this.eventualExpectations.length) {
      const p = this.eventualExpectations.shift()!;
      try {
        await p;
      } catch (ex) {
        this.rec.threw(ex);
      }
    }

    // And clean up any objects now that they're done being used.
    for (const o of this.objectsToCleanUp) {
      if ('getExtension' in o) {
        const WEBGL_lose_context = o.getExtension('WEBGL_lose_context');
        if (WEBGL_lose_context) WEBGL_lose_context.loseContext();
      } else if ('destroy' in o) {
        o.destroy();
      } else {
        o.close();
      }
    }
  }

  /**
   * Tracks an object to be cleaned up after the test finishes.
   *
   * MAINTENANCE_TODO: Use this in more places. (Will be easier once .destroy() is allowed on
   * invalid objects.)
   */
  trackForCleanup<T extends DestroyableObject>(o: T): T {
    this.objectsToCleanUp.push(o);
    return o;
  }

  /** Tracks an object, if it's destroyable, to be cleaned up after the test finishes. */
  tryTrackForCleanup<T>(o: T): T {
    if (typeof o === 'object' && o !== null) {
      if (
        'destroy' in o ||
        'close' in o ||
        o instanceof WebGLRenderingContext ||
        o instanceof WebGL2RenderingContext
      ) {
        this.objectsToCleanUp.push((o as unknown) as DestroyableObject);
      }
    }
    return o;
  }

  /** Log a debug message. */
  debug(msg: string): void {
    this.rec.debug(new Error(msg));
  }

  /** Throws an exception marking the subcase as skipped. */
  skip(msg: string): never {
    throw new SkipTestCase(msg);
  }

  /** Log a warning and increase the result status to "Warn". */
  warn(msg?: string): void {
    this.rec.warn(new Error(msg));
  }

  /** Log an error and increase the result status to "ExpectFailed". */
  fail(msg?: string): void {
    this.rec.expectationFailed(new Error(msg));
  }

  /**
   * Wraps an async function. Tracks its status to fail if the test tries to report a test status
   * before the async work has finished.
   */
  protected async immediateAsyncExpectation<T>(fn: () => Promise<T>): Promise<T> {
    this.numOutstandingAsyncExpectations++;
    const ret = await fn();
    this.numOutstandingAsyncExpectations--;
    return ret;
  }

  /**
   * Wraps an async function, passing it an `Error` object recording the original stack trace.
   * The async work will be implicitly waited upon before reporting a test status.
   */
  protected eventualAsyncExpectation<T>(fn: (niceStack: Error) => Promise<T>): void {
    const promise = fn(new Error());
    this.eventualExpectations.push(promise);
  }

  private expectErrorValue(expectedError: string | true, ex: unknown, niceStack: Error): void {
    if (!(ex instanceof Error)) {
      niceStack.message = `THREW non-error value, of type ${typeof ex}: ${ex}`;
      this.rec.expectationFailed(niceStack);
      return;
    }
    const actualName = ex.name;
    if (expectedError !== true && actualName !== expectedError) {
      niceStack.message = `THREW ${actualName}, instead of ${expectedError}: ${ex}`;
      this.rec.expectationFailed(niceStack);
    } else {
      niceStack.message = `OK: threw ${actualName}: ${ex.message}`;
      this.rec.debug(niceStack);
    }
  }

  /** Expect that the provided promise resolves (fulfills). */
  shouldResolve(p: Promise<unknown>, msg?: string): void {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'resolved as expected' + m;
      } catch (ex) {
        niceStack.message = `REJECTED${m}`;
        if (ex instanceof Error) {
          niceStack.message += '\n' + ex.message;
        }
        this.rec.expectationFailed(niceStack);
      }
    });
  }

  /** Expect that the provided promise rejects, with the provided exception name. */
  shouldReject(expectedName: string, p: Promise<unknown>, msg?: string): void {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'DID NOT REJECT' + m;
        this.rec.expectationFailed(niceStack);
      } catch (ex) {
        niceStack.message = 'rejected as expected' + m;
        this.expectErrorValue(expectedName, ex, niceStack);
      }
    });
  }

  /**
   * Expect that the provided function throws.
   * If an `expectedName` is provided, expect that the throw exception has that name.
   */
  shouldThrow(expectedError: string | boolean, fn: () => void, msg?: string): void {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      if (expectedError === false) {
        this.rec.debug(new Error('did not throw, as expected' + m));
      } else {
        this.rec.expectationFailed(new Error('unexpectedly did not throw' + m));
      }
    } catch (ex) {
      if (expectedError === false) {
        this.rec.expectationFailed(new Error('threw unexpectedly' + m));
      } else {
        this.expectErrorValue(expectedError, ex, new Error(m));
      }
    }
  }

  /** Expect that a condition is true. */
  expect(cond: boolean, msg?: string): boolean {
    if (cond) {
      const m = msg ? ': ' + msg : '';
      this.rec.debug(new Error('expect OK' + m));
    } else {
      this.rec.expectationFailed(new Error(msg));
    }
    return cond;
  }

  /**
   * If the argument is an `Error`, fail (or warn). If it's `undefined`, no-op.
   * If the argument is an array, apply the above behavior on each of elements.
   */
  expectOK(
    error: Error | undefined | (Error | undefined)[],
    { mode = 'fail', niceStack }: { mode?: 'fail' | 'warn'; niceStack?: Error } = {}
  ): void {
    const handleError = (error: Error | undefined) => {
      if (error instanceof Error) {
        if (niceStack) {
          error.stack = niceStack.stack;
        }
        if (mode === 'fail') {
          this.rec.expectationFailed(error);
        } else if (mode === 'warn') {
          this.rec.warn(error);
        } else {
          unreachable();
        }
      }
    };

    if (Array.isArray(error)) {
      for (const e of error) {
        handleError(e);
      }
    } else {
      handleError(error);
    }
  }

  eventualExpectOK(
    error: Promise<Error | undefined | (Error | undefined)[]>,
    { mode = 'fail' }: { mode?: 'fail' | 'warn' } = {}
  ) {
    this.eventualAsyncExpectation(async niceStack => {
      this.expectOK(await error, { mode, niceStack });
    });
  }
}
