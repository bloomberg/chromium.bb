if (this.importScripts)
    importScripts('../../js/resources/js-test-pre.js');

description("Tests that atob() / btoa() functions are exposed to workers");

shouldBeTrue("typeof atob === 'function'");
shouldBeTrue("typeof btoa === 'function'");

finishJSTest();
