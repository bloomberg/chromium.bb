import { timeout } from './timeout.js';

export function assert(condition: boolean, msg?: string | (() => string)): asserts condition {
  if (!condition) {
    throw new Error(msg && (typeof msg === 'string' ? msg : msg()));
  }
}

export async function assertReject(p: Promise<unknown>, msg?: string): Promise<void> {
  try {
    await p;
    unreachable(msg);
  } catch (ex) {
    // Assertion OK
  }
}

export function unreachable(msg?: string): never {
  throw new Error(msg);
}

// performance.now() is available in all browsers, but not in scope by default in Node.
const perf = typeof performance !== 'undefined' ? performance : require('perf_hooks').performance;

export function now(): number {
  return perf.now();
}

export function resolveOnTimeout(ms: number): Promise<void> {
  return new Promise(resolve => {
    timeout(() => {
      resolve();
    }, ms);
  });
}

export class PromiseTimeoutError extends Error {}

export function rejectOnTimeout(ms: number, msg: string): Promise<never> {
  return new Promise((_resolve, reject) => {
    timeout(() => {
      reject(new PromiseTimeoutError(msg));
    }, ms);
  });
}

export function raceWithRejectOnTimeout<T>(p: Promise<T>, ms: number, msg: string): Promise<T> {
  return Promise.race([p, rejectOnTimeout(ms, msg)]);
}

export function sortObjectByKey(v: { [k: string]: unknown }): { [k: string]: unknown } {
  const sortedObject: { [k: string]: unknown } = {};
  for (const k of Object.keys(v).sort()) {
    sortedObject[k] = v[k];
  }
  return sortedObject;
}

export function objectEquals(x: unknown, y: unknown): boolean {
  if (typeof x !== 'object' || typeof y !== 'object') return x === y;
  if (x === null || y === null) return x === y;
  if (x.constructor !== y.constructor) return false;
  if (x instanceof Function) return x === y;
  if (x instanceof RegExp) return x === y;
  if (x === y || x.valueOf() === y.valueOf()) return true;
  if (Array.isArray(x) && Array.isArray(y) && x.length !== y.length) return false;
  if (x instanceof Date) return false;
  if (!(x instanceof Object)) return false;
  if (!(y instanceof Object)) return false;

  const x1 = x as { [k: string]: unknown };
  const y1 = y as { [k: string]: unknown };
  const p = Object.keys(x);
  return Object.keys(y).every(i => p.indexOf(i) !== -1) && p.every(i => objectEquals(x1[i], y1[i]));
}

export function range<T>(n: number, fn: (i: number) => T): T[] {
  return [...new Array(n)].map((_, i) => fn(i));
}
