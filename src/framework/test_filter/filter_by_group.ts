import { TestFilter } from './index.js';
import { TestSpecID, TestCaseID } from '../id.js';
import { TestFileLoader, TestQueryResult, ReadmeFile, TestSpecFile } from '../loader.js';

export class FilterByGroup implements TestFilter {
  private readonly suite: string;
  private readonly groupPrefix: string;

  constructor(suite: string, groupPrefix: string) {
    this.suite = suite;
    this.groupPrefix = groupPrefix;
  }

  matches(spec: TestSpecID, testcase: TestCaseID): boolean {
    throw new Error('unimplemented');
  }

  async iterate(loader: TestFileLoader): Promise<TestQueryResult[]> {
    const specs = await loader.listing(this.suite);
    const entries: TestQueryResult[] = [];

    const suite = this.suite;
    for (const { path, description } of specs) {
      if (path.startsWith(this.groupPrefix)) {
        const isReadme = path === '' || path.endsWith('/');
        const spec = isReadme
          ? Promise.resolve({ description } as ReadmeFile)
          : (loader.import(`${suite}/${path}.spec.js`) as Promise<TestSpecFile>);
        entries.push({ id: { suite, path }, spec });
      }
    }

    return entries;
  }
}
