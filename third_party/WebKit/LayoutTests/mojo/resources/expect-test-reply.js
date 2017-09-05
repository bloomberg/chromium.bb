importScripts('../../resources/testharness.js');
importScripts('file:///gen/layout_test_data/mojo/public/js/mojo_bindings.js');
importScripts('file:///gen/content/test/data/mojo_layouttest_test.mojom.js');
importScripts('helpers.js');

promise_test(async () => {
  let helper = new content.mojom.MojoLayoutTestHelperPtr;
  Mojo.bindInterface(content.mojom.MojoLayoutTestHelper.name,
                     mojo.makeRequest(helper).handle);

  let response = await helper.reverse('the string');
  assert_equals(response.reversed, kTestReply);
}, 'Expect test interface to be overridden in worker');

done();
