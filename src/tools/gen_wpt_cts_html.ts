import { promises as fs } from 'fs';

import { TestSuiteListingEntry } from '../framework/listing.js';
import { listing } from '../suites/cts/index.js';

// TODO(kainino0x): Make this file able to take a list of suppressions as input, and a custom output file.

(async () => {
  let result = '';
  result += '<!-- AUTO-GENERATED - DO NOT EDIT. See gen_wpt_cts_html.ts. -->\n';

  result += await fs.readFile('templates/cts.html', 'utf8');
  result += '\n';

  const ls = (await listing) as TestSuiteListingEntry[];
  for (const l of ls) {
    if (l.path.length === 0 || l.path.endsWith('/')) {
      // This is a readme.
      continue;
    }
    result += `<meta name=variant content='?q=cts:${l.path}:'>\n`;
  }

  return fs.writeFile('./out-wpt/cts.html', result);
})();
