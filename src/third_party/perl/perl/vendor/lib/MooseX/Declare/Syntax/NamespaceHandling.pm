package MooseX::Declare::Syntax::NamespaceHandling;
BEGIN {
  $MooseX::Declare::Syntax::NamespaceHandling::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::NamespaceHandling::VERSION = '0.35';
}
# ABSTRACT: Handle namespaced blocks

use Moose::Role;
use Moose::Util qw( does_role );
use MooseX::Declare::Util qw( outer_stack_peek );
use Carp;

use aliased 'MooseX::Declare::Context::Namespaced';
use aliased 'MooseX::Declare::Context::WithOptions';
use aliased 'MooseX::Declare::Context::Parameterized';
use aliased 'MooseX::Declare::StackItem';

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::KeywordHandling
    MooseX::Declare::Syntax::InnerSyntaxHandling
);


requires qw(
    handle_missing_block
);


sub add_namespace_customizations { }
sub add_optional_customizations  { }
sub handle_post_parsing          { }
sub make_anon_metaclass          { }

around context_traits => sub { super, WithOptions, Namespaced };

sub parse_specification {
    my ($self, $ctx) = @_;

    $self->parse_namespace_specification($ctx);
    $self->parse_option_specification($ctx);

    return;
}

sub parse_namespace_specification {
    my ($self, $ctx) = @_;
    return scalar $ctx->strip_namespace;
}

sub parse_option_specification {
    my ($self, $ctx) = @_;
    return scalar $ctx->strip_options;
}

sub generate_inline_stack {
    my ($self, $ctx) = @_;

    return join ', ',
        map { $_->serialize }
            @{ $ctx->stack },
            $self->generate_current_stack_item($ctx);
}

sub generate_current_stack_item {
    my ($self, $ctx) = @_;

    return StackItem->new(
        identifier       => $self->identifier,
        is_dirty         => $ctx->options->{is}{dirty},
        is_parameterized => does_role($ctx, Parameterized) && $ctx->has_parameter_signature,
        handler          => ref($self),
        namespace        => $ctx->namespace,
    );
}


sub parse {
    my ($self, $ctx) = @_;

    # keyword comes first
    $ctx->skip_declarator;

    # read the name and unwrap the options
    $self->parse_specification($ctx);

    my $name = $ctx->namespace;

    my ($package, $anon);

    # we have a name in the declaration, which will be used as package name
    if (defined $name) {
        $package = $name;

        # there is an outer namespace stack item, meaning we namespace below
        # it, if the name starts with ::
        if (my $outer = outer_stack_peek $ctx->caller_file) {
            $package = $outer . $package
                if $name =~ /^::/;
        }
    }

    # no name, no options, no block. Probably { class => 'foo' }
    elsif (not(keys %{ $ctx->options }) and $ctx->peek_next_char ne '{') {
        return;
    }

    # we have options and/or a block, but not name
    else {
        $anon = $self->make_anon_metaclass
            or croak sprintf 'Unable to create an anonymized %s namespace', $self->identifier;
        $package = $anon->name;
    }

    # namespace and mx:d initialisations
    $ctx->add_preamble_code_parts(
        "package ${package}",
        sprintf(
            "use %s %s => '%s', file => __FILE__, stack => [ %s ]",
            $ctx->provided_by,
            outer_package => $package,
            $self->generate_inline_stack($ctx),
        ),
    );

    # allow consumer to provide specialisations
    $self->add_namespace_customizations($ctx, $package);

    # make options a separate step
    $self->add_optional_customizations($ctx, $package);

    # finish off preamble with a namespace cleanup
    $ctx->add_preamble_code_parts(
        $ctx->options->{is}->{dirty}
            ? 'use namespace::clean -except => [qw( meta )]'
            : 'use namespace::autoclean'
    );

    # clean up our stack afterwards, if there was a name
    $ctx->add_cleanup_code_parts(
        ['BEGIN',
            'MooseX::Declare::Util::outer_stack_pop __FILE__',
        ],
    );

    # actual code injection
    $ctx->inject_code_parts(
        missing_block_handler => sub { $self->handle_missing_block(@_) },
    );

    # a last chance to change things
    $self->handle_post_parsing($ctx, $package, defined($name) ? $name : $anon);
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::NamespaceHandling - Handle namespaced blocks

=head1 DESCRIPTION

Allows the implementation of namespaced blocks like the
L<role|MooseX::Declare::Syntax::Keyword::Role> and
L<class|MooseX::Declare::Syntax::Keyword::Class> keyword handlers.

Namespaces are automatically nested. Meaning that, for example, a C<class Bar>
declaration inside another C<class Foo> block gives the inner one actually the
name C<Foo::Bar>.

=head1 METHODS

=head2 parse

  Any Object->parse (Object $context)

This is the main handling routine for namespaces. It will remove the namespace
name and its options. If the handler was invoked without a name, options or
a following block, it is assumed that this is an instance of an autoquoted
bareword like C<class => "Foo">.

The return value of the C<parse> method is also the value that is returned
to the user of the keyword.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::KeywordHandling>

=item *

L<MooseX::Declare::Syntax::InnerSyntaxHandling>

=back

=head1 REQUIRED METHODS

=head2 handle_missing_block

  Object->handle_missing_block (Object $context, Str $body, %args)

This must be implemented to decide what to do in case the statement is
terminated rather than followed by a block. It will receive the context
object, the produced code that needs to be injected, and all the arguments
that were passed to the call to L<MooseX::Declare::Context/inject_code_parts>.

The return value will be ignored.

=head1 EXTENDABLE STUB METHODS

=head2 add_namespace_customizations

=head2 add_optional_customizations

  Object->add_namespace_customizations (Object $context, Str $package, HashRef $options)
  Object->add_optional_customizations  (Object $context, Str $package, HashRef $options)

These will be called (in this order) by the L</parse> method. They allow specific hooks
to attach before/after/around the customizations for the namespace and the provided
options that are not attached to the namespace directly.

While this distinction might seem superficial, we advise library developers faciliating
this role to follow the precendent. This ensures that when another component needs to
tie between the namspace and any additional customizations everythign will run in the
correct order. An example of this separation would be

  class Foo is mutable ...

being an option of the namespace generation, while

  class Foo with Bar ...

is an additional optional customization.

=head2 handle_post_parsing

  Object->handle_post_parsing (Object $context, Str $package, Str | Object $name)

Allows for additional modifications to te namespace after everything else has been
done. It will receive the context, the fully qualified package name, and either a
string with the name that was specified (might not be fully qualified, since
namespaces can be nested) or the anonymous metaclass instance if no name was
specified.

The return value of this method will be the value returned to the user of the
keyword. If you always return the C<$package> argument like this:

  sub handle_post_parsing {
      my ($self, $context, $package, $name) = @_;
      return $package;
  }

and set this up in a C<foo> keyword handler, you can use it like this:

  foo Cthulhu {

      my $fhtagn = foo Fhtagn { }
      my $anon   = foo { };

      say $fhtagn;  # Cthulhu::Fhtagn
      say $anon;    # some autogenerated package name
  }

=head2 make_anon_metaclass

  Class::MOP::Class Object->make_anon_metaclass ()

This method should be overridden if you want to provide anonymous namespaces.

It does not receive any arguments for customization of the metaclass, because
the configuration and customization will be done by L<MooseX::Declare> in the
package of the generated class in the same way as in those that have specified
names. This way ensures that anonymous and named namespaces are always handled
equally.

If you do not extend this method (it will return nothing by default), an error
will be thrown when a user attempts to declare an anonymous namespace.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<MooseX::Declare::Syntax::MooseSetup>

=back

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Ash Berlin <ash@cpan.org>

=item *

Chas. J. Owens IV <chas.owens@gmail.com>

=item *

Chris Prather <chris@prather.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Devin Austin <dhoss@cpan.org>

=item *

Hans Dieter Pearcey <hdp@cpan.org>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Matt Kraai <kraai@ftbfs.org>

=item *

Michele Beltrame <arthas@cpan.org>

=item *

Nelo Onyiah <nelo.onyiah@gmail.com>

=item *

nperez <nperez@cpan.org>

=item *

Piers Cawley <pdcawley@bofh.org.uk>

=item *

Rafael Kitover <rkitover@io.com>

=item *

Robert 'phaylon' Sedlacek <rs@474.at>

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

