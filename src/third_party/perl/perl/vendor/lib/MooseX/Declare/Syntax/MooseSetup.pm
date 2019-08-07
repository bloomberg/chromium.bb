package MooseX::Declare::Syntax::MooseSetup;
BEGIN {
  $MooseX::Declare::Syntax::MooseSetup::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Syntax::MooseSetup::VERSION = '0.35';
}
# ABSTRACT: Common Moose namespaces declarations

use Moose::Role;

use Moose::Util  qw( find_meta );
use Sub::Install qw( install_sub );

use aliased 'MooseX::Declare::Syntax::Keyword::MethodModifier';
use aliased 'MooseX::Declare::Syntax::Keyword::Method';
use aliased 'MooseX::Declare::Syntax::Keyword::With', 'WithKeyword';
use aliased 'MooseX::Declare::Syntax::Keyword::Clean', 'CleanKeyword';

use namespace::clean -except => 'meta';


with qw(
    MooseX::Declare::Syntax::NamespaceHandling
    MooseX::Declare::Syntax::EmptyBlockIfMissing
);


sub auto_make_immutable { 0 }


sub imported_moose_symbols { qw( confess blessed ) }


sub import_symbols_from { 'Moose' }


around default_inner => sub {
    return [
        WithKeyword->new(identifier => 'with'),
        Method->new(identifier => 'method'),
        MethodModifier->new(
            identifier           => 'around',
            modifier_type        => 'around',
            prototype_injections => {
                declarator => 'around',
                injections => [ 'CodeRef $orig' ],
            },
        ),
        map { MethodModifier->new(identifier => $_, modifier_type => $_) }
            qw( after before override augment ),
    ];
};


after setup_inner_for => sub {
    my ($self, $setup_class, %args) = @_;
    my $keyword = CleanKeyword->new(identifier => 'clean');
    $keyword->setup_for($setup_class, %args);
};


after add_namespace_customizations => sub {
    my ($self, $ctx, $package) = @_;

    # add Moose initializations to preamble
    $ctx->add_preamble_code_parts(
        sprintf 'use %s qw( %s )', $self->import_symbols_from($ctx), join ' ', $self->imported_moose_symbols($ctx),
    );

    # make class immutable unless specified otherwise
    $ctx->add_cleanup_code_parts(
        "${package}->meta->make_immutable",
    ) if $self->auto_make_immutable
         and not exists $ctx->options->{is}{mutable};
};


after handle_post_parsing => sub {
    my ($self, $ctx, $package, $class) = @_;
    $ctx->shadow(sub (&) { shift->(); return $class; });
};



1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Syntax::MooseSetup - Common Moose namespaces declarations

=head1 DESCRIPTION

This role is basically an extension to
L<NamespaceHandling|MooseX::Declare::Syntax::NamespaceHandling>. It adds all
the common parts for L<Moose> namespace definitions. Examples of this role
can be found in the L<class|MooseX::Declare::Syntax::Keyword::Class> and
L<role|MooseX::Declare::Syntax::Keyword::Role> keywords.

=head1 METHODS

=head2 auto_make_immutable

  Bool Object->auto_make_immutable ()

Since L<Moose::Role>s can't be made immutable (this is not a bug or a
missing feature, it would make no sense), this always returns false.

=head2 imported_moose_symbols

  List Object->imported_moose_symbols ()

This will return C<confess> and C<blessed> by default to provide as
additional imports to the namespace.

=head2 import_symbols_from

  Str Object->import_symbols_from ()

The namespace from which the additional imports will be imported. This
will return C<Moose> by default.

=head1 CONSUMES

=over 4

=item *

L<MooseX::Declare::Syntax::NamespaceHandling>

=item *

L<MooseX::Declare::Syntax::EmptyBlockIfMissing>

=back

=head1 MODIFIED METHODS

=head2 default_inner

  ArrayRef default_inner ()

This will provide the following default inner-handlers to the namspace:

=over 4

=item *

method

A simple L<Method|MooseX::Declare::Syntax::Keyword::Method> handler.

=item *

around

This is a L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
handler that will start the signature of the generated method with
C<$orig: $self> to provide the original method in C<$orig>.

=item *

after

=item *

before

=item *

override

=item *

augment

These four handlers are L<MethodModifier|MooseX::Declare::Syntax::Keyword::MethodModifier>
instances.

=item *

clean

This is an instance of the L<Clean|MooseX::Declare::Syntax::Keyword::Clean> keyword
handler.

=back

The original method will never be called and all arguments are ignored at the
moment.

=head2 setup_inner_for

  Object->setup_inner_for (ClassName $class)

This will install a C<with> function that will push its arguments onto a global
storage array holding the roles of the current namespace.

=head2 add_namespace_customizations

  Object->add_namespace_customizations (Object $context, Str $package, HashRef $options)

After all other customizations, this will first add code to import the
L</imported_moose_symbols> from the package returned in L</import_symbols_from> to
the L<preamble|MooseX::Declare::Context/preamble_code_parts>.

Then it will add a code part that will immutabilize the class to the
L<cleanup|MooseX::Declare::Context/cleanup_code_parts> code if the
L</auto_make_immutable> method returned a true value and C<$options->{is}{mutable}>
does not exist.

=head2 handle_post_parsing

  CodeRef Object->handle_post_parsing (Object $context, Str $package, Str|Object $name)

Generates a callback that sets up the roles in the global role storage for the current
namespace. The C<$name> parameter will be the specified name (in contrast to C<$package>
which will always be the fully qualified name) or the anonymous metaclass instance if
none was specified.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<Moose>

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

