import { TestCaseID, TestSpecID } from '../id.js';
import { TestFileLoader, TestSpec } from '../loader.js';
import { TestSpecRecorder } from '../logger.js';
import { ParamsAny, paramsEquals, paramsSupersets } from '../params/index.js';
import { RunCase, RunCaseIterable } from '../test_group.js';

import { TestFilterResult } from './index.js';
import { TestFilter } from './internal.js';

abstract class FilterOneFile implements TestFilter {
  protected readonly specId: TestSpecID;

  constructor(specId: TestSpecID) {
    this.specId = specId;
  }

  abstract getCases(spec: TestSpec): RunCaseIterable;
  abstract matches(spec: TestSpecID, testcase: TestCaseID): boolean;

  async iterate(loader: TestFileLoader): Promise<TestFilterResult[]> {
    const spec = (await loader.import(
      `${this.specId.suite}/${this.specId.path}.spec.js`
    )) as TestSpec;
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
    *iterate(log: TestSpecRecorder): Iterable<RunCase> {
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

  getCases(spec: TestSpec): RunCaseIterable {
    return filterTestGroup(spec.g, testcase => testcase.test.startsWith(this.testPrefix));
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

  getCases(spec: TestSpec): RunCaseIterable {
    return filterTestGroup(
      spec.g,
      testcase => testcase.test === this.test && paramsSupersets(testcase.params, this.params)
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

  getCases(spec: TestSpec): RunCaseIterable {
    return filterTestGroup(
      spec.g,
      testcase => testcase.test === this.test && paramsEquals(testcase.params, this.params)
    );
  }

  matches(spec: TestSpecID, testcase: TestCaseID): boolean {
    throw new Error('unimplemented');
  }
}
