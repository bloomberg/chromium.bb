// A listing of all specs within a single suite. This is the (awaited) type of
// `groups` in '{cts,unittests}/listing.ts' and the auto-generated
// 'out/{cts,unittests}/listing.js' files (see tools/gen_listings).
export type TestSuiteListing = Iterable<TestSuiteListingEntry>;

export interface TestSuiteListingEntry {
  readonly path: string;
  readonly description: string;
}
