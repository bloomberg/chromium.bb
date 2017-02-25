importScripts('../../../resources/testharness.js');
importScripts('../../../resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_properties_missing(this,
    {'InstallEvent':['registerForeignFetch'],
     'global':['onforeignfetch']});
}, 'ForeignFetch related properties on interfaces in service worker, without trial.');

test(t => {
  OriginTrialsHelper.check_interfaces_missing(this,
    ['ForeignFetchEvent']);
}, 'ForeignFetch related interfaces in service worker, without trial.');
