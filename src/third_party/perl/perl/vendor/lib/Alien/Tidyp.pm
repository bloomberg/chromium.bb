package Alien::Tidyp;

use warnings;
use strict;

use Alien::Tidyp::ConfigData;
use File::ShareDir qw(dist_dir);
use File::Spec::Functions qw(catdir catfile rel2abs);

=head1 NAME

Alien::Tidyp - Building, finding and using tidyp library - L<http://www.tidyp.com>

=cut

our $VERSION = 'v1.4.7';

=head1 VERSION

Version 1.4.7 of Alien::Tidyp uses I<tidyp> sources v1.04.

=head1 SYNOPSIS

B<IMPORTANT:> This module is not a perl binding for I<tidyp> library; it is just a helper module.
The real perl binding is implemented by e.g L<HTML::Tidy|HTML::Tidy> module, which is able to
use Alien::Tidyp to locate I<tidyp> library on your system (or build it from source codes).

Alien::Tidyp tries (in given order) during its installation:

=over

=item * Locate an already installed I<tidyp> + ask user whether to use the already installed
I<tidyp> or whether to build I<tidyp> from sources

=item * Via env variable TIDYP_DIR you can specify where the build script should look
for the already installed I<tidyp> (directories $TIDYP_DIR/lib and $TIDYP_DIR/include/tidyp
are expected to exist)

=item * When not using the already installed I<tidyp> build process continues with
the following steps

=item * Download I<tidyp> source code tarball

=item * Build I<tidyp> binaries from source codes (note: static libraries are build in this case)

=item * Install binaries and dev files (*.h, *.a) into I<share> directory of Alien::Tidyp
distribution - I<share> directory is usually something like this: /usr/lib/perl5/site_perl/5.10/auto/share/dist/Alien-Tidyp

=back

Later on you can use Alien::Tidyp in your module that needs to link with
I<tidyp> like this:

 # Sample Makefile.pl
 use ExtUtils::MakeMaker;
 use Alien::Tidyp;

 WriteMakefile(
   NAME         => 'Any::Tidyp::Module',
   VERSION_FROM => 'lib/Any/Tidyp/Module.pm',
   LIBS         => Alien::Tidyp->config('LIBS'),
   INC          => Alien::Tidyp->config('INC'),
   # + additional params
 );

B<IMPORTANT:> As Alien::Tidyp builds static libraries the modules using Alien::Tidyp (e.g. L<HTML::Tidy|HTML::Tidy>)
need to have Alien::Tidyp just for building, not for later use. In other words Alien:Tidyp is just "build dependency"
not "run-time dependency".

=head1 Build.PL options

=head2 --srctarball=<url_or_filename>

This option might come handy if you are not connected to the Internet. You can use it like:

 Build.PL --srctarball=/path/to/file/tidyp-1.04.tar.gz

or:

 Build.PL --srctarball=http://any.server.com/path/to/tidyp-1.04.tar.gz

IMPORTANT: The file should always be exactly the same source code tarball as specified in the end of Build.PL - see
source code (SHA1 checksum of the tarball is checked).
 
=head1 METHODS

=head2 config()

This function is the main public interface to this module.

 Alien::Tidyp->config('LIBS');

Returns a string like: '-L/path/to/tidyp/dir/lib -ltidyp'

 Alien::Tidyp->config('INC');

Returns a string like: '-I/path/to/tidyp/dir/include/tidyp'

 Alien::Tidyp->config('PREFIX');

Returns a string like: '/path/to/tidyp/dir' (note: if using the already installed
tidyp config('PREFIX') returns undef)

=head1 AUTHOR

KMX, E<lt>kmx at cpan.orgE<gt>

=head1 BUGS

Please report any bugs or feature requests to E<lt>bug-Alien-Tidyp at rt.cpan.orgE<gt>, or through
the web interface at L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Alien-Tidyp>.

=head1 LICENSE AND COPYRIGHT

Please notice that the source code of tidyp library has a different license than module itself.

=head2 Alien::Tidyp perl module

Copyright (c) 2010 KMX.

This program is free software; you can redistribute it and/or modify it
under the terms of either: the GNU General Public License as published
by the Free Software Foundation; or the Artistic License.

See http://dev.perl.org/licenses/ for more information.

=head2 Source code of tidyp library

Copyright (c) 1998-2003 World Wide Web Consortium
(Massachusetts Institute of Technology, European Research
Consortium for Informatics and Mathematics, Keio University).
All Rights Reserved.

This software and documentation is provided "as is," and
the copyright holders and contributing author(s) make no
representations or warranties, express or implied, including
but not limited to, warranties of merchantability or fitness
for any particular purpose or that the use of the software or
documentation will not infringe any third party patents,
copyrights, trademarks or other rights.

The copyright holders and contributing author(s) will not be held
liable for any direct, indirect, special or consequential damages
arising out of any use of the software or documentation, even if
advised of the possibility of such damage.

Permission is hereby granted to use, copy, modify, and distribute
this source code, or portions hereof, documentation and executables,
for any purpose, without fee, subject to the following restrictions:

1. The origin of this source code must not be misrepresented.

2. Altered versions must be plainly marked as such and must not be misrepresented as being the original source.

3. This Copyright notice may not be removed or altered from any  source or altered source distribution.

The copyright holders and contributing author(s) specifically
permit, without fee, and encourage the use of this source code
as a component for supporting the Hypertext Markup Language in
commercial products. If you use this source code in a product,
acknowledgment is not required but would be appreciated.

=cut

sub config
{
  my ($package, $param) = @_;
  return unless ($param =~ /[a-z0-9_]*/i);
  my $subdir = Alien::Tidyp::ConfigData->config('share_subdir');
  unless ($subdir) {
    #we are using tidyp already installed on your system not compiled by Alien::Tidyp
    #therefore no additinal magic needed
    return Alien::Tidyp::ConfigData->config('config')->{$param};
  }
  my $share_dir = dist_dir('Alien-Tidyp');
  my $real_prefix = catdir($share_dir, $subdir);
  my $val = Alien::Tidyp::ConfigData->config('config')->{$param};
  return unless $val;
  $val =~ s/\@PrEfIx\@/$real_prefix/g; # handle @PrEfIx@ replacement
  return $val;
}

1;
