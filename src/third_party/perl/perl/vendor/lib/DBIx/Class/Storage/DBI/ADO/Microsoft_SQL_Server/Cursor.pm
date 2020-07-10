package DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor;

use strict;
use warnings;
use base 'DBIx::Class::Storage::DBI::Cursor';
use mro 'c3';
use DBIx::Class::Storage::DBI::ADO::CursorUtils qw/_normalize_guids _strip_trailing_binary_nulls/;
use namespace::clean;

=head1 NAME

DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server::Cursor - Remove trailing
NULLs in binary data and normalize GUIDs for MSSQL over ADO

=head1 DESCRIPTION

This class is for removing trailing C<NULL>s from binary data and removing braces
from GUIDs retrieved from Microsoft SQL Server over ADO.

You probably don't want to be here, see
L<DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server> for information on the
Microsoft SQL Server driver for ADO and L<DBIx::Class::Storage::DBI::MSSQL> for
the Microsoft SQL Server driver base class.

Unfortunately when using L<DBD::ADO>, binary data comes back padded with
trailing C<NULL>s and GUIDs come back wrapped in braces, the purpose of this
class is to remove the C<NULL>s and braces.
L<DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server> sets
L<cursor_class|DBIx::Class::Storage::DBI/cursor_class> to this class by
default. It is overridable via your
L<connect_info|DBIx::Class::Storage::DBI/connect_info>.

You can use L<DBIx::Class::Cursor::Cached> safely with this class and not lose
the binary data normalizing functionality,
L<::Cursor::Cached|DBIx::Class::Cursor::Cached> uses the underlying class data
for the inner cursor class.

=cut

sub next {
  my $self = shift;

  my @row = $self->next::method(@_);

  $self->{_colinfos} ||= $self->storage->_resolve_column_info($self->args->[0]);

  _normalize_guids(
    $self->args->[1],
    $self->{_colinfos},
    \@row,
    $self->storage
  );

  _strip_trailing_binary_nulls(
    $self->args->[1],
    $self->{_colinfos},
    \@row,
    $self->storage
  );

  return @row;
}

sub all {
  my $self = shift;

  my @rows = $self->next::method(@_);

  $self->{_colinfos} ||= $self->storage->_resolve_column_info($self->args->[0]);

  for (@rows) {
    _normalize_guids(
      $self->args->[1],
      $self->{_colinfos},
      $_,
      $self->storage
    );

    _strip_trailing_binary_nulls(
      $self->args->[1],
      $self->{_colinfos},
      $_,
      $self->storage
    );
  }

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
