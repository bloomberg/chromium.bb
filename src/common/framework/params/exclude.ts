import { ParamSpec, ParamSpecIterable, ParamSpecIterator, paramsEquals } from './index.js';

export function pexclude(params: ParamSpecIterable, exclude: ParamSpecIterable): PExclude {
  return new PExclude(params, exclude);
}

class PExclude implements ParamSpecIterable {
  private cases: ParamSpecIterable;
  private exclude: ParamSpec[];

  constructor(cases: ParamSpecIterable, exclude: ParamSpecIterable) {
    this.cases = cases;
    this.exclude = Array.from(exclude);
  }

  *[Symbol.iterator](): ParamSpecIterator {
    for (const p of this.cases) {
      if (this.exclude.every(e => !paramsEquals(p, e))) {
        yield p;
      }
    }
  }
}
