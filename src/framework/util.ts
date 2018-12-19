export function getStackTrace(): string {
  const e = new Error();
  if (e.stack) {
    return e.stack.split("\n").splice(2).map((s) => s.trim()).join("\n");
  } else {
    return "";
  }
}
