import { TestCaseID, TestSpecID, TestSpecOrTestOrCaseID, testSpecEquals } from '../id.js';
import { TestFileLoader, TestSpec } from '../loader.js';
import { TestSpecRecorder } from '../logger.js';
import { ParamSpec, paramsEquals, paramsSupersets } from '../params/index.js';
import { RunCase, RunCaseIterable } from '../test_group.js';

import { TestFilterResult } from './index.js';
import { TestFilter } from './internal.js';

abstract class FilterOneFile implements TestFilter {
  protected readonly specId: TestSpecID;

  constructor(specId: TestSpecID) {
    this.specId = specId;
  }

  abstract getCases(spec: TestSpec): RunCaseIterable;

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

  definitelyOneFile(): true {
    return true;
  }

  abstract idIfSingle(): TestSpecOrTestOrCaseID | undefined;
  abstract matches(id: TestSpecOrTestOrCaseID): boolean;
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
    return filterTestGroup(spec.g, testcase => this.testMatches(testcase.test));
  }

  idIfSingle(): TestSpecOrTestOrCaseID | undefined {
    if (this.testPrefix.length !== 0) {
      return undefined;
    }
    // This is one whole spec file.
    return { spec: this.specId };
  }

  matches(id: TestSpecOrTestOrCaseID): boolean {
    if (id.test === undefined) {
      return false;
    }
    return testSpecEquals(id.spec, this.specId) && this.testMatches(id.test);
  }

  private testMatches(test: string): boolean {
    return test.startsWith(this.testPrefix);
  }
}

export class FilterByParamsMatch extends FilterOneFile {
  private readonly test: string;
  private readonly params: ParamSpec | null;

  constructor(specId: TestSpecID, test: string, params: ParamSpec | null) {
    super(specId);
    this.test = test;
    this.params = params;
  }

  getCases(spec: TestSpec): RunCaseIterable {
    return filterTestGroup(spec.g, testcase => this.caseMatches(testcase.test, testcase.params));
  }

  idIfSingle(): TestSpecOrTestOrCaseID | undefined {
    if (this.params !== null) {
      return undefined;
    }
    // This is one whole test.
    return { spec: this.specId, test: this.test };
  }

  matches(id: TestSpecOrTestOrCaseID): boolean {
    if (id.test === undefined) {
      return false;
    }
    return testSpecEquals(id.spec, this.specId) && this.caseMatches(id.test, id.params);
  }

  private caseMatches(test: string, params: ParamSpec | null | undefined): boolean {
    if (params === undefined) {
      return false;
    }
    return test === this.test && paramsSupersets(params, this.params);
  }
}

export class FilterByParamsExact extends FilterOneFile {
  private readonly test: string;
  private readonly params: ParamSpec | null;

  constructor(specId: TestSpecID, test: string, params: ParamSpec | null) {
    super(specId);
    this.test = test;
    this.params = params;
  }

  getCases(spec: TestSpec): RunCaseIterable {
    return filterTestGroup(spec.g, testcase => this.caseMatches(testcase.test, testcase.params));
  }

  idIfSingle(): TestSpecOrTestOrCaseID | undefined {
    // This is one single test case.
    return { spec: this.specId, test: this.test, params: this.params };
  }

  matches(id: TestSpecOrTestOrCaseID): boolean {
    if (id.test === undefined || id.params === undefined) {
      return false;
    }
    return testSpecEquals(id.spec, this.specId) && this.caseMatches(id.test, id.params);
  }

  private caseMatches(test: string, params: ParamSpec | null): boolean {
    return test === this.test && paramsEquals(params, this.params);
  }
}
