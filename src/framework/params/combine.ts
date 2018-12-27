import { ParamIterable, ParamIterator, punit } from "./index.js";

export function pcombine(params: ParamIterable[]) { return new PCombine(params); }
export class PCombine implements ParamIterable {

  private static merge(a: object, b: object): object {
    for (const key of Object.keys(a)) {
      if (b.hasOwnProperty(key)) {
        throw new Error("Duplicate key: " + key);
      }
    }
    return {...a, ...b};
  }

  private static * cartesian(iters: Array<Iterable<object>>): ParamIterator {
    if (iters.length === 0) {
      yield* punit();
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
  private params: ParamIterable[];

  constructor(params: ParamIterable[]) {
    this.params = params;
  }

  public [Symbol.iterator](): ParamIterator {
    return PCombine.cartesian(this.params);
  }
}
