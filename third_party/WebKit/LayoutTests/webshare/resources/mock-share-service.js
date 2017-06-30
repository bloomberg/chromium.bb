'use strict';

let mockShareService = loadMojoModules(
    'mockShareService',
    ['mojo/public/js/bindings',
     'third_party/WebKit/public/platform/modules/webshare/webshare.mojom',
    ]).then(mojo => {
  let [bindings, webshare] = mojo.modules;

  class MockShareService {
    constructor(interfaceProvider) {
      this.webshare_ = webshare;
      this.bindingSet_ = new bindings.BindingSet(webshare.ShareService);

      interfaceProvider.addInterfaceOverrideForTesting(
          webshare.ShareService.name,
          handle => this.bindingSet_.addBinding(this, handle));
    }

    // Returns a Promise that gets rejected if the test should fail.
    init_() {
      // sequence of [expectedTitle, expectedText, result].
      this.shareResultQueue_ = [];

      return new Promise((resolve, reject) => {this.reject_ = reject});
    }

    share(title, text, url) {
      let callback = null;
      let result = new Promise(resolve => {callback = resolve;});

      if (!this.shareResultQueue_.length) {
        this.reject_('Unexpected call to mojo share method');
        return result;
      }

      let [expectedTitle, expectedText, expectedUrl, error] =
          this.shareResultQueue_.shift();
      try {
        assert_equals(title, expectedTitle);
        assert_equals(text, expectedText);
        assert_equals(url.url, expectedUrl);
      } catch (e) {
        this.reject_(e);
        return result;
      }
      callback({error: error});

      return result;
    }

    pushShareResult(expectedTitle, expectedText, expectedUrl, result) {
      this.shareResultQueue_.push(
          [expectedTitle, expectedText, expectedUrl, result]);
    }
  }
  return new MockShareService(mojo.frameInterfaces);
});

function share_test(func, name, properties) {
  promise_test(t => mockShareService.then(mock => {
    let mockPromise = mock.init_();
    return Promise.race([func(t, mock.webshare_, mock), mockPromise]);
  }), name, properties);
}

// Copied from resources/bluetooth/bluetooth-helpers.js.
function callWithKeyDown(functionCalledOnKeyPress) {
  return new Promise((resolve, reject) => {
    function onKeyPress() {
      document.removeEventListener('keypress', onKeyPress, false);
      try {
        resolve(functionCalledOnKeyPress());
      } catch (e) {
        reject(e);
      }
    }
    document.addEventListener('keypress', onKeyPress, false);

    eventSender.keyDown(' ', []);
  });
}
