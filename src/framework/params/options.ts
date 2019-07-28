import { ParamArgument, ParamSpecIterable, ParamSpecIterator } from './index.js';

export function poptions(name: string, values: ParamArgument[]): POptions {
  return new POptions(name, values);
}

export function pbool(name: string): POptions {
  return new POptions(name, [false, true]);
}

class POptions implements ParamSpecIterable {
  private name: string;
  private values: ParamArgument[];

  constructor(name: string, values: ParamArgument[]) {
    this.name = name;
    this.values = values;
  }

  *[Symbol.iterator](): ParamSpecIterator {
    for (const value of this.values) {
      yield { [this.name]: value };
    }
  }
}
