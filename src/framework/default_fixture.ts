import { CaseRecorder } from "./logger.js";
import { IParamsAny } from "./params/index.js";
import { Fixture, FixtureCreate } from "./test_group.js";

export function makeDefaultFixtureCreate<FC extends typeof DefaultFixture, F extends DefaultFixture>(fixture: FC): FixtureCreate<F> {
  return async (log: CaseRecorder, params: IParamsAny) => {
    return new fixture(log, params) as F;
  };
}

export class DefaultFixture extends Fixture {
  public static create = makeDefaultFixtureCreate(DefaultFixture);

  public constructor(log: CaseRecorder, params: IParamsAny) {
    super(log, params);
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
