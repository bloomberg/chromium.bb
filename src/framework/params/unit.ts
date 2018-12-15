import { ParamIterable, ParamIterator } from ".";

export function punit() { return new PUnit(); }
class PUnit implements ParamIterable {
  public * [Symbol.iterator](): ParamIterator {
    yield {};
  }
}
