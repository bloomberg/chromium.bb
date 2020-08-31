use strict;
use warnings;

package MooseX::Method::Signatures; # git description: v0.48-15-gd03dfc1
# ABSTRACT: (DEPRECATED) Method declarations with type constraints and no source filter
# KEYWORDS: moose extension method declaration signature prototype syntax sugar deprecated

our $VERSION = '0.49';

use Moose 0.89;
use Devel::Declare 0.005011 ();
use B::Hooks::EndOfScope 0.10;
use Moose::Meta::Class;
use MooseX::LazyRequire 0.06;
use MooseX::Types::Moose 0.19 qw/Str Bool CodeRef/;
use Text::Balanced qw/extract_quotelike/;
use MooseX::Method::Signatures::Meta::Method;
use MooseX::Method::Signatures::Types qw/PrototypeInjections/;
use Sub::Name;
use Moose::Util 'find_meta';
use Module::Runtime 'use_module';
use Carp;

use aliased 'Devel::Declare::Context::Simple', 'ContextSimple';

use namespace::autoclean;

has package => (
    is            => 'ro',
    isa           => Str,
    lazy_required => 1,
);

has context => (
    is      => 'ro',
    isa     => ContextSimple,
    lazy    => 1,
    builder => '_build_context',
);

has initialized_context => (
    is      => 'ro',
    isa     => Bool,
    default => 0,
);

has custom_method_application => (
    is        => 'ro',
    isa       => CodeRef,
    predicate => 'has_custom_method_application',
);

has prototype_injections => (
    is        => 'ro',
    isa       => PrototypeInjections,
    predicate => 'has_prototype_injections',
);

sub _build_context {
    my ($self) = @_;
    return ContextSimple->new(into => $self->package);
}

sub import {
    my ($class, %args) = @_;
    my $caller = caller();
    $class->setup_for($caller, \%args);
}

sub setup_for {
    my ($class, $pkg, $args) = @_;

    # process arguments to import
    while (my ($declarator, $injections) = each %{ $args }) {
        my $obj = $class->new(
            package              => $pkg,
            prototype_injections => {
                declarator => $declarator,
                injections => $injections,
            },
        );

        Devel::Declare->setup_for($pkg, {
            $declarator => { const => sub { $obj->parser(@_) } },
        });

        {
            no strict 'refs';
            *{ "${pkg}::$declarator" } = sub {};
        }
    }

    my $self = $class->new(package => $pkg);

    Devel::Declare->setup_for($pkg, {
        method => { const => sub { $self->parser(@_) } },
    });

    {
        no strict 'refs';
        *{ "${pkg}::method" } = sub {};
    }

    return;
}

sub strip_name {
    my ($self) = @_;
    my $ctx = $self->context;
    my $ret = $ctx->strip_name;
    return $ret if defined $ret;

    my $line = $ctx->get_linestr;
    my $offset = $ctx->offset;
    local $@;
    my $copy = substr($line, $offset);
    my ($str) = extract_quotelike($copy);
    return unless defined $str;

    return if ($@ && $@ =~ /^No quotelike operator found/);
    die $@ if $@;

    substr($line, $offset, length $str) = '';
    $ctx->set_linestr($line);

    return \$str;
}

sub strip_traits {
    my ($self) = @_;

    my $ctx = $self->context;
    my $linestr = $ctx->get_linestr;

    unless (substr($linestr, $ctx->offset, 2) eq 'is' ||
            substr($linestr, $ctx->offset, 4) eq 'does') {
        # No 'is' means no traits
        return;
    }

    my @traits;

    while (1) {
        if (substr($linestr, $ctx->offset, 2) eq 'is') {
            # Eat the 'is' so we can call strip_names_and_args
            substr($linestr, $ctx->offset, 2) = '';
        } elsif (substr($linestr, $ctx->offset, 4) eq 'does') {
            # Eat the 'does' so we can call strip_names_and_args
            substr($linestr, $ctx->offset, 4) = '';
        } else {
            last;
        }

        $ctx->set_linestr($linestr);
        push @traits, @{ $ctx->strip_names_and_args };
        # Get the current linestr so that the loop can look for more 'is'
        $ctx->skipspace;
        $linestr = $ctx->get_linestr;
    }

    confess "expected traits after 'is' or 'does', found nothing"
        unless scalar(@traits);

    # Let's check to make sure these traits aren't aliased locally
    for my $t (@traits) {
        next if $t->[0] =~ /::/;
        my $class = $ctx->get_curstash_name;
        my $meta = find_meta($class) || Moose::Meta::Class->initialize($class);
        my $func = $meta->get_package_symbol('&' . $t->[0]);
        next unless $func;

        my $proto = prototype $func;
        next if !defined $proto || length $proto;

        $t->[0] = $func->();
    }

    return \@traits;
}

sub strip_return_type_constraint {
    my ($self) = @_;
    my $ctx = $self->context;
    my $returns = $ctx->strip_name;
    return unless defined $returns;
    confess "expected 'returns', found '${returns}'"
        unless $returns eq 'returns';
    return $ctx->strip_proto;
}

sub parser {
    my $self = shift;
    my $err;

    # Keep any previous compile errors from getting stepped on. But report
    # errors from inside MXMS nicely.
    {
        local $@;
        eval { $self->_parser(@_) };
        $err = $@;
    }

    die $err if $err;
}

my $anon_counter = 1;
sub _parser {
    my $self = shift;
    my $ctx = $self->context;
    $ctx->init(@_) unless $self->initialized_context;

    $ctx->skip_declarator;
    my $name   = $self->strip_name;
    my $proto  = $ctx->strip_proto;
    my $attrs  = $ctx->strip_attrs || '';
    my $traits = $self->strip_traits;
    my $ret_tc = $self->strip_return_type_constraint;

    my $compile_stash = $ctx->get_curstash_name;

    my %args = (
      # This might get reset later, but its where we search for exported
      # symbols at compile time
      package_name => $compile_stash,
    );
    $args{ signature        } = qq{($proto)} if defined $proto;
    $args{ traits           } = $traits      if $traits;
    $args{ return_signature } = $ret_tc      if defined $ret_tc;

    # Class::MOP::Method requires a name
    $args{ name             } = $name || '__ANON__'.($anon_counter++).'__';

    if ($self->has_prototype_injections) {
        confess('Configured declarator does not match context declarator')
            if $ctx->declarator ne $self->prototype_injections->{declarator};
        $args{prototype_injections} = $self->prototype_injections->{injections};
    }

    my $meth_class = 'MooseX::Method::Signatures::Meta::Method';
    if ($args{traits}) {
        my @traits = ();
        foreach my $t (@{$args{traits}}) {
            use_module($t->[0]);
            if ($t->[1]) {
                %args = (%args, eval $t->[1]);
            };
            push @traits, $t->[0];
        }
        my $meta = Moose::Meta::Class->create_anon_class(
            superclasses => [ $meth_class  ],
            roles        => [ @traits ],
            cache        => 1,
        );
        $meth_class = $meta->name;
        delete $args{traits};
    }

    my $proto_method = $meth_class->wrap(sub { }, %args);

    my $after_block = ')';

    if ($traits) {
        if (my @trait_args = grep { defined } map { $_->[1] } @{ $traits }) {
            $after_block = q{, } . join(q{,} => @trait_args) . $after_block;
        }
    }

    if (defined $name) {
        my $name_arg = q{, } . (ref $name ? ${$name} : qq{q[${name}]});
        $after_block = $name_arg . $after_block . q{;};
    }

    my $inject = $proto_method->injectable_code;
    $inject = $self->scope_injector_call($after_block) . $inject;

    $ctx->inject_if_block($inject, "(sub ${attrs} ");

    my $create_meta_method = sub {
        my ($code, $pkg, $meth_name, @args) = @_;
        subname $pkg . "::" .$meth_name, $code;

        # we want to reinitialize with all the args,
        # so we give the opportunity for traits to wrap the correct
        # closure.
        my %other_args = %{$proto_method};
        delete $other_args{body};
        delete $other_args{actual_body};

        my $ret = $meth_class->wrap(
            $code,
            %other_args, @args
        );
    };

    if (defined $name) {
        my $apply = $self->has_custom_method_application
            ? $self->custom_method_application
            : sub {
                my ($meta, $name, $method) = @_;

                if (warnings::enabled("redefine") && (my $meta_meth = $meta->get_method($name))) {
                    warnings::warn("redefine", "Method $name redefined on package ${ \$meta->name }")
                        if $meta_meth->isa('MooseX::Method::Signatures::Meta::Method');
                }

                $meta->add_method($name => $method);
            };

        $ctx->shadow(sub {
            my ($code, $name, @args) = @_;

            my $pkg = $compile_stash;
            ($pkg, $name) = $name =~ /^(.*)::([^:]+)$/
                if $name =~ /::/;

            my $meth = $create_meta_method->($code, $pkg, $name, @args);
            my $meta = Moose::Meta::Class->initialize($pkg);

            $meta->$apply($name, $meth);
            return;
        });
    }
    else {
        $ctx->shadow(sub {
            return $create_meta_method->(shift, $compile_stash, '__ANON__', @_);
        });
    }
}

sub scope_injector_call {
    my ($self, $code) = @_;
    $code =~ s/'/\\'/g; # we're generating code that's quoted with single quotes
    return qq[BEGIN { ${\ref $self}->inject_scope('${code}') }];
}

sub inject_scope {
    my ($class, $inject) = @_;
    on_scope_end {
        my $line = Devel::Declare::get_linestr();
        return unless defined $line;
        my $offset = Devel::Declare::get_linestr_offset();
        substr($line, $offset, 0) = $inject;
        Devel::Declare::set_linestr($line);
    };
}

__PACKAGE__->meta->make_immutable;

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Method::Signatures - (DEPRECATED) Method declarations with type constraints and no source filter

=head1 VERSION

version 0.49

=head1 SYNOPSIS

    package Foo;

    use Moose;
    use MooseX::Method::Signatures;

    method morning (Str $name) {
        $self->say("Good morning ${name}!");
    }

    method hello (Str :$who, Int :$age where { $_ > 0 }) {
        $self->say("Hello ${who}, I am ${age} years old!");
    }

    method greet (Str $name, Bool :$excited = 0) {
        if ($excited) {
            $self->say("GREETINGS ${name}!");
        }
        else {
            $self->say("Hi ${name}!");
        }
    }

    $foo->morning('Resi');                          # This works.

    $foo->hello(who => 'world', age => 42);         # This too.

    $foo->greet('Resi', excited => 1);              # And this as well.

    $foo->hello(who => 'world', age => 'fortytwo'); # This doesn't.

    $foo->hello(who => 'world', age => -23);        # This neither.

    $foo->morning;                                  # Won't work.

    $foo->greet;                                    # Will fail.

=head1 DESCRIPTION

Provides a proper method keyword, like "sub" but specifically for making methods
and validating their arguments against Moose type constraints.

=head1 DEPRECATION NOTICE

=for stopwords mst

=for comment rafl agreed we should have a warning, and mst wrote this for MooseX::Declare, but it applies equally well here:

B<Warning:> MooseX::Method::Signatures and L<MooseX::Declare> are based on
L<Devel::Declare>, a giant bag of crack originally implemented by mst with the
goal of upsetting the perl core developers so much by its very existence that
they implemented proper keyword handling in the core.

As of perl5 version 14, this goal has been achieved, and modules such as
L<Devel::CallParser>, L<Function::Parameters>, and L<Keyword::Simple> provide
mechanisms to mangle perl syntax that don't require hallucinogenic drugs to
interpret the error messages they produce.

If you want to use declarative syntax in new code, please for the love
of kittens get yourself a recent perl and look at L<Moops> and
L<core signatures|perlsub/Signatures> instead.

=head1 SIGNATURE SYNTAX

The signature syntax is heavily based on Perl 6. However not the full Perl 6
signature syntax is supported yet and some of it never will be.

=head2 Type Constraints

    method foo (             $affe) # no type checking
    method bar (Animal       $affe) # $affe->isa('Animal')
    method baz (Animal|Human $affe) # $affe->isa('Animal') || $affe->isa('Human')

=head2 Positional vs. Named

    method foo ( $a,  $b,  $c) # positional
    method bar (:$a, :$b, :$c) # named
    method baz ( $a,  $b, :$c) # combined

=head2 Required vs. Optional

    method foo ($a , $b!, :$c!, :$d!) # required
    method bar ($a?, $b?, :$c , :$d?) # optional

=head2 Defaults

    method foo ($a = 42) # defaults to 42

=head2 Constraints

    method foo ($foo where { $_ % 2 == 0 }) # only even

=for stopwords Invocant

=head2 Invocant

    method foo (        $moo) # invocant is called $self and is required
    method bar ($self:  $moo) # same, but explicit
    method baz ($class: $moo) # invocant is called $class

=head2 Labels

    method foo (:     $affe ) # called as $obj->foo(affe => $value)
    method bar (:apan($affe)) # called as $obj->foo(apan => $value)

=head2 Traits

    method foo (Affe $bar does trait)
    method foo (Affe $bar is trait)

The only currently supported trait is C<coerce>, which will attempt to coerce
the value provided if it doesn't satisfy the requirements of the type
constraint.

=head2 Placeholders

    method foo ($bar, $, $baz)

=for stopwords sigil

Sometimes you don't care about some parameters you're being called with. Just put
the bare sigil instead of a full variable name into the signature to avoid an
extra lexical variable to be created.

=head2 Complex Example

    method foo ( SomeClass $thing where { $_->can('stuff') }:
                 Str  $bar  = "apan",
                 Int :$baz! = 42 where { $_ % 2 == 0 } where { $_ > 10 } )

    # the invocant is called $thing, must be an instance of SomeClass and
           has to implement a 'stuff' method
    # $bar is positional, required, must be a string and defaults to "apan"
    # $baz is named, required, must be an integer, defaults to 42 and needs
    #      to be even and greater than 10

=head1 CAVEATS AND NOTES

This module is as stable now, but this is not to say that it is entirely bug
free. If you notice any odd behaviour (messages not being as good as they could
for example) then please raise a bug.

=head2 Fancy signatures

L<Parse::Method::Signatures> is used to parse the signatures. However, some
signatures that can be parsed by it aren't supported by this module (yet).

=head2 No source filter

While this module does rely on the hairy black magic of L<Devel::Declare> it
does not depend on a source filter. As such, it doesn't try to parse and
rewrite your source code and there should be no weird side effects.

Devel::Declare only effects compilation. After that, it's a normal subroutine.
As such, for all that hairy magic, this module is surprisingly stable.

=head2 What about regular subroutines?

L<Devel::Declare> cannot yet change the way C<sub> behaves. However, the
L<signatures|signatures> module can. Right now it only provides very basic
signatures, but it's extendable enough that plugging MooseX::Method::Signatures
signatures into that should be quite possible.

=head2 What about the return value?

Type constraints for return values can be declared using

  method foo (Int $x, Str $y) returns (Bool) { ... }

however, this feature only works with scalar return values and is still
considered to be experimental.

=head2 Interaction with L<Moose::Role>

=head3 Methods not seen by a role's C<requires>

Because the processing of the L<MooseX::Method::Signatures>
C<method> and the L<Moose> C<with> keywords are both
done at runtime, it can happen that a role will require
a method before it is declared (which will cause
Moose to complain very loudly and abort the program).

For example, the following will not work:

    # in file Canine.pm

    package Canine;

    use Moose;
    use MooseX::Method::Signatures;

    with 'Watchdog';

    method bark { print "Woof!\n"; }

    1;


    # in file Watchdog.pm

    package Watchdog;

    use Moose::Role;

    requires 'bark';  # will assert! evaluated before 'method' is processed

    sub warn_intruder {
        my $self = shift;
        my $intruder = shift;

        $self->bark until $intruder->gone;
    }

    1;

A workaround for this problem is to use C<with> only
after the methods have been defined.  To take our previous
example, B<Canine> could be reworked thus:

    package Canine;

    use Moose;
    use MooseX::Method::Signatures;

    method bark { print "Woof!\n"; }

    with 'Watchdog';

    1;

A better solution is to use L<MooseX::Declare> instead of plain
L<MooseX::Method::Signatures>. It defers application of roles until the end
of the class definition. With it, our example would becomes:

    # in file Canine.pm

    use MooseX::Declare;

    class Canine with Watchdog {
        method bark { print "Woof!\n"; }
    }

    1;

    # in file Watchdog.pm

    use MooseX::Declare;

    role Watchdog {
        requires 'bark';

        method warn_intruder ( $intruder ) {
            $self->bark until $intruder->gone;
        }
    }

    1;

=head3 I<Subroutine redefined> warnings

When composing a L<Moose::Role> into a class that uses
L<MooseX::Method::Signatures>, you may get a "Subroutine redefined"
warning. This happens when both the role and the class define a
method/subroutine of the same name. (The way roles work, the one
defined in the class takes precedence.) To eliminate this warning,
make sure that your C<with> declaration happens after any
method/subroutine declarations that may have the same name as a
method/subroutine within a role.

=head1 SEE ALSO

=over 4

=item *

L<MooseX::Declare>

=item *

L<Method::Signatures::Simple>

=item *

L<Method::Signatures>

=item *

L<Devel::Declare>

=item *

L<Parse::Method::Signatures>

=item *

L<Moose>

=item *

L<signatures>

=back

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=MooseX-Method-Signatures>
(or L<bug-MooseX-Method-Signatures@rt.cpan.org|mailto:bug-MooseX-Method-Signatures@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
irc://irc.perl.org/#moose.

I am also usually active on irc, as 'ether' at C<irc.perl.org>.

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Ash Berlin Daniel Ruoso Justin Hunter Nicholas Perez Dagfinn Ilmari Mannsåker Rhesa Rozendaal Yanick Champoux Cory Watson Kent Fredric Lukas Mai Matt Kraai Jonathan Scott Duff Jesse Luehrs Hakim Cassimally Dave Rolsky Ricardo SIGNES Sebastian Willert Steffen Schwigon

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Ash Berlin <ash@cpan.org>

=item *

Daniel Ruoso <daniel@ruoso.com>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Nicholas Perez <nperez@cpan.org>

=item *

Dagfinn Ilmari Mannsåker <ilmari@ilmari.org>

=item *

Rhesa Rozendaal <rhesa@cpan.org>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=item *

Cory Watson <gphat@cpan.org>

=item *

Kent Fredric <kentfredric@gmail.com>

=item *

Lukas Mai <l.mai@web.de>

=item *

Matt Kraai <kraai@ftbfs.org>

=item *

Jonathan Scott Duff <duff@pobox.com>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

Hakim Cassimally <osfameron@cpan.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Sebastian Willert <willert@cpan.org>

=item *

Steffen Schwigon <ss5@renormalist.net>

=back

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
