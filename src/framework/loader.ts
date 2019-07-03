import { GroupRecorder } from './logger.js';
import { ParamsAny, paramsEquals, paramsSupersets } from './params/index.js';
import { RunCaseIterable, TestCaseID, RunCase } from './test_group.js';
import { allowedTestNameCharacters } from './allowed_characters.js';
import { TestSuiteListing } from './listing.js';

// One of the following:
// - A shell object describing a directory (from its README.txt).
// - An actual .spec.ts file, as imported.
// - A *filtered* list of cases from a single .spec.ts file.
export interface TestSpecFile {
  readonly description: string;
  // undefined for README.txt, defined for a test module.
  readonly g?: RunCaseIterable;
}

// Identifies a test spec (a .spec.ts file).
export interface TestSpecPath {
  // The spec's suite, e.g. 'cts'.
  readonly suite: string;
  // The spec's path within the suite, e.g. 'command_buffer/compute/basic'.
  readonly path: string;
}

type TestQueryResults = IterableIterator<TestQueryResult>;
export interface TestQueryResult extends TestSpecPath {
  readonly spec: Promise<TestSpecFile>;
}

function* concat(lists: TestQueryResult[][]): TestQueryResults {
  for (const specs of lists) {
    yield* specs;
  }
}

type TestGroupFilter = (testcase: TestCaseID) => boolean;
function filterTestGroup(group: RunCaseIterable, filter: TestGroupFilter): RunCaseIterable {
  return {
    *iterate(log: GroupRecorder): Iterable<RunCase> {
      for (const rc of group.iterate(log)) {
        if (filter(rc.id)) {
          yield rc;
        }
      }
    },
  };
}

export interface TestFileLoader {
  listing(suite: string): Promise<TestSuiteListing>;
  import(path: string): Promise<TestSpecFile>;
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
      return this.filterByGroup(suite, await this.fileLoader.listing(suite), groupPrefix);
    }

    const path = filter.substring(i1 + 1, i2);
    const endOfTestName = new RegExp('[^' + allowedTestNameCharacters + ']');
    const i3sub = filter.substring(i2 + 1).search(endOfTestName);
    if (i3sub === -1) {
      // - cts:buffers/mapWriteAsync:
      // - cts:buffers/mapWriteAsync:ba
      const testPrefix = filter.substring(i2 + 1);
      return [
        {
          suite,
          path,
          spec: this.filterByTestMatch(suite, path, testPrefix),
        },
      ];
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
      return [
        {
          suite,
          path,
          spec: this.filterByParamsMatch(suite, path, test, params),
        },
      ];
    } else if (token === ':') {
      // - cts:buffers/mapWriteAsync:basic:
      // - cts:buffers/mapWriteAsync:basic:{}
      // - cts:buffers/mapWriteAsync:basic:{exact:"params"}
      return [
        {
          suite,
          path,
          spec: this.filterByParamsExact(suite, path, test, params),
        },
      ];
    } else {
      throw new Error("invalid character after test name; must be '~' or ':'");
    }
  }

  private filterByGroup(
    suite: string,
    specs: TestSuiteListing,
    groupPrefix: string
  ): TestQueryResult[] {
    const entries: TestQueryResult[] = [];

    for (const { path, description } of specs) {
      if (path.startsWith(groupPrefix)) {
        const isReadme = path === '' || path.endsWith('/');
        const spec: Promise<TestSpecFile> = isReadme
          ? Promise.resolve({ description })
          : this.fileLoader.import(`${suite}/${path}.spec.js`);
        entries.push({ suite, path, spec });
      }
    }

    return entries;
  }

  private async filterByTestMatch(
    suite: string,
    group: string,
    testPrefix: string
  ): Promise<TestSpecFile> {
    const spec = (await this.fileLoader.import(`${suite}/${group}.spec.js`)) as TestSpecFile;
    if (!spec.g) {
      return spec;
    }
    return {
      description: spec.description,
      g: filterTestGroup(spec.g, testcase => testcase.name.startsWith(testPrefix)),
    };
  }

  private async filterByParamsMatch(
    suite: string,
    group: string,
    test: string,
    paramsMatch: ParamsAny | null
  ): Promise<TestSpecFile> {
    const spec = (await this.fileLoader.import(`${suite}/${group}.spec.js`)) as TestSpecFile;
    if (!spec.g) {
      return spec;
    }
    return {
      description: spec.description,
      g: filterTestGroup(
        spec.g,
        testcase => testcase.name === test && paramsSupersets(testcase.params, paramsMatch)
      ),
    };
  }

  private async filterByParamsExact(
    suite: string,
    group: string,
    test: string,
    paramsExact: ParamsAny | null
  ): Promise<TestSpecFile> {
    const spec = (await this.fileLoader.import(`${suite}/${group}.spec.js`)) as TestSpecFile;
    if (!spec.g) {
      return spec;
    }
    return {
      description: spec.description,
      g: filterTestGroup(
        spec.g,
        testcase => testcase.name === test && paramsEquals(testcase.params, paramsExact)
      ),
    };
  }
}
