import { TestSuiteListing } from './listing.js';
import { TestFilterResult, loadFilter } from './test_filter/index.js';
import { RunCaseIterable } from './test_group.js';

// One of the following:
// - An actual .spec.ts file, as imported.
// - A *filtered* list of cases from a single .spec.ts file.
export interface TestSpec {
  readonly description: string;
  readonly g: RunCaseIterable;
}

// A shell object describing a directory (from its README.txt).
export interface ReadmeFile {
  readonly description: string;
}

export type TestSpecOrReadme = TestSpec | ReadmeFile;

type TestFilterResultIterator = IterableIterator<TestFilterResult>;
function* concat(lists: TestFilterResult[][]): TestFilterResultIterator {
  for (const specs of lists) {
    yield* specs;
  }
}

export interface TestFileLoader {
  listing(suite: string): Promise<TestSuiteListing>;
  import(path: string): Promise<TestSpecOrReadme>;
}

class DefaultTestFileLoader implements TestFileLoader {
  async listing(suite: string): Promise<TestSuiteListing> {
    return (await import(`../suites/${suite}/index.js`)).listing;
  }

  import(path: string): Promise<TestSpec> {
    return import('../suites/' + path);
  }
}

export class TestLoader {
  private fileLoader: TestFileLoader;

  constructor(fileLoader: TestFileLoader = new DefaultTestFileLoader()) {
    this.fileLoader = fileLoader;
  }

  // TODO: Test
  async loadTestsFromQuery(query: string): Promise<TestFilterResultIterator> {
    return this.loadTests(new URLSearchParams(query).getAll('q'));
  }

  // TODO: Test
  // TODO: Probably should actually not exist at all, just use queries on cmd line too.
  async loadTestsFromCmdLine(filters: string[]): Promise<TestFilterResultIterator> {
    return this.loadTests(filters);
  }

  async loadTests(filters: string[]): Promise<TestFilterResultIterator> {
    const loads = filters.map(f => loadFilter(this.fileLoader, f));
    return concat(await Promise.all(loads));
  }
}
