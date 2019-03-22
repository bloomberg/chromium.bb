package Filter::tee ;

require 5.002;
require DynaLoader;
use strict;
use warnings;
use vars qw( @ISA $VERSION);
@ISA = qw(DynaLoader);
$VERSION = "1.43" ;

bootstrap Filter::tee ;

1;
__END__

=head1 NAME

Filter::tee - tee source filter

=head1 SYNOPSIS

    use Filter::tee 'filename' ;
    use Filter::tee '>filename' ;
    use Filter::tee '>>filename' ;

=head1 DESCRIPTION

This filter copies all text from the line after the C<use> in the
current source file to the file specified by the parameter
C<filename>.

By default and when the filename is prefixed with a '>' the output file
will be emptied first if it already exists.

If the output filename is prefixed with '>>' it will be opened for
appending.

This filter is useful as a debugging aid when developing other source
filters.

=head1 AUTHOR

Paul Marquess 

=head1 DATE

20th June 1995.

=cut

