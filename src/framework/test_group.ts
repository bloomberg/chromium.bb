import { CaseRecorder, GroupRecorder, IResult } from './logger.js';
import { IParamsAny, IParamsSpec } from './params/index.js';

type TestFn<F extends Fixture> = (t: F) => (Promise<void> | void);
export interface ICase {
  name: string;
  params?: IParamsSpec;
  run: (log: CaseRecorder) => Promise<void>;
}

export class RunCase {
  public readonly testcase: ICase;
  private recorder: GroupRecorder;

  constructor(recorder: GroupRecorder, testcase: ICase) {
    this.recorder = recorder;
    this.testcase = testcase;
  }

  public async run(): Promise<IResult> {
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
  public params: IParamsAny;
  protected rec: CaseRecorder;

  public constructor(log: CaseRecorder, params: IParamsAny) {
    this.rec = log;
    this.params = params;
  }

  public async init(): Promise<void> { }

  public log(msg: string) {
    this.rec.log(msg);
  }
}

export const allowedTestNameCharacters: string = 'a-zA-Z0-9/_ ';

export interface ITestGroup {
  iterate(log: GroupRecorder): Iterable<RunCase>;
}

export class TestGroup implements ITestGroup {
  private seen: Set<string> = new Set();
  private tests: ICase[] = [];

  public constructor() { }

  public * iterate(log: GroupRecorder): Iterable<RunCase> {
    for (const t of this.tests) {
      yield new RunCase(log, t);
    }
  }

  public test<F extends Fixture>(
    name: string, fixture: FixtureClass<F>, fn: TestFn<F>, params?: IParamsSpec): void {
    // It may be OK to add more allowed characters here.
    const validNames = new RegExp('^[' + allowedTestNameCharacters + ']+$');
    if (!validNames.test(name)) {
      throw new Error(`Invalid test name ${name}; must match ${validNames}`);
    }

    const key = params ? (name + ':' + JSON.stringify(params)) : name;
    // Disallow '%' so that decodeURIComponent(unencodedQuery) == unencodedQuery. Hopefully.
    if (key.indexOf('%') !== -1) {
      throw new Error('Invalid character % in test name or parameters');
    }

    const p = params ? params : {};
    if (this.seen.has(key)) {
      throw new Error('Duplicate test name');
    }
    this.seen.add(key);

    this.tests.push({
      name,
      params,
      async run(log) {
        const inst = new fixture(log, p);
        await inst.init();
        return fn(inst);
      },
    });
  }
}
