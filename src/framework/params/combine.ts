import { ParamSpec, ParamSpecIterable, ParamSpecIterator } from './index.js';

export function pcombine(params: ParamSpecIterable[]): PCombine {
  return new PCombine(params);
}

function merge(a: ParamSpec, b: ParamSpec): ParamSpec {
  for (const key of Object.keys(a)) {
    if (b.hasOwnProperty(key)) {
      throw new Error('Duplicate key: ' + key);
    }
  }
  return { ...a, ...b };
}

function* cartesian(iters: ParamSpecIterable[]): ParamSpecIterator {
  if (iters.length === 0) {
    return;
  }
  if (iters.length === 1) {
    yield* iters[0];
    return;
  }
  const [as, ...rest] = iters;
  for (const a of as) {
    for (const b of cartesian(rest)) {
      yield merge(a, b);
    }
  }
}

class PCombine implements ParamSpecIterable {
  private params: ParamSpecIterable[];

  constructor(params: ParamSpecIterable[]) {
    this.params = params;
  }

  [Symbol.iterator](): ParamSpecIterator {
    return cartesian(this.params);
  }
}
