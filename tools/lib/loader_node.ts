import * as fs from 'fs';

import { IGroupDesc, IListing, ITestNode, TestLoader } from '../../src/framework/loader.js';

export class TestLoaderNode extends TestLoader {
  private suites: Map<string, IListing> = new Map();

  protected async fetch(outDir: string, suite: string): Promise<IListing> {
    let listing = this.suites.get(suite);
    if (listing) {
      return listing;
    }
    const listingPath = `${outDir}/${suite}/listing.json`;
    const fetched = await fs.promises.readFile(listingPath, 'utf-8');
    const groups: IGroupDesc[] = JSON.parse(fetched);

    listing = { suite, groups };
    this.suites.set(suite, listing);
    return listing;
  }

  protected import(path: string): Promise<ITestNode> {
    return import('../../src/' + path);
  }
}
