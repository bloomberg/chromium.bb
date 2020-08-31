# Copyright (c) 2003-2004, 2012-2013 by the gtk2-perl team (see the file
# AUTHORS)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

package ExtUtils::PkgConfig;

use strict;
use Carp;
use English qw(-no_match_vars); # avoid regex performance penalty

use vars qw/ $VERSION $AUTOLOAD/;

$VERSION = '1.16';

sub import {
	my $class = shift;
	return unless @_;
        die "$class version $_[0] is required--this is only version $VERSION"
		if $VERSION < $_[0];
}

sub AUTOLOAD
{
	my $class = shift;
	my $modulename = shift;
	my $function = $AUTOLOAD;
	$function =~ s/.*://;
	$function =~ s/_/-/g;

	my $ans = undef;
	my $arg = shift;
	if (grep {$_ eq $function} qw/libs
	                              modversion
	                              cflags
	                              cflags-only-I
	                              cflags-only-other
	                              libs-only-L
	                              libs-only-l
	                              libs-only-other/)
	{
		# simple
		$ans = `pkg-config --$function \"$modulename\"`;
	}
	elsif ('static-libs' eq $function)
	{
		$ans = `pkg-config --libs --static \"$modulename\"`;
	}
	elsif ('variable' eq $function)
	{
		# variable
		$ans = `pkg-config --${function}=$arg \"$modulename\"`;
	}
	elsif (grep {$_ eq $function} qw/atleast-version
	                                 exact-version
	                                 max-version/)
	{
		# boolean
		$ans = not system (
			"pkg-config --${function}=$arg \"$modulename\"");
	}
	else
	{
		croak "method '$function' not implemented";
		$ans = '';
	}

	chomp($ans);
	$ans = undef if $ans eq '';

	return $ans;
}

sub exists {
	my ($class, @pkg_candidates) = @_;
	
	foreach my $candidate (@pkg_candidates)
	{
		my $output = qx/pkg-config --exists "$candidate" 2>&1/;
		if (0 == $CHILD_ERROR) {
			return 1;
		}
	}
	
	return '';
}

sub find {
	my ($class, @pkg_candidates) = @_;
	my (@pkgs_found, @error_messages);

	# try all package candidates and save all those that pkg-config
	# recognizes
	foreach my $candidate (@pkg_candidates)
	{
		my $output = qx/pkg-config --exists --print-errors "$candidate" 2>&1/;
		if (0 == $CHILD_ERROR) {
			push @pkgs_found, $candidate;
		}
		else
		{
			push @error_messages, $output;
		}
	}

	if (0 == scalar @pkgs_found)
	{
		foreach my $message (@error_messages) {
			carp $message;
		}

		if (@pkg_candidates > 1)
		{
			my $list = join ', ', @pkg_candidates;
			croak "*** can not find package for any of ($list)\n"
			    . "*** check that one of them is properly installed and available in PKG_CONFIG_PATH\n";
		}
		else
		{
			croak "*** can not find package $pkg_candidates[0]\n"
			    . "*** check that it is properly installed and available in PKG_CONFIG_PATH\n";
		}
	}

	# return the data of the package that was found first
	my %data = ();
	$data{pkg} = $pkgs_found[0];
	foreach my $what (qw/modversion cflags libs/) {
		$data{$what} = `pkg-config --$what \"$data{pkg}\"`;
                $data{$what} =~ s/[\015\012]+$//;
		croak "*** can't find $what for \"$data{pkg}\"\n"
		    . "*** is it properly installed and available in PKG_CONFIG_PATH?\n"
			unless defined $data{$what};
	}
	return %data;
}

sub create_version_macros {
	my ($class, $pkg, $stem) = @_;

	if( $pkg && $stem ) {
		my %data = ExtUtils::PkgConfig->find ($pkg);

		if( %data ) {
			my @modversion = split /\./, $data{modversion};
			$modversion[1] = 0 unless defined $modversion[1];
			$modversion[2] = 0 unless defined $modversion[2];

			# If a version part contains non-numeric characters,
			# see if it at least starts with numbers and use those.
			# This is needed for versions like '2.0b2'.
			# foreach ( @modversion ) {
			# 	if (/\D/ && /^(\d+)/) {
			# 		$_ = $1;
			# 	}
			# }
			@modversion =
				map { /\D/ && /^(\d+)/ ? $1 : $_ } @modversion;

			return <<__EOD__;
#define $stem\_MAJOR_VERSION ($modversion[0])
#define $stem\_MINOR_VERSION ($modversion[1])
#define $stem\_MICRO_VERSION ($modversion[2])
#define $stem\_CHECK_VERSION(major,minor,micro) \\
	($stem\_MAJOR_VERSION > (major) || \\
	 ($stem\_MAJOR_VERSION == (major) && $stem\_MINOR_VERSION > (minor)) || \\
	 ($stem\_MAJOR_VERSION == (major) && $stem\_MINOR_VERSION == (minor) && $stem\_MICRO_VERSION >= (micro)))
__EOD__
		}
	}

	return undef;
}

sub write_version_macros {
	my ($class, $file, @pkgs) = @_;

	open FILE, ">$file" or croak "*** can not open file $file for writing\n";

	for (my $i = 0; $i < @pkgs; $i += 2) {
		my $macros = ExtUtils::PkgConfig->create_version_macros ($pkgs[$i], $pkgs[$i+1]);
		if( defined $macros ) {
			print FILE $macros;
		}
	}

	close FILE or croak "*** can not close file $file\n";
}

1;

=head1 NAME

ExtUtils::PkgConfig - simplistic interface to pkg-config

=head1 SYNOPSIS

 use ExtUtils::PkgConfig;

 $package = 'gtk+-2.0';

 %pkg_info = ExtUtils::PkgConfig->find ($package);
 print "modversion:  $pkg_info{modversion}\n";
 print "cflags:      $pkg_info{cflags}\n";
 print "libs:        $pkg_info{libs}\n";

 $exists = ExtUtils::PkgConfig->exists($package);

 $modversion = ExtUtils::PkgConfig->modversion($package);

 $libs = ExtUtils::PkgConfig->libs($package);

 $cflags = ExtUtils::PkgConfig->cflags($package);

 $cflags_only_I = ExtUtils::PkgConfig->cflags_only_I($package);

 $cflags_only_other = ExtUtils::PkgConfig->cflags_only_other($package);

 $libs_only_L = ExtUtils::PkgConfig->libs_only_L($package);

 $libs_only_l = ExtUtils::PkgConfig->libs_only_l($package);

 $libs_only_other = ExtUtils::PkgConfig->libs_only_other($package);

 $static_libs = ExtUtils::PkgConfig->static_libs($package);

 $var_value = ExtUtils::PkgConfig->variable($package, $var);

 if (ExtUtils::PkgConfig->atleast_version($package,$version)) {
    ...
 }

 if (ExtUtils::PkgConfig->exact_version($package,$version)) {
    ...
 }

 if (ExtUtils::PkgConfig->max_version($package,$version)) {
    ...
 }

=head1 DESCRIPTION

The pkg-config program retrieves information about installed libraries,
usually for the purposes of compiling against and linking to them.

ExtUtils::PkgConfig is a very simplistic interface to this utility, intended
for use in the Makefile.PL of perl extensions which bind libraries that
pkg-config knows.  It is really just boilerplate code that you would've
written yourself.

=head2 USAGE

=over

=item HASH = ExtUtils::PkgConfig->find (STRING, [STRING, ...])

Call pkg-config on the library specified by I<STRING> (you'll have to know what
to use here).  The returned I<HASH> contains the modversion, cflags, and libs
values under keys with those names. If multiple STRINGS are passed they are
attempted in the order they are given till a working package is found.

If pkg-config fails to find a working I<STRING>, this function croaks with a
message intended to be helpful to whomever is attempting to compile your
package.

For example:

  *** can not find package bad1
  *** check that it is properly installed and available
  *** in PKG_CONFIG_PATH

or

  *** can't find cflags for gtk+-2.0
  *** is it properly installed and available in PKG_CONFIG_PATH?

=item STRING = ExtUtils::PkgConfig->create_version_macros (PACKAGE, STEM)

Create a set of version macros with the prefix I<STEM> for the library
specified by I<PACKAGE>.  The result is returned.

Example input would be "gtk+-2.0" for I<PACKAGE> and "GTK" for I<STEM>.

=item ExtUtils::PkgConfig->write_version_macros (FILE, PACKAGE, STEM, [PACKAGE, STEM, ...])

Create one or more sets of version macros for the libraries and prefixes
specified by the I<PACKAGE> and I<STEM> pairs and write them to the file
I<FILE>.  If it doesn't exist, I<FILE> will be created.  If it does exist, it
will be overwritten.

=back

=head1 SEE ALSO

ExtUtils::PkgConfig was designed to work with ExtUtils::Depends for compiling
the various modules of the gtk2-perl project.

  L<ExtUtils::Depends>

  L<http://gtk2-perl.sourceforge.net/>

This module is really just an interface to the pkg-config utility program.
http://www.freedesktop.org/Software/pkgconfig

=head1 AUTHORS

muppet E<lt>scott at asofyet dot orgE<gt>.

=head1 COPYRIGHT AND LICENSE

Copyright 2003-2004, 2012-2013 by muppet, Ross McFarland, and the gtk2-perl
team

This library is free software; you can redistribute it and/or modify
it under the terms of the Lesser General Public License (LGPL).  For
more information, see http://www.fsf.org/licenses/lgpl.txt

=cut
