package Filter::cpp;
 
use Config ;
use Carp ;
use Filter::Util::Exec ;
use strict;
use warnings;
use vars qw($VERSION);

$VERSION = '1.43' ;

my $cpp;
my $sep;
if ($^O eq 'MSWin32') {
    $cpp = 'cpp.exe' ;
    $sep = ';';
}
else {
    ($cpp) = $Config{cppstdin} =~ /^(\S+)/;
    $sep = ':';
}

croak ("Cannot find cpp\n")
    if ! $cpp;

# Check if cpp is installed
if ( ! -x $cpp) {
    my $foundCPP = 0 ;
    foreach my $dir (split($sep, $ENV{PATH}), '')
    {
        if (-x "$dir/$cpp")
        {
            $foundCPP = 1;
            last ;
        }
    }

    croak "Cannot find cpp\n"
        if ! $foundCPP ;
}

sub import 
{ 
    my($self, @args) = @_ ;

    #require "Filter/exec.pm" ;

    if ($^O eq 'MSWin32') {
        Filter::Util::Exec::filter_add ($self, 'cmd', '/c', 
		"cpp.exe 2>nul") ;
    }
    else {
        Filter::Util::Exec::filter_add ($self, 'sh', '-c', 
		"$Config{'cppstdin'} $Config{'cppminus'} 2>/dev/null") ;
    }
}

1 ;
__END__

=head1 NAME

Filter::cpp - cpp source filter

=head1 SYNOPSIS

    use Filter::cpp ;

=head1 DESCRIPTION

This source filter pipes the current source file through the C
pre-processor (cpp) if it is available.

As with all source filters its scope is limited to the current source
file only. Every file you want to be processed by the filter must have a

    use Filter::cpp ;

near the top.

Here is an example script which uses the filter:

    use Filter::cpp ;

    #define FRED 1
    $a = 2 + FRED ;
    print "a = $a\n" ;
    #ifdef FRED
    print "Hello FRED\n" ;
    #else
    print "Where is FRED\n" ;
    #endif

And here is what it will output:

    a = 3
    Hello FRED

This example below, provided by Michael G Schwern, shows a clever way
to get Perl to use a C pre-processor macro when the Filter::cpp module
is available, or to use a Perl sub when it is not.

    # use Filter::cpp if we can.
    BEGIN { eval 'use Filter::cpp' }

    sub PRINT {
        my($string) = shift;

    #define PRINT($string) \
        (print $string."\n")
    }
     
    PRINT("Mu");

Look at Michael's Tie::VecArray module for a practical use.

=head1 AUTHOR

Paul Marquess 

=head1 DATE

11th December 1995.

=cut

