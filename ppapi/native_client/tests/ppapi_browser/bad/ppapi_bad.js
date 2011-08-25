// Helper routines for generating bad load tests.
// Webpage must have an 'embeds' div for injecting NaCl modules.
// Depends on nacltest.js.

function createModule(id, src) {
  return createNaClEmbed({
    id: id,
    src: src,
    width: 100,
    height: 20
  });
}


function addModule(module) {
  $('embeds').appendChild(module);
}


function removeModule(module) {
  $('embeds').removeChild(module);
}


function badLoadTest(tester, id, src, error_string) {
  tester.addAsyncTest(id, function(test){
    var module = createModule(id, src);

    test.expectEvent(module, 'load', function(e) {
      removeModule(module);
      test.fail('Module loaded successfully.');
    });
    test.expectEvent(module, 'error', function(e) {
      test.assertEqual(module.readyState, 4);
      test.assertEqual(module.lastError, error_string);
      test.expectEvent(module, 'loadend', function(e) {
        removeModule(module);
        test.pass();
      });
    });
    addModule(module);
  });
}
