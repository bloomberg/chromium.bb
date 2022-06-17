export const description = `
Execution tests for the 'exp2' builtin function

S is AbstractFloat, f32, f16
T is S or vecN<S>
@const fn exp2(e: T ) -> T
Returns 2 raised to the power e (e.g. 2^e). Component-wise when T is a vector.
`;

import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { ulpComparator } from '../../../../../util/compare.js';
import { kBit, kValue } from '../../../../../util/constants.js';
import { f32, f32Bits, TypeF32 } from '../../../../../util/conversion.js';
import { biasedRange } from '../../../../../util/math.js';
import { allInputSources, Case, run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('abstract_float')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(`abstract float tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4] as const)
  )
  .unimplemented();

g.test('f32')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(`f32 tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4] as const)
  )
  .fn(async t => {
    const n = (x: number): number => {
      return 3 + 2 * Math.abs(x);
    };

    const makeCase = (x: number): Case => {
      const expected = f32(Math.pow(2, x));
      return { input: f32(x), expected: ulpComparator(x, expected, n) };
    };

    // floor(log2(max f32 value)) = 127, so exp2(127) will be within range of a f32, but exp2(128) will not
    const cases: Array<Case> = [
      makeCase(0), // Returns 1 by definition
      makeCase(-127), // Returns subnormal value
      makeCase(kValue.f32.negative.min), // Closest to returning 0 as possible
      { input: f32(128), expected: f32Bits(kBit.f32.infinity.positive) }, // Overflows
      ...biasedRange(kValue.f32.negative.max, -127, 100).map(x => makeCase(x)),
      ...biasedRange(kValue.f32.positive.min, 127, 100).map(x => makeCase(x)),
    ];

    run(t, builtin('exp2'), [TypeF32], TypeF32, t.params, cases);
  });

g.test('f16')
  .specURL('https://www.w3.org/TR/WGSL/#float-builtin-functions')
  .desc(`f16 tests`)
  .params(u =>
    u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4] as const)
  )
  .unimplemented();
