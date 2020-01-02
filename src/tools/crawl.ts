// Node can look at the filesystem, but JS in the browser can't.
// This crawls the file tree under src/suites/${suite} to generate a (non-hierarchical) static
// listing file that can then be used in the browser to load the modules containing the tests.

// tslint:disable: no-console

import * as fs from 'fs';
import * as path from 'path';

const fg = require('fast-glob');

import { assert } from '../framework/index.js';
import { TestSuiteListingEntry } from '../framework/listing.js';
import { TestSpec } from '../framework/loader.js';

const specSuffix = '.spec.ts';

export async function crawl(suite: string): Promise<TestSuiteListingEntry[]> {
  const specDir = 'src/suites/' + suite;
  if (!fs.existsSync(specDir)) {
    console.error(`Could not find ${specDir}`);
    process.exit(1);
  }

  const specFiles = await fg(`${specDir}/**/{README.txt,*${specSuffix}}`, { onlyFiles: true });
  specFiles.sort();

  const groups: TestSuiteListingEntry[] = [];
  for (const file of specFiles) {
    const f = file.substring((specDir + '/').length);
    if (f.endsWith(specSuffix)) {
      const testPath = f.substring(0, f.length - specSuffix.length);
      const filename = `../../${specDir}/${testPath}.spec.js`;
      const mod = (await import(filename)) as TestSpec;
      assert(mod.description !== undefined, 'Test spec file missing description: ' + filename);
      assert(mod.g !== undefined, 'Test spec file missing TestGroup definition: ' + filename);
      groups.push({
        path: testPath,
        description: mod.description.trim(),
      });
    } else if (path.basename(file) === 'README.txt') {
      const group = f.substring(0, f.length - 'README.txt'.length);
      const description = fs.readFileSync(file, 'utf8').trim();
      groups.push({
        path: group,
        description,
      });
    } else {
      console.error('Unrecognized file: ' + file);
      process.exit(1);
    }
  }

  return groups;
}

export function makeListing(filename: string): Promise<TestSuiteListingEntry[]> {
  const suite = path.basename(path.dirname(filename));
  return crawl(suite);
}
