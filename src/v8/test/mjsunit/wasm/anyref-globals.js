// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-anyref --expose-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestDefaultValue() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const g_nullref = builder.addGlobal(kWasmAnyRef, true);
  builder.addFunction("main", kSig_r_v)
    .addBody([kExprGetGlobal, g_nullref.index])
    .exportAs("main");

  const instance = builder.instantiate();
  assertNull(instance.exports.main());
})();

(function TestDefaultValueSecondGlobal() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const g_setref = builder.addGlobal(kWasmAnyRef, true);
  const g_nullref = builder.addGlobal(kWasmAnyRef, true);
  builder.addFunction("main", kSig_r_r)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g_setref.index,
        kExprGetGlobal, g_nullref.index
    ])
    .exportAs("main");

  const instance = builder.instantiate();
  assertNull(instance.exports.main({}));
})();

(function TestGlobalChangeValue() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  // Dummy global for offset.
  builder.addGlobal(kWasmAnyRef, true);
  const g = builder.addGlobal(kWasmAnyRef, true);
  builder.addFunction("main", kSig_r_r)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g.index,
        kExprGetGlobal, g.index
    ])
    .exportAs("main");

  const instance = builder.instantiate();

  const test_value = {hello: 'world'};
  assertSame(test_value, instance.exports.main(test_value));
})();

(function TestGlobalChangeValueWithGC() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const gc_index = builder.addImport("q", "gc", kSig_v_v);
  // Dummy global for offset.
  builder.addGlobal(kWasmAnyRef, true);
  const g = builder.addGlobal(kWasmAnyRef, true);
  builder.addFunction("main", kSig_r_r)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g.index,
        kExprCallFunction, gc_index,  // call gc
        kExprGetGlobal, g.index
    ])
    .exportAs("main");

  const instance = builder.instantiate({q: {gc: gc}});

  const test_value = {hello: 'world'};
  assertSame(test_value, instance.exports.main(test_value));
  assertSame(5, instance.exports.main(5));
  assertSame("Hello", instance.exports.main("Hello"));
})();

(function TestGlobalAsRoot() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const g = builder.addGlobal(kWasmAnyRef, true);
  builder.addFunction("get_global", kSig_r_v)
    .addBody([
        kExprGetGlobal, g.index
    ])
    .exportAs("get_global");

  builder.addFunction("set_global", kSig_v_r)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g.index
    ])
    .exportAs("set_global");

  const instance = builder.instantiate();

  let test_value = {hello: 'world'};
  instance.exports.set_global(test_value);
  test_value = null;
  gc();

  const result = instance.exports.get_global();

  assertEquals('world', result.hello);
})();

(function TestImported() {
  print(arguments.callee.name);
  function Test(obj) {
    let builder = new WasmModuleBuilder();
    const g = builder.addImportedGlobal('m', 'val', kWasmAnyRef);
    builder.addFunction('main', kSig_r_v)
        .addBody([kExprGetGlobal, g])
        .exportAs('main');

    const instance = builder.instantiate({m: {val: obj}});
    assertSame(obj, instance.exports.main());
  }
  Test(null);
  Test(undefined);
  Test(1653);
  Test("mystring");
  Test({q: 14});
  Test(print);
})();

(function TestAnyRefGlobalObjectDefaultValue() {
  print(arguments.callee.name);
  let default_init = new WebAssembly.Global({value: 'anyref', mutable: true});
  assertSame(null, default_init.value);
  assertSame(null, default_init.valueOf());
})();


(function TestAnyRefGlobalObject() {
  print(arguments.callee.name);
  function TestGlobal(obj) {
    const global = new WebAssembly.Global({value: 'anyref'}, obj);
    assertSame(obj, global.value);
    assertSame(obj, global.valueOf());
  }

  TestGlobal(null);
  TestGlobal(undefined);
  TestGlobal(1663);
  TestGlobal("testmyglobal");
  TestGlobal({a: 11});
  TestGlobal(print);
})();

(function TestAnyRefGlobalObjectSetValue() {
  print(arguments.callee.name);
  let global = new WebAssembly.Global({value: 'anyref', mutable: true});

  function TestGlobal(obj) {
    global.value = obj;
    assertSame(obj, global.value);
    assertSame(obj, global.valueOf());
  }

  TestGlobal(null);
  assertThrows(() => TestGlobal(undefined), TypeError);
  TestGlobal(1663);
  TestGlobal("testmyglobal");
  TestGlobal({a: 11});
  TestGlobal(print);
})();

(function TestExportMutableAnyRefGlobal() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const g1 = builder.addGlobal(kWasmAnyRef, true).exportAs("global1");
  builder.addGlobal(kWasmI32, true); // Dummy.
  builder.addGlobal(kWasmAnyRef, true); // Dummy.
  const g2 = builder.addGlobal(kWasmAnyRef, true).exportAs("global2");
  builder.addFunction("main", kSig_v_rr)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g1.index,
        kExprGetLocal, 1,
        kExprSetGlobal, g2.index
    ])
    .exportAs("main");

  const instance = builder.instantiate();
  const obj1 = {x: 221};
  const obj2 = print;
  instance.exports.main(obj1, obj2);
  assertSame(obj1, instance.exports.global1.value);
  assertSame(obj2, instance.exports.global2.value);
})();

(function TestImportMutableAnyRefGlobal() {
  print(arguments.callee.name);
  function Test(obj) {
    let builder = new WasmModuleBuilder();
    const g = builder.addImportedGlobal('m', 'val', kWasmAnyRef, true);
    builder.addFunction('main', kSig_r_v)
        .addBody([kExprGetGlobal, g])
        .exportAs('main');

    const global = new WebAssembly.Global({value: 'anyref', mutable: 'true'}, obj);
    const instance = builder.instantiate({m: {val: global}});
    assertSame(obj, instance.exports.main());
  }
  Test(null);
  Test(undefined);
  Test(1653);
  Test("mystring");
  Test({q: 14});
  Test(print);
})();

(function TestImportMutableAnyRefGlobalFromOtherInstance() {
  print(arguments.callee.name);

  // Create an instance which exports globals.
  let builder1 = new WasmModuleBuilder();
  const g3 = builder1.addGlobal(kWasmAnyRef, true).exportAs("e3");
  builder1.addGlobal(kWasmI32, true).exportAs("e1"); // Dummy.
  builder1.addGlobal(kWasmAnyRef, true).exportAs("e4"); // Dummy.
  const g2 = builder1.addGlobal(kWasmAnyRef, true).exportAs("e2");

  builder1.addFunction("set_globals", kSig_v_rr)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, g2.index,
        kExprGetLocal, 1,
        kExprSetGlobal, g3.index,
    ])
    .exportAs("set_globals");

  builder1.addFunction('get_global2', kSig_r_v)
      .addBody([kExprGetGlobal, g2.index])
      .exportAs('get_global2');

  builder1.addFunction('get_global3', kSig_r_v)
      .addBody([kExprGetGlobal, g3.index])
      .exportAs('get_global3');

  const instance1 = builder1.instantiate();
  const obj2 = {x: 221};
  const obj3 = print;
  instance1.exports.set_globals(obj2, obj3);

  // Create an instance which imports the globals of the other instance.
  let builder2 = new WasmModuleBuilder();
  const i1 = builder2.addImportedGlobal('exports', 'e1', kWasmI32, true);
  const i2 = builder2.addImportedGlobal('exports', 'e2', kWasmAnyRef, true);
  const i3 = builder2.addImportedGlobal('exports', 'e3', kWasmAnyRef, true);
  const i4 = builder2.addImportedGlobal('exports', 'e4', kWasmAnyRef, true);

  builder2.addExportOfKind("reexport1", kExternalGlobal, i1);
  builder2.addExportOfKind("reexport2", kExternalGlobal, i2);
  builder2.addExportOfKind("reexport3", kExternalGlobal, i3);
  builder2.addExportOfKind("reexport4", kExternalGlobal, i4);

  builder2.addFunction("set_globals", kSig_v_rr)
    .addBody([
        kExprGetLocal, 0,
        kExprSetGlobal, i2,
        kExprGetLocal, 1,
        kExprSetGlobal, i3,
    ])
    .exportAs("set_globals");

  builder2.addFunction('get_global2', kSig_r_v)
      .addBody([kExprGetGlobal, i2])
      .exportAs('get_global2');

  builder2.addFunction('get_global3', kSig_r_v)
      .addBody([kExprGetGlobal, i3])
      .exportAs('get_global3');

  const instance2 = builder2.instantiate(instance1);
  // Check if the globals were imported correctly.
  assertSame(obj2, instance2.exports.get_global2());
  assertSame(obj3, instance2.exports.get_global3());

  assertSame(obj2, instance2.exports.reexport2.value);
  assertSame(obj3, instance2.exports.reexport3.value);

  // Check if instance2 can make changes visible for instance1.
  instance2.exports.set_globals(null, undefined);
  assertEquals(null, instance1.exports.get_global2());
  assertEquals(undefined, instance1.exports.get_global3());

  assertEquals(null, instance2.exports.reexport2.value);
  assertEquals(undefined, instance2.exports.reexport3.value);

  // Check if instance1 can make changes visible for instance2.
  instance1.exports.set_globals("foo", 66343);
  assertEquals("foo", instance2.exports.get_global2());
  assertEquals(66343, instance2.exports.get_global3());

  assertEquals("foo", instance2.exports.reexport2.value);
  assertEquals(66343, instance2.exports.reexport3.value);

  const bar2 = {f: "oo"};
  const bar3 = {b: "ar"};
  instance2.exports.reexport2.value = bar2;
  instance2.exports.reexport3.value = bar3;

  assertSame(bar2, instance1.exports.get_global2());
  assertSame(bar3, instance1.exports.get_global3());
  assertSame(bar2, instance2.exports.get_global2());
  assertSame(bar3, instance2.exports.get_global3());
})();
