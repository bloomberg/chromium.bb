import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

import { crawl } from './crawl.js';

function usage(rc: number): void {
  console.error('Usage:');
  console.error('  tools/gen_listings [SUITES...]');
  console.error('  tools/gen_listings webgpu unittests');
  process.exit(rc);
}

if (process.argv.length <= 2) {
  usage(0);
}

const myself = 'src/common/tools/gen_listings.ts';
if (!fs.existsSync(myself)) {
  console.error('Must be run from repository root');
  usage(1);
}

(async () => {
  for (const suite of process.argv.slice(2)) {
    const listing = await crawl(suite);

    const outFile = path.normalize(`out/${suite}/listing.js`);
    fs.mkdirSync(`out/${suite}`, { recursive: true });
    fs.writeFileSync(
      outFile,
      `\
// AUTO-GENERATED - DO NOT EDIT. See ${myself}.

export const listing = ${JSON.stringify(listing, undefined, 2)};
`
    );
    try {
      fs.unlinkSync(outFile + '.map');
      /* eslint-disable-next-line no-empty */
    } catch (ex) {}
  }
})();
