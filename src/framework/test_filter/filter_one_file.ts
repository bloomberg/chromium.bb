import { TestFilter, TestFilterResult } from './index.js';
import { TestSpecID, TestCaseID } from '../id.js';
import { TestSpecFile, TestFileLoader } from '../loader.js';
import { RunCaseIterable, RunCase } from '../test_group.js';
import { ParamsAny, paramsSupersets, paramsEquals } from '../params/index.js';
import { GroupRecorder } from '../logger.js';

abstract class FilterOneFile implements TestFilter {
  protected readonly specId: TestSpecID;

  constructor(specId: TestSpecID) {
    this.specId = specId;
  }

  abstract getCases(spec: TestSpecFile): RunCaseIterable;
  abstract matches(spec: TestSpecID, testcase: TestCaseID): boolean;

  async iterate(loader: TestFileLoader): Promise<TestFilterResult[]> {
    const spec = (await loader.import(
      `${this.specId.suite}/${this.specId.path}.spec.js`
    )) as TestSpecFile;
    return [
      {
        id: this.specId,
        spec: {
          description: spec.description,
          g: this.getCases(spec),
        },
      },
    ];
  }
}

type TestGroupFilter = (testcase: TestCaseID) => boolean;
function filterTestGroup(group: RunCaseIterable, filter: TestGroupFilter): RunCaseIterable {
  return {
    *iterate(log: GroupRecorder): Iterable<RunCase> {
      for (const rc of group.iterate(log)) {
        if (filter(rc.id)) {
          yield rc;
        }
      }
    },
  };
}

export class FilterByTestMatch extends FilterOneFile {
  private readonly testPrefix: string;

  constructor(specId: TestSpecID, testPrefix: string) {
    super(specId);
    this.testPrefix = testPrefix;
  }

  getCases(spec: TestSpecFile): RunCaseIterable {
    return filterTestGroup(spec.g, testcase => testcase.name.startsWith(this.testPrefix));
  }

  matches(spec: TestSpecID, testcase: TestCaseID): boolean {
    throw new Error('unimplemented');
  }
}

export class FilterByParamsMatch extends FilterOneFile {
  private readonly test: string;
  private readonly params: ParamsAny | null;

  constructor(specId: TestSpecID, test: string, params: ParamsAny | null) {
    super(specId);
    this.test = test;
    this.params = params;
  }

  getCases(spec: TestSpecFile): RunCaseIterable {
    return filterTestGroup(
      spec.g,
      testcase => testcase.name === this.test && paramsSupersets(testcase.params, this.params)
    );
  }

  matches(spec: TestSpecID, testcase: TestCaseID): boolean {
    throw new Error('unimplemented');
  }
}

export class FilterByParamsExact extends FilterOneFile {
  private readonly test: string;
  private readonly params: ParamsAny | null;

  constructor(specId: TestSpecID, test: string, params: ParamsAny | null) {
    super(specId);
    this.test = test;
    this.params = params;
  }

  getCases(spec: TestSpecFile): RunCaseIterable {
    return filterTestGroup(
      spec.g,
      testcase => testcase.name === this.test && paramsEquals(testcase.params, this.params)
    );
  }

  matches(spec: TestSpecID, testcase: TestCaseID): boolean {
    throw new Error('unimplemented');
  }
}
