package DBIx::Class::ResultSource::Table;

use strict;
use warnings;

use DBIx::Class::ResultSet;

use base qw/DBIx::Class/;
__PACKAGE__->load_components(qw/ResultSource/);

=head1 NAME

DBIx::Class::ResultSource::Table - Table object

=head1 SYNOPSIS

=head1 DESCRIPTION

Table object that inherits from L<DBIx::Class::ResultSource>.

=head1 METHODS

=head2 from

Returns the FROM entry for the table (i.e. the table name)

=cut

sub from { shift->name; }

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
