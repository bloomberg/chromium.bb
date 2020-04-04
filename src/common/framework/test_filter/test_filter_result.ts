import { TestSpecID } from '../id.js';
import { TestSpecOrReadme } from '../loader.js';

// Result of iterating a test filter. Contains a loaded spec (.spec.ts) file and its id.
export interface TestFilterResult {
  readonly id: TestSpecID;
  readonly spec: TestSpecOrReadme;
}
