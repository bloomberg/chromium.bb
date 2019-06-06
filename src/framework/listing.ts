import { TestGroup } from './test_group.js';

interface ITestDesc {
    path: string;
    description: string;
}
type Listing = Iterable<ITestDesc>;

interface ITestNode {
    // undefined for README.txt, defined for a test module.
    group?: TestGroup;
    description: string;
}

export async function loadListing(suite: string, listing: Listing):
        Promise<Map<string, ITestNode>> {
    const promises: Array<Promise<[string, ITestNode]>> = [];
    for (const { path } of listing) {
        if (path.endsWith('/')) {
            promises.push(fetch(`../${suite}${path}/README.txt`)
                .then((resp) => resp.text())
                .then((description) => [path, { description }]));
        } else {
            promises.push(import(`../${suite}${path}.spec.js`)
                .then((mod) => [path, mod]));
        }
    }
    return new Map(await Promise.all(promises));
}

export function* filterListing(filters: string[], fullListing: Listing): Listing {
    // TODO: this is probably going to be slow when the listing becomes large.
    for (const { path, description } of fullListing) {
        for (const filter of filters) {
            if (path.startsWith(filter)) {
                yield { path, description };
            }
        }
    }
}
