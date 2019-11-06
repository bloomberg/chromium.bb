package Filter::sh;
 
use Carp ;
use strict ;
use warnings ;
use vars qw($VERSION) ;
$VERSION = "1.43" ;

use Filter::Util::Exec ;

sub import 
{ 
    my($self, @args) = @_ ;

    croak ("Usage: use Filter::sh 'command'")
	unless @args ;

    #require "Filter/exec.pm" ;
    #Filter::exec::import ($self, 'sh', '-c', "@args") ; 
    if ($^O eq 'MSWin32') {
        Filter::Util::Exec::filter_add ($self, 'cmd', '/c', "@args") ; 
    }
    else {
        Filter::Util::Exec::filter_add ($self, 'sh', '-c', "@args") ; 
    }
}

1 ;
__END__

=head1 NAME

Filter::sh - sh source filter

=head1 SYNOPSIS

    use Filter::sh 'command' ;

=head1 DESCRIPTION

This filter pipes the current source file through the program which
corresponds to the C<command> parameter using the Bourne shell. 

As with all source filters its scope is limited to the current source
file only. Every file you want to be processed by the filter must have a

    use Filter::sh 'command' ;

near the top.

Here is an example script which uses the filter:

    use Filter::sh 'tr XYZ PQR' ;
    $a = 1 ;
    print "XYZ a = $a\n" ;

And here is what it will output:

    PQR = 1

=head1 WARNING

You should be I<very> careful when using this filter. Because of the
way the filter is implemented it is possible to end up with deadlock.

Be especially careful when stacking multiple instances of the filter in
a single source file.

=head1 AUTHOR

Paul Marquess 

=head1 DATE

11th December 1995.

=cut

