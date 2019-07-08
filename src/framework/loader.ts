import { ParamsAny } from './params/index.js';
import { RunCaseIterable } from './test_group.js';
import { allowedTestNameCharacters } from './allowed_characters.js';
import { TestSuiteListing } from './listing.js';
import { TestSpecID } from './id.js';
import {
  FilterByGroup,
  FilterByTestMatch,
  FilterByParamsMatch,
  FilterByParamsExact,
} from './test_filter/index.js';

// One of the following:
// - An actual .spec.ts file, as imported.
// - A *filtered* list of cases from a single .spec.ts file.
export interface TestSpecFile {
  readonly description: string;
  readonly g: RunCaseIterable;
}

// A shell object describing a directory (from its README.txt).
export interface ReadmeFile {
  readonly description: string;
}

export type TestSpecOrReadme = TestSpecFile | ReadmeFile;

// A pending loaded spec (.spec.ts) file, plus identifying information.
export interface TestQueryResult {
  readonly id: TestSpecID;
  readonly spec: Promise<TestSpecOrReadme>;
}

export type TestQueryResults = IterableIterator<TestQueryResult>;

function* concat(lists: TestQueryResult[][]): TestQueryResults {
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

  import(path: string): Promise<TestSpecFile> {
    return import('../suites/' + path);
  }
}

export class TestLoader {
  private fileLoader: TestFileLoader;

  constructor(fileLoader: TestFileLoader = new DefaultTestFileLoader()) {
    this.fileLoader = fileLoader;
  }

  async loadTestsFromQuery(query: string): Promise<TestQueryResults> {
    return this.loadTests(new URLSearchParams(query).getAll('q'));
  }

  async loadTestsFromCmdLine(filters: string[]): Promise<TestQueryResults> {
    // In actual URL queries (?q=...), + represents a space. But decodeURIComponent doesn't do this,
    // so do it manually. (+ is used over %20 for readability.) (See also encodeSelectively.)
    return this.loadTests(filters.map(f => decodeURIComponent(f.replace(/\+/g, '%20'))));
  }

  async loadTests(filters: string[]): Promise<TestQueryResults> {
    const loads = filters.map(f => this.loadFilter(f));
    return concat(await Promise.all(loads));
  }

  // Each filter is of one of the forms below (urlencoded).
  private async loadFilter(filter: string): Promise<TestQueryResult[]> {
    const i1 = filter.indexOf(':');
    if (i1 === -1) {
      throw new Error('Test queries must fully specify their suite name (e.g. "cts:")');
    }

    const suite = filter.substring(0, i1);
    const i2 = filter.indexOf(':', i1 + 1);
    if (i2 === -1) {
      // - cts:
      // - cts:buf
      // - cts:buffers/
      // - cts:buffers/map
      const groupPrefix = filter.substring(i1 + 1);
      return new FilterByGroup(suite, groupPrefix).iterate(this.fileLoader);
    }

    const path = filter.substring(i1 + 1, i2);
    const endOfTestName = new RegExp('[^' + allowedTestNameCharacters + ']');
    const i3sub = filter.substring(i2 + 1).search(endOfTestName);
    if (i3sub === -1) {
      // - cts:buffers/mapWriteAsync:
      // - cts:buffers/mapWriteAsync:ba
      const testPrefix = filter.substring(i2 + 1);
      return new FilterByTestMatch({ suite, path }, testPrefix).iterate(this.fileLoader);
    }

    const i3 = i2 + 1 + i3sub;
    const test = filter.substring(i2 + 1, i3);
    const token = filter.charAt(i3);

    let params = null;
    if (i3 + 1 < filter.length) {
      params = JSON.parse(filter.substring(i3 + 1)) as ParamsAny;
    }

    if (token === '~') {
      // - cts:buffers/mapWriteAsync:basic~
      // - cts:buffers/mapWriteAsync:basic~{}
      // - cts:buffers/mapWriteAsync:basic~{filter:"params"}
      return new FilterByParamsMatch({ suite, path }, test, params).iterate(this.fileLoader);
    } else if (token === ':') {
      // - cts:buffers/mapWriteAsync:basic:
      // - cts:buffers/mapWriteAsync:basic:{}
      // - cts:buffers/mapWriteAsync:basic:{exact:"params"}
      return new FilterByParamsExact({ suite, path }, test, params).iterate(this.fileLoader);
    } else {
      throw new Error("invalid character after test name; must be '~' or ':'");
    }
  }
}
