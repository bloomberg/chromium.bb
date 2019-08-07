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
#!/usr/bin/perl -w
#line 15
#========================================================================
#
# ttree
#
# DESCRIPTION
#   Script for processing all directory trees containing templates.
#   Template files are processed and the output directed to the 
#   relvant file in an output tree.  The timestamps of the source and
#   destination files can then be examined for future invocations 
#   to process only those files that have changed.  In other words,
#   it's a lot like 'make' for templates.
#
# AUTHOR
#   Andy Wardley   <abw@wardley.org>
#
# COPYRIGHT
#   Copyright (C) 1996-2003 Andy Wardley.  All Rights Reserved.
#   Copyright (C) 1998-2003 Canon Research Centre Europe Ltd.
#
#   This module is free software; you can redistribute it and/or
#   modify it under the same terms as Perl itself.
#
#------------------------------------------------------------------------
#
# $Id$
#
#========================================================================

use strict;
use Template;
use AppConfig qw( :expand );
use File::Copy;
use File::Path;
use File::Spec;
use File::Basename;
use Text::ParseWords qw(quotewords);

my $NAME     = "ttree";
my $VERSION  = 2.90;
my $HOME     = $ENV{ HOME } || '';
my $RCFILE   = $ENV{"\U${NAME}rc"} || "$HOME/.${NAME}rc";
my $TTMODULE = 'Template';

#------------------------------------------------------------------------
# configuration options
#------------------------------------------------------------------------

# offer create a sample config file if it doesn't exist, unless a '-f'
# has been specified on the command line
unless (-f $RCFILE or grep(/^(-f|-h|--help)$/, @ARGV) ) {
    print("Do you want me to create a sample '.ttreerc' file for you?\n",
      "(file: $RCFILE)   [y/n]: ");
    my $y = <STDIN>;
    if ($y =~ /^y(es)?/i) {
        write_config($RCFILE);
        exit(0);
    }
}

# read configuration file and command line arguments - I need to remember 
# to fix varlist() and varhash() in AppConfig to make this nicer...
my $config   = read_config($RCFILE);
my $dryrun   = $config->nothing;
my $verbose  = $config->verbose || $dryrun;
my $colour   = $config->colour;
my $summary  = $config->summary;
my $recurse  = $config->recurse;
my $preserve = $config->preserve;
my $all      = $config->all;
my $libdir   = $config->lib;
my $ignore   = $config->ignore;
my $copy     = $config->copy;
my $accept   = $config->accept;
my $absolute = $config->absolute;
my $relative = $config->relative;
my $suffix   = $config->suffix;
my $binmode  = $config->binmode;
my $depends  = $config->depend;
my $depsfile = $config->depend_file;
my ($n_proc, $n_unmod, $n_skip, $n_copy, $n_mkdir) = (0) x 5;

my $srcdir   = $config->src
    || die "Source directory not set (-s)\n";
my $destdir  = $config->dest
    || die "Destination directory not set (-d)\n";
die "Source and destination directories may not be the same:\n  $srcdir\n"
    if $srcdir eq $destdir;

# unshift any perl5lib directories onto front of INC
unshift(@INC, @{ $config->perl5lib });

# get all template_* options from the config and fold keys to UPPER CASE
my %ttopts   = $config->varlist('^template_', 1);
my $ttmodule = delete($ttopts{ module });
my $ucttopts = {
    map { my $v = $ttopts{ $_ }; defined $v ? (uc $_, $v) : () }
    keys %ttopts,
};

# get all template variable definitions
my $replace = $config->get('define');

# now create complete parameter hash for creating template processor
my $ttopts   = {
    %$ucttopts,
    RELATIVE     => $relative,
    ABSOLUTE     => $absolute,
    INCLUDE_PATH => [ $srcdir, @$libdir ],
    OUTPUT_PATH  => $destdir,
};

# load custom template module 
if ($ttmodule) {
    my $ttpkg = $ttmodule;
    $ttpkg =~ s[::][/]g;
    $ttpkg .= '.pm';
    require $ttpkg;
}
else {
    $ttmodule = $TTMODULE;
}


#------------------------------------------------------------------------
# inter-file dependencies
#------------------------------------------------------------------------

if ($depsfile or $depends) {
    $depends = dependencies($depsfile, $depends);
} 
else {
    $depends = { };
}

my $global_deps = $depends->{'*'} || [ ];

# add any PRE_PROCESS, etc., templates as global dependencies
foreach my $ttopt (qw( PRE_PROCESS POST_PROCESS PROCESS WRAPPER )) {
    my $deps = $ucttopts->{ $ttopt } || next;
    my @deps = ref $deps eq 'ARRAY' ? (@$deps) : ($deps);
    next unless @deps;
    push(@$global_deps, @deps);
}

# remove any duplicates
$global_deps = { map { ($_ => 1) } @$global_deps };
$global_deps = [ keys %$global_deps ];

# update $depends hash or delete it if there are no dependencies
if (@$global_deps) {
    $depends->{'*'} = $global_deps;
}
else {
    delete $depends->{'*'};
    $global_deps = undef;
}
$depends = undef
    unless keys %$depends;

my $DEP_DEBUG = $config->depend_debug();


#------------------------------------------------------------------------
# pre-amble
#------------------------------------------------------------------------

if ($colour) {
    no strict 'refs';
    *red    = \&_red;
    *green  = \&_green;
    *yellow = \&_yellow;
    *blue   = \&_blue;
}
else {
    no strict 'refs';
    *red    = \&_white;
    *green  = \&_white;
    *yellow = \&_white;
    *blue   = \&_white;
}

if ($verbose) {
    local $" = ', ';


    print "$NAME $VERSION (Template Toolkit version $Template::VERSION)\n\n";

    my $sfx = join(', ', map { "$_ => $suffix->{$_}" } keys %$suffix);

    print("      Source: $srcdir\n",
          " Destination: $destdir\n",
          "Include Path: [ @$libdir ]\n",
          "      Ignore: [ @$ignore ]\n",
          "        Copy: [ @$copy ]\n",
          "      Accept: [ @$accept ]\n",
          "      Suffix: [ $sfx ]\n");
    print("      Module: $ttmodule ", $ttmodule->module_version(), "\n")
        unless $ttmodule eq $TTMODULE;

    if ($depends && $DEP_DEBUG) {
        print "Dependencies:\n";
        foreach my $key ('*', grep { !/\*/ } keys %$depends) {
            printf( "    %-16s %s\n", $key, 
                    join(', ', @{ $depends->{ $key } }) ) 
                if defined $depends->{ $key };

        }
    }
    print "\n" if $verbose > 1;
    print red("NOTE: dry run, doing nothing...\n")
        if $dryrun;
}

#------------------------------------------------------------------------
# main processing loop
#------------------------------------------------------------------------

my $template = $ttmodule->new($ttopts)
    || die $ttmodule->error();

if (@ARGV) {
    # explicitly process files specified on command lines 
    foreach my $file (@ARGV) {
        my $path = $srcdir ? File::Spec->catfile($srcdir, $file) : $file;
        if ( -d $path ) {
            process_tree($file);
        }
        else {
            process_file($file, $path, force => 1);
        }
    }
}
else {
    # implicitly process all file in source directory
    process_tree();
}

if ($summary || $verbose) {
    my $format  = "%13d %s %s\n";
    print "\n" if $verbose > 1;
    print(
        "     Summary: ",
        $dryrun ? red("This was a dry run.  Nothing was actually done\n") : "\n",
        green(sprintf($format, $n_proc,  $n_proc  == 1 ? 'file' : 'files', 'processed')),
        green(sprintf($format, $n_copy,  $n_copy  == 1 ? 'file' : 'files', 'copied')),
        green(sprintf($format, $n_mkdir, $n_mkdir == 1 ? 'directory' : 'directories', 'created')),
        yellow(sprintf($format, $n_unmod, $n_unmod == 1 ? 'file' : 'files', 'skipped (not modified)')),
        yellow(sprintf($format, $n_skip,  $n_skip  == 1 ? 'file' : 'files', 'skipped (ignored)'))
    );
}

exit(0);


#========================================================================
# END 
#========================================================================


#------------------------------------------------------------------------
# process_tree($dir)
#
# Walks the directory tree starting at $dir or the current directory
# if unspecified, processing files as found.
#------------------------------------------------------------------------

sub process_tree {
    my $dir = shift;
    my ($file, $path, $abspath, $check);
    my $target;
    local *DIR;

    my $absdir = join('/', $srcdir ? $srcdir : (), defined $dir ? $dir : ());
    $absdir ||= '.';

    opendir(DIR, $absdir) || do { warn "$absdir: $!\n"; return undef; };

    FILE: while (defined ($file = readdir(DIR))) {
        next if $file eq '.' || $file eq '..';
        $path = defined $dir ? "$dir/$file" : $file;
        $abspath = "$absdir/$file";
        
        next unless -e $abspath;

        # check against ignore list
        foreach $check (@$ignore) {
            if ($path =~ /$check/) {
                printf yellow("  - %-32s (ignored, matches /$check/)\n"), $path
                    if $verbose > 1;
                $n_skip++;
                next FILE;
            }
        }

        # check against acceptance list
        if (@$accept) {
            unless ((-d $abspath && $recurse) || grep { $path =~ /$_/ } @$accept) {
                printf yellow("  - %-32s (not accepted)\n"), $path
                    if $verbose > 1;
                $n_skip++;
                next FILE;
            }
        }

        if (-d $abspath) {
            if ($recurse) {
                my ($uid, $gid, $mode);
                
                (undef, undef, $mode, undef, $uid, $gid, undef, undef,
                 undef, undef, undef, undef, undef)  = stat($abspath);
                
                # create target directory if required
                $target = "$destdir/$path";
                unless (-d $target || $dryrun) {
                    mkpath($target, $verbose, $mode) or 
                        die red("Could not mkpath ($target): $!\n");

                    # commented out by abw on 2000/12/04 - seems to raise a warning?
                    # chown($uid, $gid, $target) || warn "chown($target): $!\n";

                    $n_mkdir++;
                    printf green("  + %-32s (created target directory)\n"), $path
                        if $verbose;
                }
                # recurse into directory
                process_tree($path);
            }
            else {
                $n_skip++;
                printf yellow("  - %-32s (directory, not recursing)\n"), $path
                    if $verbose > 1;
            }
        }
        else {
            process_file($path, $abspath);
        }
    }
    closedir(DIR);
}
    

#------------------------------------------------------------------------
# process_file()
#
# File filtering and processing sub-routine called by process_tree()
#------------------------------------------------------------------------

sub process_file {
    my ($file, $absfile, %options) = @_;
    my ($dest, $destfile, $filename, $check, 
        $srctime, $desttime, $mode, $uid, $gid);
    my ($old_suffix, $new_suffix);
    my $is_dep = 0;
    my $copy_file = 0;

    $absfile ||= $file;
    $filename = basename($file);
    $destfile = $file;
    
    # look for any relevant suffix mapping
    if (%$suffix) {
        if ($filename =~ m/\.(.+)$/) {
            $old_suffix = $1;
            if ($new_suffix = $suffix->{ $old_suffix }) {
                $destfile =~ s/$old_suffix$/$new_suffix/;
            }
        }
    }
    $dest = $destdir ? "$destdir/$destfile" : $destfile;
                   
#    print "proc $file => $dest\n";
    
    # check against copy list
    foreach my $copy_pattern (@$copy) {
        if ($filename =~ /$copy_pattern/) {
            $copy_file = 1;
            $check = $copy_pattern;
            last;
        }
    }

    # stat the source file unconditionally, so we can preserve
    # mode and ownership
    ( undef, undef, $mode, undef, $uid, $gid, undef, 
      undef, undef, $srctime, undef, undef, undef ) = stat($absfile);
    
    # test modification time of existing destination file
    if (! $all && ! $options{ force } && -f $dest) {
        $desttime = ( stat($dest) )[9];

        if (defined $depends and not $copy_file) {
            my $deptime  = depend_time($file, $depends);
            if (defined $deptime && ($srctime < $deptime)) {
                $srctime = $deptime;
                $is_dep = 1;
            }
        }
    
        if ($desttime >= $srctime) {
            printf yellow("  - %-32s (not modified)\n"), $file
                if $verbose > 1;
            $n_unmod++;
            return;
        }
    }
    
    # check against copy list
    if ($copy_file) {
        $n_copy++;
        unless ($dryrun) {
            copy($absfile, $dest) or die red("Could not copy ($absfile to $dest) : $!\n");

            if ($preserve) {
                chown($uid, $gid, $dest) || warn red("chown($dest): $!\n");
                chmod($mode, $dest) || warn red("chmod($dest): $!\n");
            }
        }

        printf green("  > %-32s (copied, matches /$check/)\n"), $file
            if $verbose;

        return;
    }

    $n_proc++;
    
    if ($verbose) {
        printf(green("  + %-32s"), $file);
        print(green(" (changed suffix to $new_suffix)")) if $new_suffix;
        print "\n";
    }

    # process file
    unless ($dryrun) {
        $template->process($file, $replace, $destfile,
            $binmode ? {binmode => $binmode} : {})
            || print(red("  ! "), $template->error(), "\n");

        if ($preserve) {
            chown($uid, $gid, $dest) || warn red("chown($dest): $!\n");
            chmod($mode, $dest) || warn red("chmod($dest): $!\n");
        }
    }
}


#------------------------------------------------------------------------
# dependencies($file, $depends)
# 
# Read the dependencies from $file, if defined, and merge in with 
# those passed in as the hash array $depends, if defined.
#------------------------------------------------------------------------

sub dependencies {
    my ($file, $depend) = @_;
    my %depends = ();

    if (defined $file) {
        my ($fh, $text, $line);
        open $fh, $file or die "Can't open $file, $!";
        local $/ = undef;
        $text = <$fh>;
        close($fh);
        $text =~ s[\\\n][]mg;
        
        foreach $line (split("\n", $text)) {
            next if $line =~ /^\s*(#|$)/;
            chomp $line;
            my ($file, @files) = quotewords('\s*:\s*', 0, $line);
            $file =~ s/^\s+//;
            @files = grep(defined, quotewords('(,|\s)\s*', 0, @files));
            $depends{$file} = \@files;
        }
    }

    if (defined $depend) {
        foreach my $key (keys %$depend) {
            $depends{$key} = [ quotewords(',', 0, $depend->{$key}) ];
        }
    }

    return \%depends;
}



#------------------------------------------------------------------------
# depend_time($file, \%depends)
#
# Returns the mtime of the most recent in @files.
#------------------------------------------------------------------------

sub depend_time {
    my ($file, $depends) = @_;
    my ($deps, $absfile, $modtime);
    my $maxtime = 0;
    my @pending = ($file);
    my @files;
    my %seen;

    # push any global dependencies onto the pending list
    if ($deps = $depends->{'*'}) {
        push(@pending, @$deps);
    }

    print "    # checking dependencies for $file...\n"
        if $DEP_DEBUG;

    # iterate through the list of pending files
    while (@pending) {
        $file = shift @pending;
        next if $seen{ $file }++;

        if (File::Spec->file_name_is_absolute($file) && -f $file) {
            $modtime = (stat($file))[9];
            print "    #   $file [$modtime]\n"
                if $DEP_DEBUG;
        }
        else {
            $modtime = 0;
            foreach my $dir ($srcdir, @$libdir) {
                $absfile = File::Spec->catfile($dir, $file);
                if (-f $absfile) {
                    $modtime = (stat($absfile))[9];
                    print "    #   $absfile [$modtime]\n"
                        if $DEP_DEBUG;
                    last;
                }
            }
        }
        $maxtime = $modtime
            if $modtime > $maxtime;

        if ($deps = $depends->{ $file }) {
            push(@pending, @$deps);
            print "    #     depends on ", join(', ', @$deps), "\n"
                if $DEP_DEBUG;
        }
    }

    return $maxtime;
}


#------------------------------------------------------------------------
# read_config($file)
#
# Handles reading of config file and/or command line arguments.
#------------------------------------------------------------------------

sub read_config {
    my $file    = shift;
    my $verbose = 0;
    my $verbinc = sub {
        my ($state, $var, $value) = @_;
        $state->{ VARIABLE }->{ verbose } = $value ? ++$verbose : --$verbose;
    };
    my $config  = AppConfig->new(
        { 
            ERROR  => sub { die(@_, "\ntry `$NAME --help'\n") }
        }, 
        'help|h'      => { ACTION => \&help },
        'src|s=s'     => { EXPAND => EXPAND_ALL },
        'dest|d=s'    => { EXPAND => EXPAND_ALL },
        'lib|l=s@'    => { EXPAND => EXPAND_ALL },
        'cfg|c=s'     => { EXPAND => EXPAND_ALL, DEFAULT => '.' },
        'verbose|v'   => { DEFAULT => 0, ACTION => $verbinc },
        'recurse|r'   => { DEFAULT => 0 },
        'nothing|n'   => { DEFAULT => 0 },
        'preserve|p'  => { DEFAULT => 0 },
        'absolute'    => { DEFAULT => 0 },
        'relative'    => { DEFAULT => 0 },
        'colour|color'=> { DEFAULT => 0 },
        'summary'     => { DEFAULT => 0 },
        'all|a'       => { DEFAULT => 0 },
        'define=s%',
        'suffix=s%',
        'binmode=s',
        'ignore=s@',
        'copy=s@',
        'accept=s@',
        'depend=s%',
        'depend_debug|depdbg',
        'depend_file|depfile=s' => { EXPAND => EXPAND_ALL },
        'template_module|module=s',
        'template_anycase|anycase',
        'template_encoding|encoding=s',
        'template_eval_perl|eval_perl',
        'template_load_perl|load_perl',
        'template_interpolate|interpolate',
        'template_pre_chomp|pre_chomp|prechomp',
        'template_post_chomp|post_chomp|postchomp',
        'template_trim|trim',
        'template_pre_process|pre_process|preprocess=s@',
        'template_post_process|post_process|postprocess=s@',
        'template_process|process=s',
        'template_wrapper|wrapper=s',
        'template_recursion|recursion',
        'template_expose_blocks|expose_blocks',
        'template_default|default=s',
        'template_error|error=s',
        'template_debug|debug=s',
        'template_start_tag|start_tag|starttag=s',
        'template_end_tag|end_tag|endtag=s',
        'template_tag_style|tag_style|tagstyle=s',
        'template_compile_ext|compile_ext=s',
        'template_compile_dir|compile_dir=s' => { EXPAND => EXPAND_ALL },
        'template_plugin_base|plugin_base|pluginbase=s@' => { EXPAND => EXPAND_ALL },
        'perl5lib|perllib=s@' => { EXPAND => EXPAND_ALL },
    );

    # add the 'file' option now that we have a $config object that we 
    # can reference in a closure
    $config->define(
        'file|f=s@' => { 
            EXPAND => EXPAND_ALL, 
            ACTION => sub { 
                my ($state, $item, $file) = @_;
                $file = $state->cfg . "/$file" 
                    unless $file =~ /^[\.\/]|(?:\w:)/;
                $config->file($file) }  
        }
    );

    # process main config file, then command line args
    $config->file($file) if -f $file;
    $config->args();

    $config;
}


sub ANSI_escape {
    my $attr = shift;
    my $text = join('', @_);
    return join("\n",
        map {
            # look for an existing escape start sequence and add new
            # attribute to it, otherwise add escape start/end sequences
            s/ \e \[ ([1-9][\d;]*) m/\e[$1;${attr}m/gx
                ? $_
                : "\e[${attr}m" . $_ . "\e[0m";
        }
        split(/\n/, $text, -1)   # -1 prevents it from ignoring trailing fields
    );
}

sub _red(@)    { ANSI_escape(31, @_) }
sub _green(@)  { ANSI_escape(32, @_) }
sub _yellow(@) { ANSI_escape(33, @_) }
sub _blue(@)   { ANSI_escape(34, @_) }
sub _white(@)  { @_ }                   # nullop


#------------------------------------------------------------------------
# write_config($file)
#
# Writes a sample configuration file to the filename specified.
#------------------------------------------------------------------------

sub write_config {
    my $file = shift;

    open(CONFIG, ">$file") || die "failed to create $file: $!\n";
    print(CONFIG <<END_OF_CONFIG);
#------------------------------------------------------------------------
# sample .ttreerc file created automatically by $NAME version $VERSION
#
# This file originally written to $file
#
# For more information on the contents of this configuration file, see
# 
#     perldoc ttree
#     ttree -h
#
#------------------------------------------------------------------------

# The most flexible way to use ttree is to create a separate directory 
# for configuration files and simply use the .ttreerc to tell ttree where
# it is.  
#
#     cfg = /path/to/ttree/config/directory

# print summary of what's going on 
verbose 

# recurse into any sub-directories and process files
recurse

# regexen of things that aren't templates and should be ignored
ignore = \\b(CVS|RCS)\\b
ignore = ^#

# ditto for things that should be copied rather than processed.
copy = \\.png\$ 
copy = \\.gif\$ 

# by default, everything not ignored or copied is accepted; add 'accept'
# lines if you want to filter further. e.g.
#
#    accept = \\.html\$
#    accept = \\.tt2\$

# options to rewrite files suffixes (htm => html, tt2 => html)
#
#    suffix htm=html
#    suffix tt2=html

# options to define dependencies between templates
#
#    depend *=header,footer,menu
#    depend index.html=mainpage,sidebar
#    depend menu=menuitem,menubar
# 

#------------------------------------------------------------------------
# The following options usually relate to a particular project so 
# you'll probably want to put them in a separate configuration file 
# in the directory specified by the 'cfg' option and then invoke tree 
# using '-f' to tell it which configuration you want to use.
# However, there's nothing to stop you from adding default 'src',
# 'dest' or 'lib' options in the .ttreerc.  The 'src' and 'dest' options
# can be re-defined in another configuration file, but be aware that 'lib' 
# options accumulate so any 'lib' options defined in the .ttreerc will
# be applied every time you run ttree.
#------------------------------------------------------------------------
# # directory containing source page templates
# src = /path/to/your/source/page/templates
#
# # directory where output files should be written
# dest = /path/to/your/html/output/directory
# 
# # additional directories of library templates
# lib = /first/path/to/your/library/templates
# lib = /second/path/to/your/library/templates

END_OF_CONFIG

    close(CONFIG);
    print "$file created.  Please edit accordingly and re-run $NAME\n"; 
}


#------------------------------------------------------------------------
# help()
#
# Prints help message and exits.
#------------------------------------------------------------------------

sub help {
    print<<END_OF_HELP;
$NAME $VERSION (Template Toolkit version $Template::VERSION)

usage: $NAME [options] [files]

Options:
   -a      (--all)          Process all files, regardless of modification
   -r      (--recurse)      Recurse into sub-directories
   -p      (--preserve)     Preserve file ownership and permission
   -n      (--nothing)      Do nothing, just print summary (enables -v)
   -v      (--verbose)      Verbose mode. Use twice for more verbosity: -v -v
   -h      (--help)         This help
   -s DIR  (--src=DIR)      Source directory
   -d DIR  (--dest=DIR)     Destination directory
   -c DIR  (--cfg=DIR)      Location of configuration files
   -l DIR  (--lib=DIR)      Library directory (INCLUDE_PATH)  (multiple)
   -f FILE (--file=FILE)    Read named configuration file     (multiple)

Display options:
   --colour / --color       Enable colo(u)rful verbose output.
   --summary                Show processing summary.

File search specifications (all may appear multiple times):
   --ignore=REGEX           Ignore files matching REGEX
   --copy=REGEX             Copy files matching REGEX
   --accept=REGEX           Process only files matching REGEX 

File Dependencies Options:
   --depend foo=bar,baz     Specify that 'foo' depends on 'bar' and 'baz'.
   --depend_file FILE       Read file dependancies from FILE.
   --depend_debug           Enable debugging for dependencies

File suffix rewriting (may appear multiple times)
   --suffix old=new         Change any '.old' suffix to '.new'

File encoding options
   --binmode=value          Set binary mode of output files
   --encoding=value         Set encoding of input files

Additional options to set Template Toolkit configuration items:
   --define var=value       Define template variable
   --interpolate            Interpolate '\$var' references in text
   --anycase                Accept directive keywords in any case.
   --pre_chomp              Chomp leading whitespace 
   --post_chomp             Chomp trailing whitespace
   --trim                   Trim blank lines around template blocks
   --eval_perl              Evaluate [% PERL %] ... [% END %] code blocks
   --load_perl              Load regular Perl modules via USE directive
   --absolute               Enable the ABSOLUTE option
   --relative               Enable the RELATIVE option
   --pre_process=TEMPLATE   Process TEMPLATE before each main template
   --post_process=TEMPLATE  Process TEMPLATE after each main template
   --process=TEMPLATE       Process TEMPLATE instead of main template
   --wrapper=TEMPLATE       Process TEMPLATE wrapper around main template
   --default=TEMPLATE       Use TEMPLATE as default
   --error=TEMPLATE         Use TEMPLATE to handle errors
   --debug=STRING           Set TT DEBUG option to STRING
   --start_tag=STRING       STRING defines start of directive tag
   --end_tag=STRING         STRING defined end of directive tag
   --tag_style=STYLE        Use pre-defined tag STYLE    
   --plugin_base=PACKAGE    Base PACKAGE for plugins            
   --compile_ext=STRING     File extension for compiled template files
   --compile_dir=DIR        Directory for compiled template files
   --perl5lib=DIR           Specify additional Perl library directories
   --template_module=MODULE Specify alternate Template module

See 'perldoc ttree' for further information.  

END_OF_HELP

    exit(0);
}

__END__


#------------------------------------------------------------------------
# IMPORTANT NOTE
#   This documentation is generated automatically from source
#   templates.  Any changes you make here may be lost.
# 
#   The 'docsrc' documentation source bundle is available for download
#   from http://www.template-toolkit.org/docs.html and contains all
#   the source templates, XML files, scripts, etc., from which the
#   documentation for the Template Toolkit is built.
#------------------------------------------------------------------------

=head1 NAME

Template::Tools::ttree - Process entire directory trees of templates

=head1 SYNOPSIS

    ttree [options] [files]

=head1 DESCRIPTION

The F<ttree> script is used to process entire directory trees containing
template files.  The resulting output from processing each file is then 
written to a corresponding file in a destination directory.  The script
compares the modification times of source and destination files (where
they already exist) and processes only those files that have been modified.
In other words, it is the equivalent of 'make' for the Template Toolkit.

It supports a number of options which can be used to configure
behaviour, define locations and set Template Toolkit options.  The
script first reads the F<.ttreerc> configuration file in the HOME
directory, or an alternative file specified in the TTREERC environment
variable.  Then, it processes any command line arguments, including
any additional configuration files specified via the C<-f> (file)
option.

=head2 The F<.ttreerc> Configuration File

When you run F<ttree> for the first time it will ask you if you want
it to create a F<.ttreerc> file for you.  This will be created in your
home directory.

    $ ttree
    Do you want me to create a sample '.ttreerc' file for you?
    (file: /home/abw/.ttreerc)   [y/n]: y
    /home/abw/.ttreerc created.  Please edit accordingly and re-run ttree

The purpose of this file is to set any I<global> configuration options
that you want applied I<every> time F<ttree> is run.  For example, you
can use the C<ignore> and C<copy> option to provide regular expressions
that specify which files should be ignored and which should be copied 
rather than being processed as templates.  You may also want to set 
flags like C<verbose> and C<recurse> according to your preference.

A minimal F<.ttreerc>:

    # ignore these files
    ignore = \b(CVS|RCS)\b
    ignore = ^#
    ignore = ~$

    # copy these files
    copy   = \.(gif|png|jpg|pdf)$ 

    # recurse into directories
    recurse

    # provide info about what's going on
    verbose

In most cases, you'll want to create a different F<ttree> configuration 
file for each project you're working on.  The C<cfg> option allows you
to specify a directory where F<ttree> can find further configuration 
files.

    cfg = /home/abw/.ttree

The C<-f> command line option can be used to specify which configuration
file should be used.  You can specify a filename using an absolute or 
relative path:

    $ ttree -f /home/abw/web/example/etc/ttree.cfg
    $ ttree -f ./etc/ttree.cfg
    $ ttree -f ../etc/ttree.cfg

If the configuration file does not begin with C</> or C<.> or something
that looks like a MS-DOS absolute path (e.g. C<C:\\etc\\ttree.cfg>) then
F<ttree> will look for it in the directory specified by the C<cfg> option.

    $ ttree -f test1          # /home/abw/.ttree/test1

The C<cfg> option can only be used in the F<.ttreerc> file.  All the
other options can be used in the F<.ttreerc> or any other F<ttree>
configuration file.  They can all also be specified as command line
options.

Remember that F<.ttreerc> is always processed I<before> any
configuration file specified with the C<-f> option.  Certain options
like C<lib> can be used any number of times and accumulate their values.

For example, consider the following configuration files:

F</home/abw/.ttreerc>:

    cfg = /home/abw/.ttree
    lib = /usr/local/tt2/templates

F</home/abw/.ttree/myconfig>:

    lib = /home/abw/web/example/templates/lib

When F<ttree> is invoked as follows:

    $ ttree -f myconfig

the C<lib> option will be set to the following directories:

    /usr/local/tt2/templates
    /home/abw/web/example/templates/lib

Any templates located under F</usr/local/tt2/templates> will be used
in preference to those located under
F</home/abw/web/example/templates/lib>.  This may be what you want,
but then again, it might not.  For this reason, it is good practice to
keep the F<.ttreerc> as simple as possible and use different
configuration files for each F<ttree> project.

=head2 Directory Options

The C<src> option is used to define the directory containing the
source templates to be processed.  It can be provided as a command
line option or in a configuration file as shown here:

    src = /home/abw/web/example/templates/src

Each template in this directory typically corresponds to a single
web page or other document. 

The C<dest> option is used to specify the destination directory for the
generated output.

    dest = /home/abw/web/example/html

The C<lib> option is used to define one or more directories containing
additional library templates.  These templates are not documents in
their own right and typically comprise of smaller, modular components
like headers, footers and menus that are incorporated into pages templates.

    lib = /home/abw/web/example/templates/lib
    lib = /usr/local/tt2/templates

The C<lib> option can be used repeatedly to add further directories to
the search path.

A list of templates can be passed to F<ttree> as command line arguments.

    $ ttree foo.html bar.html

It looks for these templates in the C<src> directory and processes them
through the Template Toolkit, using any additional template components
from the C<lib> directories.  The generated output is then written to 
the corresponding file in the C<dest> directory.

If F<ttree> is invoked without explicitly specifying any templates
to be processed then it will process every file in the C<src> directory.
If the C<-r> (recurse) option is set then it will additionally iterate
down through sub-directories and process and other template files it finds
therein.

    $ ttree -r

If a template has been processed previously, F<ttree> will compare the
modification times of the source and destination files.  If the source
template (or one it is dependant on) has not been modified more
recently than the generated output file then F<ttree> will not process
it.  The F<-a> (all) option can be used to force F<ttree> to process
all files regardless of modification time.

    $ tree -a

Any templates explicitly named as command line argument are always
processed and the modification time checking is bypassed.

=head2 File Options

The C<ignore>, C<copy> and C<accept> options are used to specify Perl
regexen to filter file names.  Files that match any of the C<ignore>
options will not be processed.  Remaining files that match any of the
C<copy> regexen will be copied to the destination directory.  Remaining
files that then match any of the C<accept> criteria are then processed
via the Template Toolkit.  If no C<accept> parameter is specified then 
all files will be accepted for processing if not already copied or
ignored.

    # ignore these files
    ignore = \b(CVS|RCS)\b
    ignore = ^#
    ignore = ~$

    # copy these files
    copy   = \.(gif|png|jpg|pdf)$ 

    # accept only .tt2 templates
    accept = \.tt2$

The C<suffix> option is used to define mappings between the file
extensions for source templates and the generated output files.  The
following example specifies that source templates with a C<.tt2>
suffix should be output as C<.html> files:

    suffix tt2=html

Or on the command line, 

    --suffix tt2=html

You can provide any number of different suffix mappings by repeating 
this option.

The C<binmode> option is used to set the encoding of the output file.
For example use C<--binmode=:utf8> to set the output format to unicode.

=head2 Template Dependencies

The C<depend> and C<depend_file> options allow you to specify
how any given template file depends on another file or group of files. 
The C<depend> option is used to express a single dependency.

  $ ttree --depend foo=bar,baz

This command line example shows the C<--depend> option being used to
specify that the F<foo> file is dependant on the F<bar> and F<baz>
templates.  This option can be used many time on the command line:

  $ ttree --depend foo=bar,baz --depend crash=bang,wallop

or in a configuration file:

  depend foo=bar,baz
  depend crash=bang,wallop

The file appearing on the left of the C<=> is specified relative to
the C<src> or C<lib> directories.  The file(s) appearing on the right
can be specified relative to any of these directories or as absolute
file paths.

For example:

  $ ttree --depend foo=bar,/tmp/baz

To define a dependency that applies to all files, use C<*> on the 
left of the C<=>.

  $ ttree --depend *=header,footer

or in a configuration file:

  depend *=header,footer

Any templates that are defined in the C<pre_process>, C<post_process>,
C<process> or C<wrapper> options will automatically be added to the
list of global dependencies that apply to all templates.

The C<depend_file> option can be used to specify a file that contains
dependency information.  

    $ ttree --depend_file=/home/abw/web/example/etc/ttree.dep

Here is an example of a dependency file:

   # This is a comment. It is ignored.
  
   index.html: header footer menubar 
  
   header: titlebar hotlinks
  
   menubar: menuitem
  
   # spanning multiple lines with the backslash
   another.html: header footer menubar \
   sidebar searchform

Lines beginning with the C<#> character are comments and are ignored.
Blank lines are also ignored.  All other lines should provide a
filename followed by a colon and then a list of dependant files
separated by whitespace, commas or both.  Whitespace around the colon
is also optional.  Lines ending in the C<\> character are continued
onto the following line.

Files that contain spaces can be quoted. That is only necessary
for files after the colon (':'). The file before the colon may be
quoted if it contains a colon. 

As with the command line options, the C<*> character can be used
as a wildcard to specify a dependency for all templates.

    * : config,header

=head2 Template Toolkit Options

F<ttree> also provides access to the usual range of Template Toolkit
options.  For example, the C<--pre_chomp> and C<--post_chomp> F<ttree>
options correspond to the C<PRE_CHOMP> and C<POST_CHOMP> options.

Run C<ttree -h> for a summary of the options available.

=head1 AUTHORS

Andy Wardley E<lt>abw@andywardley.comE<gt>

L<http://www.andywardley.com/|http://www.andywardley.com/>

With contributions from Dylan William Hardison (support for
dependencies), Bryce Harrington (C<absolute> and C<relative> options),
Mark Anderson (C<suffix> and C<debug> options), Harald Joerg and Leon
Brocard who gets everywhere, it seems.

=head1 VERSION

2.68, distributed as part of the
Template Toolkit version 2.19, released on 27 April 2007.

=head1 COPYRIGHT

  Copyright (C) 1996-2007 Andy Wardley.  All Rights Reserved.


This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<tpage|Template::Tools::tpage>


__END__
:endofperl
