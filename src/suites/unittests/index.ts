import { IGroupDesc } from '../../framework/loader.js';
import { makeListing } from '../../tools/crawl.js';

export const listing: Promise<IGroupDesc[]> = makeListing(__filename);
