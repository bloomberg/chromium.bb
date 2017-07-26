#!/usr/bin/perl
use CGI;

my $query = new CGI;

if ($query->param("x") || 0) {
  print "Content-type: text/html\r\n\r\n";
  print "<html><head><title>Hello World 2b</title></head>\n";
  print "<script>window.location.reload();</script>\n";
  print "</body></html>\n";
} else {
  print "Content-type: text/html\r\n\r\n";
  print "<html><head><title>Hello World 2a</title></head>\n";
  print "<body onload='formElem.submit()'>\n";
  print "<form method='get' action='' name='formElem'>";
  print "<input type='hidden' name='x' value='y'>";
  print "</form></body></html>\n";
}

