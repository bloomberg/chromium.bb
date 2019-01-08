import { GroupRecorder, CaseRecorder, IResult } from "./logger";
import { ParamIterable } from "./params";

type TestFn = (log: CaseRecorder, param: object) => (Promise<void> | void);
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
interface TestOptions {
  class?: ITestClass;
  cases?: ParamIterable;
}
interface ITestClass {
  new(log: CaseRecorder, params: object): TestClass;
}

export abstract class TestClass {
  log: CaseRecorder;
  params: object;
  constructor(log: CaseRecorder, params: object) {
    this.log = log;
    this.params = params;
  }
}

export class TestGroup {
  private tests: ICase[] = [];

  public constructor() {
  }

  public test(name: string, options: TestOptions, fn: TestFn): void {
    const opt = Object.assign({}, options);
    opt.cases = opt.cases || [{}];
    for (const p of opt.cases) {
      this.tests.push({ name, params: p, run: (log) => {
        const inst = opt.class ? new opt.class(log, p) : undefined;
        return fn.call(inst, log, p);
       }});
    }
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
