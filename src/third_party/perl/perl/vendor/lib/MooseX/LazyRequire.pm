package MooseX::LazyRequire;
# git description: v0.10-7-gf996968
$MooseX::LazyRequire::VERSION = '0.11';
# ABSTRACT: Required attributes which fail only when trying to use them
# KEYWORDS: moose extension attribute required lazy defer populate method

use Moose 0.94 ();
use Moose::Exporter;
use aliased 0.30 'MooseX::LazyRequire::Meta::Attribute::Trait::LazyRequire';
use namespace::autoclean;

#pod =head1 SYNOPSIS
#pod
#pod     package Foo;
#pod
#pod     use Moose;
#pod     use MooseX::LazyRequire;
#pod
#pod     has foo => (
#pod         is            => 'ro',
#pod         lazy_required => 1,
#pod     );
#pod
#pod     has bar => (
#pod         is      => 'ro',
#pod         builder => '_build_bar',
#pod     );
#pod
#pod     sub _build_bar { shift->foo }
#pod
#pod
#pod     Foo->new(foo => 42); # succeeds, foo and bar will be 42
#pod     Foo->new(bar => 42); # succeeds, bar will be 42
#pod     Foo->new;            # fails, neither foo nor bare were given
#pod
#pod =head1 DESCRIPTION
#pod
#pod This module adds a C<lazy_required> option to Moose attribute declarations.
#pod
#pod The reader methods for all attributes with that option will throw an exception
#pod unless a value for the attributes was provided earlier by a constructor
#pod parameter or through a writer method.
#pod
#pod =head1 CAVEATS
#pod
#pod Prior to Moose 1.9900, roles didn't have an attribute metaclass, so this module can't
#pod easily apply its magic to attributes defined in roles. If you want to use
#pod C<lazy_required> in role attributes, you'll have to apply the attribute trait
#pod yourself:
#pod
#pod     has foo => (
#pod         traits        => ['LazyRequire'],
#pod         is            => 'ro',
#pod         lazy_required => 1,
#pod     );
#pod
#pod With Moose 1.9900, you can use this module in roles just the same way you can
#pod in classes.
#pod
#pod =cut

my %metaroles = (
    class_metaroles => {
        attribute => [LazyRequire],
    },
);

$metaroles{role_metaroles} = {
    applied_attribute => [LazyRequire],
    }
    if $Moose::VERSION >= 1.9900;

Moose::Exporter->setup_import_methods(%metaroles);

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::LazyRequire - Required attributes which fail only when trying to use them

=head1 VERSION

version 0.11

=head1 SYNOPSIS

    package Foo;

    use Moose;
    use MooseX::LazyRequire;

    has foo => (
        is            => 'ro',
        lazy_required => 1,
    );

    has bar => (
        is      => 'ro',
        builder => '_build_bar',
    );

    sub _build_bar { shift->foo }


    Foo->new(foo => 42); # succeeds, foo and bar will be 42
    Foo->new(bar => 42); # succeeds, bar will be 42
    Foo->new;            # fails, neither foo nor bare were given

=head1 DESCRIPTION

This module adds a C<lazy_required> option to Moose attribute declarations.

The reader methods for all attributes with that option will throw an exception
unless a value for the attributes was provided earlier by a constructor
parameter or through a writer method.

=head1 CAVEATS

Prior to Moose 1.9900, roles didn't have an attribute metaclass, so this module can't
easily apply its magic to attributes defined in roles. If you want to use
C<lazy_required> in role attributes, you'll have to apply the attribute trait
yourself:

    has foo => (
        traits        => ['LazyRequire'],
        is            => 'ro',
        lazy_required => 1,
    );

With Moose 1.9900, you can use this module in roles just the same way you can
in classes.

=for Pod::Coverage init_meta

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

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge David Precious Jesse Luehrs

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

David Precious <davidp@preshweb.co.uk>

=item *

Jesse Luehrs <doy@tozt.net>

=back

=cut
