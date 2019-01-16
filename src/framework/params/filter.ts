import {
  IParamsAny,
  ParamSpecIterable,
  ParamSpecIterator,
} from "./index.js";

type Predicate = (o: IParamsAny) => boolean;

export function pfilter(cases: ParamSpecIterable, pred: Predicate) {
  return new PFilter(cases, pred);
}

class PFilter implements ParamSpecIterable {
  private cases: ParamSpecIterable;
  private pred: Predicate;

  public constructor(cases: ParamSpecIterable, pred: Predicate) {
    this.cases = cases;
    this.pred = pred;
  }

  public * [Symbol.iterator](): ParamSpecIterator {
    for (const p of this.cases) {
      if (this.pred(p)) {
        yield p;
      }
    }
  }
}
