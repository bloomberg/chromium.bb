// tslint:disable: no-console

// tslint:disable-next-line: no-implicit-dependencies
import * as fg from 'fast-glob';
import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

function usage(rc: number) {
  console.error('Usage:');
  console.error('  node tools/gen [SUITES...]');
  console.error('  node tools/gen cts unittests demos');
  process.exit(rc);
}

if (process.argv.length <= 2) {
  usage(0);
}

if (!fs.existsSync('tools/lib/gen.ts')) {
  console.error('Must be run from repository root');
  usage(1);
}

(async () => {
  for (const suite of process.argv.slice(2)) {
    const outFile = path.normalize(`out/${suite}/listing.json`);
    const specDir = path.normalize(`src/${suite}/`);  // always ends in /

    const specSuffix = '.spec.ts';
    const specFiles = fg.sync(specDir + '**/{README.txt,*' + specSuffix + '}', {
      onlyFiles: false,
      markDirectories: true,
    });

    const listing = [];
    const modules = {};
    for (const file of specFiles) {
      const f = file.substring(specDir.length);
      if (f.endsWith(specSuffix)) {
        const mod = await import('../../' + file);
        const testPath = f.substring(0, f.length - specSuffix.length);
        modules[testPath] = mod;
        listing.push({
          path: testPath,
          description: mod.description.trim(),
        });
      } else if (path.basename(file) === 'README.txt') {
        const readme = file;
        if (fs.existsSync(readme)) {
          const group = f.substring(0, f.length - 'README.txt'.length);
          const description = fs.readFileSync(readme, 'utf8').trim();
          listing.push({
            path: group,
            description,
          });
        }
        // ignore
      } else {
        console.error('Unrecognized file: ' + file);
        process.exit(1);
      }
    }

    fs.writeFileSync(outFile, JSON.stringify(listing, undefined, 2));
  }
})();
