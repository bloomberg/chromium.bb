use 5.006;    # pragmas
use strict;
use warnings;

package Test::File::ShareDir;

our $VERSION = '1.001002';

# ABSTRACT: Create a Fake ShareDir for your modules for testing.

our $AUTHORITY = 'cpan:KENTNL'; # AUTHORITY












use File::ShareDir 1.00 qw();
use Exporter qw();
use Test::File::ShareDir::Utils qw( extract_dashes );
use Carp qw( croak );
use parent qw( Exporter );

our @EXPORT_OK = qw( with_dist_dir with_module_dir );

sub import {
  my ( $package, @args ) = @_;

  my ( @imports, %params );

  # ->import( {  }, qw( imports ) )
  if ( 'HASH' eq ref $args[0] ) {
    %params  = %{ shift @args };
    @imports = @args;
  }
  else {
    # ->import( -arg  => value, -arg => value, @imports );
    while (@args) {
      if ( $args[0] =~ /\A-(.*)\z/msx ) {
        $params{ $args[0] } = $args[1];
        splice @args, 0, 2, ();
        next;
      }
      push @imports, shift @args;
    }
  }

  if ( keys %params ) {
    require Test::File::ShareDir::TempDirObject;

    my $tempdir_object = Test::File::ShareDir::TempDirObject->new( \%params );

    for my $module ( $tempdir_object->_module_names ) {
      $tempdir_object->_install_module($module);
    }

    for my $dist ( $tempdir_object->_dist_names ) {
      $tempdir_object->_install_dist($dist);
    }

    unshift @INC, $tempdir_object->_tempdir->stringify;

  }
  if (@imports) {
    $package->export_to_level( 1, undef, @imports );
  }
  return;

}

# This code is just to make sure any guard objects
# are not lexically visible to the sub they contain creating a self reference.
sub _mk_clearer {
  my ($clearee) = @_;
  return sub { $clearee->clear };
}


























sub with_dist_dir {
  my ( $config, $code ) = @_;
  if ( 'CODE' ne ( ref $code || q{} ) ) {
    croak( 'CodeRef expected at end of with_dist_dir(), ' . ( ref $code || qq{scalar="$code"} ) . ' found' );
  }
  require Test::File::ShareDir::Object::Dist;
  require Scope::Guard;
  my $dist_object = Test::File::ShareDir::Object::Dist->new( extract_dashes( 'dists', $config ) );
  $dist_object->install_all_dists();
  $dist_object->register();
  my $guard = Scope::Guard->new( _mk_clearer($dist_object) );    ## no critic (Variables::ProhibitUnusedVarsStricter)
  return $code->();
}


























sub with_module_dir {
  my ( $config, $code ) = @_;
  if ( 'CODE' ne ( ref $code || q{} ) ) {
    croak( 'CodeRef expected at end of with_module_dir(), ' . ( ref $code || qq{scalar="$code"} ) . ' found' );
  }

  require Test::File::ShareDir::Object::Module;
  require Scope::Guard;

  my $module_object = Test::File::ShareDir::Object::Module->new( extract_dashes( 'modules', $config ) );

  $module_object->install_all_modules();
  $module_object->register();
  my $guard = Scope::Guard->new( _mk_clearer($module_object) );    ## no critic (Variables::ProhibitUnusedVarsStricter)

  return $code->();
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::File::ShareDir - Create a Fake ShareDir for your modules for testing.

=head1 VERSION

version 1.001002

=head1 SYNOPSIS

    use Test::More;

    # use FindBin; optional

    use Test::File::ShareDir
        # -root => "$FindBin::Bin/../" # optional,
        -share => {
            -module => { 'My::Module' => 'share/MyModule' }
            -dist   => { 'My-Dist'    => 'share/somefolder' }
        };

    use My::Module;

    use File::ShareDir qw( module_dir dist_dir );

    module_dir( 'My::Module' ) # dir with files from $dist/share/MyModule

    dist_dir( 'My-Dist' ) # dir with files from $dist/share/somefolder

=head1 DESCRIPTION

C<Test::File::ShareDir> is some low level plumbing to enable a distribution to perform tests while consuming its own C<share>
directories in a manner similar to how they will be once installed.

This allows C<File::ShareDir> to see the I<latest> version of content instead of simply whatever is installed on whichever target
system you happen to be testing on.

B<Note:> This module only has support for creating 'new' style share dirs and are NOT compatible with old File::ShareDirs.

For this reason, unless you have File::ShareDir 1.00 or later installed, this module will not be usable by you.

=head1 SIMPLE INTERFACE

Starting with version C<0.4.0>, there are a few extra interfaces you can use.

These will probably be more useful, and easier to grok, because they don't have a layer of
indirection in order to simultaneously support both C<Module> and C<Dist> C<ShareDir>'s.

=head2 Simple Exporter Interfaces

=head3 C<Test::File::ShareDir::Dist>

L<< C<Test::File::ShareDir::Dist>|Test::File::ShareDir::Dist >> provides a simple export interface
for making C<TempDir> C<ShareDir>'s from a given path:

    use Test::File::ShareDir::Dist { "Dist-Name" => "share/" };

This will automatically create a C<ShareDir> for C<Dist-Name> in a C<TempDir> based on the contents of C<CWD/share/>

See L<< C<Test::File::ShareDir::Dist>|Test::File::ShareDir::Dist >> for details.

=head3 C<Test::File::ShareDir::Module>

L<< C<Test::File::ShareDir::Module>|Test::File::ShareDir::Module >> provides a simple export interface
for making C<TempDir> C<ShareDir>'s from a given path:

    use Test::File::ShareDir::Module { "Module::Name" => "share/" };

This will automatically create a C<ShareDir> for C<Module::Name> in a C<TempDir> based on the contents of C<CWD/share/>

See L<< C<Test::File::ShareDir::Module>|Test::File::ShareDir::Module >> for details.

=head2 Simple Object Oriented Interfaces

=head3 C<Test::File::ShareDir::Object::Dist>

L<< C<Test::File::ShareDir::Object::Dist>|Test::File::ShareDir::Object::Dist >> provides a simple object oriented interface for
making C<TempDir> C<ShareDir>'s from a given path:

    use Test::File::ShareDir::Object::Dist;

    my $obj = Test::File::ShareDir::Object::Dist->new( dists => { "Dist-Name" => "share/" } );
    $obj->install_all_dists;
    $obj->register;

This will automatically create a C<ShareDir> for C<Dist-Name> in a C<TempDir> based on the contents of C<CWD/share/>

See L<< C<Test::File::ShareDir::Object::Dist>|Test::File::ShareDir::Object::Dist >> for details.

=head3 C<Test::File::ShareDir::Object::Module>

L<< C<Test::File::ShareDir::Object::Module>|Test::File::ShareDir::Object::Module >> provides a simple object oriented interface
for making C<TempDir> C<ShareDir>'s from a given path:

    use Test::File::ShareDir::Object::Module;

    my $obj = Test::File::ShareDir::Object::Module->new( modules => { "Module::Name" => "share/" } );
    $obj->install_all_modules;
    $obj->register;

This will automatically create a C<ShareDir> for C<Module::Name> in a C<TempDir> based on the contents of C<CWD/share/>

See L<< C<Test::File::ShareDir::Object::Module>|Test::File::ShareDir::Object::Module >> for details.

=head1 SCOPE LIMITED UTILITIES

C<Test::File::ShareDir> provides a few utility functions to aide in temporarily adjusting C<ShareDir> behavior.

    use Test::File::ShareDir qw( with_dist_dir with_module_dir );

    with_dist_dir({ 'Dist-Name' => 'Some/Path' }, sub {
      # dist_dir() now behaves differently here
    });
    with_module_dir({ 'Module::Name' => 'Some/Path' }, sub {
      # module_dir() now behaves differently here
    });

See L<< C<EXPORTABLE FUNCTIONS>|/EXPORTABLE FUNCTIONS >> for details.

=head1 IMPORTING

Since C<1.001000>, there are 2 ways of passing arguments to C<import>

  use Foo { -root => ... options }, qw( functions to import );
  use Foo -optname => option, -optname => option, qw( functions to import );

Both should work, but the former might be less prone to accidental issues.

=head2 IMPORT OPTIONS

=head3 -root

This parameter is the prefix the other paths are relative to.

If this parameter is not specified, it defaults to the Current Working Directory ( C<CWD> ).

In versions prior to C<0.3.0>, this value was mandatory.

The rationale behind using C<CWD> as the default value is as follows.

=over 4

=item * Most users of this module are likely to be using it to test distributions

=item * Most users of this module will be using it in C<$project/t/> to load files from C<$project/share/>

=item * Most C<CPAN> tools run tests with C<CWD> = $project

=back

Therefor, defaulting to C<CWD> is a reasonably sane default for most people, but where it is not it can
still be overridden.

  -root => "$FindBin::Bin/../" # resolves to project root from t/ regardless of Cwd.

=head3 -share

This parameter is mandatory, and contains a C<hashref> containing the data that explains what directories you want shared.

  -share =>  { ..... }

=head4 -module

C<-module> contains a C<hashref> mapping Module names to path names for module_dir style share dirs.

  -share => {
    -module => { 'My::Module' => 'share/mymodule/', }
  }

  ...

  module_dir('My::Module')

Notedly, it is a C<hashref>, which means there is a limitation of one share dir per module. This is simply because having more
than one share dir per module makes no sense at all.

=head4 -dist

C<-dist> contains a C<hashref> mapping Distribution names to path names for dist_dir style share dirs. The same limitation
applied to C<-module> applies here.

  -share => {
    -dist => { 'My-Dist' => 'share/mydist' }
  }
  ...
  dist_dir('My-Dist')

=head1 EXPORTABLE FUNCTIONS

=head2 with_dist_dir

Sets up a C<ShareDir> environment with limited context.

  # with_dist_dir(\%config, \&sub);
  with_dist_dir( { 'Dist-Name' => 'share/' } => sub {

      # File::ShareDir resolves to a copy of share/ in this context.

  } );

C<%config> can contain anything L<< C<Test::File::ShareDir::Dist>|Test::File::ShareDir::Dist >> accepts.

=over 4

=item C<-root>: Defaults to C<$CWD>

=item C<I<$distName>>: Declare C<$distName>'s C<ShareDir>.

=back

I<Since 1.001000>

=head2 with_module_dir

Sets up a C<ShareDir> environment with limited context.

  # with_module_dir(\%config, \&sub);
  with_module_dir( { 'Module::Name' => 'share/' } => sub {

      # File::ShareDir resolves to a copy of share/ in this context.

  } );

C<%config> can contain anything L<< C<Test::File::ShareDir::Module>|Test::File::ShareDir::Module >> accepts.

=over 4

=item C<-root>: Defaults to C<$CWD>

=item C<I<$moduleName>>: Declare C<$moduleName>'s C<ShareDir>.

=back

I<Since 1.001000>

=begin MetaPOD::JSON v1.1.0

{
    "namespace":"Test::File::ShareDir",
    "interface":"exporter"
}


=end MetaPOD::JSON

=head1 THANKS

Thanks to the C<#distzilla> crew for ideas,suggestions, code review and debugging, even though not all of it made it into releases.

=for stopwords DOLMEN ETHER HAARG RJBS

=over 4

=item * L<DOLMEN|cpan:///author/dolmen>

=item * L<ETHER|cpan:///author/ether>

=item * L<HAARG|cpan:///author/haarg>

=item * L<RJBS|cpan:///author/rjbs>

=back

=head1 AUTHOR

Kent Fredric <kentnl@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Kent Fredric <kentnl@cpan.org>.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
