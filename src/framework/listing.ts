import { ICase, TestGroup } from './test_group.js';

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
    group?: TestGroup;
    description: string;
}

interface IEntry {
    suite: string;
    path: string;
}

interface IPendingEntry extends IEntry {
    node: Promise<ITestNode>;
}

function encodeSelectively(s: string) {
    let ret = encodeURIComponent(s);
    ret = ret.replace(/%20/g, '+');
    ret = ret.replace(/%22/g, '"');
    ret = ret.replace(/%2C/g, ',');
    ret = ret.replace(/%2F/g, '/');
    ret = ret.replace(/%3A/g, ':');
    ret = ret.replace(/%7B/g, '{');
    ret = ret.replace(/%7D/g, '}');
    return ret;
}

export function makeQueryString(entry: IEntry, testcase: ICase): string {
    let s = entry.suite + ':';
    s += entry.path + ':';
    s += testcase.name + ':';
    if (testcase.params) {
        s += JSON.stringify(testcase.params);
    }
    return '?q=' + encodeSelectively(s);
}

function* concatAndDedup(lists: IPendingEntry[][]): Iterator<IPendingEntry> {
    const seen = new Set<string>();
    for (const nodes of lists) {
        for (const node of nodes) {
            if (!seen.has(node.path)) {
                seen.add(node.path);
                yield node;
            }
        }
    }
}

function filterByGroup({ suite, groups }: IListing, prefix: string): IPendingEntry[] {
    const entries: IPendingEntry[] = [];

    for (const { path, description } of groups) {
        if (path.startsWith(prefix)) {
            const isReadme = path === '' || path.endsWith('/');
            const node = isReadme ?
                Promise.resolve({ description }) :
                import(`../${suite}/${path}.spec.js`);
            entries.push({ suite, path, node });
        }
    }

    return entries;
}

function filterByTest(): IPendingEntry[] {
    throw new Error();
}

function filterByParams(): IPendingEntry[] {
    throw new Error();
}

class ListingFetcher {
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

export async function loadTests(outDir: string, query: string):
        Promise<Iterator<IPendingEntry>> {
    const filters = new URLSearchParams(query).getAll('q');
    const fetcher = new ListingFetcher();

    // Each filter is of one of these forms (urlencoded):
    //    cts
    //    cts:
    //    cts:buf
    //    cts:buffers/
    //    cts:buffers/map
    //
    //    cts:buffers/mapWriteAsync:
    //    cts:buffers/mapWriteAsync:ba
    //
    //    cts:buffers/mapWriteAsync:basic~
    //    cts:buffers/mapWriteAsync:basic~{}
    //    cts:buffers/mapWriteAsync:basic~{filter:"params"}
    //
    //    cts:buffers/mapWriteAsync:basic:
    //    cts:buffers/mapWriteAsync:basic:{}
    //    cts:buffers/mapWriteAsync:basic:{exact:"params"}
    const listings = [];
    for (const filter of filters) {
        const parts = filter.split(':', 3);
        if (parts.length === 1) {
            parts.push('');
        }
        if (parts.length === 2) {
            const listing = await fetcher.get(outDir, parts[0]);
            listings.push(filterByGroup(listing, parts[1]));
        } else if (parts.length === 3) {
            throw new Error();
        }
    }
    return concatAndDedup(listings);
}
