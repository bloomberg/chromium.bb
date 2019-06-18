import { ParamSpec, ParamSpecIterable, ParamSpecIterator } from './index.js';

export function poptions(name: string, values: ParamSpec[]) {
  return new POptions(name, values);
}

class POptions implements ParamSpecIterable {
  private name: string;
  private values: ParamSpec[];

  constructor(name: string, values: ParamSpec[]) {
    this.name = name;
    this.values = values;
  }

  public *[Symbol.iterator](): ParamSpecIterator {
    for (const value of this.values) {
      yield { [this.name]: value };
    }
  }
}
