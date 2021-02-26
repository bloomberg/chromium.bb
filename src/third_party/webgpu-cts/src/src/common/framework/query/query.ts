import { CaseParams } from '../params_utils.js';
import { assert } from '../util/util.js';

import { encodeURIComponentSelectively } from './encode_selectively.js';
import { kBigSeparator, kPathSeparator, kWildcard, kParamSeparator } from './separators.js';
import { stringifyPublicParams } from './stringify_params.js';

/**
 * Represents a test query of some level.
 *
 * TestQuery types are immutable.
 */
export type TestQuery =
  | TestQuerySingleCase
  | TestQueryMultiCase
  | TestQueryMultiTest
  | TestQueryMultiFile;

export type TestQueryLevel =
  | 1 // MultiFile
  | 2 // MultiTest
  | 3 // MultiCase
  | 4; // SingleCase

/**
 * A multi-file test query, like `s:*` or `s:a,b,*`.
 *
 * Immutable (makes copies of constructor args).
 */
export class TestQueryMultiFile {
  readonly level: TestQueryLevel = 1;
  readonly isMultiFile: boolean = true;
  readonly suite: string;
  readonly filePathParts: readonly string[];

  constructor(suite: string, file: readonly string[]) {
    this.suite = suite;
    this.filePathParts = [...file];
  }

  toString(): string {
    return encodeURIComponentSelectively(this.toStringHelper().join(kBigSeparator));
  }

  protected toStringHelper(): string[] {
    return [this.suite, [...this.filePathParts, kWildcard].join(kPathSeparator)];
  }
}

/**
 * A multi-test test query, like `s:f:*` or `s:f:a,b,*`.
 *
 * Immutable (makes copies of constructor args).
 */
export class TestQueryMultiTest extends TestQueryMultiFile {
  readonly level: TestQueryLevel = 2;
  readonly isMultiFile: false = false;
  readonly isMultiTest: boolean = true;
  readonly testPathParts: readonly string[];

  constructor(suite: string, file: readonly string[], test: readonly string[]) {
    super(suite, file);
    assert(file.length > 0, 'multi-test (or finer) query must have file-path');
    this.testPathParts = [...test];
  }

  protected toStringHelper(): string[] {
    return [
      this.suite,
      this.filePathParts.join(kPathSeparator),
      [...this.testPathParts, kWildcard].join(kPathSeparator),
    ];
  }
}

/**
 * A multi-case test query, like `s:f:t:*` or `s:f:t:a,b,*`.
 *
 * Immutable (makes copies of constructor args), except for param values
 * (which aren't normally supposed to change; they're marked readonly in CaseParams).
 */
export class TestQueryMultiCase extends TestQueryMultiTest {
  readonly level: TestQueryLevel = 3;
  readonly isMultiTest: false = false;
  readonly isMultiCase: boolean = true;
  readonly params: CaseParams;

  constructor(suite: string, file: readonly string[], test: readonly string[], params: CaseParams) {
    super(suite, file, test);
    assert(test.length > 0, 'multi-case (or finer) query must have test-path');
    this.params = { ...params };
  }

  protected toStringHelper(): string[] {
    const paramsParts = stringifyPublicParams(this.params);
    return [
      this.suite,
      this.filePathParts.join(kPathSeparator),
      this.testPathParts.join(kPathSeparator),
      [...paramsParts, kWildcard].join(kParamSeparator),
    ];
  }
}

/**
 * A multi-case test query, like `s:f:t:` or `s:f:t:a=1,b=1`.
 *
 * Immutable (makes copies of constructor args).
 */
export class TestQuerySingleCase extends TestQueryMultiCase {
  readonly level: TestQueryLevel = 4;
  readonly isMultiCase: false = false;

  protected toStringHelper(): string[] {
    const paramsParts = stringifyPublicParams(this.params);
    return [
      this.suite,
      this.filePathParts.join(kPathSeparator),
      this.testPathParts.join(kPathSeparator),
      paramsParts.join(kParamSeparator),
    ];
  }
}
