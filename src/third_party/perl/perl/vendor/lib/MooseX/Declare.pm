use strict;
use warnings;

package MooseX::Declare; # git description: v0.42-6-gab03158
# ABSTRACT: (DEPRECATED) Declarative syntax for Moose
# KEYWORDS: moose extension declaration syntax sugar method class deprecated

our $VERSION = '0.43';

use aliased 'MooseX::Declare::Syntax::Keyword::Class',      'ClassKeyword';
use aliased 'MooseX::Declare::Syntax::Keyword::Role',       'RoleKeyword';
use aliased 'MooseX::Declare::Syntax::Keyword::Namespace',  'NamespaceKeyword';

use namespace::clean 0.19;

sub import {
    my ($class, %args) = @_;

    my $caller = caller();

    strict->import;
    warnings->import;

    for my $keyword ($class->keywords) {
        $keyword->setup_for($caller, %args, provided_by => $class);
    }
}

sub keywords {
    ClassKeyword->new(identifier => 'class'),
    RoleKeyword->new(identifier => 'role'),
    NamespaceKeyword->new(identifier => 'namespace'),
}

#pod =head1 SYNOPSIS
#pod
#pod     use MooseX::Declare;
#pod
#pod     class BankAccount {
#pod         has 'balance' => ( isa => 'Num', is => 'rw', default => 0 );
#pod
#pod         method deposit (Num $amount) {
#pod             $self->balance( $self->balance + $amount );
#pod         }
#pod
#pod         method withdraw (Num $amount) {
#pod             my $current_balance = $self->balance();
#pod             ( $current_balance >= $amount )
#pod                 || confess "Account overdrawn";
#pod             $self->balance( $current_balance - $amount );
#pod         }
#pod     }
#pod
#pod     class CheckingAccount extends BankAccount {
#pod         has 'overdraft_account' => ( isa => 'BankAccount', is => 'rw' );
#pod
#pod         before withdraw (Num $amount) {
#pod             my $overdraft_amount = $amount - $self->balance();
#pod             if ( $self->overdraft_account && $overdraft_amount > 0 ) {
#pod                 $self->overdraft_account->withdraw($overdraft_amount);
#pod                 $self->deposit($overdraft_amount);
#pod             }
#pod         }
#pod     }
#pod
#pod =head1 DESCRIPTION
#pod
#pod This module provides syntactic sugar for Moose, the postmodern object system
#pod for Perl 5. When used, it sets up the C<class> and C<role> keywords.
#pod
#pod B<Note:> Please see the L</WARNING> section below!
#pod
#pod =head1 KEYWORDS
#pod
#pod =head2 class
#pod
#pod     class Foo { ... }
#pod
#pod     my $anon_class = class { ... };
#pod
#pod Declares a new class. The class can be either named or anonymous, depending on
#pod whether or not a classname is given. Within the class definition Moose and
#pod L<MooseX::Method::Signatures> are set up automatically in addition to the other
#pod keywords described in this document. At the end of the definition the class
#pod will be made immutable. namespace::autoclean is injected to clean up Moose and
#pod other imports for you.
#pod
#pod Because of the way the options are parsed, you cannot have a class named "is",
#pod "with" or "extends".
#pod
#pod It's possible to specify options for classes:
#pod
#pod =over 4
#pod
#pod =item extends
#pod
#pod     class Foo extends Bar { ... }
#pod
#pod Sets a superclass for the class being declared.
#pod
#pod =item with
#pod
#pod     class Foo with Role             { ... }
#pod     class Foo with Role1 with Role2 { ... }
#pod     class Foo with (Role1, Role2)   { ... }
#pod
#pod Applies a role or roles to the class being declared.
#pod
#pod =item is mutable
#pod
#pod     class Foo is mutable { ... }
#pod
#pod Causes the class not to be made immutable after its definition.
#pod
#pod Options can also be provided for anonymous classes using the same syntax:
#pod
#pod     my $meta_class = class with Role;
#pod
#pod =back
#pod
#pod =head2 role
#pod
#pod     role Foo { ... }
#pod
#pod     my $anon_role = role { ... };
#pod
#pod Declares a new role. The role can be either named or anonymous, depending on
#pod whether or not a name is given. Within the role definition Moose::Role and
#pod MooseX::Method::Signatures are set up automatically in addition to the other
#pod keywords described in this document. Again, namespace::autoclean is injected to
#pod clean up Moose::Role and other imports for you.
#pod
#pod It's possible to specify options for roles:
#pod
#pod =over 4
#pod
#pod =item with
#pod
#pod     role Foo with Bar { ... }
#pod
#pod Applies a role to the role being declared.
#pod
#pod =back
#pod
#pod =head2 before / after / around / override / augment
#pod
#pod     before   foo ($x, $y, $z) { ... }
#pod     after    bar ($x, $y, $z) { ... }
#pod     around   baz ($x, $y, $z) { ... }
#pod     override moo ($x, $y, $z) { ... }
#pod     augment  kuh ($x, $y, $z) { ... }
#pod
#pod Add a method modifier. Those work like documented in L<Moose|Moose>, except for
#pod the slightly nicer syntax and the method signatures, which work like documented
#pod in L<MooseX::Method::Signatures|MooseX::Method::Signatures>.
#pod
#pod For the C<around> modifier an additional argument called C<$orig> is
#pod automatically set up as the invocant for the method.
#pod
#pod =head2 clean
#pod
#pod Sometimes you don't want the automatic cleaning the C<class> and C<role>
#pod keywords provide using namespace::autoclean. In those cases you can specify the
#pod C<dirty> trait for your class or role:
#pod
#pod     use MooseX::Declare;
#pod     class Foo is dirty { ... }
#pod
#pod This will prevent cleaning of your namespace, except for the keywords imported
#pod from C<Moose> or C<Moose::Role>. Additionally, a C<clean> keyword is provided,
#pod which allows you to explicitly clean all functions that were defined prior to
#pod calling C<clean>. Here's an example:
#pod
#pod     use MooseX::Declare;
#pod     class Foo is dirty {
#pod         sub helper_function { ... }
#pod         clean;
#pod         method foo ($stuff) { ...; return helper_function($stuff); }
#pod     }
#pod
#pod With that, the helper function won't be available as a method to a user of your
#pod class, but you're still able to use it inside your class.
#pod
#pod =head1 NOTE ON IMPORTS
#pod
#pod When creating a class with MooseX::Declare like:
#pod
#pod     use MooseX::Declare;
#pod     class Foo { ... }
#pod
#pod What actually happens is something like this:
#pod
#pod     {
#pod         package Foo;
#pod         use Moose;
#pod         use namespace::autoclean;
#pod         ...
#pod         __PACKAGE__->meta->make_immutable;
#pod     }
#pod
#pod So if you declare imports outside the class, the symbols get imported into the
#pod C<main::> namespace, not the class' namespace. The symbols then cannot be called
#pod from within the class:
#pod
#pod     use MooseX::Declare;
#pod     use Data::Dump qw/dump/;
#pod     class Foo {
#pod         method dump($value) { return dump($value) } # Data::Dump::dump IS NOT in Foo::
#pod         method pp($value)   { $self->dump($value) } # an alias for our dump method
#pod     }
#pod
#pod To solve this, only import MooseX::Declare outside the class definition
#pod (because you have to). Make all other imports inside the class definition.
#pod
#pod     use MooseX::Declare;
#pod     class Foo {
#pod         use Data::Dump qw/dump/;
#pod         method dump($value) { return dump($value) } # Data::Dump::dump IS in Foo::
#pod         method pp($value)   { $self->dump($value) } # an alias for our dump method
#pod     }
#pod
#pod     Foo->new->dump($some_value);
#pod     Foo->new->pp($some_value);
#pod
#pod B<NOTE> that the import C<Data::Dump::dump()> and the method C<Foo::dump()>,
#pod although having the same name, do not conflict with each other, because the
#pod imported C<dump> function will be cleaned during compile time, so only the
#pod method remains there at run time. If you want to do more esoteric things with
#pod imports, have a look at the C<clean> keyword and the C<dirty> trait.
#pod
#pod =head1 WARNING
#pod
#pod =for comment rafl agreed we should have a warning, and mst wrote this:
#pod
#pod B<Warning:> MooseX::Declare is based on L<Devel::Declare>, a giant bag of crack
#pod originally implemented by mst with the goal of upsetting the perl core
#pod developers so much by its very existence that they implemented proper
#pod keyword handling in the core.
#pod
#pod As of perl5 version 14, this goal has been achieved, and modules such
#pod as L<Devel::CallParser>, L<Function::Parameters>, and L<Keyword::Simple> provide
#pod mechanisms to mangle perl syntax that don't require hallucinogenic
#pod drugs to interpret the error messages they produce.
#pod
#pod If you want to use declarative syntax in new code, please for the love
#pod of kittens get yourself a recent perl and look at L<Moops> instead.
#pod
#pod =head1 SEE ALSO
#pod
#pod =for :list
#pod * L<Moose>
#pod * L<Moose::Role>
#pod * L<MooseX::Method::Signatures>
#pod * L<namespace::autoclean>
#pod * vim syntax: L<http://www.vim.org/scripts/script.php?script_id=2526>
#pod * emacs syntax: L<http://github.com/jrockway/cperl-mode>
#pod * Geany syntax + notes: L<http://www.cattlegrid.info/blog/2009/09/moosex-declare-geany-syntax.html>
#pod * L<Devel::CallParser>
#pod * L<Function::Parameters>
#pod * L<Keyword::Simple>
#pod * L<Moops>
#pod
#pod =cut


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Declare - (DEPRECATED) Declarative syntax for Moose

=head1 VERSION

version 0.43

=head1 SYNOPSIS

    use MooseX::Declare;

    class BankAccount {
        has 'balance' => ( isa => 'Num', is => 'rw', default => 0 );

        method deposit (Num $amount) {
            $self->balance( $self->balance + $amount );
        }

        method withdraw (Num $amount) {
            my $current_balance = $self->balance();
            ( $current_balance >= $amount )
                || confess "Account overdrawn";
            $self->balance( $current_balance - $amount );
        }
    }

    class CheckingAccount extends BankAccount {
        has 'overdraft_account' => ( isa => 'BankAccount', is => 'rw' );

        before withdraw (Num $amount) {
            my $overdraft_amount = $amount - $self->balance();
            if ( $self->overdraft_account && $overdraft_amount > 0 ) {
                $self->overdraft_account->withdraw($overdraft_amount);
                $self->deposit($overdraft_amount);
            }
        }
    }

=head1 DESCRIPTION

This module provides syntactic sugar for Moose, the postmodern object system
for Perl 5. When used, it sets up the C<class> and C<role> keywords.

B<Note:> Please see the L</WARNING> section below!

=head1 KEYWORDS

=head2 class

    class Foo { ... }

    my $anon_class = class { ... };

Declares a new class. The class can be either named or anonymous, depending on
whether or not a classname is given. Within the class definition Moose and
L<MooseX::Method::Signatures> are set up automatically in addition to the other
keywords described in this document. At the end of the definition the class
will be made immutable. namespace::autoclean is injected to clean up Moose and
other imports for you.

Because of the way the options are parsed, you cannot have a class named "is",
"with" or "extends".

It's possible to specify options for classes:

=over 4

=item extends

    class Foo extends Bar { ... }

Sets a superclass for the class being declared.

=item with

    class Foo with Role             { ... }
    class Foo with Role1 with Role2 { ... }
    class Foo with (Role1, Role2)   { ... }

Applies a role or roles to the class being declared.

=item is mutable

    class Foo is mutable { ... }

Causes the class not to be made immutable after its definition.

Options can also be provided for anonymous classes using the same syntax:

    my $meta_class = class with Role;

=back

=head2 role

    role Foo { ... }

    my $anon_role = role { ... };

Declares a new role. The role can be either named or anonymous, depending on
whether or not a name is given. Within the role definition Moose::Role and
MooseX::Method::Signatures are set up automatically in addition to the other
keywords described in this document. Again, namespace::autoclean is injected to
clean up Moose::Role and other imports for you.

It's possible to specify options for roles:

=over 4

=item with

    role Foo with Bar { ... }

Applies a role to the role being declared.

=back

=head2 before / after / around / override / augment

    before   foo ($x, $y, $z) { ... }
    after    bar ($x, $y, $z) { ... }
    around   baz ($x, $y, $z) { ... }
    override moo ($x, $y, $z) { ... }
    augment  kuh ($x, $y, $z) { ... }

Add a method modifier. Those work like documented in L<Moose|Moose>, except for
the slightly nicer syntax and the method signatures, which work like documented
in L<MooseX::Method::Signatures|MooseX::Method::Signatures>.

For the C<around> modifier an additional argument called C<$orig> is
automatically set up as the invocant for the method.

=head2 clean

Sometimes you don't want the automatic cleaning the C<class> and C<role>
keywords provide using namespace::autoclean. In those cases you can specify the
C<dirty> trait for your class or role:

    use MooseX::Declare;
    class Foo is dirty { ... }

This will prevent cleaning of your namespace, except for the keywords imported
from C<Moose> or C<Moose::Role>. Additionally, a C<clean> keyword is provided,
which allows you to explicitly clean all functions that were defined prior to
calling C<clean>. Here's an example:

    use MooseX::Declare;
    class Foo is dirty {
        sub helper_function { ... }
        clean;
        method foo ($stuff) { ...; return helper_function($stuff); }
    }

With that, the helper function won't be available as a method to a user of your
class, but you're still able to use it inside your class.

=head1 NOTE ON IMPORTS

When creating a class with MooseX::Declare like:

    use MooseX::Declare;
    class Foo { ... }

What actually happens is something like this:

    {
        package Foo;
        use Moose;
        use namespace::autoclean;
        ...
        __PACKAGE__->meta->make_immutable;
    }

So if you declare imports outside the class, the symbols get imported into the
C<main::> namespace, not the class' namespace. The symbols then cannot be called
from within the class:

    use MooseX::Declare;
    use Data::Dump qw/dump/;
    class Foo {
        method dump($value) { return dump($value) } # Data::Dump::dump IS NOT in Foo::
        method pp($value)   { $self->dump($value) } # an alias for our dump method
    }

To solve this, only import MooseX::Declare outside the class definition
(because you have to). Make all other imports inside the class definition.

    use MooseX::Declare;
    class Foo {
        use Data::Dump qw/dump/;
        method dump($value) { return dump($value) } # Data::Dump::dump IS in Foo::
        method pp($value)   { $self->dump($value) } # an alias for our dump method
    }

    Foo->new->dump($some_value);
    Foo->new->pp($some_value);

B<NOTE> that the import C<Data::Dump::dump()> and the method C<Foo::dump()>,
although having the same name, do not conflict with each other, because the
imported C<dump> function will be cleaned during compile time, so only the
method remains there at run time. If you want to do more esoteric things with
imports, have a look at the C<clean> keyword and the C<dirty> trait.

=head1 WARNING

=for comment rafl agreed we should have a warning, and mst wrote this:

B<Warning:> MooseX::Declare is based on L<Devel::Declare>, a giant bag of crack
originally implemented by mst with the goal of upsetting the perl core
developers so much by its very existence that they implemented proper
keyword handling in the core.

As of perl5 version 14, this goal has been achieved, and modules such
as L<Devel::CallParser>, L<Function::Parameters>, and L<Keyword::Simple> provide
mechanisms to mangle perl syntax that don't require hallucinogenic
drugs to interpret the error messages they produce.

If you want to use declarative syntax in new code, please for the love
of kittens get yourself a recent perl and look at L<Moops> instead.

=head1 SEE ALSO

=over 4

=item *

L<Moose>

=item *

L<Moose::Role>

=item *

L<MooseX::Method::Signatures>

=item *

L<namespace::autoclean>

=item *

vim syntax: L<http://www.vim.org/scripts/script.php?script_id=2526>

=item *

emacs syntax: L<http://github.com/jrockway/cperl-mode>

=item *

Geany syntax + notes: L<http://www.cattlegrid.info/blog/2009/09/moosex-declare-geany-syntax.html>

=item *

L<Devel::CallParser>

=item *

L<Function::Parameters>

=item *

L<Keyword::Simple>

=item *

L<Moops>

=back

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Piers Cawley Robert 'phaylon' Sedlacek Ash Berlin Nick Perez Nelo Onyiah Chas. J. Owens IV leedo Michele Beltrame Frank Wiegand David Steinbrunner Oleg Kostyuk Dave Rolsky Rafael Kitover Chris Prather Stevan Little Tomas Doran Yanick Champoux Justin Hunter

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Piers Cawley <pdcawley@bofh.org.uk>

=item *

Robert 'phaylon' Sedlacek <rs@474.at>

=item *

Ash Berlin <ash_github@firemirror.com>

=item *

Nick Perez <nperez@cpan.org>

=item *

Nelo Onyiah <nelo.onyiah@gmail.com>

=item *

Chas. J. Owens IV <chas.owens@gmail.com>

=item *

leedo <lee@laylward.com>

=item *

Michele Beltrame <arthas@cpan.org>

=item *

Frank Wiegand <fwie@cpan.org>

=item *

David Steinbrunner <dsteinbrunner@pobox.com>

=item *

Oleg Kostyuk <cub.uanic@gmail.com>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Rafael Kitover <rkitover@io.com>

=item *

Chris Prather <chris@prather.org>

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
