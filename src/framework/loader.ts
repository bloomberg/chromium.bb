import { GroupRecorder } from './logger.js';
import { IParamsAny } from './params/index.js';
import { allowedTestNameCharacters, ICase, ITestGroup, RunCase } from './test_group.js';
import { objectEquals } from './util.js';

export interface IGroupDesc {
  path: string;
  description: string;
}

export interface IListing {
  suite: string;
  groups: Iterable<IGroupDesc>;
}

export interface ITestNode {
  // undefined for README.txt, defined for a test module.
  group?: ITestGroup;
  description: string;
}

export interface IEntry {
  suite: string;
  path: string;
}

interface IPendingEntry extends IEntry {
  node: Promise<ITestNode>;
}

function* concat(lists: IPendingEntry[][]): IterableIterator<IPendingEntry> {
  for (const nodes of lists) {
    for (const node of nodes) {
      yield node;
    }
  }
}

type TestGroupFilter = (testcase: ICase) => boolean;
function filterTestGroup(group: ITestGroup, filter: TestGroupFilter) {
  return {
    * iterate(log: GroupRecorder): Iterable<RunCase> {
      for (const rc of group.iterate(log)) {
        if (filter(rc.testcase)) {
          yield rc;
        }
      }
    },
  };
}

// TODO: Unit test this.
export abstract class TestLoader {
  public async loadTests(outDir: string, filters: string[]): Promise<IterableIterator<IPendingEntry>> {
    const listings = [];
    for (const filter of filters) {
      listings.push(this.loadFilter(outDir, filter));
    }

    return concat(await Promise.all(listings));
  }

  protected abstract fetch(outDir: string, suite: string): Promise<IListing>;
  protected abstract import(path: string): Promise<ITestNode>;

  // Each filter is of one of the forms below (urlencoded).
  private async loadFilter(outDir: string, filter: string): Promise<IPendingEntry[]> {
    const i1 = filter.indexOf(':');
    if (i1 === -1) {
      // - cts
      return this.filterByGroup(await this.fetch(outDir, filter), '');
    }

    const suite = filter.substring(0, i1);
    const i2 = filter.indexOf(':', i1 + 1);
    if (i2 === -1) {
      // - cts:
      // - cts:buf
      // - cts:buffers/
      // - cts:buffers/map
      const groupPrefix = filter.substring(i1 + 1);
      return this.filterByGroup(await this.fetch(outDir, suite), groupPrefix);
    }

    const group = filter.substring(i1 + 1, i2);
    const endOfTestName = new RegExp('[^' + allowedTestNameCharacters + ']');
    const i3sub = filter.substring(i2 + 1).search(endOfTestName);
    if (i3sub === -1) {
      // - cts:buffers/mapWriteAsync:
      // - cts:buffers/mapWriteAsync:ba
      const testPrefix = filter.substring(i2 + 1);
      return [{ suite, path: group, node: this.filterByTestMatch(suite, group, testPrefix) }];
    }

    const i3 = i2 + 1 + i3sub;
    const test = filter.substring(i2 + 1, i3);
    const token = filter.charAt(i3);

    let params;
    if (i3 + 1 < filter.length) {
      params = JSON.parse(filter.substring(i3 + 1)) as IParamsAny;
    }

    if (token === '~') {
      // - cts:buffers/mapWriteAsync:basic~
      // - cts:buffers/mapWriteAsync:basic~{}
      // - cts:buffers/mapWriteAsync:basic~{filter:"params"}
      throw new Error('params matching (~) is unimplemented');
    } else if (token === ':') {
      // - cts:buffers/mapWriteAsync:basic:
      // - cts:buffers/mapWriteAsync:basic:{}
      // - cts:buffers/mapWriteAsync:basic:{exact:"params"}
      return [{
        suite,
        path: group,
        node: this.filterByParamsExact(suite, group, test, params),
      }];
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

  private async filterByTestMatch(suite: string, group: string, testPrefix: string): Promise<ITestNode> {
    const node = (await this.import(`${suite}/${group}.spec.js`)) as ITestNode;
    if (!node.group) {
      return node;
    }
    return {
      description: node.description,
      group: filterTestGroup(node.group, testcase => testcase.name.startsWith(testPrefix)),
    };
  }

  private async filterByParamsExact(suite: string, group: string, test: string, paramsExact?: IParamsAny): Promise<ITestNode> {
    const node = (await this.import(`${suite}/${group}.spec.js`)) as ITestNode;
    if (!node.group) {
      return node;
    }
    return {
      description: node.description,
      group: filterTestGroup(node.group, testcase =>
        testcase.name === test && objectEquals(testcase.params, paramsExact)),
    };
  }
}
