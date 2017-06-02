#!/usr/bin/perl

print "Status: 401 Unauthorized\r\n";
print "WWW-Authenticate: Basic realm=\"WebKit Test Realm\"\r\n\r\n";
