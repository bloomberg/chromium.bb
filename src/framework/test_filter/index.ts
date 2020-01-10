import { TestSpecID } from '../id.js';
import { TestSpecOrReadme } from '../loader.js';

export { loadFilter, makeFilter } from './load_filter.js';

// Result of iterating a test filter. Contains a loaded spec (.spec.ts) file and its id.
export interface TestFilterResult {
  readonly id: TestSpecID;
  readonly spec: TestSpecOrReadme;
}
