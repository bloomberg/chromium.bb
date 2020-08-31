package # hide form PAUSE
    DBIx::Class::CDBICompat::AbstractSearch;

use strict;
use warnings;

=head1 NAME

DBIx::Class::CDBICompat::AbstractSearch - Emulates Class::DBI::AbstractSearch

=head1 SYNOPSIS

See DBIx::Class::CDBICompat for usage directions.

=head1 DESCRIPTION

Emulates L<Class::DBI::AbstractSearch>.

=cut

# The keys are mostly the same.
my %cdbi2dbix = (
    limit               => 'rows',
);

sub search_where {
    my $class = shift;
    my $where = (ref $_[0]) ? $_[0] : { @_ };
    my $attr  = (ref $_[0]) ? $_[1] : {};

    # Translate the keys
    $attr->{$cdbi2dbix{$_}} = delete $attr->{$_} for keys %cdbi2dbix;

    return $class->resultset_instance->search($where, $attr);
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
