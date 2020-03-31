// A listing of all specs within a single suite. This is the (awaited) type of
// `groups` in '{cts,unittests}/index.ts' and the auto-generated
// 'out/{cts,unittests}/index.js' files (see tools/gen_listings).
export type TestSuiteListing = Iterable<TestSuiteListingEntry>;

export interface TestSuiteListingEntry {
  readonly path: string;
  readonly description: string;
}
