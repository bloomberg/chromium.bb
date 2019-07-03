import { ParamsAny } from './params/index.js';
import { CaseRecorder } from './logger.js';

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

  log(msg: string) {
    this.rec.log(msg);
  }
}
