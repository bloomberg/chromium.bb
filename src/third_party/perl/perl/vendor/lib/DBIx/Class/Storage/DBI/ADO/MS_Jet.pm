package DBIx::Class::Storage::DBI::ADO::MS_Jet;

use strict;
use warnings;
use base qw/
  DBIx::Class::Storage::DBI::ADO
  DBIx::Class::Storage::DBI::ACCESS
/;
use mro 'c3';
use DBIx::Class::Storage::DBI::ADO::CursorUtils '_normalize_guids';
use namespace::clean;

__PACKAGE__->cursor_class('DBIx::Class::Storage::DBI::ADO::MS_Jet::Cursor');

=head1 NAME

DBIx::Class::Storage::DBI::ADO::MS_Jet - Support for MS Access over ADO

=head1 DESCRIPTION

This driver is a subclass of L<DBIx::Class::Storage::DBI::ADO> and
L<DBIx::Class::Storage::DBI::ACCESS> for connecting to MS Access via
L<DBD::ADO>.

See the documentation for L<DBIx::Class::Storage::DBI::ACCESS> for
information on the MS Access driver for L<DBIx::Class>.

This driver implements workarounds for C<TEXT/IMAGE/MEMO> columns, sets the
L<cursor_class|DBIx::Class::Storage::DBI/cursor_class> to
L<DBIx::Class::Storage::DBI::ADO::MS_Jet::Cursor> to normalize returned
C<GUID> values and provides L<DBIx::Class::InflateColumn::DateTime> support
for C<DATETIME> columns.

=head1 EXAMPLE DSNs

  # older Access versions:
  dbi:ADO:Microsoft.Jet.OLEDB.4.0;Data Source=C:\Users\rkitover\Documents\access_sample.accdb

  # newer Access versions:
  dbi:ADO:Provider=Microsoft.ACE.OLEDB.12.0;Data Source=C:\Users\rkitover\Documents\access_sample.accdb;Persist Security Info=False'

=head1 TEXT/IMAGE/MEMO COLUMNS

The ADO driver does not suffer from the
L<problems|DBIx::Class::Storage::DBI::ODBC::ACCESS/"TEXT/IMAGE/MEMO COLUMNS">
the L<ODBC|DBIx::Class::Storage::DBI::ODBC::ACCESS> driver has with these types
of columns. You can use them safely.

When you execute a C<CREATE TABLE> statement over this driver with a C<TEXT>
column, it will be converted to C<MEMO>, while in the
L<ODBC|DBIx::Class::Storage::DBI::ODBC::ACCESS> driver it is converted to
C<VARCHAR(255)>.

However, the caveat about L<LongReadLen|DBI/LongReadLen> having to be twice the
max size of your largest C<MEMO/TEXT> column C<+1> still applies. L<DBD::ADO>
sets L<LongReadLen|DBI/LongReadLen> to a large value by default, so it should be
safe to just leave it unset. If you do pass a L<LongReadLen|DBI/LongReadLen> in
your L<connect_info|DBIx::Class::Storage::DBI/connect_info>, it will be
multiplied by two and C<1> added, just as for the
L<ODBC|DBIx::Class::Storage::DBI::ODBC::ACCESS> driver.

=cut

# set LongReadLen = LongReadLen * 2 + 1 (see docs on MEMO)
sub _run_connection_actions {
  my $self = shift;

  my $long_read_len = $self->_dbh->{LongReadLen};

# This is the DBD::ADO default.
  if ($long_read_len != 2147483647) {
    $self->_dbh->{LongReadLen} = $long_read_len * 2 + 1;
  }

  return $self->next::method(@_);
}

# AutoCommit does not get reset properly after transactions for some reason
# (probably because of my nested transaction hacks in ACCESS.pm) fix it up
# here.

sub _exec_txn_commit {
  my $self = shift;
  $self->next::method(@_);
  $self->_dbh->{AutoCommit} = $self->_dbh_autocommit
    if $self->{transaction_depth} == 1;
}

sub _exec_txn_rollback {
  my $self = shift;
  $self->next::method(@_);
  $self->_dbh->{AutoCommit} = $self->_dbh_autocommit
    if $self->{transaction_depth} == 1;
}

# Fix up GUIDs for ->find, for cursors see the cursor_class above.

sub select_single {
  my $self = shift;
  my ($ident, $select) = @_;

  my @row = $self->next::method(@_);

  return @row unless
    $self->cursor_class->isa('DBIx::Class::Storage::DBI::ADO::MS_Jet::Cursor');

  my $col_infos = $self->_resolve_column_info($ident);

  _normalize_guids($select, $col_infos, \@row, $self);

  return @row;
}

sub datetime_parser_type {
  'DBIx::Class::Storage::DBI::ADO::MS_Jet::DateTime::Format'
}

package # hide from PAUSE
  DBIx::Class::Storage::DBI::ADO::MS_Jet::DateTime::Format;

my $datetime_format = '%m/%d/%Y %I:%M:%S %p';
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
