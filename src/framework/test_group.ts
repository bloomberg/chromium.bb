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

interface IFixtureClass<F extends Fixture> {
  create(log: CaseRecorder, params: IParamsAny): F | Promise<F>;
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

export class DefaultFixture extends Fixture {
  public static create(log: CaseRecorder, params: IParamsAny): DefaultFixture {
    return new DefaultFixture(log, params);
  }

  public warn(msg?: string) {
    this.rec.warn(msg);
  }

  public fail(msg?: string) {
    this.rec.fail(msg);
  }

  public ok(msg?: string) {
    if (msg) {
      this.log("OK: " + msg);
    } else {
      this.log("OK");
    }
  }

  public expect(cond: boolean, msg?: string) {
    if (cond) {
      this.ok(msg);
    } else {
      this.rec.fail(msg);
    }
  }
}

export class TestGroup {
  private tests: ICase[] = [];

  public constructor() {
  }

  public testpf<F extends Fixture>(name: string,
                                   params: IParamsSpec,
                                   fixture: IFixtureClass<F>,
                                   fn: TestFn<F>): void {
    return this.testImpl(name, params, fixture, fn);
  }

  public testf<F extends Fixture>(name: string, fixture: IFixtureClass<F>, fn: TestFn<F>): void {
    return this.testImpl(name, undefined, fixture, fn);
  }

  public testp(name: string, params: IParamsSpec, fn: TestFn<DefaultFixture>): void {
    return this.testImpl(name, params, DefaultFixture, fn);
  }

  public test(name: string, fn: TestFn<DefaultFixture>): void {
    return this.testImpl(name, undefined, DefaultFixture, fn);
  }

  public * iterate(log: GroupRecorder): Iterable<IRunCase> {
    for (const t of this.tests) {
      const [res, rec] = log.record(t.name, t.params);
      yield {name: t.name, params: t.params, run: async () => {
        rec.start();
        try {
          await t.run(rec);
        } catch (e) {
          rec.threw(e);
        }
        rec.finish();
        return res;
      }};
    }
  }

  private testImpl<F extends Fixture>(name: string,
                                      params: (IParamsSpec | undefined),
                                      fixture: IFixtureClass<F>,
                                      fn: TestFn<F>): void {
    const n = params ? (name + "/" + JSON.stringify(params)) : name;
    const p = params ? params : {};
    this.tests.push({ name: n, run: async (log) => {
      const inst = await fixture.create(log, p);
      return fn(inst);
    }});
  }
}
