#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "X-XSS-Protection: 1\n";
print "Content-Type: text/html\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print $cgi->param('q');
print "</body>\n";
print "</html>\n";
