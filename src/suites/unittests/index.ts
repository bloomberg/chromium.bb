import { TestGroupDesc } from '../../framework/loader.js';
import { makeListing } from '../../tools/crawl.js';

export const listing: Promise<TestGroupDesc[]> = makeListing(__filename);
