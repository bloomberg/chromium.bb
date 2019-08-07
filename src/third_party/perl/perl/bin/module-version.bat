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
#!perl
#line 15

use 5.008009;
use strict;
use warnings;

use App::module::version;

my $app = App::module::version->new();
$app->parse_options(@ARGV);
$app->do_job() or exit 1;

__END__

=head1 NAME

module-version - Gets the version info about a module

=head1 VERSION

This document describes module-version version 1.004

=head1 DESCRIPTION

This script gets the version of a requested list of modules.

It also can check the version of perl, or of Strawberry Perl or 
ActivePerl.

=head1 SYNOPSIS

  module-version [ --help ] [ --usage ] [ --man ] [ --version ] [ -?] 
                 [--prompt] [perl] [strawberry[perl]] [activeperl]
                 Module1::To::Check Module2::To::Check ...

  Options:
    -usage          Gives a minimum amount of aid and comfort.
    -help           Gives aid and comfort.
    -?              Gives aid and comfort.
    -man            Gives maximum aid and comfort.

    -version        Gives the name, version and copyright of the script.

    -prompt         Prompts for module names to print the versions of.

    perl            Gives the version, $^O, and $Config{archname} of perl.
    strawberryperl  Gives the version, bitness and version of gcc of
                    Strawberry Perl.
    activeperl      Gives the version and build number of ActivePerl.

    Module1::To::Check Module2::To::Check
                    Prints the version of the module if it exists and
                    is easily retrievable.

=head1 DEPENDENCIES

Perl 5.8.9 is the mimimum version of perl that this script will run on.

Other modules that this script depends on are 
L<Getopt::Long|Getopt::Long>, L<Pod::Usage|Pod::Usage>, 
and L<Term::ReadKey|Term::ReadKey>

=head1 SUPPORT

Support is provided for this script on the same basis as Strawberry Perl.

=head1 AUTHOR

Curtis Jewell, E<lt>csjewell@cpan.orgE<gt>

=head1 COPYRIGHT & LICENSE

Copyright 2010 Curtis Jewell.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this distribution.

=cut

__END__
:endofperl
