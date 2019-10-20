import { ParamSpec, ParamSpecIterable, ParamSpecIterator } from './index.js';

type Predicate = (o: ParamSpec) => boolean;

export function pfilter(cases: ParamSpecIterable, pred: Predicate): PFilter {
  return new PFilter(cases, pred);
}

class PFilter implements ParamSpecIterable {
  private cases: ParamSpecIterable;
  private pred: Predicate;

  constructor(cases: ParamSpecIterable, pred: Predicate) {
    this.cases = cases;
    this.pred = pred;
  }

  *[Symbol.iterator](): ParamSpecIterator {
    for (const p of this.cases) {
      if (this.pred(p)) {
        yield p;
      }
    }
  }
}
