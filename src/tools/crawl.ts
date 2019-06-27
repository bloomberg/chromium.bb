import * as fg from 'fast-glob';
import * as fs from 'fs';
import * as path from 'path';

import { IGroupDesc } from '../framework/loader.js';

const specSuffix = '.spec.ts';

export async function crawl(suite: string): Promise<IGroupDesc[]> {
  const specDir = path.normalize(`src/suites/${suite}/`); // Always ends in /
  if (!fs.existsSync(specDir)) {
    console.error(`Could not find ${specDir}`);
    process.exit(1);
  }

  const specFiles = fg.sync(specDir + '**/{README.txt,*' + specSuffix + '}', {
    onlyFiles: false,
    markDirectories: true,
  });

  const groups: IGroupDesc[] = [];
  for (const file of specFiles) {
    const f = file.substring(specDir.length);
    if (f.endsWith(specSuffix)) {
      const mod = await import('../../' + file);
      const testPath = f.substring(0, f.length - specSuffix.length);
      groups.push({
        path: testPath,
        description: mod.description.trim(),
      });
    } else if (path.basename(file) === 'README.txt') {
      const readme = file;
      if (fs.existsSync(readme)) {
        const group = f.substring(0, f.length - 'README.txt'.length);
        const description = fs.readFileSync(readme, 'utf8').trim();
        groups.push({
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

  return groups;
}

export function makeListing(filename: string): Promise<IGroupDesc[]> {
  const suite = path.basename(path.dirname(filename));
  return crawl(suite);
}
