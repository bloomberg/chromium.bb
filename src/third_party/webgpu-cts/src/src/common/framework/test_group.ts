import { Fixture, SkipTestCase } from './fixture.js';
import { TestCaseRecorder } from './logging/test_case_recorder.js';
import { CaseParams, CaseParamsIterable, extractPublicParams } from './params_utils.js';
import { kPathSeparator } from './query/separators.js';
import { stringifyPublicParams, stringifyPublicParamsUniquely } from './query/stringify_params.js';
import { validQueryPart } from './query/validQueryPart.js';
import { assert } from './util/util.js';

export type RunFn = (rec: TestCaseRecorder) => Promise<void>;

export interface TestCaseID {
  readonly test: readonly string[];
  readonly params: CaseParams;
}

export interface RunCase {
  readonly id: TestCaseID;
  run: RunFn;
}

// Interface for defining tests
export interface TestGroupBuilder<F extends Fixture> {
  test(name: string): TestBuilderWithName<F, never>;
}
export function makeTestGroup<F extends Fixture>(fixture: FixtureClass<F>): TestGroupBuilder<F> {
  return new TestGroup(fixture);
}

// Interfaces for running tests
export interface IterableTestGroup {
  iterate(): Iterable<IterableTest>;
  validate(): void;
}
export interface IterableTest {
  testPath: string[];
  description: string | undefined;
  iterate(): Iterable<RunCase>;
}

export function makeTestGroupForUnitTesting<F extends Fixture>(
  fixture: FixtureClass<F>
): TestGroup<F> {
  return new TestGroup(fixture);
}

type FixtureClass<F extends Fixture> = new (log: TestCaseRecorder, params: CaseParams) => F;
type TestFn<F extends Fixture, P extends {}> = (t: F & { params: P }) => Promise<void> | void;

class TestGroup<F extends Fixture> implements TestGroupBuilder<F> {
  private fixture: FixtureClass<F>;
  private seen: Set<string> = new Set();
  private tests: Array<TestBuilder<F, never>> = [];

  constructor(fixture: FixtureClass<F>) {
    this.fixture = fixture;
  }

  iterate(): Iterable<IterableTest> {
    return this.tests;
  }

  private checkName(name: string): void {
    assert(
      // Shouldn't happen due to the rule above. Just makes sure that treated
      // unencoded strings as encoded strings is OK.
      name === decodeURIComponent(name),
      `Not decodeURIComponent-idempotent: ${name} !== ${decodeURIComponent(name)}`
    );
    assert(!this.seen.has(name), `Duplicate test name: ${name}`);

    this.seen.add(name);
  }

  // TODO: This could take a fixture, too, to override the one for the group.
  test(name: string): TestBuilderWithName<F, never> {
    const testCreationStack = new Error(`Test created: ${name}`);

    this.checkName(name);

    const parts = name.split(kPathSeparator);
    for (const p of parts) {
      assert(validQueryPart.test(p), `Invalid test name part ${p}; must match ${validQueryPart}`);
    }

    const test = new TestBuilder<F, never>(parts, this.fixture, testCreationStack);
    this.tests.push(test);
    return test;
  }

  validate(): void {
    for (const test of this.tests) {
      test.validate();
    }
  }
}

interface TestBuilderWithName<F extends Fixture, P extends {}> extends TestBuilderWithParams<F, P> {
  desc(description: string): this;
  params<NewP extends {}>(specs: Iterable<NewP>): TestBuilderWithParams<F, NewP>;
}

interface TestBuilderWithParams<F extends Fixture, P extends {}> {
  fn(fn: TestFn<F, P>): void;
  unimplemented(): void;
}

class TestBuilder<F extends Fixture, P extends {}> {
  readonly testPath: string[];
  description: string | undefined;

  private readonly fixture: FixtureClass<F>;
  private testFn: TestFn<F, P> | undefined;
  private cases?: CaseParamsIterable = undefined;
  private testCreationStack: Error;

  constructor(testPath: string[], fixture: FixtureClass<F>, testCreationStack: Error) {
    this.testPath = testPath;
    this.fixture = fixture;
    this.testCreationStack = testCreationStack;
  }

  desc(description: string): this {
    this.description = description.trim();
    return this;
  }

  fn(fn: TestFn<F, P>): void {
    assert(this.testFn === undefined);
    this.testFn = fn;
  }

  unimplemented(): void {
    assert(this.testFn === undefined);
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

    if (this.cases === undefined) {
      return;
    }

    const seen = new Set<string>();
    for (const testcase of this.cases) {
      // stringifyPublicParams also checks for invalid params values
      const testcaseString = stringifyPublicParams(testcase);

      // A (hopefully) unique representation of a params value.
      const testcaseStringUnique = stringifyPublicParamsUniquely(testcase);
      assert(
        !seen.has(testcaseStringUnique),
        `Duplicate public test case params for test ${testPathString}: ${testcaseString}`
      );
      seen.add(testcaseStringUnique);
    }
  }

  params<NewP extends {}>(casesIterable: Iterable<NewP>): TestBuilderWithParams<F, NewP> {
    assert(this.cases === undefined, 'test case is already parameterized');
    this.cases = Array.from(casesIterable);

    return (this as unknown) as TestBuilderWithParams<F, NewP>;
  }

  *iterate(): IterableIterator<RunCase> {
    assert(this.testFn !== undefined, 'No test function (.fn()) for test');
    for (const params of this.cases || [{}]) {
      yield new RunCaseSpecific(this.testPath, params, this.fixture, this.testFn);
    }
  }
}

class RunCaseSpecific<F extends Fixture> implements RunCase {
  readonly id: TestCaseID;

  private readonly params: CaseParams | null;
  private readonly fixture: FixtureClass<F>;
  private readonly fn: TestFn<F, never>;

  constructor(
    testPath: string[],
    params: CaseParams,
    fixture: FixtureClass<F>,
    fn: TestFn<F, never>
  ) {
    this.id = { test: testPath, params: extractPublicParams(params) };
    this.params = params;
    this.fixture = fixture;
    this.fn = fn;
  }

  async run(rec: TestCaseRecorder): Promise<void> {
    rec.start();
    try {
      const inst = new this.fixture(rec, this.params || {});

      try {
        await inst.init();
        /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
        await this.fn(inst as any);
      } finally {
        // Runs as long as constructor succeeded, even if initialization or the test failed.
        await inst.finalize();
      }
    } catch (ex) {
      // There was an exception from constructor, init, test, or finalize.
      // An error from init or test may have been a SkipTestCase.
      // An error from finalize may have been an eventualAsyncExpectation failure
      // or unexpected validation/OOM error from the GPUDevice.
      rec.threw(ex);
    }
    rec.finish();
  }
}
