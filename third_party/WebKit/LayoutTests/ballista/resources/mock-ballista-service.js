'use strict';

let mockBallistaService = loadMojoModules(
    'mockBallistaService',
    ['mojo/public/js/router',
     'third_party/WebKit/public/platform/modules/ballista/ballista.mojom',
    ]).then(mojo => {
  let [router, ballista] = mojo.modules;

  class MockBallistaService extends ballista.BallistaService.stubClass {
    constructor(serviceRegistry) {
      super();
      serviceRegistry.addServiceOverrideForTesting(
          ballista.BallistaService.name,
          handle => this.connect_(handle));
    }

    // Returns a Promise that gets rejected if the test should fail.
    init_() {
      // sequence of [expectedTitle, expectedText, result].
      this.shareResultQueue_ = [];

      return new Promise((resolve, reject) => {this.reject_ = reject});
    }

    connect_(handle) {
      this.router_ = new router.Router(handle);
      this.router_.setIncomingReceiver(this);
    }

    share(title, text) {
      let callback = null;
      let result = new Promise(resolve => {callback = resolve;});

      if (!this.shareResultQueue_.length) {
        this.reject_('Unexpected call to mojo share method');
        return result;
      }

      let expectedTitle, expectedText, error;
      [expectedTitle, expectedText, error] = this.shareResultQueue_.shift();
      try {
        assert_equals(title, expectedTitle);
        assert_equals(text, expectedText);
      } catch (e) {
        this.reject_(e);
        return result;
      }
      callback({error: error});

      return result;
    }

    pushShareResult(expectedTitle, expectedText, result) {
      this.shareResultQueue_.push([expectedTitle, expectedText, result]);
    }
  }
  return new MockBallistaService(mojo.frameServiceRegistry);
});

function ballista_test(func, name, properties) {
  promise_test(t => mockBallistaService.then(mock => {
    let mockPromise = mock.init_();
    return Promise.race([func(t, mock), mockPromise]);
  }), name, properties);
}
