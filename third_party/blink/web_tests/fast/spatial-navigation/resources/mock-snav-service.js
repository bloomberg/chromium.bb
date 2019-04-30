'use strict';

class MockSpatialNavigationHostService {
  constructor() {
    this.canExitFocus = false;
    this.canSelectElement = false;
    this.hasNextFormElement = false;
    this.callback = null;
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.SpatialNavigationHost);
    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.SpatialNavigationHost.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  spatialNavigationStateChanged(state) {
    this.canExitFocus = state.canExitFocus;
    this.canSelectElement = state.canSelectElement;
    this.hasNextFormElement = state.hasNextFormElement;
    if (this.callback) {
      this.callback();
    }
  }
}

let mockSnavService = new MockSpatialNavigationHostService();

function snavCallback() {
  return new Promise(resolve => {
    mockSnavService.callback = resolve;
  });
}
