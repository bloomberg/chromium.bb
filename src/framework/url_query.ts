import { TestCaseID, TestSpecID } from './id.js';
import { ParamArgument, ParamSpec } from './params/index.js';

export function encodeSelectively(s: string): string {
  let ret = encodeURIComponent(s);
  ret = ret.replace(/%20/g, '+'); // Encode space with + (equivalent but more readable)
  ret = ret.replace(/%22/g, '"');
  ret = ret.replace(/%2C/g, ',');
  ret = ret.replace(/%2F/g, '/');
  ret = ret.replace(/%3A/g, ':');
  ret = ret.replace(/%3D/g, '=');
  ret = ret.replace(/%5B/g, '[');
  ret = ret.replace(/%5D/g, ']');
  ret = ret.replace(/%7B/g, '{');
  ret = ret.replace(/%7D/g, '}');
  return ret;
}

export function extractPublicParams(params: ParamSpec): ParamSpec {
  const publicParams: ParamSpec = {};
  for (const k of Object.keys(params)) {
    if (!k.startsWith('_')) {
      publicParams[k] = params[k];
    }
  }
  return publicParams;
}

export function checkPublicParamType(v: ParamArgument): void {
  if (typeof v === 'number' || typeof v === 'string' || typeof v === 'boolean' || v === undefined) {
    return;
  }
  if (v instanceof Array) {
    for (const x of v) {
      if (typeof x !== 'number') {
        break;
      }
    }
    return;
  }
  throw new Error('Invalid type for test case params ' + v);
}

export function makeQueryString(spec: TestSpecID, testcase?: TestCaseID): string {
  let s = spec.suite + ':';
  s += spec.path + ':';
  if (testcase !== undefined) {
    s += testcase.test + '=';
    if (testcase.params) {
      s += JSON.stringify(extractPublicParams(testcase.params));
    }
  }
  return encodeSelectively(s);
}
