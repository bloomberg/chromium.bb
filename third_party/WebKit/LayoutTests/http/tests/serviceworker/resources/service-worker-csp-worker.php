<?php 
header('Content-Type: application/javascript');
$directive = $_GET['directive'];

if ($directive == 'default') {
  header('Content-Security-Policy: default-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('../../resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTP_REMOTE_ORIGIN +
        '/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for default-src');

async_test(function(t) {
    fetch(host_info.HTTP_REMOTE_ORIGIN +
          '/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for default-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      '/serviceworker/resources/redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTP_REMOTE_ORIGIN +
      '/serviceworker/resources/fetch-access-control.php?'
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('Redirected fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for default-src');

<?php

} else if ($directive == 'script') {
  header('Content-Security-Policy: script-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('../../resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTP_REMOTE_ORIGIN +
        '/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for script-src');

async_test(function(t) {
    fetch(host_info.HTTP_REMOTE_ORIGIN +
          '/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          t.done();
        }, function(){
          assert_unreached('fetch should not fail.');
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for script-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      '/serviceworker/resources/redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTP_REMOTE_ORIGIN +
      '/serviceworker/resources/fetch-access-control.php?'
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          t.done();
        }, function(){
          assert_unreached('Redirected fetch should not fail.');
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for script-src');

<?php

} else if ($directive == 'connect') {
  header('Content-Security-Policy: connect-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');
importScripts('../../resources/get-host-info.js');

var host_info = get_host_info();

test(function() {
    var import_script_failed = false;
    try {
      importScripts(host_info.HTTP_REMOTE_ORIGIN +
        '/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_false(import_script_failed,
                 'Importing the other origins script should not fail.');
  }, 'importScripts test for connect-src');

async_test(function(t) {
    fetch(host_info.HTTP_REMOTE_ORIGIN +
          '/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for connect-src');

async_test(function(t) {
    var REDIRECT_URL = host_info.HTTP_ORIGIN +
      '/serviceworker/resources/redirect.php?Redirect=';
    var OTHER_BASE_URL = host_info.HTTP_REMOTE_ORIGIN +
      '/serviceworker/resources/fetch-access-control.php?'
    fetch(REDIRECT_URL + encodeURIComponent(OTHER_BASE_URL + 'ACAOrigin=*'),
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('Redirected fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Redirected fetch test for connect-src');

<?php
}
?>
