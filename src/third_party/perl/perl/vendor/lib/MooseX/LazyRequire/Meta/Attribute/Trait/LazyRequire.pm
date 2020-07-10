package MooseX::LazyRequire::Meta::Attribute::Trait::LazyRequire;
# ABSTRACT: Attribute trait to make getters fail on unset attributes
$MooseX::LazyRequire::Meta::Attribute::Trait::LazyRequire::VERSION = '0.11';
use Moose::Role;
use Carp qw/cluck/;
use namespace::autoclean;

has lazy_required => (
    is       => 'ro',
    isa      => 'Bool',
    default  => 0,
);

after _process_options => sub {
    my ($class, $name, $options) = @_;

    if (exists $options->{lazy_require}) {
        cluck "deprecated option 'lazy_require' used. use 'lazy_required' instead.";
        $options->{lazy_required} = delete $options->{lazy_require};
    }

    return unless $options->{lazy_required};

    # lazy_required + default or builder doesn't make sense because if there
    # is a default/builder, the reader will always be able to return a value.
    Moose->throw_error(
        "You may not use both a builder or a default and lazy_required for one attribute ($name)",
        data => $options,
    ) if $options->{builder} or $options->{default};

    $options->{ lazy     } = 1;
    $options->{ required } = 1;
    $options->{ default  } = sub {
        confess "Attribute '$name' must be provided before calling reader"
    };
};

package # hide
    Moose::Meta::Attribute::Custom::Trait::LazyRequire;

sub register_implementation { 'MooseX::LazyRequire::Meta::Attribute::Trait::LazyRequire' }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::LazyRequire::Meta::Attribute::Trait::LazyRequire - Attribute trait to make getters fail on unset attributes

=head1 VERSION

version 0.11

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Dave Rolsky <autarch@urth.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2009 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
