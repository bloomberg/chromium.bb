package Filter::decrypt ;

require 5.002 ;
require DynaLoader;
use strict;
use warnings;
use vars qw(@ISA $VERSION);
@ISA = qw(DynaLoader);
$VERSION = "1.43" ;

bootstrap Filter::decrypt ;
1;
__END__

=head1 NAME

Filter::decrypt - template for a decrypt source filter

=head1 SYNOPSIS

    use Filter::decrypt ;

=head1 DESCRIPTION

This is a sample decrypting source filter.

Although this is a fully functional source filter and it does implement
a I<very> simple decrypt algorithm, it is I<not> intended to be used as
it is supplied. Consider it to be a template which you can combine with
a proper decryption algorithm to develop your own decryption filter.

=head1 WARNING

It is important to note that a decryption filter can I<never> provide
complete security against attack. At some point the parser within Perl
needs to be able to scan the original decrypted source. That means that
at some stage fragments of the source will exist in a memory buffer. 

Also, with the introduction of the Perl Compiler backend modules, and
the B::Deparse module in particular, using a Source Filter to hide source
code is becoming an increasingly futile exercise.

The best you can hope to achieve by decrypting your Perl source using a
source filter is to make it unavailable to the casual user.

Given that proviso, there are a number of things you can do to make
life more difficult for the prospective cracker.

=over 5

=item 1.

Strip the Perl binary to remove all symbols.

=item 2.

Build the decrypt extension using static linking. If the extension is
provided as a dynamic module, there is nothing to stop someone from
linking it at run time with a modified Perl binary.

=item 3.

Do not build Perl with C<-DDEBUGGING>. If you do then your source can
be retrieved with the C<-Dp> command line option. 

The sample filter contains logic to detect the C<DEBUGGING> option.

=item 4.

Do not build Perl with C debugging support enabled.

=item 5.

Do not implement the decryption filter as a sub-process (like the cpp
source filter). It is possible to peek into the pipe that connects to
the sub-process.

=item 6.

Check that the Perl Compiler isn't being used. 

There is code in the BOOT: section of decrypt.xs that shows how to detect
the presence of the Compiler. Make sure you include it in your module.

Assuming you haven't taken any steps to spot when the compiler is in
use and you have an encrypted Perl script called "myscript.pl", you can
get access the source code inside it using the perl Compiler backend,
like this

    perl -MO=Deparse myscript.pl

Note that even if you have included the BOOT: test, it is still
possible to use the Deparse module to get the source code for individual
subroutines.

=item 7.

Do not use the decrypt filter as-is. The algorithm used in this filter
has been purposefully left simple.

=back

If you feel that the source filtering mechanism is not secure enough
you could try using the unexec/undump method. See the Perl FAQ for
further details.

=head1 AUTHOR

Paul Marquess 

=head1 DATE

19th December 1995

=cut
