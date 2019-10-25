import { objectEquals } from '../util/index.js';

export * from './combine.js';
export * from './exclude.js';
export * from './filter.js';
export * from './options.js';

// tslint:disable-next-line: no-any
export type ParamArgument = any;
export interface ParamSpec {
  [k: string]: ParamArgument;
}
export type ParamSpecIterable = Iterable<ParamSpec>;
export type ParamSpecIterator = IterableIterator<ParamSpec>;

export function paramsEquals(x: ParamSpec | null, y: ParamSpec | null): boolean {
  if (x === y) {
    return true;
  }
  if (x === null || y === null) {
    return false;
  }

  for (const xk of Object.keys(x)) {
    if (x[xk] !== undefined && !y.hasOwnProperty(xk)) {
      return false;
    }
    if (!objectEquals(x[xk], y[xk])) {
      return false;
    }
  }

  for (const yk of Object.keys(y)) {
    if (y[yk] !== undefined && !x.hasOwnProperty(yk)) {
      return false;
    }
  }
  return true;
}

export function paramsSupersets(sup: ParamSpec | null, sub: ParamSpec | null): boolean {
  if (sub === null) {
    return true;
  }
  if (sup === null) {
    // && sub !== undefined
    return false;
  }
  for (const k of Object.keys(sub)) {
    if (!sup.hasOwnProperty(k) || sup[k] !== sub[k]) {
      return false;
    }
  }
  return true;
}
