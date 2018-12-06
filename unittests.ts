import {
  TestCollection,
} from './framework';
import {
  ParamIterable,
  PUnit,
  PList,
  PCombiner,
} from './params';

// "unit tests" for the testing framework
(async function unittests() {
  const tests = new TestCollection();

  tests.add("async_callback",
      new PUnit(),
      async p => console.log(p));
  
  function add(name: string, params: ParamIterable) {
    tests.add(name, params, console.log);
  }

  add("list", new PList('hello', [1, 2, 3]));
  add("unit", new PUnit());
  add("combine_none", new PCombiner([]));
  add("combine_unit_unit", new PCombiner([new PUnit(), new PUnit()]));
  add("combiner_lists", new PCombiner([
    new PList('x', [1, 2]),
    new PList('y', ['a', 'b']),
    new PUnit(),
  ]));
  add("combiner_arrays", new PCombiner([
    [{x: 1, y: 2}, {x: 10, y: 20}],
    [{z: 'z'}, {w: 'w'}],
  ]));
  add("combiner_mixed", new PCombiner([
    new PList('x', [1, 2]),
    [{z: 'z'}, {w: 'w'}],
  ]));

  await tests.run();
})();