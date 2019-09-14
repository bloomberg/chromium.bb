export * from './combine.js';
export * from './filter.js';
export * from './options.js';
export * from './exclude.js';

export interface ParamsAny {
  // tslint:disable-next-line: no-any // Too annoying to force every test to type convert params
  [k: string]: any;
}
export type ParamAnyIterable = Iterable<ParamsAny>;
export type ParamAnyIterator = IterableIterator<ParamsAny>;

export type ParamArgument = undefined | boolean | number | string | number[];
export interface ParamsSpec {
  [k: string]: ParamArgument;
}
export type ParamSpecIterable = Iterable<ParamsSpec>;
export type ParamSpecIterator = IterableIterator<ParamsSpec>;

export function paramsEquals(x: ParamsSpec | null, y: ParamsSpec | null): boolean {
  if (x === y) {
    return true;
  }
  if (x === null || y === null) {
    return false;
  }

  for (const xk of Object.keys(x)) {
    if (!y.hasOwnProperty(xk)) {
      return false;
    }
    if (x[xk] !== y[xk]) {
      return false;
    }
  }

  for (const yk of Object.keys(y)) {
    if (!x.hasOwnProperty(yk)) {
      return false;
    }
  }
  return true;
}

export function paramsSupersets(sup: ParamsSpec | null, sub: ParamsSpec | null): boolean {
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
