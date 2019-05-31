package DBIx::Class::Storage::DBI::ACCESS;

use strict;
use warnings;
use base 'DBIx::Class::Storage::DBI::UniqueIdentifier';
use mro 'c3';

use DBI ();
use List::Util 'first';
use namespace::clean;

__PACKAGE__->sql_limit_dialect ('Top');
__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker::ACCESS');
__PACKAGE__->sql_quote_char ([qw/[ ]/]);

sub sqlt_type { 'ACCESS' }

__PACKAGE__->new_guid(undef);

=head1 NAME

DBIx::Class::Storage::DBI::ACCESS - Support specific to MS Access

=head1 DESCRIPTION

This is the base class for Microsoft Access support.

This driver supports L<last_insert_id|DBIx::Class::Storage::DBI/last_insert_id>,
empty inserts for tables with C<AUTOINCREMENT> columns, nested transactions via
L<auto_savepoint|DBIx::Class::Storage::DBI/auto_savepoint>, C<GUID> columns via
L<DBIx::Class::Storage::DBI::UniqueIdentifier>.

=head1 SUPPORTED VERSIONS

This module has currently only been tested on MS Access 2010.

Information about how well it works on different version of MS Access is welcome
(write the mailing list, or submit a ticket to RT if you find bugs.)

=head1 USING GUID COLUMNS

If you have C<GUID> PKs or other C<GUID> columns with
L<auto_nextval|DBIx::Class::ResultSource/auto_nextval> you will need to set a
L<new_guid|DBIx::Class::Storage::DBI::UniqueIdentifier/new_guid> callback, like
so:

  $schema->storage->new_guid(sub { Data::GUID->new->as_string });

Under L<Catalyst> you can use code similar to this in your
L<Catalyst::Model::DBIC::Schema> C<Model.pm>:

  after BUILD => sub {
    my $self = shift;
    $self->storage->new_guid(sub { Data::GUID->new->as_string });
  };

=cut

sub _dbh_last_insert_id { $_[1]->selectrow_array('select @@identity') }

# support empty insert
sub insert {
  my $self = shift;
  my ($source, $to_insert) = @_;

  my $columns_info = $source->columns_info;

  if (keys %$to_insert == 0) {
    my $autoinc_col = first {
      $columns_info->{$_}{is_auto_increment}
    } keys %$columns_info;

    $self->throw_exception(
      'empty insert only supported for tables with an autoincrement column'
    ) unless $autoinc_col;

    my $table = $source->from;
    $table = $$table if ref $table;

    $to_insert->{$autoinc_col} = \"dmax('${autoinc_col}', '${table}')+1";
  }

  return $self->next::method(@_);
}

sub bind_attribute_by_data_type {
  my $self = shift;
  my ($data_type) = @_;

  my $attributes = $self->next::method(@_) || {};

  if ($self->_is_text_lob_type($data_type)) {
    $attributes->{TYPE} = DBI::SQL_LONGVARCHAR;
  }
  elsif ($self->_is_binary_lob_type($data_type)) {
    $attributes->{TYPE} = DBI::SQL_LONGVARBINARY;
  }

  return $attributes;
}

# savepoints are not supported, but nested transactions are.
# Unfortunately DBI does not support nested transactions.
# WARNING: this code uses the undocumented 'BegunWork' DBI attribute.

sub _exec_svp_begin {
  my ($self, $name) = @_;

  local $self->_dbh->{AutoCommit} = 1;
  local $self->_dbh->{BegunWork}  = 0;
  $self->_exec_txn_begin;
}

# A new nested transaction on the same level releases the previous one.
sub _exec_svp_release { 1 }

sub _exec_svp_rollback {
  my ($self, $name) = @_;

  local $self->_dbh->{AutoCommit} = 0;
  local $self->_dbh->{BegunWork}  = 1;
  $self->_exec_txn_rollback;
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
