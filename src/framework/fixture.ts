import { ParamsAny } from './params/index.js';
import { CaseRecorder } from './logger.js';

// A Fixture is a class used to instantiate each test case at run time.
// A new instance of the Fixture is created for every single test case
// (i.e. every time the test function is run).
export abstract class Fixture {
  params: ParamsAny;
  protected rec: CaseRecorder;

  constructor(rec: CaseRecorder, params: ParamsAny) {
    this.rec = rec;
    this.params = params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  async init(): Promise<void> {}

  finalize(): void {}

  log(msg: string): void {
    this.rec.log(msg);
  }
}
