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
#!/usr/bin/perl -sI../lib -Ilib/
#line 15
##
## makerandom - interface to crypt::random
##
## Copyright (c) 1998, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: makerandom,v 1.1.1.1 2001/06/21 15:34:49 vipul Exp $

use Crypt::Random qw( makerandom makerandom_itv );

unless ( $size || $lower )  { 
    my $name = $0; 
    $name =~ s:.*/::;
    print "usage: $name [options] \
       -size=bitsize \
       -strength=[01] \
       -dev=device \ 
       -lower=lower_bound \
       -upper=upper_bound \
       -num=quantity\n\n";
    exit 0;
}


$strength = 0 unless $strength; 
my $i     = 1; 
   $num   = 1 unless $num; 

while ( $i++ <= $num ) { 

my $r; 

if ( $size ) { 
    $r = makerandom Size => $size, 
                    Strength => $strength,
                    Device => $dev; 
}

if ( $lower ) { 
    $r = makerandom_itv Lower => $lower, 
                        Upper => $upper, 
                        Strength => $strength, 
                        Device => $dev;
}

print "$r\n";

}

__END__
:endofperl
