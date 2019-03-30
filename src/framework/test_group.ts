import { CaseRecorder, GroupRecorder, IResult } from "./logger.js";
import { IParamsAny, IParamsSpec } from "./params/index.js";

type TestFn<F extends Fixture> = (t: F) => (Promise<void> | void);
interface ICase {
  name: string;
  params?: object;
  run: (log: CaseRecorder) => Promise<void>;
}
interface IRunCase {
  name: string;
  params?: object;
  run: () => Promise<IResult>;
}

export type FixtureCreate<F extends Fixture> = (log: CaseRecorder, params: IParamsAny) => Promise<F>;

interface IFixture<F extends Fixture> {
  create: FixtureCreate<F>,
}

export abstract class Fixture {
  public params: IParamsAny;
  protected rec: CaseRecorder;

  protected constructor(log: CaseRecorder, params: IParamsAny) {
    this.rec = log;
    this.params = params;
  }

  public log(msg: string) {
    this.rec.log(msg);
  }
}

export class TestGroup {
  private tests: ICase[] = [];

  public constructor() {
  }

  public testp<F extends Fixture>(name: string,
                                  params: IParamsSpec,
                                  fixture: IFixture<F>,
                                  fn: TestFn<F>): void {
    return this.testImpl(name, params, fixture, fn);
  }

  public test<F extends Fixture>(name: string, fixture: IFixture<F>, fn: TestFn<F>): void {
    return this.testImpl(name, undefined, fixture, fn);
  }

  public * iterate(log: GroupRecorder): Iterable<IRunCase> {
    for (const t of this.tests) {
      const [res, rec] = log.record(t.name, t.params);
      yield {name: t.name, params: t.params, run: async () => {
        rec.start();
        try {
          await t.run(rec);
        } catch (e) {
          console.warn(e);
          rec.threw(e);
        }
        rec.finish();
        return res;
      }};
    }
  }

  private testImpl<F extends Fixture>(name: string,
                                      params: (IParamsSpec | undefined),
                                      fixture: IFixture<F>,
                                      fn: TestFn<F>): void {
    const n = params ? (name + "/" + JSON.stringify(params)) : name;
    const p = params ? params : {};
    this.tests.push({ name: n, run: async (log) => {
      const inst = await fixture.create(log, p);
      return fn(inst);
    }});
  }
}
