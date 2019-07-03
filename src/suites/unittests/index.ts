import { TestSuiteListing } from '../../framework/listing.js';
import { makeListing } from '../../tools/crawl.js';

export const listing: Promise<TestSuiteListing> = makeListing(__filename);
