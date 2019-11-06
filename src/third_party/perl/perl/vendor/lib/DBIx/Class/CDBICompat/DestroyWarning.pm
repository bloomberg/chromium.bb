package # hide from PAUSE
    DBIx::Class::CDBICompat::DestroyWarning;

use strict;
use warnings;
use DBIx::Class::_Util 'detected_reinvoked_destructor';
use namespace::clean;

sub DESTROY {
  return if &detected_reinvoked_destructor;

  my ($self) = @_;
  my $class = ref $self;
  warn "$class $self destroyed without saving changes to "
         .join(', ', keys %{$self->{_dirty_columns} || {}})
    if keys %{$self->{_dirty_columns} || {}};
}

1;
