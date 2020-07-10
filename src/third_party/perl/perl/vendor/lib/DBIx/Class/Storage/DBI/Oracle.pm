package DBIx::Class::Storage::DBI::Oracle;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';
use Try::Tiny;
use namespace::clean;

sub _rebless {
  my ($self) = @_;

  # Default driver
  my $class = $self->_server_info->{normalized_dbms_version} < 9
    ? 'DBIx::Class::Storage::DBI::Oracle::WhereJoins'
    : 'DBIx::Class::Storage::DBI::Oracle::Generic';

  $self->ensure_class_loaded ($class);
  bless $self, $class;
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::Oracle - Base class for Oracle driver

=head1 DESCRIPTION

This class simply provides a mechanism for discovering and loading a sub-class
for a specific version Oracle backend. It should be transparent to the user.

For Oracle major versions < 9 it loads the ::Oracle::WhereJoins subclass,
which unrolls the ANSI join style DBIC normally generates into entries in
the WHERE clause for compatibility purposes. To force usage of this version
no matter the database version, add

  __PACKAGE__->storage_type('::DBI::Oracle::WhereJoins');

to your Schema class.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
