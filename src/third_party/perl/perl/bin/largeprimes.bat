@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
"%~dp0perl.exe" -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl -sI.. -I../lib/ -Ilib/
#line 15
##
## largeprimes -- generates large provable primes, uniformally distributed
##                in random intervals, with maurer's recursive algorithm.
##
## Copyright (c) 1998, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id$


use Crypt::Primes; 

unless ( $bitsize ) { 
    print "$0 -bitsize=bits -num=number_of_primes -v=verbosity\n\n";
    exit 0;
}

for ( $i=0; $i <= $num; $i++ ) { 
    my $prime = Crypt::Primes::maurer Size => $bitsize, 
                                      Verbosity => $v, 
                                      Generator => $generator;
    my $g = 0;
    if ( ref $prime eq 'HASH') { 
        $g = $prime->{Generator};
        $prime = $prime->{Prime};
    }

    if ($t) { print "$prime\n" } else { 
        print "\n" if $v;
        print "random $bitsize bit prime: $prime";
        print ", $g" if $g;
        print "\n";
    }
}

=head1 NAME

largeprimes -- generate large, random primes using Crypt::Primes.

=head1 SYNOPSIS

    largeprimes -bitsize=128 -v
    largeprimes -bitsize=512 -v=2
    largeprimes -bitsize=512 -v -num=10
    largeprimes -bitsize=1024 -generator -v

=head1 DESCRIPTION

largeprimes generates a provable prime of specified bitsize and prints it on
STDOUT.  For more details see Crypt::Primes(3) manpage.

=head1 SEE ALSO

Crypt::Primes(3)

=head1 AUTHOR

Vipul Ved Prakash, mail@vipul.net

=head1 LICENSE

Artistic.

=cut



__END__
:endofperl
