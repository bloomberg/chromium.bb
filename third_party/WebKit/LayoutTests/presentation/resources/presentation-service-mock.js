/*
 * Mock implementation of mojo PresentationService.
 */

"use strict";

const presentationServiceMock =
    loadMojoModules('presentationServiceMock', [
      'third_party/WebKit/public/platform/modules/presentation/presentation.mojom',
      'mojo/public/js/bindings',
    ]).then(mojo => {
      const [presentationService, bindings] = mojo.modules;

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

        setClient(client) { this.client_ = client; }

        startSession(urls) {
          return Promise.resolve({
              sessionInfo: { url: urls[0], id: 'fakesession' },
              error: null,
          });
        }

        joinSession(urls) {
          return Promise.resolve({
              sessionInfo: { url: urls[0], id: 'fakeSessionId' },
              error: null,
          });
        }

        closeConnection(url, id) {
          this.client_.onConnectionClosed(
              {url: url, id: id},
              presentationService.PresentationConnectionCloseReason.CLOSED, '');
        }
      }

      return new PresentationServiceMock(mojo.frameInterfaces);
    });

function waitForClick(callback, button) {
  button.addEventListener('click', callback, { once: true });

  if (!('eventSender' in window))
    return;

  const boundingRect = button.getBoundingClientRect();
  const x = boundingRect.left + boundingRect.width / 2;
  const y = boundingRect.top + boundingRect.height / 2;

  eventSender.mouseMoveTo(x, y);
  eventSender.mouseDown();
  eventSender.mouseUp();
}
