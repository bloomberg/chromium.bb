// Node can look at the filesystem, but JS in the browser can't.
// This crawls the file tree under src/suites/${suite} to generate a (non-hierarchical) static
// listing file that can then be used in the browser to load the modules containing the tests.

import * as fs from 'fs';
import * as path from 'path';

import { SpecFile } from '../framework/file_loader.js';
import { validQueryPart } from '../framework/query/validQueryPart.js';
import { TestSuiteListingEntry, TestSuiteListing } from '../framework/test_suite_listing.js';
import { assert, unreachable } from '../framework/util/util.js';

const fg = require('fast-glob');

const specFileSuffix = '.spec.ts';

export async function crawl(suite: string): Promise<TestSuiteListingEntry[]> {
  const suiteDir = 'src/' + suite;
  if (!fs.existsSync(suiteDir)) {
    console.error(`Could not find ${suiteDir}`);
    process.exit(1);
  }

  const glob = `${suiteDir}/**/{README.txt,*${specFileSuffix}}`;
  const filesToEnumerate: string[] = await fg(glob, { onlyFiles: true });
  filesToEnumerate.sort();

  const entries: TestSuiteListingEntry[] = [];
  for (const file of filesToEnumerate) {
    const f = file.substring((suiteDir + '/').length); // Suite-relative file path
    if (f.endsWith(specFileSuffix)) {
      const filepathWithoutExtension = f.substring(0, f.length - specFileSuffix.length);
      const filename = `../../../${suiteDir}/${filepathWithoutExtension}.spec.js`;

      let mod: SpecFile;
      if (process.env.STANDALONE_DEV_SERVER) {
        mod = require(filename) as SpecFile;
        // Delete the cache so that changes to the file are picked up.
        delete require.cache[require.resolve(filename)];
      } else {
        mod = (await import(filename)) as SpecFile;
      }
      assert(mod.description !== undefined, 'Test spec file missing description: ' + filename);
      assert(mod.g !== undefined, 'Test spec file missing TestGroup definition: ' + filename);

      mod.g.validate();

      const path = filepathWithoutExtension.split('/');
      for (const p of path) {
        assert(validQueryPart.test(p), `Invalid directory name ${p}; must match ${validQueryPart}`);
      }
      entries.push({ file: path, description: mod.description.trim() });
    } else if (path.basename(file) === 'README.txt') {
      const filepathWithoutExtension = f.substring(0, f.length - '/README.txt'.length);
      const readme = fs.readFileSync(file, 'utf8').trim();

      const path = filepathWithoutExtension ? filepathWithoutExtension.split('/') : [];
      entries.push({ file: path, readme });
    } else {
      unreachable(`glob ${glob} matched an unrecognized filename ${file}`);
    }
  }

  return entries;
}

export function makeListing(filename: string): Promise<TestSuiteListing> {
  const suite = path.basename(path.dirname(filename));
  return crawl(suite);
}
