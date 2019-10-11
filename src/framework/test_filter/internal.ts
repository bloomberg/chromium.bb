import { TestFileLoader } from '../loader.js';

import { TestFilterResult } from './index.js';

export interface TestFilter {
  // Iterates over the test cases matched by the filter.
  iterate(loader: TestFileLoader): Promise<TestFilterResult[]>;
}
