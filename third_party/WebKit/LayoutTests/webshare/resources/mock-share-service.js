'use strict';

let mockShareService = loadMojoModules(
    'mockShareService',
    ['mojo/public/js/router',
     'third_party/WebKit/public/platform/modules/webshare/webshare.mojom',
    ]).then(mojo => {
  let [router, webshare] = mojo.modules;

  class MockShareService extends webshare.ShareService.stubClass {
    constructor(interfaceProvider) {
      super();
      interfaceProvider.addInterfaceOverrideForTesting(
          webshare.ShareService.name,
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
  return new MockShareService(mojo.frameInterfaces);
});

function share_test(func, name, properties) {
  promise_test(t => mockShareService.then(mock => {
    let mockPromise = mock.init_();
    return Promise.race([func(t, mock), mockPromise]);
  }), name, properties);
}

// Copied from resources/bluetooth/bluetooth-helpers.js.
function callWithKeyDown(functionCalledOnKeyPress) {
  return new Promise(resolve => {
    function onKeyPress() {
      document.removeEventListener('keypress', onKeyPress, false);
      resolve(functionCalledOnKeyPress());
    }
    document.addEventListener('keypress', onKeyPress, false);

    eventSender.keyDown(' ', []);
  });
}
