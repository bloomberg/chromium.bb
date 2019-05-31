package DBIx::Class::Serialize::Storable;
use strict;
use warnings;

use Storable();
use DBIx::Class::Carp;
use namespace::clean;

carp 'The Serialize::Storable component is now *DEPRECATED*. It has not '
    .'been providing any useful functionality for quite a while, and in fact '
    .'destroys prefetched results in its current implementation. Do not use!';


sub STORABLE_freeze {
    my ($self, $cloning) = @_;
    my $to_serialize = { %$self };

    # Dynamic values, easy to recalculate
    delete $to_serialize->{$_} for qw/related_resultsets _inflated_column/;

    return (Storable::nfreeze($to_serialize));
}

sub STORABLE_thaw {
    my ($self, $cloning, $serialized) = @_;

    %$self = %{ Storable::thaw($serialized) };
}

1;

__END__

=head1 NAME

    DBIx::Class::Serialize::Storable - hooks for Storable nfreeze/thaw

=head1 DEPRECATION NOTE

This component is now B<DEPRECATED>. It has not been providing any useful
functionality for quite a while, and in fact destroys prefetched results
in its current implementation. Do not use!

=head1 SYNOPSIS

    # in a table class definition
    __PACKAGE__->load_components(qw/Serialize::Storable/);

    # meanwhile, in a nearby piece of code
    my $cd = $schema->resultset('CD')->find(12);
    # if the cache uses Storable, this will work automatically
    $cache->set($cd->ID, $cd);

=head1 DESCRIPTION

This component adds hooks for Storable so that result objects can be
serialized. It assumes that your result object class (C<result_class>) is
the same as your table class, which is the normal situation.

=head1 HOOKS

The following hooks are defined for L<Storable> - see the
documentation for L<Storable/Hooks> for detailed information on these
hooks.

=head2 STORABLE_freeze

The serializing hook, called on the object during serialization. It
can be inherited, or defined in the class itself, like any other
method.

=head2 STORABLE_thaw

The deserializing hook called on the object during deserialization.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
