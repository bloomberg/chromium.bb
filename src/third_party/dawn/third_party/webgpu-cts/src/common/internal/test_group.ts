import {
  Fixture,
  SubcaseBatchState,
  SkipTestCase,
  TestParams,
  UnexpectedPassError,
} from '../framework/fixture.js';
import {
  CaseParamsBuilder,
  builderIterateCasesWithSubcases,
  kUnitCaseParamsBuilder,
  ParamsBuilderBase,
  SubcaseParamsBuilder,
} from '../framework/params_builder.js';
import { Expectation } from '../internal/logging/result.js';
import { TestCaseRecorder } from '../internal/logging/test_case_recorder.js';
import { extractPublicParams, Merged, mergeParams } from '../internal/params_utils.js';
import { compareQueries, Ordering } from '../internal/query/compare.js';
import { TestQuerySingleCase, TestQueryWithExpectation } from '../internal/query/query.js';
import { kPathSeparator } from '../internal/query/separators.js';
import {
  stringifyPublicParams,
  stringifyPublicParamsUniquely,
} from '../internal/query/stringify_params.js';
import { validQueryPart } from '../internal/query/validQueryPart.js';
import { assert, unreachable } from '../util/util.js';

export type RunFn = (
  rec: TestCaseRecorder,
  expectations?: TestQueryWithExpectation[]
) => Promise<void>;

export interface TestCaseID {
  readonly test: readonly string[];
  readonly params: TestParams;
}

export interface RunCase {
  readonly id: TestCaseID;
  readonly isUnimplemented: boolean;
  run(
    rec: TestCaseRecorder,
    selfQuery: TestQuerySingleCase,
    expectations: TestQueryWithExpectation[]
  ): Promise<void>;
}

// Interface for defining tests
export interface TestGroupBuilder<S extends SubcaseBatchState, F extends Fixture<S>> {
  test(name: string): TestBuilderWithName<S, F>;
}
export function makeTestGroup<S extends SubcaseBatchState, F extends Fixture<S>>(
  fixture: FixtureClass<S, F>
): TestGroupBuilder<S, F> {
  return new TestGroup((fixture as unknown) as FixtureClass);
}

// Interfaces for running tests
export interface IterableTestGroup {
  iterate(): Iterable<IterableTest>;
  validate(): void;
}
export interface IterableTest {
  testPath: string[];
  description: string | undefined;
  readonly testCreationStack: Error;
  iterate(): Iterable<RunCase>;
}

export function makeTestGroupForUnitTesting<F extends Fixture>(
  fixture: FixtureClass<SubcaseBatchState, F>
): TestGroup<SubcaseBatchState, F> {
  return new TestGroup(fixture);
}

export type FixtureClass<
  S extends SubcaseBatchState = SubcaseBatchState,
  F extends Fixture<S> = Fixture<S>
> = {
  new (sharedState: S, log: TestCaseRecorder, params: TestParams): F;
  MakeSharedState(params: TestParams): S;
};
type TestFn<F extends Fixture, P extends {}> = (t: F & { params: P }) => Promise<void> | void;
type BeforeAllSubcasesFn<S extends SubcaseBatchState, P extends {}> = (
  s: S & { params: P }
) => Promise<void> | void;

export class TestGroup<S extends SubcaseBatchState, F extends Fixture<S>>
  implements TestGroupBuilder<S, F> {
  private fixture: FixtureClass;
  private seen: Set<string> = new Set();
  private tests: Array<TestBuilder<S, F>> = [];

  constructor(fixture: FixtureClass) {
    this.fixture = fixture;
  }

  iterate(): Iterable<IterableTest> {
    return this.tests;
  }

  private checkName(name: string): void {
    assert(
      // Shouldn't happen due to the rule above. Just makes sure that treating
      // unencoded strings as encoded strings is OK.
      name === decodeURIComponent(name),
      `Not decodeURIComponent-idempotent: ${name} !== ${decodeURIComponent(name)}`
    );
    assert(!this.seen.has(name), `Duplicate test name: ${name}`);

    this.seen.add(name);
  }

  test(name: string): TestBuilderWithName<S, F> {
    const testCreationStack = new Error(`Test created: ${name}`);

    this.checkName(name);

    const parts = name.split(kPathSeparator);
    for (const p of parts) {
      assert(validQueryPart.test(p), `Invalid test name part ${p}; must match ${validQueryPart}`);
    }

    const test = new TestBuilder(parts, this.fixture, testCreationStack);
    this.tests.push(test);
    return (test as unknown) as TestBuilderWithName<S, F>;
  }

  validate(): void {
    for (const test of this.tests) {
      test.validate();
    }
  }
}

interface TestBuilderWithName<S extends SubcaseBatchState, F extends Fixture<S>>
  extends TestBuilderWithParams<S, F, {}, {}> {
  desc(description: string): this;
  /**
   * A noop function to associate a test with the relevant part of the specification.
   *
   * @param url a link to the spec where test is extracted from.
   */
  specURL(url: string): this;
  /**
   * Parameterize the test, generating multiple cases, each possibly having subcases.
   *
   * The `unit` value passed to the `cases` callback is an immutable constant
   * `CaseParamsBuilder<{}>` representing the "unit" builder `[ {} ]`,
   * provided for convenience. The non-callback overload can be used if `unit` is not needed.
   */
  params<CaseP extends {}, SubcaseP extends {}>(
    cases: (unit: CaseParamsBuilder<{}>) => ParamsBuilderBase<CaseP, SubcaseP>
  ): TestBuilderWithParams<S, F, CaseP, SubcaseP>;
  /**
   * Parameterize the test, generating multiple cases, each possibly having subcases.
   *
   * Use the callback overload of this method if a "unit" builder is needed.
   */
  params<CaseP extends {}, SubcaseP extends {}>(
    cases: ParamsBuilderBase<CaseP, SubcaseP>
  ): TestBuilderWithParams<S, F, CaseP, SubcaseP>;

  /**
   * Parameterize the test, generating multiple cases, without subcases.
   */
  paramsSimple<P extends {}>(cases: Iterable<P>): TestBuilderWithParams<S, F, P, {}>;

  /**
   * Parameterize the test, generating one case with multiple subcases.
   */
  paramsSubcasesOnly<P extends {}>(subcases: Iterable<P>): TestBuilderWithParams<S, F, {}, P>;
  /**
   * Parameterize the test, generating one case with multiple subcases.
   *
   * The `unit` value passed to the `subcases` callback is an immutable constant
   * `SubcaseParamsBuilder<{}>`, with one empty case `{}` and one empty subcase `{}`.
   */
  paramsSubcasesOnly<P extends {}>(
    subcases: (unit: SubcaseParamsBuilder<{}, {}>) => SubcaseParamsBuilder<{}, P>
  ): TestBuilderWithParams<S, F, {}, P>;
}

interface TestBuilderWithParams<
  S extends SubcaseBatchState,
  F extends Fixture<S>,
  CaseP extends {},
  SubcaseP extends {}
> {
  /**
   * Limit subcases to a maximum number of per testcase.
   * @param b the maximum number of subcases per testcase.
   *
   * If the number of subcases exceeds `b`, add an internal
   * numeric, incrementing `batch__` param to split subcases
   * into groups of at most `b` subcases.
   */
  batch(b: number): this;
  /**
   * Run a function on shared subcase batch state before each
   * batch of subcases.
   * @param fn the function to run. It is called with the test
   * fixture's shared subcase batch state.
   *
   * Generally, this function should be careful to avoid mutating
   * any state on the shared subcase batch state which could result
   * in unexpected order-dependent test behavior.
   */
  beforeAllSubcases(fn: BeforeAllSubcasesFn<S, CaseP>): this;
  /**
   * Set the test function.
   * @param fn the test function.
   */
  fn(fn: TestFn<F, Merged<CaseP, SubcaseP>>): void;
  /**
   * Mark the test as unimplemented.
   */
  unimplemented(): void;
}

class TestBuilder<S extends SubcaseBatchState, F extends Fixture> {
  readonly testPath: string[];
  isUnimplemented: boolean;
  description: string | undefined;
  readonly testCreationStack: Error;

  private readonly fixture: FixtureClass;
  private testFn: TestFn<Fixture, {}> | undefined;
  private beforeFn: BeforeAllSubcasesFn<SubcaseBatchState, {}> | undefined;
  private testCases?: ParamsBuilderBase<{}, {}> = undefined;
  private batchSize: number = 0;

  constructor(testPath: string[], fixture: FixtureClass, testCreationStack: Error) {
    this.testPath = testPath;
    this.isUnimplemented = false;
    this.fixture = fixture;
    this.testCreationStack = testCreationStack;
  }

  desc(description: string): this {
    this.description = description.trim();
    return this;
  }

  specURL(url: string): this {
    return this;
  }

  beforeAllSubcases(fn: BeforeAllSubcasesFn<SubcaseBatchState, {}>): this {
    assert(this.beforeFn === undefined);
    this.beforeFn = fn;
    return this;
  }

  fn(fn: TestFn<Fixture, {}>): void {
    // eslint-disable-next-line no-warning-comments
    // MAINTENANCE_TODO: add "TODO" if there's no description? (and make sure it only ends up on
    // actual tests, not on test parents in the tree, which is what happens if you do it here, not
    // sure why)
    assert(this.testFn === undefined);
    this.testFn = fn;
  }

  batch(b: number): this {
    this.batchSize = b;
    return this;
  }

  unimplemented(): void {
    assert(this.testFn === undefined);

    this.description =
      (this.description ? this.description + '\n\n' : '') + 'TODO: .unimplemented()';
    this.isUnimplemented = true;

    this.testFn = () => {
      throw new SkipTestCase('test unimplemented');
    };
  }

  validate(): void {
    const testPathString = this.testPath.join(kPathSeparator);
    assert(this.testFn !== undefined, () => {
      let s = `Test is missing .fn(): ${testPathString}`;
      if (this.testCreationStack.stack) {
        s += `\n-> test created at:\n${this.testCreationStack.stack}`;
      }
      return s;
    });

    if (this.testCases === undefined) {
      return;
    }

    const seen = new Set<string>();
    for (const [caseParams, subcases] of builderIterateCasesWithSubcases(this.testCases)) {
      for (const subcaseParams of subcases ?? [{}]) {
        const params = mergeParams(caseParams, subcaseParams);
        assert(this.batchSize === 0 || !('batch__' in params));

        // stringifyPublicParams also checks for invalid params values
        const testcaseString = stringifyPublicParams(params);

        // A (hopefully) unique representation of a params value.
        const testcaseStringUnique = stringifyPublicParamsUniquely(params);
        assert(
          !seen.has(testcaseStringUnique),
          `Duplicate public test case params for test ${testPathString}: ${testcaseString}`
        );
        seen.add(testcaseStringUnique);
      }
    }
  }

  params(
    cases: ((unit: CaseParamsBuilder<{}>) => ParamsBuilderBase<{}, {}>) | ParamsBuilderBase<{}, {}>
  ): TestBuilder<S, F> {
    assert(this.testCases === undefined, 'test case is already parameterized');
    if (cases instanceof Function) {
      this.testCases = cases(kUnitCaseParamsBuilder);
    } else {
      this.testCases = cases;
    }
    return this;
  }

  paramsSimple(cases: Iterable<{}>): TestBuilder<S, F> {
    assert(this.testCases === undefined, 'test case is already parameterized');
    this.testCases = kUnitCaseParamsBuilder.combineWithParams(cases);
    return this;
  }

  paramsSubcasesOnly(
    subcases: Iterable<{}> | ((unit: SubcaseParamsBuilder<{}, {}>) => SubcaseParamsBuilder<{}, {}>)
  ): TestBuilder<S, F> {
    if (subcases instanceof Function) {
      return this.params(subcases(kUnitCaseParamsBuilder.beginSubcases()));
    } else {
      return this.params(kUnitCaseParamsBuilder.beginSubcases().combineWithParams(subcases));
    }
  }

  *iterate(): IterableIterator<RunCase> {
    assert(this.testFn !== undefined, 'No test function (.fn()) for test');
    this.testCases ??= kUnitCaseParamsBuilder;
    for (const [caseParams, subcases] of builderIterateCasesWithSubcases(this.testCases)) {
      if (this.batchSize === 0 || subcases === undefined) {
        yield new RunCaseSpecific(
          this.testPath,
          caseParams,
          this.isUnimplemented,
          subcases,
          this.fixture,
          this.testFn,
          this.beforeFn,
          this.testCreationStack
        );
      } else {
        const subcaseArray = Array.from(subcases);
        if (subcaseArray.length <= this.batchSize) {
          yield new RunCaseSpecific(
            this.testPath,
            caseParams,
            this.isUnimplemented,
            subcaseArray,
            this.fixture,
            this.testFn,
            this.beforeFn,
            this.testCreationStack
          );
        } else {
          for (let i = 0; i < subcaseArray.length; i = i + this.batchSize) {
            yield new RunCaseSpecific(
              this.testPath,
              { ...caseParams, batch__: i / this.batchSize },
              this.isUnimplemented,
              subcaseArray.slice(i, Math.min(subcaseArray.length, i + this.batchSize)),
              this.fixture,
              this.testFn,
              this.beforeFn,
              this.testCreationStack
            );
          }
        }
      }
    }
  }
}

class RunCaseSpecific implements RunCase {
  readonly id: TestCaseID;
  readonly isUnimplemented: boolean;

  private readonly params: {};
  private readonly subcases: Iterable<{}> | undefined;
  private readonly fixture: FixtureClass;
  private readonly fn: TestFn<Fixture, {}>;
  private readonly beforeFn?: BeforeAllSubcasesFn<SubcaseBatchState, {}>;
  private readonly testCreationStack: Error;

  constructor(
    testPath: string[],
    params: {},
    isUnimplemented: boolean,
    subcases: Iterable<{}> | undefined,
    fixture: FixtureClass,
    fn: TestFn<Fixture, {}>,
    beforeFn: BeforeAllSubcasesFn<SubcaseBatchState, {}> | undefined,
    testCreationStack: Error
  ) {
    this.id = { test: testPath, params: extractPublicParams(params) };
    this.isUnimplemented = isUnimplemented;
    this.params = params;
    this.subcases = subcases;
    this.fixture = fixture;
    this.fn = fn;
    this.beforeFn = beforeFn;
    this.testCreationStack = testCreationStack;
  }

  async runTest(
    rec: TestCaseRecorder,
    sharedState: SubcaseBatchState,
    params: TestParams,
    throwSkip: boolean,
    expectedStatus: Expectation
  ): Promise<void> {
    try {
      rec.beginSubCase();
      if (expectedStatus === 'skip') {
        throw new SkipTestCase('Skipped by expectations');
      }

      const inst = new this.fixture(sharedState, rec, params);
      try {
        await inst.init();
        await this.fn(inst as Fixture & { params: {} });
      } finally {
        // Runs as long as constructor succeeded, even if initialization or the test failed.
        await inst.finalize();
      }
    } catch (ex) {
      // There was an exception from constructor, init, test, or finalize.
      // An error from init or test may have been a SkipTestCase.
      // An error from finalize may have been an eventualAsyncExpectation failure
      // or unexpected validation/OOM error from the GPUDevice.
      if (throwSkip && ex instanceof SkipTestCase) {
        throw ex;
      }
      rec.threw(ex);
    } finally {
      try {
        rec.endSubCase(expectedStatus);
      } catch (ex) {
        assert(ex instanceof UnexpectedPassError);
        ex.message = `Testcase passed unexpectedly.`;
        ex.stack = this.testCreationStack.stack;
        rec.warn(ex);
      }
    }
  }

  async run(
    rec: TestCaseRecorder,
    selfQuery: TestQuerySingleCase,
    expectations: TestQueryWithExpectation[]
  ): Promise<void> {
    const getExpectedStatus = (selfQueryWithSubParams: TestQuerySingleCase) => {
      let didSeeFail = false;
      for (const exp of expectations) {
        const ordering = compareQueries(exp.query, selfQueryWithSubParams);
        if (ordering === Ordering.Unordered || ordering === Ordering.StrictSubset) {
          continue;
        }

        switch (exp.expectation) {
          // Skip takes precedence. If there is any expectation indicating a skip,
          // signal it immediately.
          case 'skip':
            return 'skip';
          case 'fail':
            // Otherwise, indicate that we might expect a failure.
            didSeeFail = true;
            break;
          default:
            unreachable();
        }
      }
      return didSeeFail ? 'fail' : 'pass';
    };

    try {
      rec.start();
      const sharedState = this.fixture.MakeSharedState(this.params);
      try {
        await sharedState.init();
        if (this.beforeFn) {
          await this.beforeFn(sharedState);
        }
        await sharedState.postInit();

        let allPreviousSubcasesFinalizedPromise: Promise<void> = Promise.resolve();
        if (this.subcases) {
          let totalCount = 0;
          let skipCount = 0;

          // Maximum number of subcases in flight. If there are too many in flight,
          // starting the next subcase will register `resolvePromiseBlockingSubcase`
          // and wait until `subcaseFinishedCallback` is called.
          const kMaxSubcasesInFlight = 500;
          let subcasesInFlight = 0;
          let resolvePromiseBlockingSubcase: (() => void) | undefined = undefined;
          const subcaseFinishedCallback = () => {
            subcasesInFlight -= 1;
            // If there is any subcase waiting on a previous subcase to finish,
            // unblock it now, and clear the resolve callback.
            if (resolvePromiseBlockingSubcase) {
              resolvePromiseBlockingSubcase();
              resolvePromiseBlockingSubcase = undefined;
            }
          };

          for (const subParams of this.subcases) {
            // Make a recorder that will defer all calls until `allPreviousSubcasesFinalizedPromise`
            // resolves. Waiting on `allPreviousSubcasesFinalizedPromise` ensures that
            // logs from all the previous subcases have been flushed before flushing new logs.
            const subRec = new Proxy(rec, {
              get: (target, k: keyof TestCaseRecorder) => {
                const prop = TestCaseRecorder.prototype[k];
                if (typeof prop === 'function') {
                  return function (...args: Parameters<typeof prop>) {
                    void allPreviousSubcasesFinalizedPromise.then(() => {
                      // eslint-disable-next-line @typescript-eslint/no-explicit-any
                      const rv = (prop as any).apply(target, args);
                      // Because this proxy executes functions in a deferred manner,
                      // it should never be used for functions that need to return a value.
                      assert(rv === undefined);
                    });
                  };
                }
                return prop;
              },
            });

            subRec.info(new Error('subcase: ' + stringifyPublicParams(subParams)));

            const params = mergeParams(this.params, subParams);
            const subcaseQuery = new TestQuerySingleCase(
              selfQuery.suite,
              selfQuery.filePathParts,
              selfQuery.testPathParts,
              params
            );

            // Limit the maximum number of subcases in flight.
            if (subcasesInFlight >= kMaxSubcasesInFlight) {
              await new Promise<void>(resolve => {
                // There should only be one subcase waiting at a time.
                assert(resolvePromiseBlockingSubcase === undefined);
                resolvePromiseBlockingSubcase = resolve;
              });
            }

            subcasesInFlight += 1;
            // Runs async without waiting so that subsequent subcases can start.
            // All finalization steps will be waited on at the end of the testcase.
            const finalizePromise = this.runTest(
              subRec,
              sharedState,
              params,
              /* throwSkip */ true,
              getExpectedStatus(subcaseQuery)
            )
              .catch(ex => {
                if (ex instanceof SkipTestCase) {
                  // Convert SkipTestCase to info messages
                  ex.message = 'subcase skipped: ' + ex.message;
                  subRec.info(ex);
                  ++skipCount;
                } else {
                  // Since we are catching all error inside runTest(), this should never happen
                  subRec.threw(ex);
                }
              })
              .finally(subcaseFinishedCallback);

            allPreviousSubcasesFinalizedPromise = allPreviousSubcasesFinalizedPromise.then(
              () => finalizePromise
            );
            ++totalCount;
          }

          // Wait for all subcases to finalize and report their results.
          await allPreviousSubcasesFinalizedPromise;

          if (skipCount === totalCount) {
            rec.skipped(new SkipTestCase('all subcases were skipped'));
          }
        } else {
          await this.runTest(
            rec,
            sharedState,
            this.params,
            /* throwSkip */ false,
            getExpectedStatus(selfQuery)
          );
        }
      } finally {
        // Runs as long as the shared state constructor succeeded, even if initialization or a test failed.
        await sharedState.finalize();
      }
    } catch (ex) {
      // There was an exception from sharedState/fixture constructor, init, beforeFn, or test.
      // An error from beforeFn may have been SkipTestCase.
      // An error from finalize may have been an eventualAsyncExpectation failure
      // or unexpected validation/OOM error from the GPUDevice.
      rec.threw(ex);
    } finally {
      rec.finish();
    }
  }
}
