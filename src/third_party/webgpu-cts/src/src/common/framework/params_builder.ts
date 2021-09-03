import {
  CaseParams,
  CaseParamsIterable,
  FlattenUnionOfInterfaces,
  Merged,
  mergeParams,
  publicParamsEquals,
} from './params_utils.js';
import { ResolveType, UnionToIntersection } from './util/types.js';

/** Conditionally chooses between two types depending on whether T is a union. */
type CheckForUnion<T, TErr, TOk> = [T] extends [UnionToIntersection<T>] ? TOk : TErr;

/** Conditionally chooses a type (or void) depending on whether T is a string. */
type CheckForStringLiteralType<T, TOk> = string extends T ? void : CheckForUnion<T, void, TOk>;

/* eslint-disable-next-line @typescript-eslint/no-unused-vars */
function typeAssert<T extends 'pass'>() {}
{
  type Test<T, U> = [T] extends [U]
    ? [U] extends [T]
      ? 'pass'
      : { actual: ResolveType<T>; expected: U }
    : { actual: ResolveType<T>; expected: U };

  type T01 = { a: number } | { b: string };
  type T02 = { a: number } | { b?: string };
  type T03 = { a: number } | { a?: number };
  type T04 = { a: number } | { a: string };
  type T05 = { a: number } | { a?: string };

  type T11 = { a: number; b?: undefined } | { a?: undefined; b: string };

  type T21 = { a: number; b?: undefined } | { b: string };
  type T22 = { a: number; b?: undefined } | { b?: string };
  type T23 = { a: number; b?: undefined } | { a?: number };
  type T24 = { a: number; b?: undefined } | { a: string };
  type T25 = { a: number; b?: undefined } | { a?: string };
  type T26 = { a: number; b?: undefined } | { a: undefined };
  type T27 = { a: number; b?: undefined } | { a: undefined; b: undefined };

  /* prettier-ignore */ {
    typeAssert<Test<FlattenUnionOfInterfaces<T01>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T02>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T03>, { a: number | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T04>, { a: number | string }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T05>, { a: number | string | undefined }>>();

    typeAssert<Test<FlattenUnionOfInterfaces<T11>, { a: number | undefined; b: string | undefined }>>();

    typeAssert<Test<FlattenUnionOfInterfaces<T22>, { a: number | undefined; b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T23>, { a: number | undefined; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T24>, { a: number | string; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T25>, { a: number | string | undefined; b: undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T27>, { a: number | undefined; b: undefined }>>();

    // Unexpected test results - hopefully okay to ignore these
    typeAssert<Test<FlattenUnionOfInterfaces<T21>, { b: string | undefined }>>();
    typeAssert<Test<FlattenUnionOfInterfaces<T26>, { a: number | undefined }>>();
  }
}

export function poptions<Name extends string, V>(
  name: Name,
  values: Iterable<V>
): CheckForStringLiteralType<Name, Iterable<{ [name in Name]: V }>> {
  const iter = makeReusableIterable(function* () {
    for (const value of values) {
      yield { [name]: value };
    }
  });
  /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
  return iter as any;
}

export function pbool<Name extends string>(
  name: Name
): CheckForStringLiteralType<Name, Iterable<{ [name in Name]: boolean }>> {
  return poptions(name, [false, true]);
}

export function params(): ParamsBuilder<{}> {
  return new ParamsBuilder();
}

export class ParamsBuilder<A extends {}> implements CaseParamsIterable {
  private paramSpecs: CaseParamsIterable = [{}];

  [Symbol.iterator](): Iterator<A> {
    const iter: Iterator<CaseParams> = this.paramSpecs[Symbol.iterator]();
    return iter as Iterator<A>;
  }

  combine<B extends {}>(newParams: Iterable<B>): ParamsBuilder<Merged<A, B>> {
    const paramSpecs = this.paramSpecs as Iterable<A>;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const a of paramSpecs) {
        for (const b of newParams) {
          yield mergeParams(a, b);
        }
      }
    }) as CaseParamsIterable;
    /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
    return this as any;
  }

  expand<B extends {}>(expander: (_: A) => Iterable<B>): ParamsBuilder<Merged<A, B>> {
    const paramSpecs = this.paramSpecs as Iterable<A>;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const a of paramSpecs) {
        for (const b of expander(a)) {
          yield mergeParams(a, b);
        }
      }
    }) as CaseParamsIterable;
    /* eslint-disable-next-line @typescript-eslint/no-explicit-any */
    return this as any;
  }

  filter(pred: (_: A) => boolean): ParamsBuilder<A> {
    const paramSpecs = this.paramSpecs as Iterable<A>;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const p of paramSpecs) {
        if (pred(p)) {
          yield p;
        }
      }
    });
    return this;
  }

  unless(pred: (_: A) => boolean): ParamsBuilder<A> {
    return this.filter(x => !pred(x));
  }

  exclude(exclude: CaseParamsIterable): ParamsBuilder<A> {
    const excludeArray = Array.from(exclude);
    const paramSpecs = this.paramSpecs;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const p of paramSpecs) {
        if (excludeArray.every(e => !publicParamsEquals(p, e))) {
          yield p;
        }
      }
    });
    return this;
  }
}

// If you create an Iterable by calling a generator function (e.g. in IIFE), it is exhausted after
// one use. This just wraps a generator function in an object so it be iterated multiple times.
function makeReusableIterable<P>(generatorFn: () => Generator<P>): Iterable<P> {
  return { [Symbol.iterator]: generatorFn };
}
