import { TestSpecOrTestOrCaseID } from '../id.js';
import { TestFileLoader } from '../loader.js';

import { TestFilterResult } from './index.js';

export interface TestFilter {
  // Iterates over the test cases matched by the filter.
  iterate(loader: TestFileLoader): Promise<TestFilterResult[]>;

  // Iff the filter could not possibly match multiple files, returns true.
  definitelyOneFile(): boolean;

  // If the filter can accept one spec, one test, or one case, returns its ID.
  idIfSingle(): TestSpecOrTestOrCaseID | undefined;

  matches(id: TestSpecOrTestOrCaseID): boolean;
}
