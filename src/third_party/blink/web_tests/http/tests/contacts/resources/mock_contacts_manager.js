'use strict';

// Mock implementation of the blink.mojom.ContactsManager interface.
class MockContactsManager {
  constructor() {
    this.selectCallback_ = null;

    this.binding_ = new mojo.Binding(blink.mojom.ContactsManager, this);
    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.ContactsManager.name);

    this.interceptor_.oninterfacerequest = e => this.binding_.bind(e.handle);
    this.interceptor_.start();
  }

  // Sets |callback| to be invoked when the ContactsManager.Select() method is
  // being called over the Mojo connection.
  setSelectCallback(callback) {
    this.selectCallback_ = callback;
  }

  async select(multiple, includeNames, includeEmails, includeTel) {
    if (this.selectCallback_) {
      return this.selectCallback_(
          { multiple, includeNames, includeEmails, includeTel });
    }

    return { contacts: null };
  }
}

const mockContactsManager = new MockContactsManager();
