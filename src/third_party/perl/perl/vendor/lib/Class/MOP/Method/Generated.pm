package Class::MOP::Method::Generated;
our $VERSION = '2.2011';

use strict;
use warnings;

use Eval::Closure;

use parent 'Class::MOP::Method';

## accessors

sub new {
    $_[0]->_throw_exception( CannotCallAnAbstractBaseMethod => package_name => __PACKAGE__ );
}

sub _initialize_body {
    $_[0]->_throw_exception( NoBodyToInitializeInAnAbstractBaseClass => package_name => __PACKAGE__ );
}

sub _generate_description {
    my ( $self, $context ) = @_;
    $context ||= $self->definition_context;

    my $desc = "generated method";
    my $origin = "unknown origin";

    if (defined $context) {
        if (defined $context->{description}) {
            $desc = $context->{description};
        }

        if (defined $context->{file} || defined $context->{line}) {
            $origin = "defined at "
                    . (defined $context->{file}
                        ? $context->{file} : "<unknown file>")
                    . " line "
                    . (defined $context->{line}
                        ? $context->{line} : "<unknown line>");
        }
    }

    return "$desc ($origin)";
}

sub _compile_code {
    my ( $self, @args ) = @_;
    unshift @args, 'source' if @args % 2;
    my %args = @args;

    my $context = delete $args{context};
    my $environment = $self->can('_eval_environment')
        ? $self->_eval_environment
        : {};

    return eval_closure(
        environment => $environment,
        description => $self->_generate_description($context),
        %args,
    );
}

1;

# ABSTRACT: Abstract base class for generated methods

__END__

=pod

=encoding UTF-8

=head1 NAME

Class::MOP::Method::Generated - Abstract base class for generated methods

=head1 VERSION

version 2.2011

=head1 DESCRIPTION

This is a C<Class::MOP::Method> subclass which is subclassed by
C<Class::MOP::Method::Accessor> and
C<Class::MOP::Method::Constructor>.

It is not intended to be used directly.

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
