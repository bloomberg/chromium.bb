export const description = `
F32Interval unit tests.
`;

import { makeTestGroup } from '../common/framework/test_group.js';
import { objectEquals } from '../common/util/util.js';
import { kValue } from '../webgpu/util/constants.js';
import {
  absoluteErrorInterval,
  correctlyRoundedInterval,
  F32Interval,
  ulpInterval,
} from '../webgpu/util/f32_interval.js';
import { hexToF32, hexToF64 } from '../webgpu/util/math.js';

import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(UnitTest);

/** Convert a pair of numbers in an array to a F32Interval
 *
 * Used for fluently specifying test params as `[a, b]` instead of
 * `new F32Interval(a, b)`
 */
function arrayToInterval(bounds: [number, number]): F32Interval {
  const [begin, end] = bounds;
  return new F32Interval(begin, end);
}

interface ContainsNumberCase {
  bounds: [number, number];
  value: number;
  expected: boolean;
}

g.test('contains_number')
  .paramsSubcasesOnly<ContainsNumberCase>(
    // prettier-ignore
    [
    // Common usage
    { bounds: [0, 10], value: 0, expected: true },
    { bounds: [0, 10], value: 10, expected: true },
    { bounds: [0, 10], value: 5, expected: true },
    { bounds: [0, 10], value: -5, expected: false },
    { bounds: [0, 10], value: 50, expected: false },
    { bounds: [0, 10], value: Number.NaN, expected: false },
    { bounds: [-5, 10], value: 0, expected: true },
    { bounds: [-5, 10], value: 10, expected: true },
    { bounds: [-5, 10], value: 5, expected: true },
    { bounds: [-5, 10], value: -5, expected: true },
    { bounds: [-5, 10], value: -6, expected: false },
    { bounds: [-5, 10], value: 50, expected: false },
    { bounds: [-5, 10], value: -10, expected: false },

    // Point
    { bounds: [0, 0], value: 0, expected: true },
    { bounds: [0, 0], value: 10, expected: false },
    { bounds: [0, 0], value: -1000, expected: false },
    { bounds: [10, 10], value: 10, expected: true },
    { bounds: [10, 10], value: 0, expected: false },
    { bounds: [10, 10], value: -10, expected: false },
    { bounds: [10, 10], value: 11, expected: false },

    // Upper infinity
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.positive.min, expected: true },
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.positive.max, expected: true },
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.infinity.positive, expected: true },
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.negative.min, expected: false },
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.negative.max, expected: false },
    { bounds: [0, kValue.f32.infinity.positive], value: kValue.f32.infinity.negative, expected: false },

    // Lower infinity
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.positive.min, expected: false },
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.positive.max, expected: false },
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.infinity.positive, expected: false },
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.negative.min, expected: true },
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.negative.max, expected: true },
    { bounds: [kValue.f32.infinity.negative, 0], value: kValue.f32.infinity.negative, expected: true },

    // Full infinity
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.positive.min, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.positive.max, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.infinity.positive, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.negative.min, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.negative.max, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: kValue.f32.infinity.negative, expected: true },
    { bounds: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], value: Number.NaN, expected: true },

    // Maximum f32 boundary
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.positive.min, expected: true },
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.positive.max, expected: true },
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.infinity.positive, expected: true },
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.negative.min, expected: false },
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.negative.max, expected: false },
    { bounds: [0, kValue.f32.positive.max], value: kValue.f32.infinity.negative, expected: false },

    // Minimum f32 boundary
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.positive.min, expected: false },
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.positive.max, expected: false },
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.infinity.positive, expected: false },
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.negative.min, expected: true },
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.negative.max, expected: true },
    { bounds: [kValue.f32.negative.min, 0], value: kValue.f32.infinity.negative, expected: true },

    // Subnormals
    { bounds: [0, kValue.f32.positive.min], value: kValue.f32.subnormal.positive.min, expected: true },
    { bounds: [0, kValue.f32.positive.min], value: kValue.f32.subnormal.positive.max, expected: true },
    { bounds: [0, kValue.f32.positive.min], value: kValue.f32.subnormal.negative.min, expected: false },
    { bounds: [0, kValue.f32.positive.min], value: kValue.f32.subnormal.negative.max, expected: false },
    { bounds: [kValue.f32.negative.max, 0], value: kValue.f32.subnormal.positive.min, expected: false },
    { bounds: [kValue.f32.negative.max, 0], value: kValue.f32.subnormal.positive.max, expected: false },
    { bounds: [kValue.f32.negative.max, 0], value: kValue.f32.subnormal.negative.min, expected: true },
    { bounds: [kValue.f32.negative.max, 0], value: kValue.f32.subnormal.negative.max, expected: true },
    { bounds: [0, kValue.f32.subnormal.positive.min], value: kValue.f32.subnormal.positive.min, expected: true },
    { bounds: [0, kValue.f32.subnormal.positive.min], value: kValue.f32.subnormal.positive.max, expected: false },
    { bounds: [0, kValue.f32.subnormal.positive.min], value: kValue.f32.subnormal.negative.min, expected: false },
    { bounds: [0, kValue.f32.subnormal.positive.min], value: kValue.f32.subnormal.negative.max, expected: false },
    { bounds: [kValue.f32.subnormal.negative.max, 0], value: kValue.f32.subnormal.positive.min, expected: false },
    { bounds: [kValue.f32.subnormal.negative.max, 0], value: kValue.f32.subnormal.positive.max, expected: false },
    { bounds: [kValue.f32.subnormal.negative.max, 0], value: kValue.f32.subnormal.negative.min, expected: false },
    { bounds: [kValue.f32.subnormal.negative.max, 0], value: kValue.f32.subnormal.negative.max, expected: true },
    ]
  )
  .fn(t => {
    const i = arrayToInterval(t.params.bounds);
    const value = t.params.value;
    const expected = t.params.expected;

    const got = i.contains(value);
    t.expect(expected === got, `${i}.contains(${value}) returned ${got}. Expected ${expected}`);
  });

interface ContainsIntervalCase {
  lhs: [number, number];
  rhs: [number, number];
  expected: boolean;
}

g.test('contains_interval')
  .paramsSubcasesOnly<ContainsIntervalCase>(
    // prettier-ignore
    [
      // Common usage
      { lhs: [-10, 10], rhs: [0, 0], expected: true},
      { lhs: [-10, 10], rhs: [-1, 0], expected: true},
      { lhs: [-10, 10], rhs: [0, 2], expected: true},
      { lhs: [-10, 10], rhs: [-1, 2], expected: true},
      { lhs: [-10, 10], rhs: [0, 10], expected: true},
      { lhs: [-10, 10], rhs: [-10, 2], expected: true},
      { lhs: [-10, 10], rhs: [-10, 10], expected: true},
      { lhs: [-10, 10], rhs: [-100, 10], expected: false},

      // Upper infinity
      { lhs: [0, kValue.f32.infinity.positive], rhs: [0, 0], expected: true},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [-1, 0], expected: false},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [0, 1], expected: true},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [0, kValue.f32.positive.max], expected: true},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [0, kValue.f32.infinity.positive], expected: true},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [100, kValue.f32.infinity.positive], expected: true},
      { lhs: [0, kValue.f32.infinity.positive], rhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], expected: false},

      // Lower infinity
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [0, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [-1, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [kValue.f32.negative.min, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [0, 1], expected: false},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [kValue.f32.infinity.negative, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [kValue.f32.infinity.negative, -100 ], expected: true},
      { lhs: [kValue.f32.infinity.negative, 0], rhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], expected: false},

      // Full infinity
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [0, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [-1, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [0, 1], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [0, kValue.f32.infinity.positive], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [100, kValue.f32.infinity.positive], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [kValue.f32.infinity.negative, 0], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [kValue.f32.infinity.negative, -100 ], expected: true},
      { lhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], rhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], expected: true},

      // Maximum f32 boundary
      { lhs: [0, kValue.f32.positive.max], rhs: [0, 0], expected: true},
      { lhs: [0, kValue.f32.positive.max], rhs: [-1, 0], expected: false},
      { lhs: [0, kValue.f32.positive.max], rhs: [0, 1], expected: true},
      { lhs: [0, kValue.f32.positive.max], rhs: [0, kValue.f32.positive.max], expected: true},
      { lhs: [0, kValue.f32.positive.max], rhs: [0, kValue.f32.infinity.positive], expected: true},
      { lhs: [0, kValue.f32.positive.max], rhs: [100, kValue.f32.infinity.positive], expected: true},
      { lhs: [0, kValue.f32.positive.max], rhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], expected: false},

      // Minimum f32 boundary
      { lhs: [kValue.f32.negative.min, 0], rhs: [0, 0], expected: true},
      { lhs: [kValue.f32.negative.min, 0], rhs: [-1, 0], expected: true},
      { lhs: [kValue.f32.negative.min, 0], rhs: [kValue.f32.negative.min, 0], expected: true},
      { lhs: [kValue.f32.negative.min, 0], rhs: [0, 1], expected: false},
      { lhs: [kValue.f32.negative.min, 0], rhs: [kValue.f32.infinity.negative, 0], expected: true},
      { lhs: [kValue.f32.negative.min, 0], rhs: [kValue.f32.infinity.negative, -100 ], expected: true},
      { lhs: [kValue.f32.negative.min, 0], rhs: [kValue.f32.infinity.negative, kValue.f32.infinity.positive], expected: false},
    ]
  )
  .fn(t => {
    const lhs = arrayToInterval(t.params.lhs);
    const rhs = arrayToInterval(t.params.rhs);
    const expected = t.params.expected;

    const got = lhs.contains(rhs);
    t.expect(expected === got, `${lhs}.contains(${rhs}) returned ${got}. Expected ${expected}`);
  });

interface SpanCase {
  intervals: Array<[number, number]>;
  expected: [number, number];
}

g.test('span')
  .paramsSubcasesOnly<SpanCase>(
    // prettier-ignore
    [
      // Single Intervals
      { intervals: [[0, 10]], expected: [0, 10]},
      { intervals: [[0, kValue.f32.positive.max]], expected: [0, kValue.f32.infinity.positive]},
      { intervals: [[0, kValue.f32.positive.nearest_max]], expected: [0, kValue.f32.positive.nearest_max]},
      { intervals: [[0, kValue.f32.infinity.positive]], expected: [0, kValue.f32.infinity.positive]},
      { intervals: [[kValue.f32.negative.min, 0]], expected: [kValue.f32.negative.min, 0]},
      { intervals: [[kValue.f32.negative.nearest_min, 0]], expected: [kValue.f32.negative.nearest_min, 0]},
      { intervals: [[kValue.f32.infinity.negative, 0]], expected: [kValue.f32.infinity.negative, 0]},

      // Double Intervals
      { intervals: [[0, 1], [2, 5]], expected: [0, 5]},
      { intervals: [[2, 5], [0, 1]], expected: [0, 5]},
      { intervals: [[0, 2], [1, 5]], expected: [0, 5]},
      { intervals: [[0, 5], [1, 2]], expected: [0, 5]},
      { intervals: [[kValue.f32.infinity.negative, 0], [0, kValue.f32.infinity.positive]], expected: [kValue.f32.infinity.negative, kValue.f32.infinity.positive]},

      // Multiple Intervals
      { intervals: [[0, 1], [2, 3], [4, 5]], expected: [0, 5]},
      { intervals: [[0, 1], [4, 5], [2, 3]], expected: [0, 5]},
      { intervals: [[0, 1], [0, 1], [0, 1]], expected: [0, 1]},
    ]
  )
  .fn(t => {
    const intervals = t.params.intervals.map(arrayToInterval);
    const expected = arrayToInterval(t.params.expected);

    const got = F32Interval.span(...intervals);
    t.expect(
      objectEquals(got, expected),
      `span({${intervals}}) returned ${got}. Expected ${expected}`
    );
  });

interface CorrectlyRoundedCase {
  value: number;
  expected: [number, number];
}

g.test('correctlyRoundedInterval')
  .paramsSubcasesOnly<CorrectlyRoundedCase>(
    // prettier-ignore
    [
      // Edge Cases
      { value: kValue.f32.infinity.positive, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.negative, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.positive.max, expected: [kValue.f32.positive.max, kValue.f32.positive.max] },
      { value: kValue.f32.negative.min, expected: [kValue.f32.negative.min, kValue.f32.negative.min] },
      { value: kValue.f32.positive.min, expected: [kValue.f32.positive.min, kValue.f32.positive.min] },
      { value: kValue.f32.negative.max, expected: [kValue.f32.negative.max, kValue.f32.negative.max] },

      // 32-bit subnormals
      { value: kValue.f32.subnormal.positive.min, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: kValue.f32.subnormal.positive.max, expected: [0, kValue.f32.subnormal.positive.max] },
      { value: kValue.f32.subnormal.negative.min, expected: [kValue.f32.subnormal.negative.min, 0] },
      { value: kValue.f32.subnormal.negative.max, expected: [kValue.f32.subnormal.negative.max, 0] },

      // 64-bit subnormals
      { value: hexToF64(0x00000000, 0x00000001), expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x00000000, 0x00000002), expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x800fffff, 0xffffffff), expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: hexToF64(0x800fffff, 0xfffffffe), expected: [kValue.f32.subnormal.negative.max, 0] },

      // 32-bit normals
      { value: 0, expected: [0, 0] },
      { value: hexToF32(0x03800000), expected: [hexToF32(0x03800000), hexToF32(0x03800000)] },
      { value: hexToF32(0x03800001), expected: [hexToF32(0x03800001), hexToF32(0x03800001)] },
      { value: hexToF32(0x83800000), expected: [hexToF32(0x83800000), hexToF32(0x83800000)] },
      { value: hexToF32(0x83800001), expected: [hexToF32(0x83800001), hexToF32(0x83800001)] },

      // 64-bit normals
      { value: hexToF64(0x3ff00000, 0x00000001), expected: [hexToF32(0x3f800000), hexToF32(0x3f800001)] },
      { value: hexToF64(0x3ff00000, 0x00000002), expected: [hexToF32(0x3f800000), hexToF32(0x3f800001)] },
      { value: hexToF64(0x3ff00010, 0x00000010), expected: [hexToF32(0x3f800080), hexToF32(0x3f800081)] },
      { value: hexToF64(0x3ff00020, 0x00000020), expected: [hexToF32(0x3f800100), hexToF32(0x3f800101)] },
      { value: hexToF64(0xbff00000, 0x00000001), expected: [hexToF32(0xbf800001), hexToF32(0xbf800000)] },
      { value: hexToF64(0xbff00000, 0x00000002), expected: [hexToF32(0xbf800001), hexToF32(0xbf800000)] },
      { value: hexToF64(0xbff00010, 0x00000010), expected: [hexToF32(0xbf800081), hexToF32(0xbf800080)] },
      { value: hexToF64(0xbff00020, 0x00000020), expected: [hexToF32(0xbf800101), hexToF32(0xbf800100)] },
    ]
  )
  .fn(t => {
    const value = t.params.value;
    const expected = arrayToInterval(t.params.expected);

    const got = correctlyRoundedInterval(value);
    t.expect(
      objectEquals(expected, got),
      `correctlyRoundedInterval(${value}) returned ${got}. Expected ${expected}`
    );
  });

interface AbsoluteErrorCase {
  value: number;
  error: number;
  expected: [number, number];
}

g.test('absoluteErrorInterval')
  .paramsSubcasesOnly<AbsoluteErrorCase>(
    // prettier-ignore
    [
      // Edge Cases
      { value: kValue.f32.infinity.positive, error: 0, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.positive, error: 2 ** -11, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.positive, error: 1, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.negative, error: 0, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.infinity.negative, error: 2 ** -11, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.infinity.negative, error: 1, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.positive.max, error: 0, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.max, error: 2 ** -11, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.max, error: 1, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.min, error: 0, expected: [kValue.f32.positive.min,  kValue.f32.positive.min] },
      { value: kValue.f32.positive.min, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.positive.min, error: 1, expected: [-1, 1] },
      { value: kValue.f32.negative.min, error: 0, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.negative.min, error: 2 ** -11, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.negative.min, error: 1, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.negative.max, error: 0, expected: [kValue.f32.negative.max, kValue.f32.negative.max] },
      { value: kValue.f32.negative.max, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.negative.max, error: 1, expected: [-1, 1] },

      // 32-bit subnormals
      { value: kValue.f32.subnormal.positive.max, error: 0, expected: [0, kValue.f32.subnormal.positive.max] },
      { value: kValue.f32.subnormal.positive.max, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.subnormal.positive.max, error: 1, expected: [-1, 1] },
      { value: kValue.f32.subnormal.positive.min, error: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: kValue.f32.subnormal.positive.min, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.subnormal.positive.min, error: 1, expected: [-1, 1] },
      { value: kValue.f32.subnormal.negative.min, error: 0, expected: [kValue.f32.subnormal.negative.min, 0] },
      { value: kValue.f32.subnormal.negative.min, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.subnormal.negative.min, error: 1, expected: [-1, 1] },
      { value: kValue.f32.subnormal.negative.max, error: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: kValue.f32.subnormal.negative.max, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: kValue.f32.subnormal.negative.max, error: 1, expected: [-1, 1] },

      // 64-bit subnormals
      { value: hexToF64(0x00000000, 0x00000001), error: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x00000000, 0x00000001), error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: hexToF64(0x00000000, 0x00000001), error: 1, expected: [-1, 1] },
      { value: hexToF64(0x00000000, 0x00000002), error: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x00000000, 0x00000002), error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: hexToF64(0x00000000, 0x00000002), error: 1, expected: [-1, 1] },
      { value: hexToF64(0x800fffff, 0xffffffff), error: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: hexToF64(0x800fffff, 0xffffffff), error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: hexToF64(0x800fffff, 0xffffffff), error: 1, expected: [-1, 1] },
      { value: hexToF64(0x800fffff, 0xfffffffe), error: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: hexToF64(0x800fffff, 0xfffffffe), error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: hexToF64(0x800fffff, 0xfffffffe), error: 1, expected: [-1, 1] },

      // Zero
      { value: 0, error: 0, expected: [0, 0] },
      { value: 0, error: 2 ** -11, expected: [-(2 ** -11), 2 ** -11] },
      { value: 0, error: 1, expected: [-1, 1] },
    ]
  )
  .fn(t => {
    const value = t.params.value;
    const error = t.params.error;
    const expected = arrayToInterval(t.params.expected);

    const got = absoluteErrorInterval(value, error);
    t.expect(
      objectEquals(expected, got),
      `absoluteErrorInterval(${value}, ${error}) returned ${got}. Expected ${expected}`
    );
  });

interface ULPCase {
  value: number;
  num_ulp: number;
  expected: [number, number];
}

g.test('ulpInterval')
  .paramsSubcasesOnly<ULPCase>(
    // prettier-ignore
    [
      // Edge Cases
      { value: kValue.f32.infinity.positive, num_ulp: 0, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.positive, num_ulp: 1, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.positive, num_ulp: 4096, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.infinity.negative, num_ulp: 0, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.infinity.negative, num_ulp: 1, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.infinity.negative, num_ulp: 4096, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.min] },
      { value: kValue.f32.positive.max, num_ulp: 0, expected: [kValue.f32.positive.max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.max, num_ulp: 1, expected: [kValue.f32.positive.nearest_max, Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.max, num_ulp: 4096, expected: [hexToF32(0x7f7fefff), Number.POSITIVE_INFINITY] },
      { value: kValue.f32.positive.min, num_ulp: 0, expected: [ kValue.f32.positive.min,  kValue.f32.positive.min] },
      { value: kValue.f32.positive.min, num_ulp: 1, expected: [0, hexToF32(0x00800001)] },
      { value: kValue.f32.positive.min, num_ulp: 4096, expected: [0, hexToF32(0x00801000)] },
      { value: kValue.f32.negative.min, num_ulp: 0, expected: [kValue.f32.negative.min, kValue.f32.negative.min] },
      { value: kValue.f32.negative.min, num_ulp: 1, expected: [Number.NEGATIVE_INFINITY, kValue.f32.negative.nearest_min] },
      { value: kValue.f32.negative.min, num_ulp: 4096, expected: [Number.NEGATIVE_INFINITY, hexToF32(0xff7fefff)] },
      { value: kValue.f32.negative.max, num_ulp: 0, expected: [kValue.f32.negative.max, kValue.f32.negative.max] },
      { value: kValue.f32.negative.max, num_ulp: 1, expected: [hexToF32(0x80800001), 0] },
      { value: kValue.f32.negative.max, num_ulp: 4096, expected: [hexToF32(0x80801000), 0] },

      // 32-bit subnormals
      { value: kValue.f32.subnormal.positive.max, num_ulp: 0, expected: [0, kValue.f32.subnormal.positive.max] },
      { value: kValue.f32.subnormal.positive.max, num_ulp: 1, expected: [kValue.f32.negative.max, hexToF32(0x00ffffff)] },
      { value: kValue.f32.subnormal.positive.max, num_ulp: 4096, expected: [hexToF32(0x86800000), hexToF64(0x38d000ff, 0xfffe0000)] },
      { value: kValue.f32.subnormal.positive.min, num_ulp: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: kValue.f32.subnormal.positive.min, num_ulp: 1, expected: [kValue.f32.negative.max, hexToF32(0x00800001)] },
      { value: kValue.f32.subnormal.positive.min, num_ulp: 4096, expected: [hexToF32(0x86800000), hexToF64(0x38d00000, 0x00020000)] },
      { value: kValue.f32.subnormal.negative.min, num_ulp: 0, expected: [kValue.f32.subnormal.negative.min, 0] },
      { value: kValue.f32.subnormal.negative.min, num_ulp: 1, expected: [hexToF32(0x80ffffff), kValue.f32.positive.min] },
      { value: kValue.f32.subnormal.negative.min, num_ulp: 4096, expected: [hexToF64(0xb8d000ff, 0xfffe0000), hexToF32(0x06800000)] },
      { value: kValue.f32.subnormal.negative.max, num_ulp: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: kValue.f32.subnormal.negative.max, num_ulp: 1, expected: [hexToF32(0x80800001), kValue.f32.positive.min] },
      { value: kValue.f32.subnormal.negative.max, num_ulp: 4096, expected: [hexToF64(0xb8d00000, 0x00020000), hexToF32(0x06800000)] },

      // 64-bit subnormals
      { value: hexToF64(0x00000000, 0x00000001), num_ulp: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x00000000, 0x00000001), num_ulp: 1, expected: [kValue.f32.negative.max, hexToF32(0x00800001)] },
      { value: hexToF64(0x00000000, 0x00000001), num_ulp: 4096, expected: [hexToF32(0x86800000), hexToF64(0x38d00000, 0x00020000)] },
      { value: hexToF64(0x00000000, 0x00000002), num_ulp: 0, expected: [0, kValue.f32.subnormal.positive.min] },
      { value: hexToF64(0x00000000, 0x00000002), num_ulp: 1, expected: [kValue.f32.negative.max, hexToF32(0x00800001)] },
      { value: hexToF64(0x00000000, 0x00000002), num_ulp: 4096, expected: [hexToF32(0x86800000), hexToF64(0x38d00000, 0x00020000)] },
      { value: hexToF64(0x800fffff, 0xffffffff), num_ulp: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: hexToF64(0x800fffff, 0xffffffff), num_ulp: 1, expected: [hexToF32(0x80800001), kValue.f32.positive.min] },
      { value: hexToF64(0x800fffff, 0xffffffff), num_ulp: 4096, expected: [hexToF64(0xb8d00000, 0x00020000), hexToF32(0x06800000)] },
      { value: hexToF64(0x800fffff, 0xfffffffe), num_ulp: 0, expected: [kValue.f32.subnormal.negative.max, 0] },
      { value: hexToF64(0x800fffff, 0xfffffffe), num_ulp: 1, expected: [hexToF32(0x80800001), kValue.f32.positive.min] },
      { value: hexToF64(0x800fffff, 0xfffffffe), num_ulp: 4096, expected: [hexToF64(0xb8d00000, 0x00020000), hexToF32(0x06800000)] },

      // Zero
      { value: 0, num_ulp: 0, expected: [0, 0] },
      { value: 0, num_ulp: 1, expected: [kValue.f32.negative.max, kValue.f32.positive.min] },
      { value: 0, num_ulp: 4096, expected: [hexToF32(0x86800000), hexToF32(0x06800000)] },
    ]
  )
  .fn(t => {
    const value = t.params.value;
    const num_ulp = t.params.num_ulp;
    const expected = arrayToInterval(t.params.expected);

    const got = ulpInterval(value, num_ulp);
    t.expect(
      objectEquals(expected, got),
      `ulpInterval(${value}, ${num_ulp}) returned ${got}. Expected ${expected}`
    );
  });
