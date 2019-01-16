export function getStackTrace(): string {
  const e = new Error();
  if (e.stack) {
    return e.stack.split("\n").splice(4, 1).map((s) => s.trim()).join("\n");
  } else {
    return "";
  }
}

// tslint:disable-next-line no-var-requires
const perf = typeof performance !== "undefined" ? performance : require("perf_hooks").performance;

export function now(): number {
  return perf.now();
}

export function objectEquals(x: any, y: any): boolean {
  if (x === null || x === undefined || y === null || y === undefined) { return x === y; }
  if (x.constructor !== y.constructor) { return false; }
  if (x instanceof Function) { return x === y; }
  if (x instanceof RegExp) { return x === y; }
  if (x === y || x.valueOf() === y.valueOf()) { return true; }
  if (Array.isArray(x) && Array.isArray(y) && x.length !== y.length) { return false; }
  if (x instanceof Date) { return false; }
  if (!(x instanceof Object)) { return false; }
  if (!(y instanceof Object)) { return false; }

  const p = Object.keys(x);
  return Object.keys(y).every(function(i) { return p.indexOf(i) !== -1; }) &&
      p.every(function(i) { return objectEquals(x[i], y[i]); });
}
