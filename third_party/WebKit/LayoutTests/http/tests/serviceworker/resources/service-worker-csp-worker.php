<?php 
header('Content-Type: application/javascript');
$directive = $_GET['directive'];

if ($directive == 'default') {
  header('Content-Security-Policy: default-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');

test(function() {
    var import_script_failed = false;
    try {
      importScripts('http://localhost:8000/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for default-src');

async_test(function(t) {
    fetch('http://localhost:8000/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for default-src');

<?php

} else if ($directive == 'script') {
  header('Content-Security-Policy: script-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');

test(function() {
    var import_script_failed = false;
    try {
      importScripts('http://localhost:8000/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_true(import_script_failed,
                'Importing the other origins script should fail.');
  }, 'importScripts test for script-src');

async_test(function(t) {
    fetch('http://localhost:8000/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          t.done();
        }, function(){
          assert_unreached('fetch should not fail.');
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for script-src');

<?php

} else if ($directive == 'connect') {
  header('Content-Security-Policy: connect-src \'self\'');

?>
importScripts('worker-testharness.js');
importScripts('test-helpers.js');

test(function() {
    var import_script_failed = false;
    try {
      importScripts('http://localhost:8000/serviceworker/resources/empty.js');
    } catch(e) {
      import_script_failed = true;
    }
    assert_false(import_script_failed,
                 'Importing the other origins script should not fail.');
  }, 'importScripts test for connect-src');

async_test(function(t) {
    fetch('http://localhost:8000/serviceworker/resources/fetch-access-control.php?ACAOrigin=*',
          {mode: 'cors'})
      .then(function(response){
          assert_unreached('fetch should fail.');
        }, function(){
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch test for connect-src');

<?php
}
?>