// Node can look at the filesystem, but JS in the browser can't.
// This crawls the file tree under src/suites/${suite} to generate a (non-hierarchical) static
// listing file that can then be used in the browser to load the modules containing the tests.

import fg from 'fast-glob';
import * as fs from 'fs';
import * as path from 'path';

import { TestGroupDesc } from '../framework/loader.js';

const specSuffix = '.spec.ts';

export async function crawl(suite: string): Promise<TestGroupDesc[]> {
  const specDir = path.normalize(`src/suites/${suite}`);
  if (!fs.existsSync(specDir)) {
    console.error(`Could not find ${specDir}`);
    process.exit(1);
  }

  const specFiles = await fg(specDir + '/**/{README.txt,*' + specSuffix + '}', { onlyFiles: true });

  const groups: TestGroupDesc[] = [];
  for (const file of specFiles) {
    const f = file.substring((specDir + '/').length);
    if (f.endsWith(specSuffix)) {
      const testPath = f.substring(0, f.length - specSuffix.length);
      const mod = await import(`../../${specDir}/${testPath}.spec.js`);
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

export function makeListing(filename: string): Promise<TestGroupDesc[]> {
  const suite = path.basename(path.dirname(filename));
  return crawl(suite);
}
