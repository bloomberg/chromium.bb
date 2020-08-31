package Config::Any;

use strict;
use warnings;

use Carp;
use Module::Pluggable::Object ();

our $VERSION = '0.32';

=head1 NAME

Config::Any - Load configuration from different file formats, transparently

=head1 SYNOPSIS

    use Config::Any;

    my $cfg = Config::Any->load_stems({stems => \@filepath_stems, ... });
    # or
    my $cfg = Config::Any->load_files({files => \@filepaths, ... });

    for (@$cfg) {
        my ($filename, $config) = %$_;
        $class->config($config);
        warn "loaded config from file: $filename";
    }

=head1 DESCRIPTION

L<Config::Any|Config::Any> provides a facility for Perl applications and libraries
to load configuration data from multiple different file formats. It supports XML, YAML,
JSON, Apache-style configuration, Windows INI files, and even Perl code.

The rationale for this module is as follows: Perl programs are deployed on many different
platforms and integrated with many different systems. Systems administrators and end
users may prefer different configuration formats than the developers. The flexibility
inherent in a multiple format configuration loader allows different users to make
different choices, without generating extra work for the developers. As a developer
you only need to learn a single interface to be able to use the power of different
configuration formats.

=head1 INTERFACE

=head2 load_files( \%args )

    Config::Any->load_files( { files => \@files } );
    Config::Any->load_files( { files => \@files, filter  => \&filter } );
    Config::Any->load_files( { files => \@files, use_ext => 1 } );
    Config::Any->load_files( { files => \@files, flatten_to_hash => 1 } );

C<load_files()> attempts to load configuration from the list of files passed in
the C<files> parameter, if the file exists.

If the C<filter> parameter is set, it is used as a callback to modify the configuration
data before it is returned. It will be passed a single hash-reference parameter which
it should modify in-place.

If the C<use_ext> parameter is defined, the loader will attempt to parse the file
extension from each filename and will skip the file unless it matches a standard
extension for the loading plugins. Only plugins whose standard extensions match the
file extension will be used. For efficiency reasons, its use is encouraged, but
be aware that you will lose flexibility -- for example, a file called C<myapp.cfg>
containing YAML data will not be offered to the YAML plugin, whereas C<myapp.yml>
or C<myapp.yaml> would be.

When the C<flatten_to_hash> parameter is defined, the loader will return a hash
keyed on the file names, as opposed to the usual list of single-key hashes.

C<load_files()> also supports a 'force_plugins' parameter, whose value should be an
arrayref of plugin names like C<Config::Any::INI>. Its intended use is to allow the use
of a non-standard file extension while forcing it to be offered to a particular parser.
It is not compatible with 'use_ext'.

You can supply a C<driver_args> hashref to pass special options to a particular
parser object. Example:

    Config::Any->load_files( { files => \@files, driver_args => {
        General => { -LowerCaseNames => 1 }
    } )

=cut

sub load_files {
    my ( $class, $args ) = @_;

    unless ( $args && exists $args->{ files } ) {
        warn "No files specified!";
        return;
    }

    return $class->_load( $args );
}

=head2 load_stems( \%args )

    Config::Any->load_stems( { stems => \@stems } );
    Config::Any->load_stems( { stems => \@stems, filter  => \&filter } );
    Config::Any->load_stems( { stems => \@stems, use_ext => 1 } );
    Config::Any->load_stems( { stems => \@stems, flatten_to_hash => 1 } );

C<load_stems()> attempts to load configuration from a list of files which it generates
by combining the filename stems list passed in the C<stems> parameter with the
potential filename extensions from each loader, which you can check with the
C<extensions()> classmethod described below. Once this list of possible filenames is
built it is treated exactly as in C<load_files()> above, as which it takes the same
parameters. Please read the C<load_files()> documentation before using this method.

=cut

sub load_stems {
    my ( $class, $args ) = @_;

    unless ( $args && exists $args->{ stems } ) {
        warn "No stems specified!";
        return;
    }

    my $stems = delete $args->{ stems };
    my @files;
    for my $s ( @$stems ) {
        for my $ext ( $class->extensions ) {
            push @files, "$s.$ext";
        }
    }

    $args->{ files } = \@files;
    return $class->_load( $args );
}

sub _load {
    my ( $class, $args ) = @_;
    croak "_load requires a arrayref of file paths" unless $args->{ files };

    my $force = defined $args->{ force_plugins };
    if ( !$force and !defined $args->{ use_ext } ) {
        warn
            "use_ext argument was not explicitly set, as of 0.09, this is true by default";
        $args->{ use_ext } = 1;
    }

    # figure out what plugins we're using
    my @plugins = $force
        ? map { eval "require $_;"; $_; } @{ $args->{ force_plugins } }
        : $class->plugins;

    # map extensions if we have to
    my ( %extension_lut, $extension_re );
    my $use_ext_lut = !$force && $args->{ use_ext };
    if ( $use_ext_lut ) {
        for my $plugin ( @plugins ) {
            for ( $plugin->extensions ) {
                $extension_lut{ $_ } ||= [];
                push @{ $extension_lut{ $_ } }, $plugin;
            }
        }

        $extension_re = join( '|', keys %extension_lut );
    }

    # map args to plugins
    my $base_class = __PACKAGE__;
    my %loader_args;
    for my $plugin ( @plugins ) {
        $plugin =~ m{^$base_class\::(.+)};
        $loader_args{ $plugin } = $args->{ driver_args }->{ $1 } || {};
    }

    my @results;

    for my $filename ( @{ $args->{ files } } ) {

        # don't even bother if it's not there
        next unless -f $filename;

        my @try_plugins = @plugins;

        if ( $use_ext_lut ) {
            $filename =~ m{\.($extension_re)\z};

            if ( !$1 ) {
                $filename =~ m{\.([^.]+)\z};
                croak "There are no loaders available for .${1} files";
            }

            @try_plugins = @{ $extension_lut{ $1 } };
        }

        # not using use_ext means we try all plugins anyway, so we'll
        # ignore it for the "unsupported" error
        my $supported = $use_ext_lut ? 0 : 1;
        for my $loader ( @try_plugins ) {
            next unless $loader->is_supported;
            $supported = 1;
            my @configs;
            my $err = do {
                local $@;
                @configs = eval { $loader->load( $filename, $loader_args{ $loader } ); };
                $@;
            };

            # fatal error if we used extension matching
            croak "Error parsing $filename: $err" if $err and $use_ext_lut;
            next if $err or !@configs;

            # post-process config with a filter callback
            if ( $args->{ filter } ) {
                $args->{ filter }->( $_ ) for @configs;
            }

            push @results,
                { $filename => @configs == 1 ? $configs[ 0 ] : \@configs };
            last;
        }

        if ( !$supported ) {
            croak
                "Cannot load $filename: required support modules are not available.\nPlease install "
                . join( " OR ", map { _support_error( $_ ) } @try_plugins );
        }
    }

    if ( defined $args->{ flatten_to_hash } ) {
        my %flattened = map { %$_ } @results;
        return \%flattened;
    }

    return \@results;
}

sub _support_error {
    my $module = shift;
    if ( $module->can( 'requires_all_of' ) ) {
        return join( ' and ',
            map { ref $_ ? join( ' ', @$_ ) : $_ } $module->requires_all_of );
    }
    if ( $module->can( 'requires_any_of' ) ) {
        return 'one of '
            . join( ' or ',
            map { ref $_ ? join( ' ', @$_ ) : $_ } $module->requires_any_of );
    }
}

=head2 finder( )

The C<finder()> classmethod returns the
L<Module::Pluggable::Object|Module::Pluggable::Object>
object which is used to load the plugins. See the documentation for that module for
more information.

=cut

sub finder {
    my $class  = shift;
    my $finder = Module::Pluggable::Object->new(
        search_path => [ __PACKAGE__ ],
        except      => [ __PACKAGE__ . '::Base' ],
        require     => 1
    );
    return $finder;
}

=head2 plugins( )

The C<plugins()> classmethod returns the names of configuration loading plugins as
found by L<Module::Pluggable::Object|Module::Pluggable::Object>.

=cut

sub plugins {
    my $class = shift;

    # filter out things that don't look like our plugins
    return grep { $_->isa( 'Config::Any::Base' ) } $class->finder->plugins;
}

=head2 extensions( )

The C<extensions()> classmethod returns the possible file extensions which can be loaded
by C<load_stems()> and C<load_files()>. This may be useful if you set the C<use_ext>
parameter to those methods.

=cut

sub extensions {
    my $class = shift;
    my @ext
        = map { $_->extensions } $class->plugins;
    return wantarray ? @ext : \@ext;
}

=head1 DIAGNOSTICS

=over

=item C<No files specified!> or C<No stems specified!>

The C<load_files()> and C<load_stems()> methods will issue this warning if
called with an empty list of files/stems to load.

=item C<_load requires a arrayref of file paths>

This fatal error will be thrown by the internal C<_load> method. It should not occur
but is specified here for completeness. If your code dies with this error, please
email a failing test case to the authors below.

=back

=head1 CONFIGURATION AND ENVIRONMENT

Config::Any requires no configuration files or environment variables.

=head1 DEPENDENCIES

L<Module::Pluggable::Object|Module::Pluggable::Object>

And at least one of the following:
L<Config::General|Config::General>
L<Config::Tiny|Config::Tiny>
L<JSON|JSON>
L<YAML|YAML>
L<JSON::Syck|JSON::Syck>
L<YAML::Syck|YAML::Syck>
L<XML::Simple|XML::Simple>

=head1 INCOMPATIBILITIES

None reported.

=head1 BUGS AND LIMITATIONS

No bugs have been reported.

Please report any bugs or feature requests to
C<bug-config-any@rt.cpan.org>, or through the web interface at
L<https://rt.cpan.org/Public/Dist/Display.html?Name=Config-Any>.

=head1 AUTHOR

Joel Bernstein <rataxis@cpan.org>

=head1 CONTRIBUTORS

This module was based on the original
L<Catalyst::Plugin::ConfigLoader|Catalyst::Plugin::ConfigLoader>
module by Brian Cassidy C<< <bricas@cpan.org> >>.

With ideas and support from Matt S Trout C<< <mst@shadowcatsystems.co.uk> >>.

Further enhancements suggested by Evan Kaufman C<< <evank@cpan.org> >>.

=head1 LICENCE AND COPYRIGHT

Copyright (c) 2006, Portugal Telecom C<< http://www.sapo.pt/ >>. All rights reserved.
Portions copyright 2007, Joel Bernstein C<< <rataxis@cpan.org> >>.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself. See L<perlartistic>.

=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER
EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE SOFTWARE IS WITH
YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL
NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL,
OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE
THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGES.

=head1 SEE ALSO

L<Catalyst::Plugin::ConfigLoader|Catalyst::Plugin::ConfigLoader>
-- now a wrapper around this module.

=cut

"Drink more beer";
