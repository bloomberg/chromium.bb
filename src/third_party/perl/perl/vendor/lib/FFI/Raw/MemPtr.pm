package FFI::Raw::MemPtr;
$FFI::Raw::MemPtr::VERSION = '0.32';
use strict;
use warnings;

=head1 NAME

FFI::Raw::MemPtr - FFI::Raw memory pointer type

=head1 VERSION

version 0.32

=head1 DESCRIPTION

A B<FFI::Raw::MemPtr> represents a memory pointer which can be passed to
functions taking a C<FFI::Raw::ptr> argument.

The allocated memory is automatically deallocated once the object is not in use
anymore.

=head1 METHODS

=head2 new( $length )

Allocate a new C<FFI::Raw::MemPtr> of size C<$length> bytes.

=head2 new_from_buf( $buffer, $length )

Allocate a new C<FFI::Raw::MemPtr> of size C<$length> bytes and copy C<$buffer>
into it. This can be used, for example, to pass a pointer to a function that
takes a C struct pointer, by using C<pack()> or the L<Convert::Binary::C> module
to create the actual struct content.

For example, consider the following C code

    struct some_struct {
      int some_int;
      char some_str[];
    };

    extern void take_one_struct(struct some_struct *arg) {
      if (arg -> some_int == 42)
        puts(arg -> some_str);
    }

It can be called using FFI::Raw as follows:

    use FFI::Raw;

    my $packed = pack('ix![p]p', 42, 'hello');
    my $arg = FFI::Raw::MemPtr -> new_from_buf($packed, length $packed);

    my $take_one_struct = FFI::Raw -> new(
      $shared, 'take_one_struct',
      FFI::Raw::void, FFI::Raw::ptr
    );

    $take_one_struct -> ($arg);

Which would print C<hello>.

=head2 new_from_ptr( $ptr )

Allocate a new C<FFI::Raw::MemPtr> pointing to the C<$ptr>, which can be either
a C<FFI::Raw::MemPtr> or a pointer returned by another function.

This is the C<FFI::Raw> equivalent of a pointer to a pointer.

=head2 tostr( [$length] )

Convert a C<FFI::Raw::MemPtr> to a Perl string. If C<$length> is not provided,
the length of the string will be computed using C<strlen()>.

=head1 AUTHOR

Alessandro Ghedini <alexbio@cpan.org>

=head1 LICENSE AND COPYRIGHT

Copyright 2013 Alessandro Ghedini.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.

=cut

1; # End of FFI::Raw::MemPtr
