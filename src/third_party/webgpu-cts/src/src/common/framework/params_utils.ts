import { comparePublicParamsPaths, Ordering } from './query/compare.js';
import { kWildcard, kParamSeparator, kParamKVSeparator } from './query/separators.js';

// Consider adding more types here if needed
//
// TODO: This type isn't actually used to constrain what you're allowed to do in `.params()`, so
// it's not really serving its purpose. Figure out how to fix that?
export type ParamArgument =
  | undefined
  | null
  | number
  | string
  | boolean
  | number[]
  | { readonly [k: string]: undefined | null | number | string | boolean };
export type CaseParams = {
  readonly [k: string]: ParamArgument;
};
export interface CaseParamsRW {
  [k: string]: ParamArgument;
}
export type CaseParamsIterable = Iterable<CaseParams>;

export function paramKeyIsPublic(key: string): boolean {
  return !key.startsWith('_');
}

export function extractPublicParams(params: CaseParams): CaseParams {
  const publicParams: CaseParamsRW = {};
  for (const k of Object.keys(params)) {
    if (paramKeyIsPublic(k)) {
      publicParams[k] = params[k];
    }
  }
  return publicParams;
}

export const badParamValueChars = new RegExp(
  '[' + kParamKVSeparator + kParamSeparator + kWildcard + ']'
);

export function publicParamsEquals(x: CaseParams, y: CaseParams): boolean {
  return comparePublicParamsPaths(x, y) === Ordering.Equal;
}
