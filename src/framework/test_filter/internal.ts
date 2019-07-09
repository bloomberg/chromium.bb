import { TestSpecID, TestCaseID } from '../id.js';
import { TestFilterResult } from './index.js';
import { TestFileLoader } from '../loader.js';

export interface TestFilter {
  // Checks whether a test case matches a filter. Used to implement negative filters.
  //
  // TODO: actually use this
  matches(spec: TestSpecID, testcase: TestCaseID): boolean;

  // Iterates over the test cases matched by the filter.
  iterate(loader: TestFileLoader): Promise<TestFilterResult[]>;
}
