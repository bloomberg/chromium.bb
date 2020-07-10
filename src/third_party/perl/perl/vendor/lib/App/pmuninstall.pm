package App::pmuninstall;
use strict;
use warnings;
use File::Spec;
use File::Basename qw(dirname);
use Getopt::Long qw(GetOptions :config bundling);
use Config;
use YAML ();
use CPAN::DistnameInfo;
use version;
use HTTP::Tiny;
use Term::ANSIColor qw(colored);
use Cwd ();
use JSON::PP qw(decode_json);

our $VERSION = "0.30";

my $perl_version     = version->new($])->numify;
my $depended_on_by   = 'http://deps.cpantesters.org/depended-on-by.pl?dist=';
my $cpanmetadb       = 'http://cpanmetadb.plackperl.org/v1.0/package';
my @core_modules_dir = do { my %h; grep !$h{$_}++, @Config{qw/archlib archlibexp privlib privlibexp/} };

$ENV{ANSI_COLORS_DISABLED} = 1 if $^O eq 'MSWin32';

our $OUTPUT_INDENT_LEVEL = 0;

sub new {
    my ($class, $inc) = @_;
    $inc = [@INC] unless ref $inc eq 'ARRAY';
    bless {
        check_deps => 1,
        verbose    => 0,
        inc        => $class->prepare_include_paths($inc),
    }, $class;
}

sub run {
    my ($self, @args) = @_;
    local @ARGV = @args;
    GetOptions(
        'f|force'                 => \$self->{force},
        'v|verbose!'              => sub { ++$self->{verbose} },
        'c|checkdeps!'            => \$self->{check_deps},
        'n|no-checkdeps!'         => sub { $self->{check_deps} = 0 },
        'q|quiet!'                => \$self->{quiet},
        'h|help!'                 => sub { $self->usage },
        'V|version!'              => \$self->{version},
        'l|local-lib=s'           => \$self->{local_lib},
        'L|local-lib-contained=s' => sub {
            $self->{local_lib}      = $_[1];
            $self->{self_contained} = 1;
        },
    ) or $self->usage;

    if ($self->{version}) {
        $self->puts("pm-uninstall (App::pmuninstall) version $App::pmuninstall::VERSION");
        exit;
    }

    $self->short_usage unless @ARGV;

    $self->uninstall(@ARGV);
}

sub uninstall {
    my ($self, @modules) = @_;

    $self->setup_local_lib;

    my $uninstalled = 0;
    for my $module (@modules) {
        $self->puts("--> Working on $module") unless $self->{quiet};
        my ($packlist, $dist, $vname) = $self->find_packlist($module);

        $packlist = File::Spec->catfile($packlist);
        if ($self->is_core_module($module, $packlist)) {
            $self->puts(colored ['red'], "! $module is a core module!! Can't be uninstalled.");
            $self->puts unless $self->{quiet};
            next;
        }

        unless ($dist) {
            $self->puts(colored ['red'], "! $module not found.");
            $self->puts unless $self->{quiet};
            next;
        }

        unless ($packlist) {
            $self->puts(colored ['red'], "! $module is not installed.");
            $self->puts unless $self->{quiet};
            next;
        }

        if ($self->ask_permission($module, $dist, $vname, $packlist)) {
            if ($self->uninstall_from_packlist($packlist)) {
                $self->puts(colored ['green'], "Successfully uninstalled $module");
                ++$uninstalled;
            }
            else {
                $self->puts(colored ['red'], "! Failed to uninstall $module");
            }
            $self->puts unless $self->{quiet};
        }
    }

    if ($uninstalled) {
        $self->puts if $self->{quiet};
        $self->puts("You may want to rebuild man(1) entries. Try `mandb -c` if needed");
    }

    return $uninstalled;
}

sub uninstall_from_packlist {
    my ($self, $packlist) = @_;

    my $inc = {
        map { File::Spec->catfile($_) => 1 } @{$self->{inc}}
    };

    my $failed;
    for my $file ($self->fixup_packlist($packlist)) {
        chomp $file;
        $self->puts(-f $file ? 'unlink   ' : 'not found', " : $file") if $self->{verbose};
        unlink $file or $self->puts("$file: $!") and $failed++;
        $self->rm_empty_dir_from_file($file, $inc);
    }
    $self->puts("unlink    : $packlist") if $self->{verbose};
    unlink $packlist or $self->puts("$packlist: $!") and $failed++;
    $self->rm_empty_dir_from_file($packlist, $inc);

    if (my $install_json = $self->{install_json}) {
        $self->puts("unlink    : $install_json") if $self->{verbose};
        unlink $install_json or $self->puts("$install_json: $!") and $failed++;
        $self->rm_empty_dir_from_file($install_json);
    }

    $self->puts unless $self->{quiet} || $self->{force};
    return !$failed;
}

sub rm_empty_dir_from_file {
    my ($self, $file, $inc) = @_;
    my $dir = dirname $file;
    return unless -d $dir;
    return if $inc->{+File::Spec->catfile($dir)};

    my $failed;
    if ($self->is_empty_dir($dir)) {
        $self->puts("rmdir     : $dir") if $self->{verbose};
        rmdir $dir or $self->puts("$dir: $!") and $failed++;
        $self->rm_empty_dir_from_file($dir, $inc);
    }

    return !$failed;
}

sub is_empty_dir {
    my ($self, $dir) = @_;
    opendir my $dh, $dir or die "$dir: $!";
    my @dir = grep !/^\.{1,2}$/, readdir $dh;
    closedir $dh;
    return @dir ? 0 : 1;
}

sub find_packlist {
    my ($self, $module) = @_;
    $self->puts("Finding $module in your \@INC") if $self->{verbose};

    # find with the given name first
    (my $try_dist = $module) =~ s!::!-!g;
    if (my $pl = $self->locate_pack($try_dist)) {
        $self->puts("-> Found $pl") if $self->{verbose};
        return ($pl, $try_dist);
    }

    $self->puts("Looking up $module on cpanmetadb") if $self->{verbose};

    # map module -> dist and retry
    my $yaml = $self->fetch("$cpanmetadb/$module") or return;
    my $meta = YAML::Load($yaml);
    my $info = CPAN::DistnameInfo->new($meta->{distfile});

    my $name = $self->find_meta($info->distvname) || $info->dist;
    if (my $pl = $self->locate_pack($name)) {
        $self->puts("-> Found $pl") if $self->{verbose};
        return ($pl, $info->dist, $info->distvname);
    }
}

sub find_meta {
    my ($self, $distvname) = @_;

    my $name;
    for my $lib (@{$self->{inc}}) {
        next unless $lib =~ /$Config{archname}/;
        my $install_json = "$lib/.meta/$distvname/install.json";
        next unless -f $install_json && -r _;
        my $data = decode_json +$self->slurp($install_json);
        $name = $data->{name} || next;
        $self->puts("-> Found $install_json") if $self->{verbose};
        $self->{meta} = $install_json;
        last;
    }
    return $name;
}

sub locate_pack {
    my ($self, $dist) = @_;
    $dist =~ s!-!/!g;
    for my $lib (@{$self->{inc}}) {
        my $packlist = "$lib/auto/$dist/.packlist";
        $self->puts("-> Finding .packlist $packlist") if $self->{verbose} > 1;
        return $packlist if -f $packlist && -r _;
    }
    return;
}

sub is_core_module {
    my ($self, $dist, $packlist) = @_;
    require Module::CoreList;
    return unless exists $Module::CoreList::version{$perl_version}{$dist};
    return 1 unless $packlist;

    my $is_core = 0;
    for my $dir (@core_modules_dir) {
        my $safe_dir = quotemeta $dir; # workaround for MSWin32
        if ($packlist =~ /^$safe_dir/) {
            $is_core = 1;
            last;
        }
    }
    return $is_core;
}

sub ask_permission {
    my($self, $module, $dist, $vname, $packlist) = @_;

    my @deps = $self->find_deps($vname, $module);

    $self->puts if $self->{verbose};
    $self->puts("$module is included in the distribution $dist and contains:\n")
        unless $self->{quiet};
    for my $file ($self->fixup_packlist($packlist)) {
        chomp $file;
        $self->puts("  $file") unless $self->{quiet};
    }
    $self->puts unless $self->{quiet};

    return 'force uninstall' if $self->{force};

    my $default = 'y';
    if (@deps) {
        $self->puts("Also, they're depended on by the following installed dists:\n");
        for my $dep (@deps) {
            $self->puts("  $dep");
        }
        $self->puts;
        $default = 'n';
    }

    return lc($self->prompt("Are you sure you want to uninstall $dist?", $default)) eq 'y';
}

sub find_deps {
    my ($self, $vname, $module) = @_;

    return unless $self->{check_deps} && !$self->{force};
    $vname ||= $self->vname_for($module) or return;

    $self->puts("Checking modules depending on $vname") if $self->{verbose};
    my $content = $self->fetch("$depended_on_by$vname") or return;

    my (@deps, %seen);
    for my $dep ($content =~ m|<li><a href=[^>]+>([a-zA-Z0-9_:-]+)|smg) {
        $dep =~ s/^\s+|\s+$//smg; # trim
        next if $seen{$dep}++;
        local $OUTPUT_INDENT_LEVEL = $OUTPUT_INDENT_LEVEL + 1;
        $self->puts("Finding $dep in your \@INC (dependencies)") if $self->{verbose};
        push @deps, $dep if $self->locate_pack($dep);
    }

    return @deps;
}

sub prompt {
    my ($self, $msg, $default) = @_;
    require ExtUtils::MakeMaker;
    ExtUtils::MakeMaker::prompt($msg, $default);
}

sub fixup_packlist {
    my ($self, $packlist) = @_;
    my @target_list;
    my $is_local_lib = $self->is_local_lib($packlist);
    open my $in, "<", $packlist or die "$packlist: $!";
    while (defined (my $file = <$in>)) {
        if ($is_local_lib) {
            next unless $self->is_local_lib($file);
        }
        push @target_list, $file;
    }
    return @target_list;
}

sub is_local_lib {
    my ($self, $file) = @_;
    return unless $self->{local_lib};

    my $local_lib_base = quotemeta File::Spec->catfile(Cwd::realpath($self->{local_lib}));
    $file = File::Spec->catfile($file);

    return $file =~ /^$local_lib_base/ ? 1 : 0;
}

sub vname_for {
    my ($self, $module) = @_;

    $self->puts("Fetching $module vname on cpanmetadb") if $self->{verbose};
    my $yaml = $self->fetch("$cpanmetadb/$module") or return;
    my $meta = YAML::Load($yaml);
    my $info = CPAN::DistnameInfo->new($meta->{distfile}) or return;

    return $info->distvname;
}

# taken from cpan-outdated
sub setup_local_lib {
    my $self = shift;
    return unless $self->{local_lib};

    unless (-d $self->{local_lib}) {
        $self->puts(colored ['red'], "! $self->{local_lib} : no such directory");
        exit 1;
    }

    local $SIG{__WARN__} = sub { }; # catch 'Attempting to write ...'
    $self->{inc} = [
        map { Cwd::realpath($_) }
            @{$self->build_active_perl5lib($self->{local_lib}, $self->{self_contained})}
    ];
    push @{$self->{inc}}, @INC unless $self->{self_contained};
}

sub build_active_perl5lib {
    my ($self, $path, $interpolate) = @_;
    my $perl5libs = [
        $self->install_base_arch_path($path),
        $self->install_base_perl_path($path),
        $interpolate && $ENV{PERL5LIB} ? $ENV{PERL5LIB} : (),
    ];
    return $perl5libs;
}

sub install_base_perl_path {
    my ($self, $path) = @_;
    File::Spec->catdir($path, 'lib', 'perl5');
}

sub install_base_arch_path {
    my ($self, $path) = @_;
    File::Spec->catdir($self->install_base_perl_path($path), $Config{archname});
}

sub fetch {
    my ($self, $url) = @_;
    $self->puts("-> Fetching from $url") if $self->{verbose};
    my $res = HTTP::Tiny->new->get($url);
    return if $res->{status} == 404;
    die "[$res->{status}] fetch $url failed!!\n" if !$res->{success};
    return $res->{content};
}

sub slurp {
    my ($self, $file) = @_;
    open my $fh, '<', $file or die "$file $!";
    do { local $/; <$fh> };
}

sub puts {
    my ($self, @msg) = @_;
    push @msg, '' unless @msg;
    print '  ' x $OUTPUT_INDENT_LEVEL if $OUTPUT_INDENT_LEVEL;
    print @msg, "\n";
}

sub usage {
    my $self = shift;
    $self->puts(<< 'USAGE');
Usage:
      pm-uninstall [options] Module [...]

      options:
          -v,--verbose                  Turns on chatty output
          -f,--force                    Uninstalls without prompts
          -c,--checkdeps                Check dependencies (defaults to on)
          -n,--no-checkdeps             Don't check dependencies
          -q,--quiet                    Suppress some messages
          -h,--help                     This help message
          -V,--version                  Show version
          -l,--local-lib                Additional module path
          -L,--local-lib-contained      Additional module path (don't include non-core modules)
USAGE

    exit 1;
}

sub short_usage {
    my $self = shift;
    $self->puts(<< 'USAGE');
Usage: pm-uninstall [options] Module [...]

Try `pm-uninstall --help` or `man pm-uninstall` for more options.
USAGE

    exit 1;
}

sub prepare_include_paths {
    my ($class, $inc) = @_;
    my $new_inc = [];
    my $archname = quotemeta $Config{archname};
    for my $path (@$inc) {
        push @$new_inc, $path;
        next if $path eq '.' or $path =~ /$archname/;
        push @$new_inc, File::Spec->catdir($path, $Config{archname});
    }
    return [do { my %h; grep !$h{$_}++, @$new_inc }];
}

1;
__END__

=head1 NAME

App::pmuninstall - Uninstall modules

=head1 DESCRIPTION

App::pmuninstall is a fast module uninstaller.
delete files from B<.packlist>.

L<App::cpanminus> and, L<App::cpanoutdated> with a high affinity.

=head1 SYNOPSIS

uninstall MODULE

  $ pm-uninstall App::pmuninstall

=head1 OPTIONS

=over

=item -f, --force

Uninstalls without prompts

  $ pm-uninstall -f App::pmuninstall

=item -v, --verbose

Turns on chatty output

  $ pm-uninstall -v App::cpnaminus

=item -c, --checkdeps

Check dependencies ( default on )

  $ pm-uninstall -c Plack

=item -n, --no-checkdeps

Don't check dependencies

  $ pm-uninstall -n LWP

=item -q, --quiet

Suppress some messages

  $ pm-uninstall -q Furl

=item -h, --help

Show help message

  $ pm-uninstall -h

=item -V, --version

Show version

  $ pm-uninstall -V

=item -l, --local-lib

Additional module path

  $ pm-uninstall -l extlib App::pmuninstall

=item -L, --local-lib-contained

Additional module path (don't include non-core modules)

  $ pm-uninstall -L extlib App::pmuninstall

=back

=head1 AUTHOR

Yuji Shimada

Tatsuhiko Miyagawa

=head1 SEE ALSO

L<pm-uninstall>

=head1 LICENSE

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

