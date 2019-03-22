goog.module('mr.RetryTest');
goog.setTestOnly();

const Retry = goog.require('mr.Retry');
const UnitTestUtils = goog.require('mr.UnitTestUtils');

describe('mr.Retry', () => {
  const SENTINEL = 'SENTINEL';
  const ERROR_SENTINEL = new Error('ERROR_SENTINEL');
  const ABORT_SENTINEL = new Error('ABORT_SENTINEL');
  const RETRY_DELAY = 100;
  let numAttempts, mockClock;

  beforeEach(() => {
    numAttempts = 0;
    mockClock = UnitTestUtils.useMockClockAndPromises();
  });

  afterEach(() => {
    UnitTestUtils.restoreRealClockAndPromises();
  });

  // Make a function that fails a certain number of times before succeeding.
  const makeAttemptFunction = numFailures => {
    return () => {
      ++numAttempts;
      if (numAttempts <= numFailures) {
        return Promise.reject(ERROR_SENTINEL);
      } else {
        return Promise.resolve(SENTINEL);
      }
    };
  };

  it('returns a successful result', () => {
    const r = new Retry(makeAttemptFunction(0), RETRY_DELAY, Infinity);
    expect(mockClock.tickPromise(r.start(), 0)).toBe(SENTINEL);
  });


  it('can retry once', () => {
    const r = new Retry(makeAttemptFunction(1), RETRY_DELAY, Infinity);
    const p = r.start();
    mockClock.tick(0);
    expect(numAttempts).toBe(1);
    mockClock.tick(RETRY_DELAY * 10);
    expect(numAttempts).toBe(2);
    expect(mockClock.tickPromise(p, 0)).toBe(SENTINEL);
  });

  it('backs off exponentially', () => {
    const r = new Retry(makeAttemptFunction(3), RETRY_DELAY, Infinity);
    const p = r.start();
    mockClock.tick(0);
    expect(numAttempts).toBe(1);
    mockClock.tick(RETRY_DELAY);
    expect(numAttempts).toBe(2);
    mockClock.tick(2 * RETRY_DELAY);
    expect(numAttempts).toBe(3);
    mockClock.tick(4 * RETRY_DELAY);
    expect(numAttempts).toBe(4);
    expect(mockClock.tickPromise(p, 0)).toBe(SENTINEL);
  });

  it('eventually gives up', () => {
    const r = new Retry(makeAttemptFunction(3), RETRY_DELAY, 2);
    const p = r.start();
    mockClock.tick(0);
    expect(numAttempts).toBe(1);
    expect(mockClock.tickRejectingPromise(p, RETRY_DELAY)).toBe(ERROR_SENTINEL);
    expect(numAttempts).toBe(2);
  });

  it('can be aborted', () => {
    const r = new Retry(() => {
      ++numAttempts;
      if (numAttempts == 1) {
        return Promise.reject(ERROR_SENTINEL);
      }
      r.abort(ABORT_SENTINEL);
      return Promise.resolve(SENTINEL);  // discarded
    }, RETRY_DELAY, Infinity);
    const p = r.start();
    mockClock.tick(0);
    expect(numAttempts).toBe(1);
    expect(mockClock.tickRejectingPromise(p, RETRY_DELAY)).toBe(ABORT_SENTINEL);
    expect(numAttempts).toBe(2);
  });
});
