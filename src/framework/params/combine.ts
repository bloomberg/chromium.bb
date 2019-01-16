import {
  IParamsSpec,
  ParamSpecIterable,
  ParamSpecIterator,
} from "./index.js";

export function pcombine(params: ParamSpecIterable[]) { return new PCombine(params); }

class PCombine implements ParamSpecIterable {
  private params: ParamSpecIterable[];

  constructor(params: ParamSpecIterable[]) {
    this.params = params;
  }

  public [Symbol.iterator](): ParamSpecIterator {
    return PCombine.cartesian(this.params);
  }

  private static merge(a: IParamsSpec, b: IParamsSpec): IParamsSpec {
    for (const key of Object.keys(a)) {
      if (b.hasOwnProperty(key)) {
        throw new Error("Duplicate key: " + key);
      }
    }
    return {...a, ...b};
  }

  private static * cartesian(iters: ParamSpecIterable[]): ParamSpecIterator {
    if (iters.length === 0) {
      return;
    }
    if (iters.length === 1) {
      yield* iters[0];
      return;
    }
    const [as, ...rest] = iters;
    for (const a of as) {
      for (const b of PCombine.cartesian(rest)) {
        yield PCombine.merge(a, b);
      }
    }
  }
}
