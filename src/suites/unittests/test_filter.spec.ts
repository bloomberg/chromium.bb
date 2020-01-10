export const description = `
Unit tests for test_filter .matches() methods.
`;

import { TestGroup } from '../../framework/index.js';
import { FilterByGroup } from '../../framework/test_filter/filter_by_group.js';
import {
  FilterByParamsExact,
  FilterByParamsMatch,
  FilterByTestMatch,
} from '../../framework/test_filter/filter_one_file.js';

import { UnitTest } from './unit_test.js';

export const g = new TestGroup(UnitTest);

g.test('FilterByGroup/matches', t => {
  {
    const f = new FilterByGroup('ab', 'de/f');
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/fg' } }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x' }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x', params: {} }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/fg/' }, test: 'x', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f/g' }, test: 'x', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f/g/' }, test: 'x', params: null }));
  }
  {
    const f = new FilterByGroup('ab', 'de/f/');
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg' } }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x' }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x', params: null }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'x', params: {} }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg/' }, test: 'x', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f/g' }, test: 'x', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f/g/' }, test: 'x', params: null }));
  }
});

g.test('FilterByTestMatch/matches', t => {
  const f = new FilterByTestMatch({ suite: 'ab', path: 'de/f' }, 'xy');
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy' }));
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: null }));
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xyz', params: null }));
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xyz', params: {} }));
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xyz', params: { z: 1 } }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' } }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f/' }, test: 'xyz', params: null }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/fg' }, test: 'xyz', params: null }));
});

g.test('FilterByParamsMatch/matches', t => {
  {
    const f = new FilterByParamsMatch({ suite: 'ab', path: 'de/f' }, 'xy', null);
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: {} }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: { z: 1 } }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' } }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy' }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: undefined }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xyz', params: {} }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f/' }, test: 'xy', params: {} }));
  }
  {
    const f = new FilterByParamsMatch({ suite: 'ab', path: 'de/f' }, 'xy', {});
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: null }));
    t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: {} }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' } }));
    t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy' }));
  }
});

g.test('FilterByParamsExact', t => {
  const f = new FilterByParamsExact({ suite: 'ab', path: 'de/f' }, 'xy', {});
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: null }));
  t.expect(f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: {} }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy', params: { z: 1 } }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xyz', params: {} }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f/' }, test: 'xy', params: {} }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' } }));
  t.expect(!f.matches({ spec: { suite: 'ab', path: 'de/f' }, test: 'xy' }));
});
