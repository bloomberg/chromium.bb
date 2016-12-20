/*
 * Mock implementation of mojo PresentationService.
 */

"use strict";

let presentationServiceMock = loadMojoModules(
    'presentationServiceMock',
    [
      'third_party/WebKit/public/platform/modules/presentation/presentation.mojom',
      'mojo/public/js/router',
    ]).then(mojo => {
      let [ presentationService, router ] = mojo.modules;

      class PresentationServiceMock {
        constructor(interfaceProvider) {
          interfaceProvider.addInterfaceOverrideForTesting(
              presentationService.PresentationService.name,
              handle => this.connectPresentationService_(handle));
          this.interfaceProvider_ = interfaceProvider;
          this.pendingResponse_ = null;
        }

        connectPresentationService_(handle) {
          this.presentationServiceStub_ = new presentationService.PresentationService.stubClass(this);
          this.presentationServiceRouter_ = new router.Router(handle);
          this.presentationServiceRouter_.setIncomingReceiver(this.presentationServiceStub_);
        }

        startSession(urls) {
          return Promise.resolve({
              sessionInfo: { url: urls[0], id: 'fakesession' },
              error: null,
          });
        }
      }

      return new PresentationServiceMock(mojo.frameInterfaces);
    });
