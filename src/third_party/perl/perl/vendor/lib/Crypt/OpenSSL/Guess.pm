package Crypt::OpenSSL::Guess;
use 5.008001;
use strict;
use warnings;

our $VERSION = "0.11";

use File::Spec;
use Config;
use Symbol qw(gensym);

use Exporter 'import';

our @EXPORT = qw(openssl_inc_paths openssl_lib_paths find_openssl_prefix find_openssl_exec openssl_version);

sub openssl_inc_paths {
    my $prefix = find_openssl_prefix();
    my $exec   = find_openssl_exec($prefix);

    return '' unless -x $exec;

    my @inc_paths;
    for ("$prefix/include", "$prefix/inc32", '/usr/kerberos/include') {
        push @inc_paths, $_ if -f "$_/openssl/ssl.h";
    }

    return join ' ', map { "-I$_" } @inc_paths;
}

sub openssl_lib_paths {
    my $prefix = find_openssl_prefix();
    my $exec   = find_openssl_exec($prefix);

    return '' unless -x $exec;

    my @lib_paths;
    for ($prefix, "$prefix/lib64", "$prefix/lib", "$prefix/out32dll") {
        push @lib_paths, $_ if -d $_;
    }

    if ($^O eq 'MSWin32') {
        push @lib_paths, "$prefix/lib/VC" if -d "$prefix/lib/VC";

        my $found = 0;
        my @pairs = ();
        # Library names depend on the compiler
        @pairs = (['eay32','ssl32'],['crypto.dll','ssl.dll'],['crypto','ssl']) if $Config{cc} =~ /gcc/;
        @pairs = (['libeay32','ssleay32'],['libeay32MD','ssleay32MD'],['libeay32MT','ssleay32MT']) if $Config{cc} =~ /cl/;
        for my $dir (@lib_paths) {
            for my $p (@pairs) {
                $found = 1 if ($Config{cc} =~ /gcc/ && -f "$dir/lib$p->[0].a" && -f "$dir/lib$p->[1].a");
                $found = 1 if ($Config{cc} =~ /cl/ && -f "$dir/$p->[0].lib" && -f "$dir/p->[1].lib");
                if ($found) {
                    @lib_paths = ($dir);
                    last;
                }
            }
        }
    }
    elsif ($^O eq 'VMS') {
        if (-r 'sslroot:[000000]openssl.cnf') {      # openssl.org source install
            @lib_paths = ('SSLLIB');
        }
        elsif (-r 'ssl$root:[000000]openssl.cnf') {  # HP install
            @lib_paths = ('SYS$SHARE');
        }
    }

    return join ' ', map { "-L$_" } @lib_paths;
}

my $other_try = 0;
my @nopath;
sub check_no_path {            # On OS/2 it would be typically on default paths
    my $p;
    if (not($other_try++) and $] >= 5.008001) {
       use ExtUtils::MM;
       my $mm = MM->new();
       my ($list) = $mm->ext("-lssl");
       return unless $list =~ /-lssl\b/;
        for $p (split /\Q$Config{path_sep}/, $ENV{PATH}) {
           @nopath = ("$p/openssl$Config{_exe}",       # exe name
                      '.')             # dummy lib path
               if -x "$p/openssl$Config{_exe}"
       }
    }
    @nopath;
}

sub find_openssl_prefix {
    my ($dir) = @_;

    if (defined $ENV{OPENSSL_PREFIX}) {
        return $ENV{OPENSSL_PREFIX};
    }

    my @guesses = (
        '/home/linuxbrew/.linuxbrew/opt/openssl/bin/openssl' => '/home/linuxbrew/.linuxbrew/opt/openssl', # LinuxBrew openssl
        '/usr/local/opt/openssl/bin/openssl' => '/usr/local/opt/openssl', # OSX homebrew openssl
        '/usr/local/bin/openssl'         => '/usr/local', # OSX homebrew openssl
        '/opt/local/bin/openssl'         => '/opt/local', # Macports openssl
        '/usr/bin/openssl'               => '/usr',
        '/usr/sbin/openssl'              => '/usr',
        '/opt/ssl/bin/openssl'           => '/opt/ssl',
        '/opt/ssl/sbin/openssl'          => '/opt/ssl',
        '/usr/local/ssl/bin/openssl'     => '/usr/local/ssl',
        '/usr/local/openssl/bin/openssl' => '/usr/local/openssl',
        '/apps/openssl/std/bin/openssl'  => '/apps/openssl/std',
        '/usr/sfw/bin/openssl'           => '/usr/sfw', # Open Solaris
        'C:\OpenSSL\bin\openssl.exe'     => 'C:\OpenSSL',
        'C:\OpenSSL-Win32\bin\openssl.exe'        => 'C:\OpenSSL-Win32',
        $Config{prefix} . '\bin\openssl.exe'      => $Config{prefix},           # strawberry perl
        $Config{prefix} . '\..\c\bin\openssl.exe' => $Config{prefix} . '\..\c', # strawberry perl
        '/sslexe/openssl.exe'            => '/sslroot',  # VMS, openssl.org
        '/ssl$exe/openssl.exe'           => '/ssl$root', # VMS, HP install
    );

    while (my $k = shift @guesses
           and my $v = shift @guesses) {
        if ( -x $k ) {
            return $v;
        }
    }
    (undef, $dir) = check_no_path()
       and return $dir;

    return;
}

sub find_openssl_exec {
    my ($prefix) = @_;

    my $exe_path;
    for my $subdir (qw( bin sbin out32dll ia64_exe alpha_exe )) {
        my $path = File::Spec->catfile($prefix, $subdir, "openssl$Config{_exe}");
        if ( -x $path ) {
            return $path;
        }
    }
    ($prefix) = check_no_path()
       and return $prefix;
    return;
}

sub openssl_version {
    my ($major, $minor, $letter);

    my $prefix = find_openssl_prefix();
    my $exec   = find_openssl_exec($prefix);

    return unless -x $exec;

    {
        my $pipe = gensym();
        open($pipe, qq{"$exec" version |})
            or die "Could not execute $exec";
        my $output = <$pipe>;
        chomp $output;
        close $pipe;

        if ( ($major, $minor, $letter) = $output =~ /^OpenSSL\s+(\d+\.\d+)\.(\d+)([a-z]?)/ ) {
        } elsif ( ($major, $minor) = $output =~ /^LibreSSL\s+(\d+\.\d+)\.(\d+)/ ) {
        } else {
            die <<EOM
*** OpenSSL version test failed
    (`$output' has been returned)
    Either you have bogus OpenSSL or a new version has changed the version
    number format. Please inform the authors!
EOM
        }
    }

    return ($major, $minor, $letter);
}

1;
__END__

=encoding utf-8

=head1 NAME

Crypt::OpenSSL::Guess - Guess OpenSSL include path

=head1 SYNOPSIS

    use ExtUtils::MakerMaker;
    use Crypt::OpenSSL::Guess;

    WriteMakefile(
        # ...
        LIBS => ['-lssl -lcrypto ' . openssl_lib_paths()],
        INC  => openssl_inc_paths(), # guess include path or get from $ENV{OPENSSL_PREFIX}
    );

=head1 DESCRIPTION

Crypt::OpenSSL::Guess provides helpers to guess OpenSSL include path on any platforms.

Often MacOS's homebrew OpenSSL cause a problem on installation due to include path is not added.
Some CPAN module provides to modify include path with configure-args, but L<Carton> or L<Module::CPANfile>
is not supported to pass configure-args to each modules. Crypt::OpenSSL::* modules should use it on your L<Makefile.PL>.

This module resolves the include path by L<Net::SSLeay>'s workaround.
Original code is taken from C<inc/Module/Install/PRIVATE/Net/SSLeay.pm> by L<Net::SSLeay>.

=head1 FUNCTIONS

=over 4

=item openssl_inc_paths()

This functions returns include paths in the format passed to CC. If OpenSSL could not find, then empty string is returned.

    openssl_inc_paths(); # on MacOS: "-I/usr/local/opt/openssl/include"

=item openssl_lib_paths()

This functions returns library paths in the format passed to CC. If OpenSSL could not find, then empty string is returned.

    openssl_lib_paths(); # on MacOS: "-L/usr/local/opt/openssl -L/usr/local/opt/openssl/lib"

=item find_openssl_prefix([$dir])

This function returns OpenSSL's prefix. If set C<OPENSSL_PREFIX> environment variable, you can overwrite the return value.

    find_openssl_prefix(); # on MacOS: "/usr/local/opt/openssl"

=item find_openssl_exec($prefix)

This functions returns OpenSSL's executable path.

    find_openssl_exec(); # on MacOS: "/usr/local/opt/openssl/bin/openssl"

=item ($major, $minor, $letter) = openssl_version()

This functions returns OpenSSL's version as major, minor, letter.

    openssl_version(); # ("1.0", "2", "n")

=back

=head1 SEE ALSO

L<Net::SSLeay>

=head1 LICENSE

Copyright (C) Takumi Akiyama.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Takumi Akiyama E<lt>t.akiym@gmail.comE<gt>

=cut

