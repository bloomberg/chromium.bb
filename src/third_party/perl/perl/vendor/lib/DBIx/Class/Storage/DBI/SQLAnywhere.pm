package DBIx::Class::Storage::DBI::SQLAnywhere;

use strict;
use warnings;
use base qw/DBIx::Class::Storage::DBI::UniqueIdentifier/;
use mro 'c3';
use List::Util 'first';
use Try::Tiny;
use namespace::clean;

__PACKAGE__->mk_group_accessors(simple => qw/_identity/);
__PACKAGE__->sql_limit_dialect ('RowNumberOver');
__PACKAGE__->sql_quote_char ('"');

__PACKAGE__->new_guid('UUIDTOSTR(NEWID())');

# default to the UUID decoding cursor, overridable by the user
__PACKAGE__->cursor_class('DBIx::Class::Storage::DBI::SQLAnywhere::Cursor');

=head1 NAME

DBIx::Class::Storage::DBI::SQLAnywhere - Driver for SQL Anywhere

=head1 DESCRIPTION

This class implements autoincrements for SQL Anywhere and provides
L<DBIx::Class::InflateColumn::DateTime> support and support for the
C<uniqueidentifier> type (via
L<DBIx::Class::Storage::DBI::SQLAnywhere::Cursor>.)

You need the C<DBD::SQLAnywhere> driver that comes with the SQL Anywhere
distribution, B<NOT> the one on CPAN. It is usually under a path such as:

  /opt/sqlanywhere11/sdk/perl

Recommended L<connect_info|DBIx::Class::Storage::DBI/connect_info> settings:

  on_connect_call => 'datetime_setup'

=head1 METHODS

=cut

sub last_insert_id { shift->_identity }

sub _prefetch_autovalues {
  my $self = shift;
  my ($source, $colinfo, $to_insert) = @_;

  my $values = $self->next::method(@_);

  my $identity_col =
    first { $colinfo->{$_}{is_auto_increment} } keys %$colinfo;

# user might have an identity PK without is_auto_increment
#
# FIXME we probably should not have supported the above, see what
# does it take to move away from it
  if (not $identity_col) {
    foreach my $pk_col ($source->primary_columns) {
      if (
        ! exists $to_insert->{$pk_col}
          and
        $colinfo->{$pk_col}{data_type}
          and
        $colinfo->{$pk_col}{data_type} !~ /^uniqueidentifier/i
      ) {
        $identity_col = $pk_col;
        last;
      }
    }
  }

  if ($identity_col && (not exists $to_insert->{$identity_col})) {
    my $dbh = $self->_get_dbh;
    my $table_name = $source->from;
    $table_name    = $$table_name if ref $table_name;

    my ($identity) = try {
      $dbh->selectrow_array("SELECT GET_IDENTITY('$table_name')")
    };

    if (defined $identity) {
      $values->{$identity_col} = $identity;
      $self->_identity($identity);
    }
  }

  return $values;
}

sub _uuid_to_str {
  my ($self, $data) = @_;

  $data = unpack 'H*', $data;

  for my $pos (8, 13, 18, 23) {
    substr($data, $pos, 0) = '-';
  }

  return $data;
}

# select_single does not invoke a cursor object at all, hence UUID decoding happens
# here if the proper cursor class is set
sub select_single {
  my $self = shift;

  my @row = $self->next::method(@_);

  return @row
    unless $self->cursor_class->isa('DBIx::Class::Storage::DBI::SQLAnywhere::Cursor');

  my ($ident, $select) = @_;

  my $col_info = $self->_resolve_column_info($ident);

  for my $select_idx (0..$#$select) {
    my $selected = $select->[$select_idx];

    next if ref $selected;

    my $data_type = $col_info->{$selected}{data_type}
      or next;

    if ($self->_is_guid_type($data_type)) {
      my $returned = $row[$select_idx];

      if (length $returned == 16) {
        $row[$select_idx] = $self->_uuid_to_str($returned);
      }
    }
  }

  return @row;
}

# this sub stolen from MSSQL

sub build_datetime_parser {
  my $self = shift;
  my $type = "DateTime::Format::Strptime";
  try {
    eval "require ${type}"
  }
  catch {
    $self->throw_exception("Couldn't load ${type}: $_");
  };

  return $type->new( pattern => '%Y-%m-%d %H:%M:%S.%6N' );
}

=head2 connect_call_datetime_setup

Used as:

    on_connect_call => 'datetime_setup'

In L<connect_info|DBIx::Class::Storage::DBI/connect_info> to set the date and
timestamp formats (as temporary options for the session) for use with
L<DBIx::Class::InflateColumn::DateTime>.

The C<TIMESTAMP> data type supports up to 6 digits after the decimal point for
second precision. The full precision is used.

The C<DATE> data type supposedly stores hours and minutes too, according to the
documentation, but I could not get that to work. It seems to only store the
date.

You will need the L<DateTime::Format::Strptime> module for inflation to work.

=cut

sub connect_call_datetime_setup {
  my $self = shift;

  $self->_do_query(
    "set temporary option timestamp_format = 'yyyy-mm-dd hh:mm:ss.ssssss'"
  );
  $self->_do_query(
    "set temporary option date_format      = 'yyyy-mm-dd hh:mm:ss.ssssss'"
  );
}

sub _exec_svp_begin {
    my ($self, $name) = @_;

    $self->_dbh->do("SAVEPOINT $name");
}

# can't release savepoints that have been rolled back
sub _exec_svp_release { 1 }

sub _exec_svp_rollback {
    my ($self, $name) = @_;

    $self->_dbh->do("ROLLBACK TO SAVEPOINT $name")
}

1;

=head1 MAXIMUM CURSORS

A L<DBIx::Class> application can use a lot of cursors, due to the usage of
L<prepare_cached|DBI/prepare_cached>.

The default cursor maximum is C<50>, which can be a bit too low. This limit can
be turned off (or increased) by the DBA by executing:

  set option max_statement_count = 0
  set option max_cursor_count    = 0

Highly recommended.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
