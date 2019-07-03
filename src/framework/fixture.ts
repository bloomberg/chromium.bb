import { IParamsAny } from './params/index.js';
import { CaseRecorder } from './logger.js';

export abstract class Fixture {
  params: IParamsAny;
  protected rec: CaseRecorder;

  constructor(rec: CaseRecorder, params: IParamsAny) {
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
