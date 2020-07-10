package Moose::Meta::Method::Constructor;
our $VERSION = '2.2011';

use strict;
use warnings;

use Scalar::Util 'weaken';

use parent 'Moose::Meta::Method',
         'Class::MOP::Method::Constructor';

use Moose::Util 'throw_exception';

sub new {
    my $class   = shift;
    my %options = @_;

    my $meta = $options{metaclass};

    (ref $options{options} eq 'HASH')
        || throw_exception( MustPassAHashOfOptions => params => \%options,
                                                      class  => $class
                          );

    ($options{package_name} && $options{name})
        || throw_exception( MustSupplyPackageNameAndName => params => \%options,
                                                            class  => $class
                          );

    my $self = bless {
        'body'          => undef,
        'package_name'  => $options{package_name},
        'name'          => $options{name},
        'options'       => $options{options},
        'associated_metaclass' => $meta,
        'definition_context' => $options{definition_context},
        '_expected_method_class' => $options{_expected_method_class} || 'Moose::Object',
    } => $class;

    # we don't want this creating
    # a cycle in the code, if not
    # needed
    weaken($self->{'associated_metaclass'});

    $self->_initialize_body;

    return $self;
}

## method

sub _initialize_body {
    my $self = shift;
    $self->{'body'} = $self->_generate_constructor_method_inline;
}

1;

# ABSTRACT: Method Meta Object for constructors

__END__

=pod

=encoding UTF-8

=head1 NAME

Moose::Meta::Method::Constructor - Method Meta Object for constructors

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This class is a subclass of L<Class::MOP::Method::Constructor> that
provides additional Moose-specific functionality

To understand this class, you should read the
L<Class::MOP::Method::Constructor> documentation as well.

=head1 INHERITANCE

C<Moose::Meta::Method::Constructor> is a subclass of
L<Moose::Meta::Method> I<and> L<Class::MOP::Method::Constructor>.

=head1 BUGS

See L<Moose/BUGS> for details on reporting bugs.

=head1 AUTHORS

=over 4

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Shawn M Moore <code@sartak.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Hans Dieter Pearcey <hdp@weftsoar.net>

=item *

Chris Prather <chris@prather.org>

=item *

Matt S Trout <mst@shadowcat.co.uk>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2006 by Infinity Interactive, Inc.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
