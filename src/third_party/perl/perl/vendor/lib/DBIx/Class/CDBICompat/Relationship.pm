package
    DBIx::Class::CDBICompat::Relationship;

use strict;
use warnings;

use DBIx::Class::_Util 'quote_sub';

=head1 NAME

DBIx::Class::CDBICompat::Relationship - Emulate the Class::DBI::Relationship object returned from meta_info()

=head1 DESCRIPTION

Emulate the Class::DBI::Relationship object returned from C<meta_info()>.

=cut

my %method2key = (
    name            => 'type',
    class           => 'self_class',
    accessor        => 'accessor',
    foreign_class   => 'class',
    args            => 'args',
);

quote_sub __PACKAGE__ . "::$_" => "\$_[0]->{$method2key{$_}}"
  for keys %method2key;

sub new {
    my($class, $args) = @_;

    return bless $args, $class;
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
