import { TestSpecOrTestOrCaseID } from '../id.js';
import { ReadmeFile, TestFileLoader, TestSpec } from '../loader.js';

import { TestFilterResult } from './index.js';
import { TestFilter } from './internal.js';

export class FilterByGroup implements TestFilter {
  private readonly suite: string;
  private readonly specPathPrefix: string;

  constructor(suite: string, groupPrefix: string) {
    this.suite = suite;
    this.specPathPrefix = groupPrefix;
  }

  matches(id: TestSpecOrTestOrCaseID): boolean {
    return id.spec.suite === this.suite && this.pathMatches(id.spec.path);
  }

  async iterate(loader: TestFileLoader): Promise<TestFilterResult[]> {
    const specs = await loader.listing(this.suite);
    const entries: TestFilterResult[] = [];

    const suite = this.suite;
    for (const { path, description } of specs) {
      if (this.pathMatches(path)) {
        const isReadme = path === '' || path.endsWith('/');
        const spec = isReadme
          ? ({ description } as ReadmeFile)
          : ((await loader.import(`${suite}/${path}.spec.js`)) as TestSpec);
        entries.push({ id: { suite, path }, spec });
      }
    }

    return entries;
  }

  definitelyOneFile(): boolean {
    // FilterByGroup could always possibly match multiple files, because it represents a prefix,
    // e.g. "a:b" not "a:b:".
    return false;
  }

  idIfSingle(): undefined {
    // FilterByGroup could be one whole suite, but we only want whole files, tests, or cases.
    return undefined;
  }

  private pathMatches(path: string): boolean {
    return path.startsWith(this.specPathPrefix);
  }
}
