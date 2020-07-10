package # hide from PAUSE
    DBIx::Class::CDBICompat::GetSet;

use strict;
use warnings;

#use base qw/Class::Accessor/;

sub get {
  my ($self, @cols) = @_;
  if (@cols > 1) {
    return map { $self->get_column($_) } @cols;
  } else {
    return $self->get_column($_[1]);
  }
}

sub set {
  my($self, %data) = @_;

  # set_columns() is going to do a string comparison before setting.
  # This breaks on DateTime objects (whose comparison is arguably broken)
  # so we stringify anything first.
  for my $key (keys %data) {
    next unless ref $data{$key};
    $data{$key} = "$data{$key}";
  }

  return shift->set_columns(\%data);
}

1;
