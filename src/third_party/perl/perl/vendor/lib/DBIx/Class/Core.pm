package DBIx::Class::Core;

use strict;
use warnings;

use base qw/DBIx::Class/;

__PACKAGE__->load_components(qw/
  Relationship
  InflateColumn
  PK::Auto
  PK
  Row
  ResultSourceProxy::Table
/);

1;

__END__

=head1 NAME

DBIx::Class::Core - Core set of DBIx::Class modules

=head1 SYNOPSIS

  # In your result (table) classes
  use base 'DBIx::Class::Core';

=head1 DESCRIPTION

This class just inherits from the various modules that make up the
L<DBIx::Class> core features.  You almost certainly want these.

The core modules currently are:

=over 4

=item L<DBIx::Class::InflateColumn>

=item L<DBIx::Class::Relationship> (See also L<DBIx::Class::Relationship::Base>)

=item L<DBIx::Class::PK::Auto>

=item L<DBIx::Class::PK>

=item L<DBIx::Class::Row>

=item L<DBIx::Class::ResultSourceProxy::Table> (See also L<DBIx::Class::ResultSource>)

=back

A better overview of the methods found in a Result class can be found
in L<DBIx::Class::Manual::ResultClass>.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
