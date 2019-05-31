package File::HomeDir::Darwin;

use 5.008003;
use strict;
use warnings;
use Cwd                 ();
use Carp                ();
use File::HomeDir::Unix ();

use vars qw{$VERSION};
use base "File::HomeDir::Unix";

BEGIN
{
    $VERSION = '1.004';
}

#####################################################################
# Current User Methods

sub _my_home
{
    my ($class, $path) = @_;
    my $home = $class->my_home;
    return undef unless defined $home;

    my $folder = "$home/$path";
    unless (-d $folder)
    {
        # Make sure that symlinks resolve to directories.
        return undef unless -l $folder;
        my $dir = readlink $folder or return;
        return undef unless -d $dir;
    }

    return Cwd::abs_path($folder);
}

sub my_desktop
{
    my $class = shift;
    $class->_my_home('Desktop');
}

sub my_documents
{
    my $class = shift;
    $class->_my_home('Documents');
}

sub my_data
{
    my $class = shift;
    $class->_my_home('Library/Application Support');
}

sub my_music
{
    my $class = shift;
    $class->_my_home('Music');
}

sub my_pictures
{
    my $class = shift;
    $class->_my_home('Pictures');
}

sub my_videos
{
    my $class = shift;
    $class->_my_home('Movies');
}

#####################################################################
# Arbitrary User Methods

sub users_home
{
    my $class = shift;
    my $home  = $class->SUPER::users_home(@_);
    return defined $home ? Cwd::abs_path($home) : undef;
}

sub users_desktop
{
    my ($class, $name) = @_;
    return undef if $name eq 'root';
    $class->_to_user($class->my_desktop, $name);
}

sub users_documents
{
    my ($class, $name) = @_;
    return undef if $name eq 'root';
    $class->_to_user($class->my_documents, $name);
}

sub users_data
{
    my ($class, $name) = @_;
    $class->_to_user($class->my_data, $name)
      || $class->users_home($name);
}

# cheap hack ... not entirely reliable, perhaps, but ... c'est la vie, since
# there's really no other good way to do it at this time, that i know of -- pudge
sub _to_user
{
    my ($class, $path, $name) = @_;
    my $my_home    = $class->my_home;
    my $users_home = $class->users_home($name);
    defined $users_home or return undef;
    $path =~ s/^\Q$my_home/$users_home/;
    return $path;
}

1;

=pod

=head1 NAME

File::HomeDir::Darwin - Find your home and other directories on Darwin (OS X)

=head1 DESCRIPTION

This module provides Mac OS X specific file path for determining
common user directories in pure perl, by just using C<$ENV{HOME}>
without Carbon nor Cocoa API calls. In normal usage this module will
always be used via L<File::HomeDir>.

=head1 SYNOPSIS

  use File::HomeDir;
  
  # Find directories for the current user
  $home    = File::HomeDir->my_home;      # /Users/mylogin
  $desktop = File::HomeDir->my_desktop;   # /Users/mylogin/Desktop
  $docs    = File::HomeDir->my_documents; # /Users/mylogin/Documents
  $music   = File::HomeDir->my_music;     # /Users/mylogin/Music
  $pics    = File::HomeDir->my_pictures;  # /Users/mylogin/Pictures
  $videos  = File::HomeDir->my_videos;    # /Users/mylogin/Movies
  $data    = File::HomeDir->my_data;      # /Users/mylogin/Library/Application Support

=cut
