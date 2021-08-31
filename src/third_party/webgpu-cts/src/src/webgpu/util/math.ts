import { assert } from '../../common/framework/util/util.js';

export function align(n: number, alignment: number): number {
  return Math.ceil(n / alignment) * alignment;
}

export function isAligned(n: number, alignment: number): boolean {
  return n === align(n, alignment);
}

export const kMaxSafeMultipleOf8 = Number.MAX_SAFE_INTEGER - 7;

export function clamp(n: number, min: number, max: number): number {
  assert(max >= min);
  return Math.min(Math.max(n, min), max);
}
