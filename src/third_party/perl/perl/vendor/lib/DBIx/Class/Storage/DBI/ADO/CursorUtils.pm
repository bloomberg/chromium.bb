package # hide from PAUSE
    DBIx::Class::Storage::DBI::ADO::CursorUtils;

use strict;
use warnings;
use base 'Exporter';

our @EXPORT_OK = qw/_normalize_guids _strip_trailing_binary_nulls/;

sub _strip_trailing_binary_nulls {
  my ($select, $col_infos, $data, $storage) = @_;

  foreach my $select_idx (0..$#$select) {

    next unless defined $data->[$select_idx];

    my $data_type = $col_infos->{$select->[$select_idx]}{data_type}
      or next;

    $data->[$select_idx] =~ s/\0+\z//
      if $storage->_is_binary_type($data_type);
  }
}

sub _normalize_guids {
  my ($select, $col_infos, $data, $storage) = @_;

  foreach my $select_idx (0..$#$select) {

    next unless defined $data->[$select_idx];

    my $data_type = $col_infos->{$select->[$select_idx]}{data_type}
      or next;

    $data->[$select_idx] =~ s/\A \{ (.+) \} \z/$1/xs
      if $storage->_is_guid_type($data_type);
  }
}

1;

# vim:sts=2 sw=2:
