package ok;
$ok::VERSION = 0.02;

use strict;
use Test::More ();

sub import {
    shift; goto &Test::More::use_ok if @_;

    # No argument list - croak as if we are prototyped like use_ok()
    my (undef, $file, $line) = caller();
    ($file =~ /^\(eval/) or die "Not enough arguments for 'use ok' at $file line $line\n";
}


__END__

=head1 NAME

ok - Alternative to Test::More::use_ok

=head1 SYNOPSIS

    use ok( 'Some::Module' );

=head1 DESCRIPTION

With this module, simply change all C<use_ok> in test scripts to C<use ok>,
and they will be executed at C<BEGIN> time.

Please see L<Test::use::ok> for the full description.

=head1 COPYRIGHT

Copyright 2005, 2006 by Audrey Tang E<lt>cpan@audreyt.orgE<gt>.

This software is released under the MIT license cited below.

=head2 The "MIT" License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=cut
