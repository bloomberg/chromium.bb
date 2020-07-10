use strict;
use warnings;
package B::Hooks::OP::Check; # git description: 0.21-5-g968d5ca
# ABSTRACT: Wrap OP check callbacks

require 5.008001;
use parent qw/DynaLoader/;

our $VERSION = '0.22';

sub dl_load_flags { 0x01 }

__PACKAGE__->bootstrap($VERSION);

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

B::Hooks::OP::Check - Wrap OP check callbacks

=head1 VERSION

version 0.22

=head1 SYNOPSIS

    # include "hook_op_check.h"

    STATIC OP *my_const_check_op (pTHX_ OP *op, void *user_data) {
        /* ... */
        return op;
    }

    STATIC hook_op_check_id my_hook_id = 0;

    void
    setup ()
        CODE:
            my_hook_id = hook_op_check (OP_CONST, my_const_check_op, NULL);

    void
    teardown ()
        CODE:
            hook_op_check_remove (OP_CONST, my_hook_id);

=head1 DESCRIPTION

This module provides a C API for XS modules to hook into the callbacks of
C<PL_check>.

L<ExtUtils::Depends> is used to export all functions for other XS modules to
use. Include the following in your Makefile.PL:

    my $pkg = ExtUtils::Depends->new('Your::XSModule', 'B::Hooks::OP::Check');
    WriteMakefile(
        ... # your normal makefile flags
        $pkg->get_makefile_vars,
    );

Your XS module can now include C<hook_op_check.h>.

=for stopwords cb

=head1 TYPES

=head2 typedef OP *(*hook_op_check_cb) (pTHX_ OP *, void *);

Type that callbacks need to implement.

=head2 typedef UV hook_op_check_id

Type to identify a callback.

=head1 FUNCTIONS

=head2 hook_op_check_id hook_op_check (opcode type, hook_op_check_cb cb, void *user_data)

Register the callback C<cb> to be called after the C<PL_check> function for
opcodes of the given C<type>. C<user_data> will be passed to the callback as
the last argument. Returns an id that can be used to remove the callback later
on.

=head2 void *hook_op_check_remove (opcode type, hook_op_check_id id)

Remove the callback identified by C<id>. Returns the user_data that the callback had.

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=B-Hooks-OP-Check>
(or L<bug-B-Hooks-OP-Check@rt.cpan.org|mailto:bug-B-Hooks-OP-Check@rt.cpan.org>).

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Zefram Alexandr Ciornii

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Zefram <zefram@fysh.org>

=item *

Alexandr Ciornii <alexchorny@gmail.com>

=back

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2008 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
