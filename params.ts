export type ParamIterable = Iterable<object>;
export type ParamIterator = IterableIterator<object>;

export class PUnit implements ParamIterable {
  * [Symbol.iterator](): ParamIterator {
    yield {};
  }
}

export class PList implements ParamIterable {
  private name: string;
  private values: any[];

  constructor(name: string, values: any[]) {
    this.name = name;
    this.values = values;
  }

  * [Symbol.iterator](): ParamIterator {
    for (const value of this.values) {
      yield {[this.name]: value};
    }
  }
}

export class PCombiner implements ParamIterable {
  private params: ParamIterable[];

  constructor(params: ParamIterable[]) {
    this.params = params;
  }

  [Symbol.iterator](): ParamIterator {
    return PCombiner.cartesian(this.params);
  }

  private static merge(a: object, b: object): object {
    for (const key of Object.keys(a)) {
      if (b.hasOwnProperty(key)) {
        throw new Error("Duplicate key: " + key);
      }
    }
    return Object.assign({}, a, b);
  }

  private static * cartesian(iters: Iterable<object>[]): ParamIterator {
    if (iters.length === 0) {
      yield* new PUnit();
      return;
    }
    if (iters.length === 1) {
      yield* iters[0];
      return;
    }
    const [as, ...rest] = iters;
    for (let a of as) {
      for (let b of PCombiner.cartesian(rest)) {
        yield PCombiner.merge(a, b);
      }
    }
  }
}
