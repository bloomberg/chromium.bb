import { TestSpecID, TestCaseID } from '../id.js';
import { TestFileLoader, TestSpecOrReadme } from '../loader.js';

export * from './load_filter.js';

// Result of iterating a test filter. Contains a loaded spec (.spec.ts) file and its id.
export interface TestFilterResult {
  readonly id: TestSpecID;
  readonly spec: TestSpecOrReadme;
}

export interface TestFilter {
  // Checks whether a test case matches a filter. Used to implement negative filters.
  //
  // TODO: actually use this
  matches(spec: TestSpecID, testcase: TestCaseID): boolean;

  // Iterates over the test cases matched by the filter.
  iterate(loader: TestFileLoader): Promise<TestFilterResult[]>;
}
