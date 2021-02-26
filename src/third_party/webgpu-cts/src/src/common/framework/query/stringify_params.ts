import {
  CaseParams,
  ParamArgument,
  badParamValueChars,
  paramKeyIsPublic,
} from '../params_utils.js';
import { assert } from '../util/util.js';

import { stringifyParamValue, stringifyParamValueUniquely } from './json_param_value.js';
import { kBigSeparator, kParamKVSeparator } from './separators.js';

export function stringifyPublicParams(p: CaseParams): string[] {
  return Object.keys(p)
    .filter(k => paramKeyIsPublic(k))
    .map(k => stringifySingleParam(k, p[k]));
}

/**
 * An _approximately_ unique string representing a CaseParams value.
 */
export function stringifyPublicParamsUniquely(p: CaseParams): string {
  const keys = Object.keys(p).sort();
  return keys
    .filter(k => paramKeyIsPublic(k))
    .map(k => stringifySingleParamUniquely(k, p[k]))
    .join(kBigSeparator);
}

export function stringifySingleParam(k: string, v: ParamArgument) {
  return `${k}${kParamKVSeparator}${stringifySingleParamValue(v)}`;
}

function stringifySingleParamUniquely(k: string, v: ParamArgument) {
  return `${k}${kParamKVSeparator}${stringifyParamValueUniquely(v)}`;
}

function stringifySingleParamValue(v: ParamArgument): string {
  const s = stringifyParamValue(v);
  assert(
    !badParamValueChars.test(s),
    `JSON.stringified param value must not match ${badParamValueChars} - was ${s}`
  );
  return s;
}
