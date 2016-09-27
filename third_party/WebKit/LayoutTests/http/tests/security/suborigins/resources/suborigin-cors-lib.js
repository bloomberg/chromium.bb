var server = 'http://127.0.0.1:8000';

var xorigin_anon_script_string =
  'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';
var xorigin_creds_script_string =
  'http://127.0.0.1:8000/security/resources/cors-script.php?' +
  'credentials=true&value=ran';
var xorigin_ineligible_script_string =
  'http://127.0.0.1:8000/security/resources/cors-script.php?value=ran';

var xorigin_anon_style_string =
  'http://127.0.0.1:8000/security/resources/cors-style.php?';
var xorigin_creds_style_string =
  'http://127.0.0.1:8000/security/resources/cors-style.php?credentials=true';
var xorigin_ineligible_style_string =
  'http://127.0.0.1:8000/security/resources/cors-style.php?';

var xorigin_anon_hello_string =
  'http://127.0.0.1:8000/security/resources/cors-hello.php?';
var xorigin_creds_hello_string =
  'http://127.0.0.1:8000/security/resources/cors-hello.php?credentials=true';
var xorigin_ineligible_hello_string =
  'http://127.0.0.1:8000/security/resources/cors-hello.php?';

var xorigin_font_string =
  'http://127.0.0.1:8000/security/resources/cors-font.php?';

function gen_cors_src(str, acao_value) {
  if (acao_value) {
    str = str + '&cors=' + acao_value;
  }
  return str;
}

function xoriginAnonScript(acao_value) {
  return gen_cors_src(xorigin_anon_script_string, acao_value);
}
function xoriginCredsScript(acao_value) {
  return gen_cors_src(xorigin_creds_script_string, acao_value);
}
function xoriginIneligibleScript() {
  return gen_cors_src(xorigin_ineligible_script_string, 'false');
}

function xoriginAnonStyle(acao_value) {
  return gen_cors_src(xorigin_anon_style_string, acao_value);
}
function xoriginCredsStyle(acao_value) {
  return gen_cors_src(xorigin_creds_style_string, acao_value);
}
function xoriginIneligibleStyle() {
  return gen_cors_src(xorigin_ineligible_style_string, 'false');
}

function xoriginAnonHello(acao_value) {
  return gen_cors_src(xorigin_anon_hello_string, acao_value);
}
function xoriginCredsHello(acao_value) {
  return gen_cors_src(xorigin_creds_hello_string, acao_value);
}
function xoriginIneligibleHello() {
  return gen_cors_src(xorigin_ineligible_hello_string, 'false');
}

function xoriginFont(acao_value) {
  return gen_cors_src(xorigin_font_string, acao_value);
}
function xoriginIneligibleFont() {
  return gen_cors_src(xorigin_font_string, 'false');
}

var SuboriginTest = function(pass, name, src, crossorigin_value) {
  this.pass = pass;
  this.name = name;
  this.src = src;
  this.crossorigin_value = crossorigin_value;
}
var SuboriginScriptTest = function(pass, name, src, crossorigin_value) {
  SuboriginTest.call(this, pass, 'Script: ' + name, src, crossorigin_value);
}

SuboriginScriptTest.prototype.execute = function() {
  var test = async_test(this.name);
  var e = document.createElement('script');
  e.src = this.src;
  if (this.crossorigin_value) {
    e.setAttribute('crossorigin', this.crossorigin_value);
  }
  if (this.pass) {
    e.addEventListener('load', function() { test.done(); });
    e.addEventListener('error', function() {
        test.step(function() { assert_unreached('Good load fired error handler.'); });
      });
  } else {
    e.addEventListener('load', function() {
        test.step(function() { assert_unreached('Bad load successful.') });
      });
    e.addEventListener('error', function() { test.done(); });
  }
  document.body.appendChild(e);
}

// <link> tests
// Link tests must be done synchronously because they rely on the presence and
// absence of global style, which can affect later tests. Thus, instead of
// executing them one at a time, the link tests are implemented as a queue
// that builds up a list of tests, and then executes them one at a time.
var SuboriginLinkTest = function(queue, rel, pass_verify, fail_verify,
                                 pass, name, href, crossorigin_value) {
  this.queue = queue;
  this.rel = rel;
  this.pass_verify = pass_verify;
  this.fail_verify = fail_verify;

  this.queue.push(this);

  SuboriginTest.call(this, pass, name, href, crossorigin_value);
}

SuboriginLinkTest.prototype.execute = function() {
  var container = document.getElementById('container');
  while (container.hasChildNodes()) {
    container.removeChild(container.firstChild);
  }

  var test = async_test(this.name);
  var pass_verify = this.pass_verify;
  var fail_verify = this.fail_verify;

  var div = document.createElement('div');
  div.className = 'id1';
  var e = document.createElement('link');
  e.rel = this.rel;
  e.href = this.src;
  if (this.crossorigin_value) {
    e.setAttribute('crossorigin', this.crossorigin_value);
  }
  if (this.pass) {
    e.addEventListener('load', function() {
        test.step(function() {
            pass_verify(div, e);
            test.done();
          });
      });
    e.addEventListener('error', function() {
        test.step(function() {
            assert_unreached('Good load fired error handler.');
          });
      });
  } else {
    e.addEventListener('load', function() {
         test.step(function() { assert_unreached('Bad load succeeded.'); });
     });
    e.addEventListener('error', function() {
        test.step(function() {
            fail_verify(div, e);
            test.done();
          });
      });
  }
  container.appendChild(div);
  container.appendChild(e);
}
