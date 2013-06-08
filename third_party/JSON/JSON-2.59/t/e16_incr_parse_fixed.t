#!/usr/bin/perl

BEGIN {
    $ENV{ PERL_JSON_BACKEND } = $ARGV[0] || 'JSON::backportPP';
}

use strict;
use Test::More tests => 4;

use JSON;

my $json = JSON->new->allow_nonref();

my @vs = $json->incr_parse('"a\"bc');

ok( not scalar(@vs) );

@vs = $json->incr_parse('"');

is( $vs[0], "a\"bc" );


$json = JSON->new;

@vs = $json->incr_parse('"a\"bc');
ok( not scalar(@vs) );
@vs = eval { $json->incr_parse('"') };
ok($@ =~ qr/JSON text must be an object or array/);

