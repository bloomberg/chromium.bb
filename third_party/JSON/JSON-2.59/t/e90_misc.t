#!/usr/bin/perl

use strict;
use Test::More tests => 4;

BEGIN {
    $ENV{ PERL_JSON_BACKEND } = $ARGV[0] || 'JSON::backportPP';
}

use JSON;

# reported by https://rt.cpan.org/Public/Bug/Display.html?id=68359

eval { JSON->to_json( 5, { allow_nonref => 1 } ) };
ok($@);

is( q{"5"}, JSON::to_json( "5", { allow_nonref => 1 } ) );
is( q{5},  JSON::to_json( 5, { allow_nonref => 1 } ) );
is( q{"JSON"}, JSON::to_json( 'JSON', { allow_nonref => 1 } ) );
