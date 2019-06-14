import { CaseRecorder, GroupRecorder, IResult } from './logger.js';
import { IParamsAny, IParamsSpec } from './params/index.js';

type TestFn<F extends Fixture> = (t: F) => (Promise<void> | void);
export interface ICase {
  name: string;
  params?: object;
  run: (log: CaseRecorder) => Promise<void>;
}

class RunCase {
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

type IFixture<F extends Fixture> = new(log: CaseRecorder, params: IParamsAny) => F;

export abstract class Fixture {
  public params: IParamsAny;
  protected rec: CaseRecorder;

  public constructor(log: CaseRecorder, params: IParamsAny) {
    this.rec = log;
    this.params = params;
  }

  public async init(): Promise<void> {}

  public log(msg: string) {
    this.rec.log(msg);
  }
}

export class TestGroup {
  private seen: Set<string> = new Set();
  private tests: ICase[] = [];

  public constructor() {}

  public testp<F extends Fixture>(name: string,
                                  params: IParamsSpec,
                                  fixture: IFixture<F>,
                                  fn: TestFn<F>): void {
    return this.testImpl(name, params, fixture, fn);
  }

  public test<F extends Fixture>(name: string, fixture: IFixture<F>, fn: TestFn<F>): void {
    return this.testImpl(name, undefined, fixture, fn);
  }

  public * iterate(log: GroupRecorder): Iterable<RunCase> {
    for (const t of this.tests) {
      yield new RunCase(log, t);
    }
  }

  private testImpl<F extends Fixture>(name: string,
                                      params: (IParamsSpec | undefined),
                                      fixture: IFixture<F>,
                                      fn: TestFn<F>): void {
    // It may be OK to add more allowed characters here.
    const validNames = /^[a-zA-Z0-9/_ ]+$/;
    if (!validNames.test(name)) {
      throw new Error(`Invalid test name ${name}; must match ${validNames}`);
    }

    const key = params ? (name + ':' + JSON.stringify(params)) : name;
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
