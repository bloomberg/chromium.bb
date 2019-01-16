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
