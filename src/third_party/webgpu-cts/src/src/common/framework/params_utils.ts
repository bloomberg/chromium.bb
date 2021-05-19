import { comparePublicParamsPaths, Ordering } from './query/compare.js';
import { kWildcard, kParamSeparator, kParamKVSeparator } from './query/separators.js';
import { UnionToIntersection } from './util/types.js';
import { assert } from './util/util.js';

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

export type KeyOfNeverable<T> = T extends never ? never : keyof T;
export type AllKeysFromUnion<T> = keyof T | KeyOfNeverable<UnionToIntersection<T>>;
export type KeyOfOr<T, K, Default> = K extends keyof T ? T[K] : Default;

/**
 * Flatten a union of interfaces into a single interface encoding the same type.
 *
 * Flattens a union in such a way that:
 * `{ a: number, b?: undefined } | { b: string, a?: undefined }`
 * (which is the value type of `[{ a: 1 }, { b: 1 }]`)
 * becomes `{ a: number | undefined, b: string | undefined }`.
 *
 * And also works for `{ a: number } | { b: string }` which maps to the same.
 */
export type FlattenUnionOfInterfaces<T> = {
  [K in AllKeysFromUnion<T>]: KeyOfOr<
    T,
    // If T always has K, just take T[K] (union of C[K] for each component C of T):
    K,
    // Otherwise, take the union of C[K] for each component C of T, PLUS undefined:
    undefined | KeyOfOr<UnionToIntersection<T>, K, void>
  >;
};

export type ValueTypeForKeyOfMergedType<A, B, Key extends keyof A | keyof B> = Key extends keyof A
  ? Key extends keyof B
    ? void // Key is in both types
    : A[Key] // Key is only in A
  : Key extends keyof B
  ? B[Key] // Key is only in B
  : void; // Key is in neither type (not possible)

export type Merged<A, B> = MergedFromFlat<A, FlattenUnionOfInterfaces<B>>;
export type MergedFromFlat<A, B> = keyof A & keyof B extends never
  ? string extends keyof A | keyof B
    ? never // (keyof A | keyof B) == string, which is too broad
    : {
        [Key in keyof A | keyof B]: ValueTypeForKeyOfMergedType<A, B, Key>;
      }
  : never; // (keyof A & keyof B) is not empty, so they overlapped

export function mergeParams<A extends {}, B extends {}>(a: A, b: B): Merged<A, B> {
  for (const key of Object.keys(a)) {
    assert(!(key in b), 'Duplicate key: ' + key);
  }
  return { ...a, ...b } as Merged<A, B>;
}
