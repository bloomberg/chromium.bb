import { CaseRecorder, GroupRecorder, IResult } from './logger.js';
import { IParamsAny, IParamsSpec, ParamSpecIterable } from './params/index.js';

type TestFn<F extends Fixture> = (t: F) => Promise<void> | void;
export interface ICase {
  readonly name: string;
  readonly params: IParamsSpec | null;
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

  params(cases: ParamSpecIterable): void {
    if (this.cases !== null) {
      throw new Error('test case is already parameterized');
    }
    this.cases = cases;
  }

  *[Symbol.iterator](): IterableIterator<ICase> {
    const fixture = this.fixture;
    const fn = this.fn;
    for (const params of this.cases || [{}]) {
      yield {
        name: this.name,
        params,
        async run(log) {
          const inst = new fixture(log, params);
          await inst.init();
          await fn(inst);
          inst.finalize();
        },
      };
    }
  }
}

export class RunCase {
  readonly testcase: ICase;
  private recorder: GroupRecorder;

  constructor(recorder: GroupRecorder, testcase: ICase) {
    this.recorder = recorder;
    this.testcase = testcase;
  }

  async run(): Promise<IResult> {
    const [res, rec] = this.recorder.record(this.testcase.name, this.testcase.params);
    rec.start();
    try {
      await this.testcase.run(rec);
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

  async init(): Promise<void> {}

  finalize(): void {}

  log(msg: string) {
    this.rec.log(msg);
  }
}

export const allowedTestNameCharacters = 'a-zA-Z0-9/_ ';

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
    // It may be OK to add more allowed characters here.
    const validNames = new RegExp('^[' + allowedTestNameCharacters + ']+$');
    if (!validNames.test(name)) {
      throw new Error(`Invalid test name ${name}; must match ${validNames}`);
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
