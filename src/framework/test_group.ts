import { CaseRecorder, GroupRecorder, IResult } from './logger.js';
import { IParamsAny, IParamsSpec, ParamSpecIterable, paramsEquals } from './params/index.js';

export interface ICaseID {
  readonly name: string;
  readonly params: IParamsSpec | null;
}

type TestFn<F extends Fixture> = (t: F) => Promise<void> | void;
export interface ICase extends ICaseID {
  run(log: CaseRecorder): Promise<void>;
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
    const seen: IParamsAny[] = [];
    // This is n^2.
    for (const spec of cases) {
      if (seen.some(x => paramsEquals(x, spec))) {
        throw new Error('Duplicate test case params');
      }
      seen.push(spec);
    }
    this.cases = cases;
  }

  *[Symbol.iterator](): IterableIterator<ICase> {
    const fixture = this.fixture;
    const fn = this.fn;
    for (const params of this.cases || [null]) {
      yield {
        name: this.name,
        params,
        async run(log) {
          const inst = new fixture(log, params || {});
          await inst.init();
          await fn(inst);
          inst.finalize();
        },
      };
    }
  }
}

export class RunCase {
  readonly testcase: ICaseID;
  private testcaseRun: (log: CaseRecorder) => Promise<void>;
  private recorder: GroupRecorder;

  constructor(recorder: GroupRecorder, testcase: ICase) {
    this.recorder = recorder;
    this.testcase = { name: testcase.name, params: testcase.params };
    this.testcaseRun = testcase.run;
  }

  async run(): Promise<IResult> {
    const [res, rec] = this.recorder.record(this.testcase.name, this.testcase.params);
    rec.start();
    try {
      await this.testcaseRun(rec);
    } catch (e) {
      // tslint:disable-next-line: no-console
      console.warn(e);
      rec.threw(e);
    }
    rec.finish();
    return res;
  }
}

type FixtureClass<F extends Fixture> = new (log: CaseRecorder, params: IParamsAny) => F;

export abstract class Fixture {
  params: IParamsAny;
  protected rec: CaseRecorder;

  constructor(rec: CaseRecorder, params: IParamsAny) {
    this.rec = rec;
    this.params = params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  async init(): Promise<void> {}

  finalize(): void {}

  log(msg: string) {
    this.rec.log(msg);
  }
}

// It may be OK to add more allowed characters here.
export const allowedTestNameCharacters = 'a-zA-Z0-9/_ ';
const validNames = new RegExp('^[' + allowedTestNameCharacters + ']+$');

export interface ITestGroup {
  iterate(log: GroupRecorder): Iterable<RunCase>;
}

export class TestGroup<F extends Fixture> implements ITestGroup {
  private fixture: FixtureClass<F>;
  private seen: Set<string> = new Set();
  private tests: Array<Test<F>> = [];

  constructor(fixture: FixtureClass<F>) {
    this.fixture = fixture;
  }

  *iterate(log: GroupRecorder): Iterable<RunCase> {
    for (const test of this.tests) {
      for (const c of test) {
        yield new RunCase(log, c);
      }
    }
  }

  private checkName(name: string) {
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
