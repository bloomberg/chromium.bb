package FFI::Raw;
$FFI::Raw::VERSION = '0.32';
use strict;
use warnings;

require XSLoader;
XSLoader::load('FFI::Raw', $FFI::Raw::VERSION);

require FFI::Raw::Ptr;

use overload
	'&{}'  => \&coderef,
	'bool' => \&_bool;

sub _bool {
	my $ffi = shift;
	return $ffi;
}

=head1 NAME

FFI::Raw - Perl bindings to the portable FFI library (libffi)

=head1 VERSION

version 0.32

=head1 SYNOPSIS

    use FFI::Raw;

    my $cos = FFI::Raw -> new(
      'libm.so', 'cos',
      FFI::Raw::double, # return value
      FFI::Raw::double  # arg #1
    );

    say $cos -> call(2.0);

=head1 DESCRIPTION

B<FFI::Raw> provides a low-level foreign function interface (FFI) for Perl based
on L<libffi|http://sourceware.org/libffi/>. In essence, it can access and call
functions exported by shared libraries without the need to write C/XS code.

Dynamic symbols can be automatically resolved at runtime so that the only
information needed to use B<FFI::Raw> is the name (or path) of the target
library, the name of the function to call and its signature (though it is also
possible to pass a function pointer obtained, for example, using L<DynaLoader>).

Note that this module has nothing to do with L<FFI>.

=head1 METHODS

=head2 new( $library, $function, $return_type [, $arg_type ...] )

Create a new C<FFI::Raw> object. It loads C<$library>, finds the function
C<$function> with return type C<$return_type> and creates a calling interface.

If C<$library> is C<undef> then the function is searched in the main program.

This method also takes a variable number of types, representing the arguments
of the wanted function.

=head2 new_from_ptr( $function_ptr, $return_type [, $arg_type ...] )

Create a new C<FFI::Raw> object from the C<$function_ptr> function pointer.

This method also takes a variable number of types, representing the arguments
of the wanted function.

=head2 call( [$arg ...] )

Execute the C<FFI::Raw> function. This methoed also takes a variable number of
arguments, which are passed to the called function. The argument types must
match the types passed to C<new> (or C<new_from_ptr>).

The C<FFI::Raw> object can be used as a CODE reference as well. Dereferencing
the object will work just like call():

    $cos -> call(2.0); # normal call() call
    $cos -> (2.0);     # dereference as CODE ref

This works because FFI::Raw overloads the C<&{}> operator.

=head2 coderef( )

Return a code reference of a given C<FFI::Raw>.

=cut

sub coderef {
	my $ffi = shift;
	return sub { $ffi -> call(@_) };
}

=head1 SUBROUTINES

=head2 memptr( $length )

Create a L<FFI::Raw::MemPtr>. This is a shortcut for C<FFI::Raw::MemPtr-E<gt>new(...)>.

=cut

sub memptr { FFI::Raw::MemPtr -> new(@_) }

=head2 callback( $coderef, $ret_type [, $arg_type ...] )

Create a L<FFI::Raw::Callback>. This is a shortcut for C<FFI::Raw::Callback-E<gt>new(...)>.

=cut

sub callback { FFI::Raw::Callback -> new(@_) }

=head1 TYPES

=head2 FFI::Raw::void

Return a C<FFI::Raw> void type.

=cut

sub void ()  { ord 'v' }

=head2 FFI::Raw::int

Return a C<FFI::Raw> integer type.

=cut

sub int ()   { ord 'i' }

=head2 FFI::Raw::uint

Return a C<FFI::Raw> unsigned integer type.

=cut

sub uint ()   { ord 'I' }

=head2 FFI::Raw::short

Return a C<FFI::Raw> short integer type.

=cut

sub short ()   { ord 'z' }

=head2 FFI::Raw::ushort

Return a C<FFI::Raw> unsigned short integer type.

=cut

sub ushort ()   { ord 'Z' }

=head2 FFI::Raw::long

Return a C<FFI::Raw> long integer type.

=cut

sub long ()   { ord 'l' }

=head2 FFI::Raw::ulong

Return a C<FFI::Raw> unsigned long integer type.

=cut

sub ulong ()   { ord 'L' }

=head2 FFI::Raw::int64

Return a C<FFI::Raw> 64 bit integer type. This requires L<Math::Int64> to work.

=cut

sub int64 ()   { ord 'x' }

=head2 FFI::Raw::uint64

Return a C<FFI::Raw> unsigned 64 bit integer type. This requires L<Math::Int64> 
to work.

=cut

sub uint64 ()   { ord 'X' }

=head2 FFI::Raw::char

Return a C<FFI::Raw> char type.

=cut

sub char ()  { ord 'c' }

=head2 FFI::Raw::uchar

Return a C<FFI::Raw> unsigned char type.

=cut

sub uchar ()  { ord 'C' }

=head2 FFI::Raw::float

Return a C<FFI::Raw> float type.

=cut

sub float () { ord 'f' }

=head2 FFI::Raw::double

Return a C<FFI::Raw> double type.

=cut

sub double () { ord 'd' }

=head2 FFI::Raw::str

Return a C<FFI::Raw> string type.

=cut

sub str ()   { ord 's' }

=head2 FFI::Raw::ptr

Return a C<FFI::Raw> pointer type.

=cut

sub ptr ()   { ord 'p' }

=head1 AUTHOR

Alessandro Ghedini <alexbio@cpan.org>

=head1 SEE ALSO

L<FFI>, L<Ctypes|http://gitorious.org/perl-ctypes>

=head1 LICENSE AND COPYRIGHT

Copyright 2012 Alessandro Ghedini.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.

=cut

1; # End of FFI::Raw
