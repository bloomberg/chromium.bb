use Test::More;
use strict;
BEGIN { plan tests => 5 };

BEGIN { $ENV{PERL_JSON_BACKEND} = "JSON::backportPP"; }

BEGIN {
    use_ok('JSON');
}

ok( exists $INC{ 'JSON/backportPP.pm' }, 'load backportPP' );
ok( ! exists $INC{ 'JSON/PP.pm' }, q/didn't load PP/ );

is( JSON->backend, 'JSON::PP' );
ok( JSON->backend->is_pp );
