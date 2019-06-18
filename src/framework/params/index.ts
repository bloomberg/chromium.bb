export * from './combine.js';
export * from './filter.js';
export * from './options.js';
export * from './exclude.js';

export interface IParamsAny {
  [k: string]: any;
}
export type ParamAnyIterable = Iterable<IParamsAny>;
export type ParamAnyIterator = IterableIterator<IParamsAny>;

export type ParamSpec = boolean | number | string;
export interface IParamsSpec {
  [k: string]: ParamSpec;
}
export type ParamSpecIterable = Iterable<IParamsSpec>;
export type ParamSpecIterator = IterableIterator<IParamsSpec>;

export function paramsEqual(x: IParamsSpec, y: IParamsSpec): boolean {
  if (x === y) {
    return true;
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
