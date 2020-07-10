package MooseX::Role::WithOverloading::Meta::Role::Application;
# ABSTRACT: (DEPRECATED) Role application role for Roles which support overloading

our $VERSION = '0.17';

use Moose::Role 1.15;
use overload ();
use namespace::autoclean;

requires 'apply_methods';

#pod =method overload_ops
#pod
#pod Returns an arrayref of the names of overloaded operations
#pod
#pod =cut

has overload_ops => (
    is      => 'ro',
    isa     => 'ArrayRef[Str]',
    builder => '_build_overload_ops',
);

sub _build_overload_ops {
    return [map { split /\s+/ } values %overload::ops];
}

#pod =method apply_methods ($role, $other)
#pod
#pod Wrapped with an after modifier which calls the C<< ->apply_overloading >>
#pod method.
#pod
#pod =cut

after apply_methods => sub {
    my ($self, $role, $other) = @_;
    $self->apply_overloading($role, $other);
};

#pod =method apply_overloading ($role, $other)
#pod
#pod Does the heavy lifting of applying overload operations to
#pod a class or role which the role is applied to.
#pod
#pod =cut

sub apply_overloading {
    my ($self, $role, $other) = @_;
    return unless overload::Overloaded($role->name);

    # &(( indicates that overloading is turned on with Perl 5.18+. &() does
    # this in earlier perls. $() stores the fallback value if one was set.
    for my $sym (qw{ &(( &() $() }) {
        # Simply checking ->has_package_symbol doesn't work. With 5.18+, a
        # package may have &() and $() symbols but they may be undef.
        my $ref = $role->get_package_symbol($sym);
        $other->add_package_symbol($sym => $ref)
            if defined $ref;
    }

    # register with magic by touching (changes to SVf_AMAGIC removed %OVERLOAD in 5.17.0)
    $other->get_or_add_package_symbol('%OVERLOAD')->{dummy}++ if $^V < 5.017000;

    for my $op (@{ $self->overload_ops }) {
        my $code_sym = '&(' . $op;

        next if overload::Method($other->name, $op);
        next unless $role->has_package_symbol($code_sym);

        my $meth = $role->get_package_symbol($code_sym);
        next unless $meth;

        # when using "use overload $op => sub { };" this is the actual method
        # to be called on overloading. otherwise it's \&overload::nil. see
        # below.
        $other->add_package_symbol($code_sym => $meth);

        # when using "use overload $op => 'method_name';" overload::nil is
        # installed into the code slot of the glob and the actual method called
        # is determined by the scalar slot of the same glob.
        if ($meth == \&overload::nil || $meth == \&overload::_nil) {
            my $scalar_sym = qq{\$($op};
            $other->add_package_symbol(
                $scalar_sym => ${ $role->get_package_symbol($scalar_sym) },
            );
        }
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Role::WithOverloading::Meta::Role::Application - (DEPRECATED) Role application role for Roles which support overloading

=head1 VERSION

version 0.17

=head1 METHODS

=head2 overload_ops

Returns an arrayref of the names of overloaded operations

=head2 apply_methods ($role, $other)

Wrapped with an after modifier which calls the C<< ->apply_overloading >>
method.

=head2 apply_overloading ($role, $other)

Does the heavy lifting of applying overload operations to
a class or role which the role is applied to.

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=MooseX-Role-WithOverloading>
(or L<bug-MooseX-Role-WithOverloading@rt.cpan.org|mailto:bug-MooseX-Role-WithOverloading@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
irc://irc.perl.org/#moose.

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=back

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2009 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
