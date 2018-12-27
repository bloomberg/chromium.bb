import { TestRecorder, CaseRecorder, IResult } from "./logger";
import { ParamIterable } from "./params";

type CasePFn = (log: CaseRecorder, param: object) => (Promise<void> | void);
type CaseFn = (log: CaseRecorder) => (Promise<void> | void);
type RunCase = () => Promise<IResult>;

interface ICase {
  name: string;
  params?: object;
  run: CaseFn;
}

export class Test {
  private tests: ICase[] = [];

  public constructor() {
  }

  public case(name: string, fn: CaseFn): void {
    this.tests.push({ name, run: fn });
  }

  public caseP(name: string, params: ParamIterable, fn: CasePFn): void {
    for (const p of params) {
      this.tests.push({ name, params: p, run: (log) => fn(log, p) });
    }
  }

  public * iterate(log: TestRecorder): Iterable<RunCase> {
    for (const t of this.tests) {
      const [res, rec] = log.record(t.name, t.params);
      yield async () => {
        rec.start();
        await t.run(rec);
        rec.finish();
        return res;
      };
    }
  }
}
