import { allowedTestNameCharacters } from './allowed_characters.js';
import { Fixture } from './fixture.js';
import { TestCaseID } from './id.js';
import { LiveTestCaseResult, TestCaseRecorder, TestSpecRecorder } from './logger.js';
import { ParamSpecIterable, ParamsAny, paramsEquals } from './params/index.js';

export interface RunCase {
  readonly id: TestCaseID;
  run(): Promise<LiveTestCaseResult>;
}

export interface RunCaseIterable {
  iterate(rec: TestSpecRecorder): Iterable<RunCase>;
}

type FixtureClass<F extends Fixture> = new (log: TestCaseRecorder, params: ParamsAny) => F;
type TestFn<F extends Fixture> = (t: F) => Promise<void> | void;

const validNames = new RegExp('^[' + allowedTestNameCharacters + ']+$');

export class TestGroup<F extends Fixture> implements RunCaseIterable {
  private fixture: FixtureClass<F>;
  private seen: Set<string> = new Set();
  private tests: Array<Test<F>> = [];

  constructor(fixture: FixtureClass<F>) {
    this.fixture = fixture;
  }

  *iterate(log: TestSpecRecorder): Iterable<RunCase> {
    for (const test of this.tests) {
      yield* test.iterate(log);
    }
  }

  private checkName(name: string): void {
    if (!validNames.test(name)) {
      throw new Error(`Invalid test name ${name}; must match [${validNames}]+`);
    }
    if (name !== decodeURIComponent(name)) {
      // Shouldn't happen due to the rule above. Just makes sure that treated
      // unencoded strings as encoded strings is OK.
      throw new Error(`Not decodeURIComponent-idempotent: ${name} !== ${decodeURIComponent(name)}`);
    }

    if (this.seen.has(name)) {
      throw new Error('Duplicate test name');
    }
    this.seen.add(name);
  }

  // TODO: This could take a fixture, too, to override the one for the group.
  test(name: string, fn: TestFn<F>): Test<F> {
    this.checkName(name);

    const test = new Test<F>(name, this.fixture, fn);
    this.tests.push(test);
    return test;
  }
}

// This test is created when it's inserted, but may be parameterized afterward (.params()).
class Test<F extends Fixture> {
  readonly name: string;
  readonly fixture: FixtureClass<F>;
  readonly fn: TestFn<F>;
  private cases: ParamSpecIterable | null = null;

  constructor(name: string, fixture: FixtureClass<F>, fn: TestFn<F>) {
    this.name = name;
    this.fixture = fixture;
    this.fn = fn;
  }

  params(specs: ParamSpecIterable): void {
    if (this.cases !== null) {
      throw new Error('test case is already parameterized');
    }
    const cases = Array.from(specs);
    const seen: ParamsAny[] = [];
    // This is n^2.
    for (const spec of cases) {
      if (seen.some(x => paramsEquals(x, spec))) {
        throw new Error('Duplicate test case params');
      }
      seen.push(spec);
    }
    this.cases = cases;
  }

  *iterate(rec: TestSpecRecorder): IterableIterator<RunCase> {
    for (const params of this.cases || [null]) {
      yield new RunCaseSpecific(rec, { test: this.name, params }, this.fixture, this.fn);
    }
  }
}

class RunCaseSpecific<F extends Fixture> implements RunCase {
  readonly id: TestCaseID;
  private readonly recorder: TestSpecRecorder;
  private readonly fixture: FixtureClass<F>;
  private readonly fn: TestFn<F>;

  constructor(recorder: TestSpecRecorder, id: TestCaseID, fixture: FixtureClass<F>, fn: TestFn<F>) {
    this.recorder = recorder;
    this.id = id;
    this.fixture = fixture;
    this.fn = fn;
  }

  async run(): Promise<LiveTestCaseResult> {
    const [rec, res] = this.recorder.record(this.id.test, this.id.params);
    rec.start();
    try {
      const inst = new this.fixture(rec, this.id.params || {});
      await inst.init();
      await this.fn(inst);
      await inst.finalize();
    } catch (e) {
      rec.threw(e);
    }
    rec.finish();
    return res;
  }
}
