package Filter::Util::Exec ;

require 5.002 ;
require DynaLoader;
use strict;
use warnings;
use vars qw(@ISA $VERSION) ;
@ISA = qw(DynaLoader);
$VERSION = "1.43" ;

bootstrap Filter::Util::Exec ;
1 ;
__END__

=head1 NAME

Filter::Util::Exec - exec source filter

=head1 SYNOPSIS
 
    use Filter::Util::Exec;

=head1 DESCRIPTION

This module is provides the interface to allow the creation of I<Source
Filters> which use a Unix coprocess.

See L<Filter::exec>, L<Filter::cpp> and L<Filter::sh> for examples of
the use of this module.

=head1 AUTHOR

Paul Marquess 

=head1 DATE

11th December 1995.

=cut

