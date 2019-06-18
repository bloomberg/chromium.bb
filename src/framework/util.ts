import { CaseRecorder } from './logger.js';

export function getStackTrace(e: Error): string {
  if (!e.stack) {
    return '';
  }

  const parts = e.stack.split('\n');
  const stack = [];

  let distanceFromTop = 0;
  for (const part of parts) {
    if (part.indexOf('internalTickCallback') > -1) {
      break;
    }
    if (part.indexOf('Object.run') > -1) {
      break;
    }
    if (part.indexOf(CaseRecorder.name) > -1) {
      // 0: CaseRecorder.fail
      // 1: DefaultFixture.fail
      // 2: actual test
      distanceFromTop = 2;
      stack.length = 0;
    }

    if (distanceFromTop <= 0) {
      stack.push(part.trim());
    }
    distanceFromTop -= 1;
  }
  return stack.join('\n');
}

// tslint:disable-next-line no-var-requires
const perf = typeof performance !== 'undefined' ? performance : require('perf_hooks').performance;

export function now(): number {
  return perf.now();
}

export function objectEquals(x: any, y: any): boolean {
  if (x === null || x === undefined || y === null || y === undefined) {
    return x === y;
  }
  if (x.constructor !== y.constructor) {
    return false;
  }
  if (x instanceof Function) {
    return x === y;
  }
  if (x instanceof RegExp) {
    return x === y;
  }
  if (x === y || x.valueOf() === y.valueOf()) {
    return true;
  }
  if (Array.isArray(x) && Array.isArray(y) && x.length !== y.length) {
    return false;
  }
  if (x instanceof Date) {
    return false;
  }
  if (!(x instanceof Object)) {
    return false;
  }
  if (!(y instanceof Object)) {
    return false;
  }

  const p = Object.keys(x);
  return Object.keys(y).every((i) => p.indexOf(i) !== -1) &&
    p.every((i) => objectEquals(x[i], y[i]));
}
