#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Origin: *\n";
print "Cache-Control: no-store\n\n";

print <<DONE
{ "https": "$ENV{"HTTP_HTTPS"}" }
DONE
