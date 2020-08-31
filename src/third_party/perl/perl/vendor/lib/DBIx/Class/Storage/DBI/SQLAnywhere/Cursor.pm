package DBIx::Class::Storage::DBI::SQLAnywhere::Cursor;

use strict;
use warnings;
use base 'DBIx::Class::Storage::DBI::Cursor';
use mro 'c3';

=head1 NAME

DBIx::Class::Storage::DBI::SQLAnywhere::Cursor - GUID Support for SQL Anywhere
over L<DBD::SQLAnywhere>

=head1 DESCRIPTION

This class is for normalizing GUIDs retrieved from SQL Anywhere via
L<DBD::SQLAnywhere>.

You probably don't want to be here, see
L<DBIx::Class::Storage::DBI::SQLAnywhere> for information on the SQL Anywhere
driver.

Unfortunately when using L<DBD::SQLAnywhere>, GUIDs come back in binary, the
purpose of this class is to transform them to text.
L<DBIx::Class::Storage::DBI::SQLAnywhere> sets
L<cursor_class|DBIx::Class::Storage::DBI/cursor_class> to this class by default.
It is overridable via your
L<connect_info|DBIx::Class::Storage::DBI/connect_info>.

You can use L<DBIx::Class::Cursor::Cached> safely with this class and not lose
the GUID normalizing functionality,
L<::Cursor::Cached|DBIx::Class::Cursor::Cached> uses the underlying class data
for the inner cursor class.

=cut

my $unpack_guids = sub {
  my ($select, $col_infos, $data, $storage) = @_;

  for my $select_idx (0..$#$select) {
    next unless (
      defined $data->[$select_idx]
        and
      length($data->[$select_idx]) == 16
    );

    my $selected = $select->[$select_idx];

    my $data_type = $col_infos->{$select->[$select_idx]}{data_type}
      or next;

    $data->[$select_idx] = $storage->_uuid_to_str($data->[$select_idx])
      if $storage->_is_guid_type($data_type);
  }
};


sub next {
  my $self = shift;

  my @row = $self->next::method(@_);

  $unpack_guids->(
    $self->args->[1],
    $self->{_colinfos} ||= $self->storage->_resolve_column_info($self->args->[0]),
    \@row,
    $self->storage
  );

  return @row;
}

sub all {
  my $self = shift;

  my @rows = $self->next::method(@_);

  $unpack_guids->(
    $self->args->[1],
    $self->{_colinfos} ||= $self->storage->_resolve_column_info($self->args->[0]),
    $_,
    $self->storage
  ) for @rows;


  return @rows;
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;

# vim:sts=2 sw=2:
