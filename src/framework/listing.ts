import { GroupRecorder } from './logger.js';
import { IParamsAny } from './params/index.js';
import { allowedTestNameCharacters, ICase, ITestGroup, RunCase } from './test_group.js';
import { objectEquals } from './util.js';

interface IGroupDesc {
  path: string;
  description: string;
}

interface IListing {
  suite: string;
  groups: Iterable<IGroupDesc>;
}

interface ITestNode {
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

function* concat(lists: IPendingEntry[][]): Iterator<IPendingEntry> {
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

function filterByGroup({ suite, groups }: IListing, groupPrefix: string): IPendingEntry[] {
  const entries: IPendingEntry[] = [];

  for (const { path, description } of groups) {
    if (path.startsWith(groupPrefix)) {
      const isReadme = path === '' || path.endsWith('/');
      const node: Promise<ITestNode> = isReadme
        ? Promise.resolve({ description })
        : import(`../${suite}/${path}.spec.js`);
      entries.push({ suite, path, node });
    }
  }

  return entries;
}

async function filterByTestMatch(suite: string, group: string, testPrefix: string): Promise<ITestNode> {
  const node = (await import(`../${suite}/${group}.spec.js`)) as ITestNode;
  if (!node.group) {
    return node;
  }
  return {
    description: node.description,
    group: filterTestGroup(node.group, testcase => testcase.name.startsWith(testPrefix)),
  };
}

async function filterByParamsExact(suite: string, group: string, test: string, paramsExact: IParamsAny | undefined): Promise<ITestNode> {
  const node = (await import(`../${suite}/${group}.spec.js`)) as ITestNode;
  if (!node.group) {
    return node;
  }
  return {
    description: node.description,
    group: filterTestGroup(node.group, testcase =>
      testcase.name === test && objectEquals(testcase.params, paramsExact)),
  };
}

interface IListingFetcher {
  get(outDir: string, suite: string): Promise<IListing>;
}

class ListingFetcher implements IListingFetcher {
  private suites: Map<string, IListing> = new Map();

  public async get(outDir: string, suite: string): Promise<IListing> {
    let listing = this.suites.get(suite);
    if (listing) {
      return listing;
    }
    const listingPath = `${outDir}/${suite}/listing.json`;
    const fetched = await fetch(listingPath);
    if (fetched.status !== 200) {
      throw new Error(listingPath + ' not found');
    }
    const groups: IGroupDesc[] = await fetched.json();

    listing = { suite, groups };
    this.suites.set(suite, listing);
    return listing;
  }
}

// TODO: Unit test this.

// Each filter is of one of the forms below (urlencoded).
async function loadFilter(fetcher: IListingFetcher, outDir: string, filter: string): Promise<IPendingEntry[]> {
  const i1 = filter.indexOf(':');
  if (i1 === -1) {
    // - cts
    const suite = filter;
    return filterByGroup(await fetcher.get(outDir, suite), '');
  } else {
    const suite = filter.substring(0, i1);
    const i2 = filter.indexOf(':', i1 + 1);
    if (i2 === -1) {
      // - cts:
      // - cts:buf
      // - cts:buffers/
      // - cts:buffers/map
      const groupPrefix = filter.substring(i1 + 1);
      return filterByGroup(await fetcher.get(outDir, suite), groupPrefix);
    } else {
      const group = filter.substring(i1 + 1, i2);
      const endOfTestName = new RegExp('[^' + allowedTestNameCharacters + ']');
      const i3sub = filter.substring(i2 + 1).search(endOfTestName);
      if (i3sub === -1) {
        // - cts:buffers/mapWriteAsync:
        // - cts:buffers/mapWriteAsync:ba
        const testPrefix = filter.substring(i2 + 1);
        return [{ suite, path: group, node: filterByTestMatch(suite, group, testPrefix) }];
      } else {
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
          return [{ suite, path: group, node: filterByParamsExact(suite, group, test, params) }];
        } else {
          throw new Error("invalid character after test name; must be '~' or ':'");
        }
      }
    }
  }
}

export async function loadTests(outDir: string, filters: string[], fetcherClass: new () => IListingFetcher = ListingFetcher): Promise<Iterator<IPendingEntry>> {
  const fetcher = new fetcherClass();

  const listings = [];
  for (const filter of filters) {
    listings.push(loadFilter(fetcher, outDir, filter));
  }

  return concat(await Promise.all(listings));
}
