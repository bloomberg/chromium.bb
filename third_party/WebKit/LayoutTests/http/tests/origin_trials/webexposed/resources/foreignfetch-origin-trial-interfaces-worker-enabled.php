<?php
// Generate token with the command:
// generate_token.py  http://127.0.0.1:8000 ForeignFetch -expire-timestamp=2000000000
header("Origin-Trial: AinuK1QNcHGJANXfN0Pzkvpxm8rocd4kcqQqhp7rBN8PEX/1JicCoDXX3fIhk7iwBOCCOQYR0cvZ8XHTLJJfVw0AAABUeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRm9yZWlnbkZldGNoIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
header('Content-Type: application/javascript');
?>
importScripts('../../../resources/testharness.js');
importScripts('../../../resources/origin-trials-helper.js');

test(t => {
  OriginTrialsHelper.check_properties(this,
    {'InstallEvent':['registerForeignFetch'],
     'global':['onforeignfetch']});
}, 'ForeignFetch related properties on interfaces in service worker, with trial enabled.');

test(t => {
  OriginTrialsHelper.check_interfaces(this,
    ['ForeignFetchEvent']);
}, 'ForeignFetch related interfaces in service worker, with trial enabled.');
