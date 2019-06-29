import { GroupRecorder } from './logger.js';
import { IParamsAny, paramsEquals, paramsSupersets } from './params/index.js';
import { allowedTestNameCharacters, ITestGroup, RunCase, ICaseID } from './test_group.js';

export interface IGroupDesc {
  readonly path: string;
  readonly description: string;
}

export interface IListing {
  readonly suite: string;
  readonly groups: Iterable<IGroupDesc>;
}

export interface ITestNode {
  // undefined for README.txt, defined for a test module.
  readonly g?: ITestGroup;
  readonly description: string;
}

export interface IEntry {
  // a suite, e.g. "cts".
  readonly suite: string;
  // a path within a suite, e.g. "command_buffer/compute/basic".
  readonly path: string;
}

interface IPendingEntry extends IEntry {
  readonly node: Promise<ITestNode>;
}

function* concat(lists: IPendingEntry[][]): IterableIterator<IPendingEntry> {
  for (const nodes of lists) {
    for (const node of nodes) {
      yield node;
    }
  }
}

type TestGroupFilter = (testcase: ICaseID) => boolean;
function filterTestGroup(group: ITestGroup, filter: TestGroupFilter) {
  return {
    *iterate(log: GroupRecorder): Iterable<RunCase> {
      for (const rc of group.iterate(log)) {
        if (filter(rc.testcase)) {
          yield rc;
        }
      }
    },
  };
}

export class TestLoader {
  async loadTestsFromQuery(query: string): Promise<IterableIterator<IPendingEntry>> {
    return this.loadTests(new URLSearchParams(query).getAll('q'));
  }

  async loadTestsFromCmdLine(filters: string[]): Promise<IterableIterator<IPendingEntry>> {
    // In actual URL queries (?q=...), + represents a space. But decodeURIComponent doesn't do this,
    // so do it manually. (+ is used over %20 for readability.) (See also encodeSelectively.)
    return this.loadTests(filters.map(f => decodeURIComponent(f.replace(/\+/g, '%20'))));
  }

  async loadTests(filters: string[]): Promise<IterableIterator<IPendingEntry>> {
    const listings = [];
    for (const filter of filters) {
      listings.push(this.loadFilter(filter));
    }

    return concat(await Promise.all(listings));
  }

  protected async listing(suite: string): Promise<IListing> {
    return { suite, groups: await (await import(`../suites/${suite}/index.js`)).listing };
  }

  protected import(path: string): Promise<ITestNode> {
    return import('../suites/' + path);
  }

  // Each filter is of one of the forms below (urlencoded).
  private async loadFilter(filter: string): Promise<IPendingEntry[]> {
    const i1 = filter.indexOf(':');
    if (i1 === -1) {
      // - cts
      return this.filterByGroup(await this.listing(filter), '');
    }

    const suite = filter.substring(0, i1);
    const i2 = filter.indexOf(':', i1 + 1);
    if (i2 === -1) {
      // - cts:
      // - cts:buf
      // - cts:buffers/
      // - cts:buffers/map
      const groupPrefix = filter.substring(i1 + 1);
      return this.filterByGroup(await this.listing(suite), groupPrefix);
    }

    const group = filter.substring(i1 + 1, i2);
    const endOfTestName = new RegExp('[^' + allowedTestNameCharacters + ']');
    const i3sub = filter.substring(i2 + 1).search(endOfTestName);
    if (i3sub === -1) {
      // - cts:buffers/mapWriteAsync:
      // - cts:buffers/mapWriteAsync:ba
      const testPrefix = filter.substring(i2 + 1);
      return [
        {
          suite,
          path: group,
          node: this.filterByTestMatch(suite, group, testPrefix),
        },
      ];
    }

    const i3 = i2 + 1 + i3sub;
    const test = filter.substring(i2 + 1, i3);
    const token = filter.charAt(i3);

    let params = null;
    if (i3 + 1 < filter.length) {
      params = JSON.parse(filter.substring(i3 + 1)) as IParamsAny;
    }

    if (token === '~') {
      // - cts:buffers/mapWriteAsync:basic~
      // - cts:buffers/mapWriteAsync:basic~{}
      // - cts:buffers/mapWriteAsync:basic~{filter:"params"}
      return [
        {
          suite,
          path: group,
          node: this.filterByParamsMatch(suite, group, test, params),
        },
      ];
    } else if (token === ':') {
      // - cts:buffers/mapWriteAsync:basic:
      // - cts:buffers/mapWriteAsync:basic:{}
      // - cts:buffers/mapWriteAsync:basic:{exact:"params"}
      return [
        {
          suite,
          path: group,
          node: this.filterByParamsExact(suite, group, test, params),
        },
      ];
    } else {
      throw new Error("invalid character after test name; must be '~' or ':'");
    }
  }

  private filterByGroup({ suite, groups }: IListing, groupPrefix: string): IPendingEntry[] {
    const entries: IPendingEntry[] = [];

    for (const { path, description } of groups) {
      if (path.startsWith(groupPrefix)) {
        const isReadme = path === '' || path.endsWith('/');
        const node: Promise<ITestNode> = isReadme
          ? Promise.resolve({ description })
          : this.import(`${suite}/${path}.spec.js`);
        entries.push({ suite, path, node });
      }
    }

    return entries;
  }

  private async filterByTestMatch(
    suite: string,
    group: string,
    testPrefix: string
  ): Promise<ITestNode> {
    const node = (await this.import(`${suite}/${group}.spec.js`)) as ITestNode;
    if (!node.g) {
      return node;
    }
    return {
      description: node.description,
      g: filterTestGroup(node.g, testcase => testcase.name.startsWith(testPrefix)),
    };
  }

  private async filterByParamsMatch(
    suite: string,
    group: string,
    test: string,
    paramsMatch: IParamsAny | null
  ): Promise<ITestNode> {
    const node = (await this.import(`${suite}/${group}.spec.js`)) as ITestNode;
    if (!node.g) {
      return node;
    }
    return {
      description: node.description,
      g: filterTestGroup(
        node.g,
        testcase => testcase.name === test && paramsSupersets(testcase.params, paramsMatch)
      ),
    };
  }

  private async filterByParamsExact(
    suite: string,
    group: string,
    test: string,
    paramsExact: IParamsAny | null
  ): Promise<ITestNode> {
    const node = (await this.import(`${suite}/${group}.spec.js`)) as ITestNode;
    if (!node.g) {
      return node;
    }
    return {
      description: node.description,
      g: filterTestGroup(
        node.g,
        testcase => testcase.name === test && paramsEquals(testcase.params, paramsExact)
      ),
    };
  }
}
