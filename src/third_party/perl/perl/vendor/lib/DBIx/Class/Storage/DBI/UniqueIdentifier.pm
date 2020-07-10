package DBIx::Class::Storage::DBI::UniqueIdentifier;

use strict;
use warnings;
use base 'DBIx::Class::Storage::DBI';
use mro 'c3';

__PACKAGE__->mk_group_accessors(inherited => 'new_guid');

=head1 NAME

DBIx::Class::Storage::DBI::UniqueIdentifier - Storage component for RDBMSes
supporting GUID types

=head1 DESCRIPTION

This is a storage component for databases that support GUID types such as
C<uniqueidentifier>, C<uniqueidentifierstr> or C<guid>.

GUIDs are generated automatically for PK columns with a supported
L<data_type|DBIx::Class::ResultSource/data_type>, as well as non-PK with
L<auto_nextval|DBIx::Class::ResultSource/auto_nextval> set.

=head1 METHODS

=head2 new_guid

The composing class must set C<new_guid> to the method used to generate a new
GUID. It can also set it to C<undef>, in which case the user is required to set
it, or a runtime error will be thrown. It can be:

=over 4

=item string

In which case it is used as the name of database function to create a new GUID,

=item coderef

In which case the coderef should return a string GUID, using L<Data::GUID>, or
whatever GUID generation method you prefer. It is passed the C<$self>
L<DBIx::Class::Storage> reference as a parameter.

=back

For example:

  $schema->storage->new_guid(sub { Data::GUID->new->as_string });

=cut

my $GUID_TYPE = qr/^(?:uniqueidentifier(?:str)?|guid)\z/i;

sub _is_guid_type {
  my ($self, $data_type) = @_;

  return $data_type =~ $GUID_TYPE;
}

sub _prefetch_autovalues  {
  my $self = shift;
  my ($source, $col_info, $to_insert) = @_;

  my %guid_cols;
  my @pk_cols = $source->primary_columns;
  my %pk_col_idx;
  @pk_col_idx{@pk_cols} = ();

  my @pk_guids = grep {
    $col_info->{$_}{data_type}
    &&
    $col_info->{$_}{data_type} =~ $GUID_TYPE
  } @pk_cols;

  my @auto_guids = grep {
    $col_info->{$_}{data_type}
    &&
    $col_info->{$_}{data_type} =~ $GUID_TYPE
    &&
    $col_info->{$_}{auto_nextval}
  } grep { not exists $pk_col_idx{$_} } $source->columns;

  my @get_guids_for =
    grep { not exists $to_insert->{$_} } (@pk_guids, @auto_guids);

  for my $guid_col (@get_guids_for) {
    my $new_guid;

    my $guid_method = $self->new_guid;

    if (not defined $guid_method) {
      $self->throw_exception(
        'You must set new_guid() on your storage. See perldoc '
       .'DBIx::Class::Storage::DBI::UniqueIdentifier'
      );
    }

    if (ref $guid_method eq 'CODE') {
      $to_insert->{$guid_col} = $guid_method->($self);
    }
    else {
      ($to_insert->{$guid_col}) = $self->_get_dbh->selectrow_array("SELECT $guid_method");
    }
  }

  return $self->next::method(@_);
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
