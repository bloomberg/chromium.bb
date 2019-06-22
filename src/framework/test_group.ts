import { CaseRecorder, GroupRecorder, IResult } from './logger.js';
import { IParamsAny, IParamsSpec } from './params/index.js';

type TestFn<F extends Fixture> = (t: F) => Promise<void> | void;
export interface ICase {
  name: string;
  params: IParamsSpec | null;
  run: (log: CaseRecorder) => Promise<void>;
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

  constructor(log: CaseRecorder, params: IParamsAny) {
    this.rec = log;
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
  private tests: ICase[] = [];

  constructor(fixture: FixtureClass<F>) {
    this.fixture = fixture;
  }

  *iterate(log: GroupRecorder): Iterable<RunCase> {
    for (const t of this.tests) {
      yield new RunCase(log, t);
    }
  }

  test(name: string, params: IParamsSpec | null, fn: TestFn<F>): void {
    // It may be OK to add more allowed characters here.
    const validNames = new RegExp('^[' + allowedTestNameCharacters + ']+$');
    if (!validNames.test(name)) {
      throw new Error(`Invalid test name ${name}; must match ${validNames}`);
    }

    const key = params ? name + ':' + JSON.stringify(params) : name;
    // Disallow '%' so that decodeURIComponent(unencodedQuery) == unencodedQuery. Hopefully.
    if (key.indexOf('%') !== -1) {
      throw new Error('Invalid character % in test name or parameters');
    }

    const p = params ? params : {};
    if (this.seen.has(key)) {
      throw new Error('Duplicate test name');
    }
    this.seen.add(key);

    const fixture = this.fixture;
    this.tests.push({
      name,
      params,
      async run(log) {
        const inst = new fixture(log, p);
        await inst.init();
        await fn(inst);
        inst.finalize();
      },
    });
  }
}
