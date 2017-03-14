'use strict';

function assert_relatedapplication_equals(actual, expected, description) {
  assert_equals(actual.platform, expected.platform, description);
  assert_equals(actual.url, expected.url, description);
  assert_equals(actual.id, expected.id, description);
}

function assert_array_relatedapplication_equals(
    actual, expected, description) {
  assert_equals(actual.length, expected.length, description);

  for (let i = 0; i < actual.length; i++)
    assert_relatedapplication_equals(actual[i], expected[i], description);
}

let mockInstalledAppProvider = loadMojoModules(
    'mockInstalledAppProvider',
    ['mojo/public/js/bindings',
     'third_party/WebKit/public/platform/modules/installedapp/installed_app_provider.mojom',
    ]).then(mojo => {
  let [bindings, installedAppProvider] = mojo.modules;

  class MockInstalledAppProvider {
    constructor(interfaceProvider) {
      this.bindingSet_ =
          new bindings.BindingSet(installedAppProvider.InstalledAppProvider);

      interfaceProvider.addInterfaceOverrideForTesting(
          installedAppProvider.InstalledAppProvider.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    // Returns a Promise that gets rejected if the test should fail.
    init_() {
      // sequence of [expectedRelatedApps, installedApps].
      this.callQueue_ = [];

      return new Promise((resolve, reject) => {this.reject_ = reject});
    }

    filterInstalledApps(relatedApps) {
      let callback = null;
      let result = new Promise(resolve => {callback = resolve;});

      if (!this.callQueue_.length) {
        this.reject_('Unexpected call to mojo FilterInstalledApps method');
        return result;
      }

      let [expectedRelatedApps, installedApps] = this.callQueue_.shift();
      try {
        assert_array_relatedapplication_equals(
            relatedApps, expectedRelatedApps);
      } catch (e) {
        this.reject_(e);
        return result;
      }
      callback({installedApps: installedApps});

      return result;
    }

    pushExpectedCall(expectedRelatedApps, installedApps) {
      this.callQueue_.push([expectedRelatedApps, installedApps]);
    }
  }
  return new MockInstalledAppProvider(mojo.frameInterfaces);
});

// Creates a test case that uses a mock InstalledAppProvider.
// |func| is a function that takes (t, mock), where |mock| is a
// MockInstalledAppProvider that can have expectations set with
// pushExpectedCall. It should return a promise, the result of
// getInstalledRelatedApps().
// |name| and |properties| are standard testharness arguments.
function installedapp_test(func, name, properties) {
  promise_test(t => mockInstalledAppProvider.then(mock => {
    let mockPromise = mock.init_();
    return Promise.race([func(t, mock), mockPromise]);
  }), name, properties);
}
