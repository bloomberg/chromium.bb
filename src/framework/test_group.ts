import { GroupRecorder, CaseRecorder, IResult } from "./logger";
import { ParamIterable } from "./params";

type TestFn<F extends Fixture> = (this: F) => (Promise<void> | void);
interface ICase {
  name: string;
  params?: object;
  run: (log: CaseRecorder) => (Promise<void> | void);
}
interface RunCase {
  name: string;
  params?: object;
  run: () => Promise<IResult>;
}
interface IFixtureClass<F extends Fixture> {
  new(log: CaseRecorder, params?: object): F;
}

export abstract class Fixture {
  protected _rec: CaseRecorder;
  public params?: object;

  public constructor(log: CaseRecorder, params?: object) {
    this._rec = log;
    this.params = params;
  }

  public log(msg: string) {
    this._rec.log(msg);
  }
}

export class DefaultFixture extends Fixture {
  public warn(msg?: string) {
    this._rec.warn(msg);
  }

  public fail(msg?: string) {
    this._rec.fail(msg);
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
      this.ok(msg)
    } else {
      this._rec.fail(msg);
    }
  }
}

export class TestGroup {
  private tests: ICase[] = [];

  public constructor() {
  }

  public testpf<F extends Fixture>(name: string, params: (object | undefined), fixture: IFixtureClass<F>, fn: TestFn<F>): void {
    const n = params ? (name + "/" + JSON.stringify(params)) : name;
    this.tests.push({ name: n, run: (log) => {
      const inst = new fixture(log, params);
      return fn.call(inst);
      }});
  }

  public testf<F extends Fixture>(name: string, fixture: IFixtureClass<F>, fn: TestFn<F>): void {
    return this.testpf(name, undefined, fixture, fn);
  }

  public testp(name: string, params: object, fn: TestFn<DefaultFixture>): void {
    return this.testpf(name, params, DefaultFixture, fn);
  }

  public test(name: string, fn: TestFn<DefaultFixture>): void {
    return this.testpf(name, undefined, DefaultFixture, fn);
  }

  public * iterate(log: GroupRecorder): Iterable<RunCase> {
    for (const t of this.tests) {
      const [res, rec] = log.record(t.name, t.params);
      yield {name: t.name, params: t.params, run: async () => {
        rec.start();
        await t.run(rec);
        rec.finish();
        return res;
      }};
    }
  }
}
