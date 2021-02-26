export function align(n: number, alignment: number): number {
  return Math.ceil(n / alignment) * alignment;
}

export function isAligned(n: number, alignment: number): boolean {
  return n === align(n, alignment);
}

export const kMaxSafeMultipleOf8 = Number.MAX_SAFE_INTEGER - 7;
