package # hide from PAUSE
    DBIx::Class::Admin::Types;

# Workaround for https://rt.cpan.org/Public/Bug/Display.html?id=83336
use warnings;
use strict;

use MooseX::Types -declare => [qw(
    DBICConnectInfo
    DBICArrayRef
    DBICHashRef
)];
use MooseX::Types::Moose qw/Int HashRef ArrayRef Str Any Bool/;
use MooseX::Types::JSON qw(JSON);

subtype DBICArrayRef,
    as ArrayRef;

subtype DBICHashRef,
    as HashRef;

coerce DBICArrayRef,
  from JSON,
  via { _json_to_data ($_) };

coerce DBICHashRef,
  from JSON,
  via { _json_to_data($_) };

subtype DBICConnectInfo,
  as ArrayRef;

coerce DBICConnectInfo,
  from JSON,
   via { return _json_to_data($_) } ;

coerce DBICConnectInfo,
  from Str,
    via { return _json_to_data($_) };

coerce DBICConnectInfo,
  from HashRef,
   via { [ $_ ] };

sub _json_to_data {
  my ($json_str) = @_;
  my $json = JSON::Any->new(allow_barekey => 1, allow_singlequote => 1, relaxed=>1);
  my $ret = $json->jsonToObj($json_str);
  return $ret;
}

1;
