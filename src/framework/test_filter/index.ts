import { TestSpecID, TestCaseID } from '../id.js';
import { TestFileLoader, TestQueryResult } from '../loader.js';

export * from './filter_by_group.js';
export * from './filter_one_file.js';

export interface TestFilter {
  matches(spec: TestSpecID, testcase: TestCaseID): boolean;
  iterate(loader: TestFileLoader): Promise<TestQueryResult[]>;
}
