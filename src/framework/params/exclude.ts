import {
  ParamSpecIterable,
  ParamSpecIterator,
  paramsEqual,
  IParamsSpec,
} from "./index.js";

export function pexclude(params: ParamSpecIterable, exclude: ParamSpecIterable) {
  return new PExclude(params, exclude);
}

class PExclude implements ParamSpecIterable {
  private cases: ParamSpecIterable;
  private exclude: IParamsSpec[];

  public constructor(cases: ParamSpecIterable, exclude: ParamSpecIterable) {
    this.cases = cases;
    this.exclude = Array.from(exclude);
  }

  public * [Symbol.iterator](): ParamSpecIterator {
    for (const p of this.cases) {
      if (this.exclude.every((e) => !paramsEqual(p, e))) {
        yield p;
      }
    }
  }
}
