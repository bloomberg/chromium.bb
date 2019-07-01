// tslint:disable: no-console

// tslint:disable-next-line: no-implicit-dependencies
import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';
import { crawl } from './crawl.js';

function usage(rc: number) {
  console.error('Usage:');
  console.error('  tools/gen [SUITES...]');
  console.error('  tools/gen cts unittests');
  process.exit(rc);
}

if (process.argv.length <= 2) {
  usage(0);
}

if (!fs.existsSync('src/tools/gen.ts')) {
  console.error('Must be run from repository root');
  usage(1);
}

(async () => {
  for (const suite of process.argv.slice(2)) {
    const listing = await crawl(suite);

    const outFile = path.normalize(`out/suites/${suite}/index.js`);
    fs.writeFileSync(
      outFile,
      `\
export const listing = ${JSON.stringify(listing, undefined, 2)};
`
    );
    try {
      fs.unlinkSync(outFile + '.map');
    } catch (ex) {}
  }
})();
