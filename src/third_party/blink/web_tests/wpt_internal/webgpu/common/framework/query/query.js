/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../util/util.js';
import { encodeURIComponentSelectively } from './encode_selectively.js';
import { kBigSeparator, kPathSeparator, kWildcard, kParamSeparator } from './separators.js';
import { stringifyPublicParams } from './stringify_params.js';

/**
 * Represents a test query of some level.
 *
 * TestQuery types are immutable.
 */

// SingleCase

/**
 * A multi-file test query, like `s:*` or `s:a,b,*`.
 *
 * Immutable (makes copies of constructor args).
 */
export class TestQueryMultiFile {
  level = 1;
  isMultiFile = true;

  constructor(suite, file) {
    this.suite = suite;
    this.filePathParts = [...file];
  }

  toString() {
    return encodeURIComponentSelectively(this.toStringHelper().join(kBigSeparator));
  }

  toStringHelper() {
    return [this.suite, [...this.filePathParts, kWildcard].join(kPathSeparator)];
  }
}

/**
 * A multi-test test query, like `s:f:*` or `s:f:a,b,*`.
 *
 * Immutable (makes copies of constructor args).
 */
export class TestQueryMultiTest extends TestQueryMultiFile {
  level = 2;
  isMultiFile = false;
  isMultiTest = true;

  constructor(suite, file, test) {
    super(suite, file);
    assert(file.length > 0, 'multi-test (or finer) query must have file-path');
    this.testPathParts = [...test];
  }

  toStringHelper() {
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
  level = 3;
  isMultiTest = false;
  isMultiCase = true;

  constructor(suite, file, test, params) {
    super(suite, file, test);
    assert(test.length > 0, 'multi-case (or finer) query must have test-path');
    this.params = { ...params };
  }

  toStringHelper() {
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
  level = 4;
  isMultiCase = false;

  toStringHelper() {
    const paramsParts = stringifyPublicParams(this.params);
    return [
      this.suite,
      this.filePathParts.join(kPathSeparator),
      this.testPathParts.join(kPathSeparator),
      paramsParts.join(kParamSeparator),
    ];
  }
}
