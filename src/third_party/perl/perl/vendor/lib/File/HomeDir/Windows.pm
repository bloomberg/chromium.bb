package File::HomeDir::Windows;

# See POD at the end of the file for documentation

use 5.00503;
use strict;
use Carp                  ();
use File::Spec            ();
use File::HomeDir::Driver ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.99';
	@ISA     = 'File::HomeDir::Driver';
}

sub CREATE () { 1 }





#####################################################################
# Current User Methods

sub my_home {
	my $class = shift;

	# A lot of unix people and unix-derived tools rely on
	# the ability to overload HOME. We will support it too
	# so that they can replace raw HOME calls with File::HomeDir.
	if ( exists $ENV{HOME} and $ENV{HOME} ) {
		return $ENV{HOME};
	}

	# Do we have a user profile?
	if ( exists $ENV{USERPROFILE} and $ENV{USERPROFILE} ) {
		return $ENV{USERPROFILE};
	}

	# Some Windows use something like $ENV{HOME}
	if ( exists $ENV{HOMEDRIVE} and exists $ENV{HOMEPATH} and $ENV{HOMEDRIVE} and $ENV{HOMEPATH} ) {
		return File::Spec->catpath(
			$ENV{HOMEDRIVE}, $ENV{HOMEPATH}, '',
		);
	}

	return undef;
}

sub my_desktop {
	my $class = shift;

	# The most correct way to find the desktop
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_DESKTOP(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	# MSWindows sets WINDIR, MS WinNT sets USERPROFILE.
	foreach my $e ( 'USERPROFILE', 'WINDIR' ) {
		next unless $ENV{$e};
		my $desktop = File::Spec->catdir($ENV{$e}, 'Desktop');
		return $desktop if $desktop and $class->_d($desktop);
	}

	# As a last resort, try some hard-wired values
	foreach my $fixed (
		# The reason there are both types of slash here is because
		# this set of paths has been kept from thethe original version
		# of File::HomeDir::Win32 (before it was rewritten).
		# I can only assume this is Cygwin-related stuff.
		"C:\\windows\\desktop",
		"C:\\win95\\desktop",
		"C:/win95/desktop",
		"C:/windows/desktop",
	) {
		return $fixed if $class->_d($fixed);
	}

	return undef;
}

sub my_documents {
	my $class = shift;

	# The most correct way to find my documents
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_PERSONAL(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	return undef;
}

sub my_data {
	my $class = shift;

	# The most correct way to find my documents
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_LOCAL_APPDATA(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	return undef;
}

sub my_music {
	my $class = shift;

	# The most correct way to find my music
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYMUSIC(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	return undef;
}

sub my_pictures {
	my $class = shift;

	# The most correct way to find my pictures
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYPICTURES(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	return undef;
}

sub my_videos {
	my $class = shift;

	# The most correct way to find my videos
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYVIDEO(), CREATE);
		return $dir if $dir and $class->_d($dir);
	}

	return undef;
}

# Special case version of -d
sub _d {
	my $self = shift;
	my $path = shift;

	# Window can legally return a UNC path from GetFolderPath.
	# Not only is the meaning of -d complicated in this situation,
	# but even on a local network calling -d "\\\\cifs\\path" can
	# take several seconds. UNC can also do even weirder things,
	# like launching processes and such.
	# To avoid various crazy bugs caused by this, we do NOT attempt
	# to validate UNC paths at all so that the code that is calling
	# us has an opportunity to take special actions without our 
	# blundering getting in the way.
	if ( $path =~ /\\\\/ ) {
		return 1;
	}

	# Otherwise do a stat as normal
	return -d $path;
}

1;

=pod

=head1 NAME

File::HomeDir::Windows - Find your home and other directories on Windows

=head1 SYNOPSIS

  use File::HomeDir;
  
  # Find directories for the current user (eg. using Windows XP Professional)
  $home    = File::HomeDir->my_home;        # C:\Documents and Settings\mylogin
  $desktop = File::HomeDir->my_desktop;     # C:\Documents and Settings\mylogin\Desktop
  $docs    = File::HomeDir->my_documents;   # C:\Documents and Settings\mylogin\My Documents
  $music   = File::HomeDir->my_music;       # C:\Documents and Settings\mylogin\My Documents\My Music
  $pics    = File::HomeDir->my_pictures;    # C:\Documents and Settings\mylogin\My Documents\My Pictures
  $videos  = File::HomeDir->my_videos;      # C:\Documents and Settings\mylogin\My Documents\My Video
  $data    = File::HomeDir->my_data;        # C:\Documents and Settings\mylogin\Local Settings\Application Data

=head1 DESCRIPTION

This module provides Windows-specific implementations for determining
common user directories.  In normal usage this module will always be
used via L<File::HomeDir>.

Internally this module will use L<Win32>::GetFolderPath to fetch the location
of your directories. As a result of this, in certain unusual situations
(usually found inside large organisations) the methods may return UNC paths
such as C<\\cifs.local\home$>.

If your application runs on Windows and you want to have it work comprehensively
everywhere, you may need to implement your own handling for these paths as they
can cause strange behaviour.

For example, stat calls to UNC paths may work but block for several seconds, but
opendir() may not be able to read any files (creating the appearance of an existing
but empty directory).

To avoid complicating the problem any further, in the rare situation that a UNC path
is returned by C<GetFolderPath> the usual -d validation checks will B<not> be done.

=head1 SUPPORT

See the support section the main L<File::HomeDir> module.

=head1 AUTHORS

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

Sean M. Burke E<lt>sburke@cpan.orgE<gt>

=head1 SEE ALSO

L<File::HomeDir>, L<File::HomeDir::Win32> (legacy)

=head1 COPYRIGHT

Copyright 2005 - 2011 Adam Kennedy.

Some parts copyright 2000 Sean M. Burke.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
