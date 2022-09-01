import { assert } from '../../common/util/util.js';

import { kValue } from './constants.js';
import { correctlyRoundedF32, flushSubnormalNumber, isF32Finite, oneULP } from './math.js';

/** Represents a closed interval in the f32 range */
export class F32Interval {
  public readonly begin: number;
  public readonly end: number;
  private static _infinite: F32Interval;

  /** Constructor
   *
   * Bounds that are out of range for F32 are converted to appropriate edge or
   * infinity values, so that all values above/below the f32 range are lumped
   * together.
   *
   * The edge values map out to the infinities due to:
   *   If an allowable return value for any operation is greater in magnitude
   *   than the largest representable finite floating-point value, then that
   *   operation may additionally return either the infinity with the same sign
   *   or the largest finite value with the same sign.
   *
   * @param begin number indicating the lower bound of the interval
   * @param end number indicating the upper bound of the interval
   */
  public constructor(begin: number, end: number) {
    assert(!Number.isNaN(begin) && !Number.isNaN(end), `bounds need to be non-NaN`);
    assert(begin <= end, `begin (${begin}) must be equal or before end (${end})`);

    if (begin === Number.NEGATIVE_INFINITY || begin <= kValue.f32.negative.min) {
      this.begin = Number.NEGATIVE_INFINITY;
    } else if (begin === Number.POSITIVE_INFINITY || begin >= kValue.f32.positive.max) {
      this.begin = kValue.f32.positive.max;
    } else {
      this.begin = begin;
    }

    if (end === Number.POSITIVE_INFINITY || end >= kValue.f32.positive.max) {
      this.end = Number.POSITIVE_INFINITY;
    } else if (end === Number.NEGATIVE_INFINITY || end <= kValue.f32.negative.min) {
      this.end = kValue.f32.negative.min;
    } else {
      this.end = end;
    }
  }

  /** @returns if a point or interval is completely contained by this interval
   *
   * Due to the fact backends may clamp out of range values to the min/max f32
   * values, which is represented in the implementation by extending the
   * begin/end values as appropriate, there some unintuitive behaviours here.
   * For example:
   *   [0, max f32].contains(+∞) will return true.
   * */
  public contains(n: number | F32Interval): boolean {
    if (Number.isNaN(n)) {
      // Being the infinite interval indicates that the accuracy is not defined
      // for this test, so the test is just checking that this input doesn't
      // cause the implementation to misbehave, so NaN is acceptable.
      return this.begin === Number.NEGATIVE_INFINITY && this.end === Number.POSITIVE_INFINITY;
    }
    const i = toInterval(n);
    return this.begin <= i.begin && this.end >= i.end;
  }

  /** @returns if this interval contains a single point */
  public isPoint(): boolean {
    return this.begin === this.end;
  }

  /** @returns an interval with the tightest bounds that includes all provided intervals */
  static span(...intervals: F32Interval[]): F32Interval {
    assert(intervals.length > 0, `span of an empty list of F32Intervals is not allowed`);
    let begin = Number.POSITIVE_INFINITY;
    let end = Number.NEGATIVE_INFINITY;
    intervals.forEach(i => {
      begin = Math.min(i.begin, begin);
      end = Math.max(i.end, end);
    });
    return new F32Interval(begin, end);
  }

  /** @returns a string representation for logging purposes */
  public toString(): string {
    return `[${this.begin}, ${this.end}]`;
  }

  /** @returns a singleton for the infinite interval
   * This interval is used in situations where accuracy is not defined, so any
   * result is valid.
   */
  public static infinite(): F32Interval {
    if (this._infinite === undefined) {
      this._infinite = new F32Interval(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY);
    }
    return this._infinite;
  }
}

/** @returns an interval containing the point or the original interval */
function toInterval(n: number | F32Interval): F32Interval {
  if (n instanceof F32Interval) {
    return n;
  }
  return new F32Interval(n, n);
}

/**
 * A function that converts a point to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface PointToInterval {
  (x: number): F32Interval;
}

/** Operation used to implement a PointToInterval */
interface PointToIntervalOp {
  /** @returns acceptance interval for a function at point x */
  impl: (x: number) => F32Interval;

  /**
   * Calculates where in the domain defined by x the min/max extrema of impl
   * occur and returns a span of those points to be used as the domain instead.
   *
   * If not defined the ends of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for functions that meet all of the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   *      i.e. fooInterval takes in x: number | F32Interval, not x: number
   */
  extrema?: (x: F32Interval) => F32Interval;
}

/**
 * A function that converts a pair of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface BinaryToInterval {
  (x: number, y: number): F32Interval;
}

/** Operation used to implement a BinaryToInterval */
interface BinaryToIntervalOp {
  /** @returns acceptance interval for a function at point (x, y) */
  impl: (x: number, y: number) => F32Interval;
  /**
   * Calculates where in domain defined by x & y the min/max extrema of impl
   * occur and returns spans of those points to be used as the domain instead.
   *
   * If not defined the ends of the existing domain are assumed to be the
   * extrema.
   *
   * This is only implemented for functions that meet all of the following
   * criteria:
   *   a) non-monotonic
   *   b) used in inherited accuracy calculations
   *   c) need to take in an interval for b)
   */
  extrema?: (x: F32Interval, y: F32Interval) => [F32Interval, F32Interval];
}

/**
 * A function that converts a triplet of points to an acceptance interval.
 * This is the public facing API for builtin implementations that is called
 * from tests.
 */
export interface TernaryToInterval {
  (x: number, y: number, z: number): F32Interval;
}

/** Operation used to implement a TernaryToInterval */
interface TernaryToIntervalOp {
  /** @returns acceptance interval for a function at point (x, y, z) */
  impl: (x: number, y: number, z: number) => F32Interval;
  // All current ternary operations that are used in inheritance (clamp*) are
  // monotonic, so extrema calculation isn't needed. Re-using the Op interface
  // pattern for symmetry with the other operations
}

/** Converts a point to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * op.extrema is invoked before this point in the call stack.
 *
 * @param n value to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushPointToInterval(n: number, op: PointToIntervalOp) {
  assert(!Number.isNaN(n), `flush not defined for NaN`);
  const values = correctlyRoundedF32(n);
  const inputs = new Set<number>([...values, ...values.map(flushSubnormalNumber)]);
  const results = new Set<F32Interval>([...inputs].map(op.impl));
  return F32Interval.span(...results);
}

/** Converts a pair to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * All unique combinations of x & y are run.
 * op.extrema is invoked before this point in the call stack.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param y second param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushBinaryToInterval(x: number, y: number, op: BinaryToIntervalOp): F32Interval {
  assert(!Number.isNaN(x), `flush not defined for NaN`);
  assert(!Number.isNaN(y), `flush not defined for NaN`);
  const x_values = correctlyRoundedF32(x);
  const y_values = correctlyRoundedF32(y);
  const x_inputs = new Set<number>([...x_values, ...x_values.map(flushSubnormalNumber)]);
  const y_inputs = new Set<number>([...y_values, ...y_values.map(flushSubnormalNumber)]);
  const intervals = new Set<F32Interval>();
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      intervals.add(op.impl(inner_x, inner_y));
    });
  });
  return F32Interval.span(...intervals);
}

/** Converts a triplet to an acceptance interval, using a specific function
 *
 * This handles correctly rounding and flushing inputs as needed.
 * Duplicate inputs are pruned before invoking op.impl.
 * All unique combinations of x, y & z are run.
 * op.extrema is invoked before this point in the call stack.
 *
 * @param x first param to flush & round then invoke op.impl on
 * @param y second param to flush & round then invoke op.impl on
 * @param z third param to flush & round then invoke op.impl on
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
function roundAndFlushTernaryToInterval(
  x: number,
  y: number,
  z: number,
  op: TernaryToIntervalOp
): F32Interval {
  assert(!Number.isNaN(x), `flush not defined for NaN`);
  assert(!Number.isNaN(y), `flush not defined for NaN`);
  assert(!Number.isNaN(z), `flush not defined for NaN`);
  const x_values = correctlyRoundedF32(x);
  const y_values = correctlyRoundedF32(y);
  const z_values = correctlyRoundedF32(z);
  const x_inputs = new Set<number>([...x_values, ...x_values.map(flushSubnormalNumber)]);
  const y_inputs = new Set<number>([...y_values, ...y_values.map(flushSubnormalNumber)]);
  const z_inputs = new Set<number>([...z_values, ...z_values.map(flushSubnormalNumber)]);
  const intervals = new Set<F32Interval>();
  // prettier-ignore
  x_inputs.forEach(inner_x => {
    y_inputs.forEach(inner_y => {
      z_inputs.forEach(inner_z => {
        intervals.add(op.impl(inner_x, inner_y, inner_z));
      });
    });
  });

  return F32Interval.span(...intervals);
}

/** Calculate the acceptance interval for a unary function over an interval
 *
 * If the interval is actually a point, this just decays to
 * roundAndFlushPointToInterval.
 *
 * The provided domain interval may be adjusted if the operation defines an
 * extrema function.
 *
 * @param x input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
// Will be used in test implementations
// eslint-disable-next-line @typescript-eslint/no-unused-vars
function runPointOp(x: F32Interval, op: PointToIntervalOp): F32Interval {
  if (x.isPoint()) {
    return roundAndFlushPointToInterval(x.begin, op);
  }

  if (op.extrema !== undefined) {
    x = op.extrema(x);
  }
  return F32Interval.span(
    roundAndFlushPointToInterval(x.begin, op),
    roundAndFlushPointToInterval(x.end, op)
  );
}

/** Calculate the acceptance interval for a binary function over an interval
 *
 * The provided domain intervals may be adjusted if the operation defines an
 * extrema function.
 *
 * @param x first input domain interval
 * @param y second input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
// Will be used in test implementations
// eslint-disable-next-line @typescript-eslint/no-unused-vars
function runBinaryOp(x: F32Interval, y: F32Interval, op: BinaryToIntervalOp): F32Interval {
  if (op.extrema !== undefined) {
    [x, y] = op.extrema(x, y);
  }
  const x_values = new Set<number>([x.begin, x.end]);
  const y_values = new Set<number>([y.begin, y.end]);

  const results = new Set<F32Interval>();
  x_values.forEach(inner_x => {
    y_values.forEach(inner_y => {
      results.add(roundAndFlushBinaryToInterval(inner_x, inner_y, op));
    });
  });

  return F32Interval.span(...results);
}

/** Calculate the acceptance interval for a ternary function over an interval
 *
 * The provided domain intervals may be adjusted if the operation defines an
 * extrema function.
 *
 * @param x first input domain interval
 * @param y second input domain interval
 * @param z third input domain interval
 * @param op operation defining the function being run
 * @returns a span over all of the outputs of op.impl
 */
// Will be used in test implementations
// eslint-disable-next-line @typescript-eslint/no-unused-vars
function runTernaryOp(
  x: F32Interval,
  y: F32Interval,
  z: F32Interval,
  op: TernaryToIntervalOp
): F32Interval {
  const x_values = new Set<number>([x.begin, x.end]);
  const y_values = new Set<number>([y.begin, y.end]);
  const z_values = new Set<number>([z.begin, z.end]);
  const results = new Set<F32Interval>();
  x_values.forEach(inner_x => {
    y_values.forEach(inner_y => {
      z_values.forEach(inner_z => {
        results.add(roundAndFlushTernaryToInterval(inner_x, inner_y, inner_z, op));
      });
    });
  });

  return F32Interval.span(...results);
}

/** @returns an interval of the correctly rounded values around the point */
export function correctlyRoundedInterval(n: number): F32Interval {
  return roundAndFlushPointToInterval(n, {
    impl: (impl_n: number) => {
      assert(!Number.isNaN(impl_n), `absolute not defined for NaN`);
      return toInterval(impl_n);
    },
  });
}

/** @returns an interval of the absolute error around the point */
export function absoluteErrorInterval(n: number, error_range: number): F32Interval {
  return roundAndFlushPointToInterval(n, {
    impl: (impl_n: number) => {
      assert(!Number.isNaN(n), `absolute not defined for NaN`);
      if (!isF32Finite(n)) {
        return toInterval(n);
      }

      return new F32Interval(impl_n - error_range, impl_n + error_range);
    },
  });
}

/** @returns an interval of N * ULP around the point */
export function ulpInterval(n: number, numULP: number): F32Interval {
  numULP = Math.abs(numULP);
  return roundAndFlushPointToInterval(n, {
    impl: (impl_n: number) => {
      if (!isF32Finite(n)) {
        return toInterval(n);
      }

      const ulp_flush = oneULP(impl_n, true);
      const ulp_noflush = oneULP(impl_n, false);
      const ulp = Math.max(ulp_flush, ulp_noflush);
      const begin = impl_n - numULP * ulp;
      const end = impl_n + numULP * ulp;
      return new F32Interval(
        Math.min(begin, flushSubnormalNumber(begin)),
        Math.max(end, flushSubnormalNumber(end))
      );
    },
  });
}
