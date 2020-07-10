package DBIx::Class::Storage::DBI::ODBC::ACCESS;

use strict;
use warnings;
use base qw/
  DBIx::Class::Storage::DBI::ODBC
  DBIx::Class::Storage::DBI::ACCESS
/;
use mro 'c3';

__PACKAGE__->mk_group_accessors(inherited =>
  'disable_sth_caching_for_image_insert_or_update'
);

__PACKAGE__->disable_sth_caching_for_image_insert_or_update(1);

=head1 NAME

DBIx::Class::Storage::DBI::ODBC::ACCESS - Support specific to MS Access over ODBC

=head1 DESCRIPTION

This class implements support specific to Microsoft Access over ODBC.

It is a subclass of L<DBIx::Class::Storage::DBI::ODBC> and
L<DBIx::Class::Storage::DBI::ACCESS>, see those classes for more
information.

It is loaded automatically by L<DBIx::Class::Storage::DBI::ODBC> when it
detects a MS Access back-end.

This driver implements workarounds for C<IMAGE> and C<MEMO> columns, and
L<DBIx::Class::InflateColumn::DateTime> support for C<DATETIME> columns.

=head1 EXAMPLE DSN

  dbi:ODBC:driver={Microsoft Access Driver (*.mdb, *.accdb)};dbq=C:\Users\rkitover\Documents\access_sample.accdb

=head1 TEXT/IMAGE/MEMO COLUMNS

Avoid using C<TEXT> columns as they will be truncated to 255 bytes. Some other
drivers (like L<ADO|DBIx::Class::Storage::DBI::ADO::MS_Jet>) will automatically
convert C<TEXT> columns to C<MEMO>, but the ODBC driver does not.

C<IMAGE> columns work correctly, but the statements for inserting or updating an
C<IMAGE> column will not be L<cached|DBI/prepare_cached>, due to a bug in the
Access ODBC driver.

C<MEMO> columns work correctly as well, but you must take care to set
L<LongReadLen|DBI/LongReadLen> to C<$max_memo_size * 2 + 1>. This is done for
you automatically if you pass L<LongReadLen|DBI/LongReadLen> in your
L<connect_info|DBIx::Class::Storage::DBI/connect_info>; but if you set this
attribute directly on the C<$dbh>, keep this limitation in mind.

=cut

# set LongReadLen = LongReadLen * 2 + 1 (see docs on MEMO)
sub _run_connection_actions {
  my $self = shift;

  my $long_read_len = $self->_dbh->{LongReadLen};

  # 80 is another default (just like 0) on some drivers
  if ($long_read_len != 0 && $long_read_len != 80) {
    $self->_dbh->{LongReadLen} = $long_read_len * 2 + 1;
  }

  # batch operations do not work
  $self->_disable_odbc_array_ops;

  return $self->next::method(@_);
}

sub insert {
  my $self = shift;
  my ($source, $to_insert) = @_;

  my $columns_info = $source->columns_info;

  my $is_image_insert = 0;

  for my $col (keys %$to_insert) {
    if ($self->_is_binary_lob_type($columns_info->{$col}{data_type})) {
      $is_image_insert = 1;
      last;
    }
  }

  local $self->{disable_sth_caching} = 1 if $is_image_insert
    && $self->disable_sth_caching_for_image_insert_or_update;

  return $self->next::method(@_);
}

sub update {
  my $self = shift;
  my ($source, $fields) = @_;

  my $columns_info = $source->columns_info;

  my $is_image_insert = 0;

  for my $col (keys %$fields) {
    if ($self->_is_binary_lob_type($columns_info->{$col}{data_type})) {
      $is_image_insert = 1;
      last;
    }
  }

  local $self->{disable_sth_caching} = 1 if $is_image_insert
    && $self->disable_sth_caching_for_image_insert_or_update;

  return $self->next::method(@_);
}

sub datetime_parser_type {
  'DBIx::Class::Storage::DBI::ODBC::ACCESS::DateTime::Format'
}

package # hide from PAUSE
  DBIx::Class::Storage::DBI::ODBC::ACCESS::DateTime::Format;

my $datetime_format = '%Y-%m-%d %H:%M:%S'; # %F %T, no fractional part
my $datetime_parser;

sub parse_datetime {
  shift;
  require DateTime::Format::Strptime;
  $datetime_parser ||= DateTime::Format::Strptime->new(
    pattern  => $datetime_format,
    on_error => 'croak',
  );
  return $datetime_parser->parse_datetime(shift);
}

sub format_datetime {
  shift;
  require DateTime::Format::Strptime;
  $datetime_parser ||= DateTime::Format::Strptime->new(
    pattern  => $datetime_format,
    on_error => 'croak',
  );
  return $datetime_parser->format_datetime(shift);
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
