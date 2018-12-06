export namespace param {
  export type ParamIterable = Iterable<object>;
  export type ParamIterator = IterableIterator<object>;

  export class Unit implements ParamIterable {
    * [Symbol.iterator](): ParamIterator {
      yield {};
    }
  }

  export class List implements ParamIterable {
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

  export class Combiner implements ParamIterable {
    private params: ParamIterable[];

    constructor(params: ParamIterable[]) {
      this.params = params;
    }

    [Symbol.iterator](): ParamIterator {
      return Combiner.cartesian(this.params);
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
        yield* new Unit();
        return;
      }
      if (iters.length === 1) {
        yield* iters[0];
        return;
      }
      const [as, ...rest] = iters;
      for (let a of as) {
        for (let b of Combiner.cartesian(rest)) {
          yield Combiner.merge(a, b);
        }
      }
    }
  }
}

namespace test {
  function debug(iter: Iterable<object>) {
    const firstIteration = Array.from(iter);
    const secondIteration = Array.from(iter);
    console.assert(firstIteration.toString() == secondIteration.toString());
    console.log(firstIteration);
  }

  debug(new param.List('hello', [1, 2, 3]));
  debug(new param.Unit());
  debug(new param.Combiner([]));
  debug(new param.Combiner([
    new param.Unit(),
    new param.Unit(),
  ]));
  debug(new param.Combiner([
    new param.List('x', [1, 2]),
    new param.List('y', ['a', 'b']),
    new param.Unit(),
  ]));
  debug(new param.Combiner([
    [{x: 1, y: 2}, {x: 10, y: 20}],
    [{z: 'z'}, {w: 'w'}],
  ]));
}