/*
 * Mock implementation of mojo PresentationService.
 */

"use strict";

let presentationServiceMock = loadMojoModules(
    'presentationServiceMock',
    [
      'third_party/WebKit/public/platform/modules/presentation/presentation.mojom',
      'mojo/public/js/bindings',
    ]).then(mojo => {
      let [ presentationService, bindings ] = mojo.modules;

      class PresentationServiceMock {
        constructor(interfaceProvider) {
          interfaceProvider.addInterfaceOverrideForTesting(
              presentationService.PresentationService.name,
              handle => this.bindingSet_.addBinding(this, handle));
          this.interfaceProvider_ = interfaceProvider;
          this.pendingResponse_ = null;
          this.bindingSet_ = new bindings.BindingSet(
              presentationService.PresentationService);
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
