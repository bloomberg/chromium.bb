import { IGroupDesc, IListing, ITestNode, TestLoader } from './loader.js';

export class TestLoaderWeb extends TestLoader {
  private suites: Map<string, IListing> = new Map();

  protected async fetch(outDir: string, suite: string): Promise<IListing> {
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

  protected import(path: string): Promise<ITestNode> {
    return import('../' + path);
  }
}
