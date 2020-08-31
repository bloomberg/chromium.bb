package # hide from PAUSE
    DBIx::Class::Relationship::ProxyMethods;

use strict;
use warnings;
use base 'DBIx::Class';
use DBIx::Class::_Util 'quote_sub';
use namespace::clean;

our %_pod_inherit_config =
  (
   class_map => { 'DBIx::Class::Relationship::ProxyMethods' => 'DBIx::Class::Relationship' }
  );

sub register_relationship {
  my ($class, $rel, $info) = @_;
  if (my $proxy_args = $info->{attrs}{proxy}) {
    $class->proxy_to_related($rel, $proxy_args);
  }
  $class->next::method($rel, $info);
}

sub proxy_to_related {
  my ($class, $rel, $proxy_args) = @_;
  my %proxy_map = $class->_build_proxy_map_from($proxy_args);

  quote_sub "${class}::$_", sprintf( <<'EOC', $rel, $proxy_map{$_} )
    my $self = shift;
    my $relobj = $self->%1$s;
    if (@_ && !defined $relobj) {
      $relobj = $self->create_related( %1$s => { %2$s => $_[0] } );
      @_ = ();
    }
    $relobj ? $relobj->%2$s(@_) : undef;
EOC
    for keys %proxy_map
}

sub _build_proxy_map_from {
  my ( $class, $proxy_arg ) = @_;
  my $ref = ref $proxy_arg;

  if ($ref eq 'HASH') {
    return %$proxy_arg;
  }
  elsif ($ref eq 'ARRAY') {
    return map {
      (ref $_ eq 'HASH')
        ? (%$_)
        : ($_ => $_)
    } @$proxy_arg;
  }
  elsif ($ref) {
    $class->throw_exception("Unable to process the 'proxy' argument $proxy_arg");
  }
  else {
    return ( $proxy_arg => $proxy_arg );
  }
}

1;
