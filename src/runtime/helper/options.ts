const url = new URL(window.location.toString());

export function optionEnabled(opt: string): boolean {
  const val = url.searchParams.get(opt);
  return val !== null && val !== '0';
}
