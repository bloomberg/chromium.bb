#
###########################################################-
#
#    perltidy - a perl script indenter and formatter
#
#    Copyright (c) 2000-2018 by Steve Hancock
#    Distributed under the GPL license agreement; see file COPYING
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
#    For brief instructions, try 'perltidy -h'.
#    For more complete documentation, try 'man perltidy'
#    or visit http://perltidy.sourceforge.net
#
#    This script is an example of the default style.  It was formatted with:
#
#      perltidy Tidy.pm
#
#    Code Contributions: See ChangeLog.html for a complete history.
#      Michael Cartmell supplied code for adaptation to VMS and helped with
#        v-strings.
#      Hugh S. Myers supplied sub streamhandle and the supporting code to
#        create a Perl::Tidy module which can operate on strings, arrays, etc.
#      Yves Orton supplied coding to help detect Windows versions.
#      Axel Rose supplied a patch for MacPerl.
#      Sebastien Aperghis-Tramoni supplied a patch for the defined or operator.
#      Dan Tyrell contributed a patch for binary I/O.
#      Ueli Hugenschmidt contributed a patch for -fpsc
#      Sam Kington supplied a patch to identify the initial indentation of
#      entabbed code.
#      jonathan swartz supplied patches for:
#      * .../ pattern, which looks upwards from directory
#      * --notidy, to be used in directories where we want to avoid
#        accidentally tidying
#      * prefilter and postfilter
#      * iterations option
#
#      Many others have supplied key ideas, suggestions, and bug reports;
#        see the CHANGES file.
#
############################################################

package Perl::Tidy;

# perlver reports minimum version needed is 5.8.0
# 5.004 needed for IO::File
# 5.008 needed for wide characters
use 5.008;
use warnings;
use strict;
use Exporter;
use Carp;
use Digest::MD5 qw(md5_hex);
use Perl::Tidy::Debugger;
use Perl::Tidy::DevNull;
use Perl::Tidy::Diagnostics;
use Perl::Tidy::FileWriter;
use Perl::Tidy::Formatter;
use Perl::Tidy::HtmlWriter;
use Perl::Tidy::IOScalar;
use Perl::Tidy::IOScalarArray;
use Perl::Tidy::IndentationItem;
use Perl::Tidy::LineSink;
use Perl::Tidy::LineSource;
use Perl::Tidy::Logger;
use Perl::Tidy::Tokenizer;
use Perl::Tidy::VerticalAligner;
local $| = 1;

use vars qw{
  $VERSION
  @ISA
  @EXPORT
  $missing_file_spec
  $fh_stderr
  $rOpts_character_encoding
};

@ISA    = qw( Exporter );
@EXPORT = qw( &perltidy );

use Cwd;
use Encode ();
use IO::File;
use File::Basename;
use File::Copy;
use File::Temp qw(tempfile);

BEGIN {

    # Release version is the approximate YYMMDD of the release.
    # Development version is (Last Release).(Development Number)

    # To make the number continually increasing, the Development Number is a 2
    # digit number starting at 01 after a release is continually bumped along
    # at significant points during developement. If it ever reaches 99 then the
    # Release version must be bumped, and it is probably past time for a
    # release anyway.

    $VERSION = '20181120';
}

sub streamhandle {

    # given filename and mode (r or w), create an object which:
    #   has a 'getline' method if mode='r', and
    #   has a 'print' method if mode='w'.
    # The objects also need a 'close' method.
    #
    # How the object is made:
    #
    # if $filename is:     Make object using:
    # ----------------     -----------------
    # '-'                  (STDIN if mode = 'r', STDOUT if mode='w')
    # string               IO::File
    # ARRAY  ref           Perl::Tidy::IOScalarArray (formerly IO::ScalarArray)
    # STRING ref           Perl::Tidy::IOScalar      (formerly IO::Scalar)
    # object               object
    #                      (check for 'print' method for 'w' mode)
    #                      (check for 'getline' method for 'r' mode)
    my ( $filename, $mode ) = @_;

    my $ref = ref($filename);
    my $New;
    my $fh;

    # handle a reference
    if ($ref) {
        if ( $ref eq 'ARRAY' ) {
            $New = sub { Perl::Tidy::IOScalarArray->new(@_) };
        }
        elsif ( $ref eq 'SCALAR' ) {
            $New = sub { Perl::Tidy::IOScalar->new(@_) };
        }
        else {

            # Accept an object with a getline method for reading. Note:
            # IO::File is built-in and does not respond to the defined
            # operator.  If this causes trouble, the check can be
            # skipped and we can just let it crash if there is no
            # getline.
            if ( $mode =~ /[rR]/ ) {

                # RT#97159; part 1 of 2: updated to use 'can'
                ##if ( $ref eq 'IO::File' || defined &{ $ref . "::getline" } ) {
                if ( $ref->can('getline') ) {
                    $New = sub { $filename };
                }
                else {
                    $New = sub { undef };
                    confess <<EOM;
------------------------------------------------------------------------
No 'getline' method is defined for object of class $ref
Please check your call to Perl::Tidy::perltidy.  Trace follows.
------------------------------------------------------------------------
EOM
                }
            }

            # Accept an object with a print method for writing.
            # See note above about IO::File
            if ( $mode =~ /[wW]/ ) {

                # RT#97159; part 2 of 2: updated to use 'can'
                ##if ( $ref eq 'IO::File' || defined &{ $ref . "::print" } ) {
                if ( $ref->can('print') ) {
                    $New = sub { $filename };
                }
                else {
                    $New = sub { undef };
                    confess <<EOM;
------------------------------------------------------------------------
No 'print' method is defined for object of class $ref
Please check your call to Perl::Tidy::perltidy. Trace follows.
------------------------------------------------------------------------
EOM
                }
            }
        }
    }

    # handle a string
    else {
        if ( $filename eq '-' ) {
            $New = sub { $mode eq 'w' ? *STDOUT : *STDIN }
        }
        else {
            $New = sub { IO::File->new(@_) };
        }
    }
    $fh = $New->( $filename, $mode )
      or Warn("Couldn't open file:$filename in mode:$mode : $!\n");

    return $fh, ( $ref or $filename );
}

sub find_input_line_ending {

    # Peek at a file and return first line ending character.
    # Quietly return undef in case of any trouble.
    my ($input_file) = @_;
    my $ending;

    # silently ignore input from object or stdin
    if ( ref($input_file) || $input_file eq '-' ) {
        return $ending;
    }

    my $fh;
    open( $fh, '<', $input_file ) || return $ending;

    binmode $fh;
    my $buf;
    read( $fh, $buf, 1024 );
    close $fh;
    if ( $buf && $buf =~ /([\012\015]+)/ ) {
        my $test = $1;

        # dos
        if ( $test =~ /^(\015\012)+$/ ) { $ending = "\015\012" }

        # mac
        elsif ( $test =~ /^\015+$/ ) { $ending = "\015" }

        # unix
        elsif ( $test =~ /^\012+$/ ) { $ending = "\012" }

        # unknown
        else { }
    }

    # no ending seen
    else { }

    return $ending;
}

sub catfile {

    # concatenate a path and file basename
    # returns undef in case of error

    my @parts = @_;

    #BEGIN { eval "require File::Spec"; $missing_file_spec = $@; }
    BEGIN {
        eval { require File::Spec };
        $missing_file_spec = $@;
    }

    # use File::Spec if we can
    unless ($missing_file_spec) {
        return File::Spec->catfile(@parts);
    }

    # Perl 5.004 systems may not have File::Spec so we'll make
    # a simple try.  We assume File::Basename is available.
    # return undef if not successful.
    my $name      = pop @parts;
    my $path      = join '/', @parts;
    my $test_file = $path . $name;
    my ( $test_name, $test_path ) = fileparse($test_file);
    return $test_file if ( $test_name eq $name );
    return if ( $^O eq 'VMS' );

    # this should work at least for Windows and Unix:
    $test_file = $path . '/' . $name;
    ( $test_name, $test_path ) = fileparse($test_file);
    return $test_file if ( $test_name eq $name );
    return;
}

# Here is a map of the flow of data from the input source to the output
# line sink:
#
# LineSource-->Tokenizer-->Formatter-->VerticalAligner-->FileWriter-->
#       input                         groups                 output
#       lines   tokens      lines       of          lines    lines
#                                      lines
#
# The names correspond to the package names responsible for the unit processes.
#
# The overall process is controlled by the "main" package.
#
# LineSource is the stream of input lines
#
# Tokenizer analyzes a line and breaks it into tokens, peeking ahead
# if necessary.  A token is any section of the input line which should be
# manipulated as a single entity during formatting.  For example, a single
# ',' character is a token, and so is an entire side comment.  It handles
# the complexities of Perl syntax, such as distinguishing between '<<' as
# a shift operator and as a here-document, or distinguishing between '/'
# as a divide symbol and as a pattern delimiter.
#
# Formatter inserts and deletes whitespace between tokens, and breaks
# sequences of tokens at appropriate points as output lines.  It bases its
# decisions on the default rules as modified by any command-line options.
#
# VerticalAligner collects groups of lines together and tries to line up
# certain tokens, such as '=>', '#', and '=' by adding whitespace.
#
# FileWriter simply writes lines to the output stream.
#
# The Logger package, not shown, records significant events and warning
# messages.  It writes a .LOG file, which may be saved with a
# '-log' or a '-g' flag.

sub perltidy {

    my %input_hash = @_;

    my %defaults = (
        argv                  => undef,
        destination           => undef,
        formatter             => undef,
        logfile               => undef,
        errorfile             => undef,
        perltidyrc            => undef,
        source                => undef,
        stderr                => undef,
        dump_options          => undef,
        dump_options_type     => undef,
        dump_getopt_flags     => undef,
        dump_options_category => undef,
        dump_options_range    => undef,
        dump_abbreviations    => undef,
        prefilter             => undef,
        postfilter            => undef,
    );

    # don't overwrite callers ARGV
    local @ARGV   = @ARGV;
    local *STDERR = *STDERR;

    if ( my @bad_keys = grep { !exists $defaults{$_} } keys %input_hash ) {
        local $" = ')(';
        my @good_keys = sort keys %defaults;
        @bad_keys = sort @bad_keys;
        confess <<EOM;
------------------------------------------------------------------------
Unknown perltidy parameter : (@bad_keys)
perltidy only understands : (@good_keys)
------------------------------------------------------------------------

EOM
    }

    my $get_hash_ref = sub {
        my ($key) = @_;
        my $hash_ref = $input_hash{$key};
        if ( defined($hash_ref) ) {
            unless ( ref($hash_ref) eq 'HASH' ) {
                my $what = ref($hash_ref);
                my $but_is =
                  $what ? "but is ref to $what" : "but is not a reference";
                croak <<EOM;
------------------------------------------------------------------------
error in call to perltidy:
-$key must be reference to HASH $but_is
------------------------------------------------------------------------
EOM
            }
        }
        return $hash_ref;
    };

    %input_hash = ( %defaults, %input_hash );
    my $argv               = $input_hash{'argv'};
    my $destination_stream = $input_hash{'destination'};
    my $errorfile_stream   = $input_hash{'errorfile'};
    my $logfile_stream     = $input_hash{'logfile'};
    my $perltidyrc_stream  = $input_hash{'perltidyrc'};
    my $source_stream      = $input_hash{'source'};
    my $stderr_stream      = $input_hash{'stderr'};
    my $user_formatter     = $input_hash{'formatter'};
    my $prefilter          = $input_hash{'prefilter'};
    my $postfilter         = $input_hash{'postfilter'};

    if ($stderr_stream) {
        ( $fh_stderr, my $stderr_file ) =
          Perl::Tidy::streamhandle( $stderr_stream, 'w' );
        if ( !$fh_stderr ) {
            croak <<EOM;
------------------------------------------------------------------------
Unable to redirect STDERR to $stderr_stream
Please check value of -stderr in call to perltidy
------------------------------------------------------------------------
EOM
        }
    }
    else {
        $fh_stderr = *STDERR;
    }

    sub Warn { my $msg = shift; $fh_stderr->print($msg); return }

    sub Exit {
        my $flag = shift;
        if   ($flag) { goto ERROR_EXIT }
        else         { goto NORMAL_EXIT }
        croak "unexpectd return to Exit";
    }

    sub Die {
        my $msg = shift;
        Warn($msg);
        Exit(1);
        croak "unexpected return to Die";
    }

    # extract various dump parameters
    my $dump_options_type     = $input_hash{'dump_options_type'};
    my $dump_options          = $get_hash_ref->('dump_options');
    my $dump_getopt_flags     = $get_hash_ref->('dump_getopt_flags');
    my $dump_options_category = $get_hash_ref->('dump_options_category');
    my $dump_abbreviations    = $get_hash_ref->('dump_abbreviations');
    my $dump_options_range    = $get_hash_ref->('dump_options_range');

    # validate dump_options_type
    if ( defined($dump_options) ) {
        unless ( defined($dump_options_type) ) {
            $dump_options_type = 'perltidyrc';
        }
        unless ( $dump_options_type =~ /^(perltidyrc|full)$/ ) {
            croak <<EOM;
------------------------------------------------------------------------
Please check value of -dump_options_type in call to perltidy;
saw: '$dump_options_type' 
expecting: 'perltidyrc' or 'full'
------------------------------------------------------------------------
EOM

        }
    }
    else {
        $dump_options_type = "";
    }

    if ($user_formatter) {

        # if the user defines a formatter, there is no output stream,
        # but we need a null stream to keep coding simple
        $destination_stream = Perl::Tidy::DevNull->new();
    }

    # see if ARGV is overridden
    if ( defined($argv) ) {

        my $rargv = ref $argv;
        if ( $rargv eq 'SCALAR' ) { $argv = ${$argv}; $rargv = undef }

        # ref to ARRAY
        if ($rargv) {
            if ( $rargv eq 'ARRAY' ) {
                @ARGV = @{$argv};
            }
            else {
                croak <<EOM;
------------------------------------------------------------------------
Please check value of -argv in call to perltidy;
it must be a string or ref to ARRAY but is: $rargv
------------------------------------------------------------------------
EOM
            }
        }

        # string
        else {
            my ( $rargv, $msg ) = parse_args($argv);
            if ($msg) {
                Die(<<EOM);
Error parsing this string passed to to perltidy with 'argv': 
$msg
EOM
            }
            @ARGV = @{$rargv};
        }
    }

    my $rpending_complaint;
    ${$rpending_complaint} = "";
    my $rpending_logfile_message;
    ${$rpending_logfile_message} = "";

    my ( $is_Windows, $Windows_type ) = look_for_Windows($rpending_complaint);

    # VMS file names are restricted to a 40.40 format, so we append _tdy
    # instead of .tdy, etc. (but see also sub check_vms_filename)
    my $dot;
    my $dot_pattern;
    if ( $^O eq 'VMS' ) {
        $dot         = '_';
        $dot_pattern = '_';
    }
    else {
        $dot         = '.';
        $dot_pattern = '\.';    # must escape for use in regex
    }

    #---------------------------------------------------------------
    # get command line options
    #---------------------------------------------------------------
    my ( $rOpts, $config_file, $rraw_options, $roption_string,
        $rexpansion, $roption_category, $roption_range )
      = process_command_line(
        $perltidyrc_stream,  $is_Windows, $Windows_type,
        $rpending_complaint, $dump_options_type,
      );

    my $saw_extrude = ( grep { m/^-extrude$/ } @{$rraw_options} ) ? 1 : 0;
    my $saw_pbp =
      ( grep { m/^-(pbp|perl-best-practices)$/ } @{$rraw_options} ) ? 1 : 0;

    #---------------------------------------------------------------
    # Handle requests to dump information
    #---------------------------------------------------------------

    # return or exit immediately after all dumps
    my $quit_now = 0;

    # Getopt parameters and their flags
    if ( defined($dump_getopt_flags) ) {
        $quit_now = 1;
        foreach my $op ( @{$roption_string} ) {
            my $opt  = $op;
            my $flag = "";

            # Examples:
            #  some-option=s
            #  some-option=i
            #  some-option:i
            #  some-option!
            if ( $opt =~ /(.*)(!|=.*|:.*)$/ ) {
                $opt  = $1;
                $flag = $2;
            }
            $dump_getopt_flags->{$opt} = $flag;
        }
    }

    if ( defined($dump_options_category) ) {
        $quit_now = 1;
        %{$dump_options_category} = %{$roption_category};
    }

    if ( defined($dump_options_range) ) {
        $quit_now = 1;
        %{$dump_options_range} = %{$roption_range};
    }

    if ( defined($dump_abbreviations) ) {
        $quit_now = 1;
        %{$dump_abbreviations} = %{$rexpansion};
    }

    if ( defined($dump_options) ) {
        $quit_now = 1;
        %{$dump_options} = %{$rOpts};
    }

    Exit(0) if ($quit_now);

    # make printable string of options for this run as possible diagnostic
    my $readable_options = readable_options( $rOpts, $roption_string );

    # dump from command line
    if ( $rOpts->{'dump-options'} ) {
        print STDOUT $readable_options;
        Exit(0);
    }

    #---------------------------------------------------------------
    # check parameters and their interactions
    #---------------------------------------------------------------
    my $tabsize =
      check_options( $rOpts, $is_Windows, $Windows_type, $rpending_complaint );

    if ($user_formatter) {
        $rOpts->{'format'} = 'user';
    }

    # there must be one entry here for every possible format
    my %default_file_extension = (
        tidy => 'tdy',
        html => 'html',
        user => '',
    );

    $rOpts_character_encoding = $rOpts->{'character-encoding'};

    # be sure we have a valid output format
    unless ( exists $default_file_extension{ $rOpts->{'format'} } ) {
        my $formats = join ' ',
          sort map { "'" . $_ . "'" } keys %default_file_extension;
        my $fmt = $rOpts->{'format'};
        Die("-format='$fmt' but must be one of: $formats\n");
    }

    my $output_extension = make_extension( $rOpts->{'output-file-extension'},
        $default_file_extension{ $rOpts->{'format'} }, $dot );

    # If the backup extension contains a / character then the backup should
    # be deleted when the -b option is used.   On older versions of
    # perltidy this will generate an error message due to an illegal
    # file name.
    #
    # A backup file will still be generated but will be deleted
    # at the end.  If -bext='/' then this extension will be
    # the default 'bak'.  Otherwise it will be whatever characters
    # remains after all '/' characters are removed.  For example:
    # -bext         extension     slashes
    #  '/'          bak           1
    #  '/delete'    delete        1
    #  'delete/'    delete        1
    #  '/dev/null'  devnull       2    (Currently not allowed)
    my $bext          = $rOpts->{'backup-file-extension'};
    my $delete_backup = ( $rOpts->{'backup-file-extension'} =~ s/\///g );

    # At present only one forward slash is allowed.  In the future multiple
    # slashes may be allowed to allow for other options
    if ( $delete_backup > 1 ) {
        Die("-bext=$bext contains more than one '/'\n");
    }

    my $backup_extension =
      make_extension( $rOpts->{'backup-file-extension'}, 'bak', $dot );

    my $html_toc_extension =
      make_extension( $rOpts->{'html-toc-extension'}, 'toc', $dot );

    my $html_src_extension =
      make_extension( $rOpts->{'html-src-extension'}, 'src', $dot );

    # check for -b option;
    # silently ignore unless beautify mode
    my $in_place_modify = $rOpts->{'backup-and-modify-in-place'}
      && $rOpts->{'format'} eq 'tidy';

    # Turn off -b with warnings in case of conflicts with other options.
    # NOTE: Do this silently, without warnings, if there is a source or
    # destination stream, or standard output is used.  This is because the -b
    # flag may have been in a .perltidyrc file and warnings break
    # Test::NoWarnings.  See email discussion with Merijn Brand 26 Feb 2014.
    if ($in_place_modify) {
        if (   $rOpts->{'standard-output'}
            || $destination_stream
            || ref $source_stream
            || $rOpts->{'outfile'}
            || defined( $rOpts->{'output-path'} ) )
        {
            $in_place_modify = 0;
        }
    }

    Perl::Tidy::Formatter::check_options($rOpts);
    if ( $rOpts->{'format'} eq 'html' ) {
        Perl::Tidy::HtmlWriter->check_options($rOpts);
    }

    # make the pattern of file extensions that we shouldn't touch
    my $forbidden_file_extensions = "(($dot_pattern)(LOG|DEBUG|ERR|TEE)";
    if ($output_extension) {
        my $ext = quotemeta($output_extension);
        $forbidden_file_extensions .= "|$ext";
    }
    if ( $in_place_modify && $backup_extension ) {
        my $ext = quotemeta($backup_extension);
        $forbidden_file_extensions .= "|$ext";
    }
    $forbidden_file_extensions .= ')$';

    # Create a diagnostics object if requested;
    # This is only useful for code development
    my $diagnostics_object = undef;
    if ( $rOpts->{'DIAGNOSTICS'} ) {
        $diagnostics_object = Perl::Tidy::Diagnostics->new();
    }

    # no filenames should be given if input is from an array
    if ($source_stream) {
        if ( @ARGV > 0 ) {
            Die(
"You may not specify any filenames when a source array is given\n"
            );
        }

        # we'll stuff the source array into ARGV
        unshift( @ARGV, $source_stream );

        # No special treatment for source stream which is a filename.
        # This will enable checks for binary files and other bad stuff.
        $source_stream = undef unless ref($source_stream);
    }

    # use stdin by default if no source array and no args
    else {
        unshift( @ARGV, '-' ) unless @ARGV;
    }

    #---------------------------------------------------------------
    # Ready to go...
    # main loop to process all files in argument list
    #---------------------------------------------------------------
    my $number_of_files = @ARGV;
    my $formatter       = undef;
    my $tokenizer       = undef;

    # If requested, process in order of increasing file size
    # This can significantly reduce perl's virtual memory usage during testing.
    if ( $number_of_files > 1 && $rOpts->{'file-size-order'} ) {
        @ARGV =
          map  { $_->[0] }
          sort { $a->[1] <=> $b->[1] }
          map  { [ $_, -e $_ ? -s $_ : 0 ] } @ARGV;
    }

    while ( my $input_file = shift @ARGV ) {
        my $fileroot;
        my $input_file_permissions;

        #---------------------------------------------------------------
        # prepare this input stream
        #---------------------------------------------------------------
        if ($source_stream) {
            $fileroot = "perltidy";

            # If the source is from an array or string, then .LOG output
            # is only possible if a logfile stream is specified.  This prevents
            # unexpected perltidy.LOG files.
            if ( !defined($logfile_stream) ) {
                $logfile_stream = Perl::Tidy::DevNull->new();
            }
        }
        elsif ( $input_file eq '-' ) {    # '-' indicates input from STDIN
            $fileroot = "perltidy";       # root name to use for .ERR, .LOG, etc
            $in_place_modify = 0;
        }
        else {
            $fileroot = $input_file;
            unless ( -e $input_file ) {

                # file doesn't exist - check for a file glob
                if ( $input_file =~ /([\?\*\[\{])/ ) {

                    # Windows shell may not remove quotes, so do it
                    my $input_file = $input_file;
                    if ( $input_file =~ /^\'(.+)\'$/ ) { $input_file = $1 }
                    if ( $input_file =~ /^\"(.+)\"$/ ) { $input_file = $1 }
                    my $pattern = fileglob_to_re($input_file);
                    ##eval "/$pattern/";
                    if ( !$@ && opendir( DIR, './' ) ) {
                        my @files =
                          grep { /$pattern/ && !-d $_ } readdir(DIR);
                        closedir(DIR);
                        if (@files) {
                            unshift @ARGV, @files;
                            next;
                        }
                    }
                }
                Warn("skipping file: '$input_file': no matches found\n");
                next;
            }

            unless ( -f $input_file ) {
                Warn("skipping file: $input_file: not a regular file\n");
                next;
            }

            # As a safety precaution, skip zero length files.
            # If for example a source file got clobbered somehow,
            # the old .tdy or .bak files might still exist so we
            # shouldn't overwrite them with zero length files.
            unless ( -s $input_file ) {
                Warn("skipping file: $input_file: Zero size\n");
                next;
            }

            unless ( ( -T $input_file ) || $rOpts->{'force-read-binary'} ) {
                Warn(
                    "skipping file: $input_file: Non-text (override with -f)\n"
                );
                next;
            }

            # we should have a valid filename now
            $fileroot               = $input_file;
            $input_file_permissions = ( stat $input_file )[2] & oct(7777);

            if ( $^O eq 'VMS' ) {
                ( $fileroot, $dot ) = check_vms_filename($fileroot);
            }

            # add option to change path here
            if ( defined( $rOpts->{'output-path'} ) ) {

                my ( $base, $old_path ) = fileparse($fileroot);
                my $new_path = $rOpts->{'output-path'};
                unless ( -d $new_path ) {
                    unless ( mkdir $new_path, 0777 ) {
                        Die("unable to create directory $new_path: $!\n");
                    }
                }
                my $path = $new_path;
                $fileroot = catfile( $path, $base );
                unless ($fileroot) {
                    Die(<<EOM);
------------------------------------------------------------------------
Problem combining $new_path and $base to make a filename; check -opath
------------------------------------------------------------------------
EOM
                }
            }
        }

        # Skip files with same extension as the output files because
        # this can lead to a messy situation with files like
        # script.tdy.tdy.tdy ... or worse problems ...  when you
        # rerun perltidy over and over with wildcard input.
        if (
            !$source_stream
            && (   $input_file =~ /$forbidden_file_extensions/o
                || $input_file eq 'DIAGNOSTICS' )
          )
        {
            Warn("skipping file: $input_file: wrong extension\n");
            next;
        }

        # the 'source_object' supplies a method to read the input file
        my $source_object =
          Perl::Tidy::LineSource->new( $input_file, $rOpts,
            $rpending_logfile_message );
        next unless ($source_object);

        # Prefilters and postfilters: The prefilter is a code reference
        # that will be applied to the source before tidying, and the
        # postfilter is a code reference to the result before outputting.
        if (
            $prefilter
            || (   $rOpts_character_encoding
                && $rOpts_character_encoding eq 'utf8' )
          )
        {
            my $buf = '';
            while ( my $line = $source_object->get_line() ) {
                $buf .= $line;
            }

            $buf = $prefilter->($buf) if $prefilter;

            if (   $rOpts_character_encoding
                && $rOpts_character_encoding eq 'utf8'
                && !utf8::is_utf8($buf) )
            {
                eval {
                    $buf = Encode::decode( 'UTF-8', $buf,
                        Encode::FB_CROAK | Encode::LEAVE_SRC );
                };
                if ($@) {
                    Warn(
"skipping file: $input_file: Unable to decode source as UTF-8\n"
                    );
                    next;
                }
            }

            $source_object = Perl::Tidy::LineSource->new( \$buf, $rOpts,
                $rpending_logfile_message );
        }

        # register this file name with the Diagnostics package
        $diagnostics_object->set_input_file($input_file)
          if $diagnostics_object;

        #---------------------------------------------------------------
        # prepare the output stream
        #---------------------------------------------------------------
        my $output_file = undef;
        my $actual_output_extension;

        if ( $rOpts->{'outfile'} ) {

            if ( $number_of_files <= 1 ) {

                if ( $rOpts->{'standard-output'} ) {
                    my $msg = "You may not use -o and -st together";
                    $msg .= " (-pbp contains -st; see manual)" if ($saw_pbp);
                    Die("$msg\n");
                }
                elsif ($destination_stream) {
                    Die(
"You may not specify a destination array and -o together\n"
                    );
                }
                elsif ( defined( $rOpts->{'output-path'} ) ) {
                    Die("You may not specify -o and -opath together\n");
                }
                elsif ( defined( $rOpts->{'output-file-extension'} ) ) {
                    Die("You may not specify -o and -oext together\n");
                }
                $output_file = $rOpts->{outfile};

                # make sure user gives a file name after -o
                if ( $output_file =~ /^-/ ) {
                    Die("You must specify a valid filename after -o\n");
                }

                # do not overwrite input file with -o
                if ( defined($input_file_permissions)
                    && ( $output_file eq $input_file ) )
                {
                    Die("Use 'perltidy -b $input_file' to modify in-place\n");
                }
            }
            else {
                Die("You may not use -o with more than one input file\n");
            }
        }
        elsif ( $rOpts->{'standard-output'} ) {
            if ($destination_stream) {
                my $msg =
                  "You may not specify a destination array and -st together\n";
                $msg .= " (-pbp contains -st; see manual)" if ($saw_pbp);
                Die("$msg\n");
            }
            $output_file = '-';

            if ( $number_of_files <= 1 ) {
            }
            else {
                Die("You may not use -st with more than one input file\n");
            }
        }
        elsif ($destination_stream) {
            $output_file = $destination_stream;
        }
        elsif ($source_stream) {    # source but no destination goes to stdout
            $output_file = '-';
        }
        elsif ( $input_file eq '-' ) {
            $output_file = '-';
        }
        else {
            if ($in_place_modify) {
                $output_file = IO::File->new_tmpfile()
                  or Die("cannot open temp file for -b option: $!\n");
            }
            else {
                $actual_output_extension = $output_extension;
                $output_file             = $fileroot . $output_extension;
            }
        }

        # the 'sink_object' knows how to write the output file
        my $tee_file = $fileroot . $dot . "TEE";

        my $line_separator = $rOpts->{'output-line-ending'};
        if ( $rOpts->{'preserve-line-endings'} ) {
            $line_separator = find_input_line_ending($input_file);
        }

        # Eventually all I/O may be done with binmode, but for now it is
        # only done when a user requests a particular line separator
        # through the -ple or -ole flags
        my $binmode = defined($line_separator)
          || defined($rOpts_character_encoding);
        $line_separator = "\n" unless defined($line_separator);

        my ( $sink_object, $postfilter_buffer );
        if ($postfilter) {
            $sink_object =
              Perl::Tidy::LineSink->new( \$postfilter_buffer, $tee_file,
                $line_separator, $rOpts, $rpending_logfile_message, $binmode );
        }
        else {
            $sink_object =
              Perl::Tidy::LineSink->new( $output_file, $tee_file,
                $line_separator, $rOpts, $rpending_logfile_message, $binmode );
        }

        #---------------------------------------------------------------
        # initialize the error logger for this file
        #---------------------------------------------------------------
        my $warning_file = $fileroot . $dot . "ERR";
        if ($errorfile_stream) { $warning_file = $errorfile_stream }
        my $log_file = $fileroot . $dot . "LOG";
        if ($logfile_stream) { $log_file = $logfile_stream }

        my $logger_object =
          Perl::Tidy::Logger->new( $rOpts, $log_file, $warning_file,
            $fh_stderr, $saw_extrude );
        write_logfile_header(
            $rOpts,        $logger_object, $config_file,
            $rraw_options, $Windows_type,  $readable_options,
        );
        if ( ${$rpending_logfile_message} ) {
            $logger_object->write_logfile_entry( ${$rpending_logfile_message} );
        }
        if ( ${$rpending_complaint} ) {
            $logger_object->complain( ${$rpending_complaint} );
        }

        #---------------------------------------------------------------
        # initialize the debug object, if any
        #---------------------------------------------------------------
        my $debugger_object = undef;
        if ( $rOpts->{DEBUG} ) {
            $debugger_object =
              Perl::Tidy::Debugger->new( $fileroot . $dot . "DEBUG" );
        }

        #---------------------------------------------------------------
        # loop over iterations for one source stream
        #---------------------------------------------------------------

        # We will do a convergence test if 3 or more iterations are allowed.
        # It would be pointless for fewer because we have to make at least
        # two passes before we can see if we are converged, and the test
        # would just slow things down.
        my $max_iterations = $rOpts->{'iterations'};
        my $convergence_log_message;
        my %saw_md5;
        my $do_convergence_test = $max_iterations > 2;

        # Since Digest::MD5 qw(md5_hex) has been in the earliest version of Perl
        # we are requiring (5.8), I have commented out this check
##?        if ($do_convergence_test) {
##?            eval "use Digest::MD5 qw(md5_hex)";
##?            $do_convergence_test = !$@;
##?
##?            ### Trying to avoid problems with ancient versions of perl
##?            ##eval { my $string = "perltidy"; utf8::encode($string) };
##?            ##$do_convergence_test = $do_convergence_test && !$@;
##?        }

        # save objects to allow redirecting output during iterations
        my $sink_object_final     = $sink_object;
        my $debugger_object_final = $debugger_object;
        my $logger_object_final   = $logger_object;

        foreach my $iter ( 1 .. $max_iterations ) {

            # send output stream to temp buffers until last iteration
            my $sink_buffer;
            if ( $iter < $max_iterations ) {
                $sink_object =
                  Perl::Tidy::LineSink->new( \$sink_buffer, $tee_file,
                    $line_separator, $rOpts, $rpending_logfile_message,
                    $binmode );
            }
            else {
                $sink_object = $sink_object_final;
            }

            # Save logger, debugger output only on pass 1 because:
            # (1) line number references must be to the starting
            # source, not an intermediate result, and
            # (2) we need to know if there are errors so we can stop the
            # iterations early if necessary.
            if ( $iter > 1 ) {
                $debugger_object = undef;
                $logger_object   = undef;
            }

            #------------------------------------------------------------
            # create a formatter for this file : html writer or
            # pretty printer
            #------------------------------------------------------------

            # we have to delete any old formatter because, for safety,
            # the formatter will check to see that there is only one.
            $formatter = undef;

            if ($user_formatter) {
                $formatter = $user_formatter;
            }
            elsif ( $rOpts->{'format'} eq 'html' ) {
                $formatter =
                  Perl::Tidy::HtmlWriter->new( $fileroot, $output_file,
                    $actual_output_extension, $html_toc_extension,
                    $html_src_extension );
            }
            elsif ( $rOpts->{'format'} eq 'tidy' ) {
                $formatter = Perl::Tidy::Formatter->new(
                    logger_object      => $logger_object,
                    diagnostics_object => $diagnostics_object,
                    sink_object        => $sink_object,
                );
            }
            else {
                Die("I don't know how to do -format=$rOpts->{'format'}\n");
            }

            unless ($formatter) {
                Die("Unable to continue with $rOpts->{'format'} formatting\n");
            }

            #---------------------------------------------------------------
            # create the tokenizer for this file
            #---------------------------------------------------------------
            $tokenizer = undef;                     # must destroy old tokenizer
            $tokenizer = Perl::Tidy::Tokenizer->new(
                source_object      => $source_object,
                logger_object      => $logger_object,
                debugger_object    => $debugger_object,
                diagnostics_object => $diagnostics_object,
                tabsize            => $tabsize,

                starting_level      => $rOpts->{'starting-indentation-level'},
                indent_columns      => $rOpts->{'indent-columns'},
                look_for_hash_bang  => $rOpts->{'look-for-hash-bang'},
                look_for_autoloader => $rOpts->{'look-for-autoloader'},
                look_for_selfloader => $rOpts->{'look-for-selfloader'},
                trim_qw             => $rOpts->{'trim-qw'},
                extended_syntax     => $rOpts->{'extended-syntax'},

                continuation_indentation =>
                  $rOpts->{'continuation-indentation'},
                outdent_labels => $rOpts->{'outdent-labels'},
            );

            #---------------------------------------------------------------
            # now we can do it
            #---------------------------------------------------------------
            process_this_file( $tokenizer, $formatter );

            #---------------------------------------------------------------
            # close the input source and report errors
            #---------------------------------------------------------------
            $source_object->close_input_file();

            # line source for next iteration (if any) comes from the current
            # temporary output buffer
            if ( $iter < $max_iterations ) {

                $sink_object->close_output_file();
                $source_object =
                  Perl::Tidy::LineSource->new( \$sink_buffer, $rOpts,
                    $rpending_logfile_message );

                # stop iterations if errors or converged
                #my $stop_now = $logger_object->{_warning_count};
                my $stop_now = $tokenizer->report_tokenization_errors();
                if ($stop_now) {
                    $convergence_log_message = <<EOM;
Stopping iterations because of severe errors.                       
EOM
                }
                elsif ($do_convergence_test) {

                    # Patch for [rt.cpan.org #88020]
                    # Use utf8::encode since md5_hex() only operates on bytes.
                    # my $digest = md5_hex( utf8::encode($sink_buffer) );

                    # Note added 20180114: this patch did not work correctly.
                    # I'm not sure why.  But switching to the method
                    # recommended in the Perl 5 documentation for Encode
                    # worked.  According to this we can either use
                    #    $octets = encode_utf8($string)  or equivalently
                    #    $octets = encode("utf8",$string)
                    # and then calculate the checksum.  So:
                    my $octets = Encode::encode( "utf8", $sink_buffer );
                    my $digest = md5_hex($octets);
                    if ( !$saw_md5{$digest} ) {
                        $saw_md5{$digest} = $iter;
                    }
                    else {

                        # Deja vu, stop iterating
                        $stop_now = 1;
                        my $iterm = $iter - 1;
                        if ( $saw_md5{$digest} != $iterm ) {

                            # Blinking (oscillating) between two stable
                            # end states.  This has happened in the past
                            # but at present there are no known instances.
                            $convergence_log_message = <<EOM;
Blinking. Output for iteration $iter same as for $saw_md5{$digest}. 
EOM
                            $diagnostics_object->write_diagnostics(
                                $convergence_log_message)
                              if $diagnostics_object;
                        }
                        else {
                            $convergence_log_message = <<EOM;
Converged.  Output for iteration $iter same as for iter $iterm.
EOM
                            $diagnostics_object->write_diagnostics(
                                $convergence_log_message)
                              if $diagnostics_object && $iterm > 2;
                        }
                    }
                } ## end if ($do_convergence_test)

                if ($stop_now) {

                    # we are stopping the iterations early;
                    # copy the output stream to its final destination
                    $sink_object = $sink_object_final;
                    while ( my $line = $source_object->get_line() ) {
                        $sink_object->write_line($line);
                    }
                    $source_object->close_input_file();
                    last;
                }
            } ## end if ( $iter < $max_iterations)
        }    # end loop over iterations for one source file

        # restore objects which have been temporarily undefined
        # for second and higher iterations
        $debugger_object = $debugger_object_final;
        $logger_object   = $logger_object_final;

        $logger_object->write_logfile_entry($convergence_log_message)
          if $convergence_log_message;

        #---------------------------------------------------------------
        # Perform any postfilter operation
        #---------------------------------------------------------------
        if ($postfilter) {
            $sink_object->close_output_file();
            $sink_object =
              Perl::Tidy::LineSink->new( $output_file, $tee_file,
                $line_separator, $rOpts, $rpending_logfile_message, $binmode );
            my $buf = $postfilter->($postfilter_buffer);
            $source_object =
              Perl::Tidy::LineSource->new( \$buf, $rOpts,
                $rpending_logfile_message );
            while ( my $line = $source_object->get_line() ) {
                $sink_object->write_line($line);
            }
            $source_object->close_input_file();
        }

        # Save names of the input and output files for syntax check
        my $ifname = $input_file;
        my $ofname = $output_file;

        #---------------------------------------------------------------
        # handle the -b option (backup and modify in-place)
        #---------------------------------------------------------------
        if ($in_place_modify) {
            unless ( -f $input_file ) {

                # oh, oh, no real file to backup ..
                # shouldn't happen because of numerous preliminary checks
                Die(
"problem with -b backing up input file '$input_file': not a file\n"
                );
            }
            my $backup_name = $input_file . $backup_extension;
            if ( -f $backup_name ) {
                unlink($backup_name)
                  or Die(
"unable to remove previous '$backup_name' for -b option; check permissions: $!\n"
                  );
            }

            # backup the input file
            # we use copy for symlinks, move for regular files
            if ( -l $input_file ) {
                File::Copy::copy( $input_file, $backup_name )
                  or Die("File::Copy failed trying to backup source: $!");
            }
            else {
                rename( $input_file, $backup_name )
                  or Die(
"problem renaming $input_file to $backup_name for -b option: $!\n"
                  );
            }
            $ifname = $backup_name;

            # copy the output to the original input file
            # NOTE: it would be nice to just close $output_file and use
            # File::Copy::copy here, but in this case $output_file is the
            # handle of an open nameless temporary file so we would lose
            # everything if we closed it.
            seek( $output_file, 0, 0 )
              or Die("unable to rewind a temporary file for -b option: $!\n");
            my $fout = IO::File->new("> $input_file")
              or Die(
"problem re-opening $input_file for write for -b option; check file and directory permissions: $!\n"
              );
            if ($binmode) {
                if (   $rOpts->{'character-encoding'}
                    && $rOpts->{'character-encoding'} eq 'utf8' )
                {
                    binmode $fout, ":encoding(UTF-8)";
                }
                else { binmode $fout }
            }
            my $line;
            while ( $line = $output_file->getline() ) {
                $fout->print($line);
            }
            $fout->close();
            $output_file = $input_file;
            $ofname      = $input_file;
        }

        #---------------------------------------------------------------
        # clean up and report errors
        #---------------------------------------------------------------
        $sink_object->close_output_file()    if $sink_object;
        $debugger_object->close_debug_file() if $debugger_object;

        # set output file permissions
        if ( $output_file && -f $output_file && !-l $output_file ) {
            if ($input_file_permissions) {

                # give output script same permissions as input script, but
                # make it user-writable or else we can't run perltidy again.
                # Thus we retain whatever executable flags were set.
                if ( $rOpts->{'format'} eq 'tidy' ) {
                    chmod( $input_file_permissions | oct(600), $output_file );
                }

                # else use default permissions for html and any other format
            }
        }

        #---------------------------------------------------------------
        # Do syntax check if requested and possible
        #---------------------------------------------------------------
        my $infile_syntax_ok = 0;    # -1 no  0=don't know   1 yes
        if (   $logger_object
            && $rOpts->{'check-syntax'}
            && $ifname
            && $ofname )
        {
            $infile_syntax_ok =
              check_syntax( $ifname, $ofname, $logger_object, $rOpts );
        }

        #---------------------------------------------------------------
        # remove the original file for in-place modify as follows:
        #   $delete_backup=0 never
        #   $delete_backup=1 only if no errors
        #   $delete_backup>1 always  : NOT ALLOWED, too risky, see above
        #---------------------------------------------------------------
        if (   $in_place_modify
            && $delete_backup
            && -f $ifname
            && ( $delete_backup > 1 || !$logger_object->{_warning_count} ) )
        {

            # As an added safety precaution, do not delete the source file
            # if its size has dropped from positive to zero, since this
            # could indicate a disaster of some kind, including a hardware
            # failure.  Actually, this could happen if you had a file of
            # all comments (or pod) and deleted everything with -dac (-dap)
            # for some reason.
            if ( !-s $output_file && -s $ifname && $delete_backup == 1 ) {
                Warn(
"output file '$output_file' missing or zero length; original '$ifname' not deleted\n"
                );
            }
            else {
                unlink($ifname)
                  or Die(
"unable to remove previous '$ifname' for -b option; check permissions: $!\n"
                  );
            }
        }

        $logger_object->finish( $infile_syntax_ok, $formatter )
          if $logger_object;
    }    # end of main loop to process all files

  NORMAL_EXIT:
    return 0;

  ERROR_EXIT:
    return 1;
}    # end of main program perltidy

sub get_stream_as_named_file {

    # Return the name of a file containing a stream of data, creating
    # a temporary file if necessary.
    # Given:
    #  $stream - the name of a file or stream
    # Returns:
    #  $fname = name of file if possible, or undef
    #  $if_tmpfile = true if temp file, undef if not temp file
    #
    # This routine is needed for passing actual files to Perl for
    # a syntax check.
    my ($stream) = @_;
    my $is_tmpfile;
    my $fname;
    if ($stream) {
        if ( ref($stream) ) {
            my ( $fh_stream, $fh_name ) =
              Perl::Tidy::streamhandle( $stream, 'r' );
            if ($fh_stream) {
                my ( $fout, $tmpnam ) = File::Temp::tempfile();
                if ($fout) {
                    $fname      = $tmpnam;
                    $is_tmpfile = 1;
                    binmode $fout;
                    while ( my $line = $fh_stream->getline() ) {
                        $fout->print($line);
                    }
                    $fout->close();
                }
                $fh_stream->close();
            }
        }
        elsif ( $stream ne '-' && -f $stream ) {
            $fname = $stream;
        }
    }
    return ( $fname, $is_tmpfile );
}

sub fileglob_to_re {

    # modified (corrected) from version in find2perl
    my $x = shift;
    $x =~ s#([./^\$()])#\\$1#g;    # escape special characters
    $x =~ s#\*#.*#g;               # '*' -> '.*'
    $x =~ s#\?#.#g;                # '?' -> '.'
    return "^$x\\z";               # match whole word
}

sub make_extension {

    # Make a file extension, including any leading '.' if necessary
    # The '.' may actually be an '_' under VMS
    my ( $extension, $default, $dot ) = @_;

    # Use the default if none specified
    $extension = $default unless ($extension);

    # Only extensions with these leading characters get a '.'
    # This rule gives the user some freedom
    if ( $extension =~ /^[a-zA-Z0-9]/ ) {
        $extension = $dot . $extension;
    }
    return $extension;
}

sub write_logfile_header {
    my (
        $rOpts,        $logger_object, $config_file,
        $rraw_options, $Windows_type,  $readable_options
    ) = @_;
    $logger_object->write_logfile_entry(
"perltidy version $VERSION log file on a $^O system, OLD_PERL_VERSION=$]\n"
    );
    if ($Windows_type) {
        $logger_object->write_logfile_entry("Windows type is $Windows_type\n");
    }
    my $options_string = join( ' ', @{$rraw_options} );

    if ($config_file) {
        $logger_object->write_logfile_entry(
            "Found Configuration File >>> $config_file \n");
    }
    $logger_object->write_logfile_entry(
        "Configuration and command line parameters for this run:\n");
    $logger_object->write_logfile_entry("$options_string\n");

    if ( $rOpts->{'DEBUG'} || $rOpts->{'show-options'} ) {
        $rOpts->{'logfile'} = 1;    # force logfile to be saved
        $logger_object->write_logfile_entry(
            "Final parameter set for this run\n");
        $logger_object->write_logfile_entry(
            "------------------------------------\n");

        $logger_object->write_logfile_entry($readable_options);

        $logger_object->write_logfile_entry(
            "------------------------------------\n");
    }
    $logger_object->write_logfile_entry(
        "To find error messages search for 'WARNING' with your editor\n");
    return;
}

sub generate_options {

    ######################################################################
    # Generate and return references to:
    #  @option_string - the list of options to be passed to Getopt::Long
    #  @defaults - the list of default options
    #  %expansion - a hash showing how all abbreviations are expanded
    #  %category - a hash giving the general category of each option
    #  %option_range - a hash giving the valid ranges of certain options

    # Note: a few options are not documented in the man page and usage
    # message. This is because these are experimental or debug options and
    # may or may not be retained in future versions.
    #
    # Here are the undocumented flags as far as I know.  Any of them
    # may disappear at any time.  They are mainly for fine-tuning
    # and debugging.
    #
    # fll --> fuzzy-line-length           # a trivial parameter which gets
    #                                       turned off for the extrude option
    #                                       which is mainly for debugging
    # scl --> short-concatenation-item-length   # helps break at '.'
    # recombine                           # for debugging line breaks
    # valign                              # for debugging vertical alignment
    # I   --> DIAGNOSTICS                 # for debugging [**DEACTIVATED**]
    ######################################################################

    # here is a summary of the Getopt codes:
    # <none> does not take an argument
    # =s takes a mandatory string
    # :s takes an optional string  (DO NOT USE - filenames will get eaten up)
    # =i takes a mandatory integer
    # :i takes an optional integer (NOT RECOMMENDED - can cause trouble)
    # ! does not take an argument and may be negated
    #  i.e., -foo and -nofoo are allowed
    # a double dash signals the end of the options list
    #
    #---------------------------------------------------------------
    # Define the option string passed to GetOptions.
    #---------------------------------------------------------------

    my @option_string   = ();
    my %expansion       = ();
    my %option_category = ();
    my %option_range    = ();
    my $rexpansion      = \%expansion;

    # names of categories in manual
    # leading integers will allow sorting
    my @category_name = (
        '0. I/O control',
        '1. Basic formatting options',
        '2. Code indentation control',
        '3. Whitespace control',
        '4. Comment controls',
        '5. Linebreak controls',
        '6. Controlling list formatting',
        '7. Retaining or ignoring existing line breaks',
        '8. Blank line control',
        '9. Other controls',
        '10. HTML options',
        '11. pod2html options',
        '12. Controlling HTML properties',
        '13. Debugging',
    );

    #  These options are parsed directly by perltidy:
    #    help h
    #    version v
    #  However, they are included in the option set so that they will
    #  be seen in the options dump.

    # These long option names have no abbreviations or are treated specially
    @option_string = qw(
      html!
      noprofile
      no-profile
      npro
      recombine!
      valign!
      notidy
    );

    my $category = 13;    # Debugging
    foreach (@option_string) {
        my $opt = $_;     # must avoid changing the actual flag
        $opt =~ s/!$//;
        $option_category{$opt} = $category_name[$category];
    }

    $category = 11;                                       # HTML
    $option_category{html} = $category_name[$category];

    # routine to install and check options
    my $add_option = sub {
        my ( $long_name, $short_name, $flag ) = @_;
        push @option_string, $long_name . $flag;
        $option_category{$long_name} = $category_name[$category];
        if ($short_name) {
            if ( $expansion{$short_name} ) {
                my $existing_name = $expansion{$short_name}[0];
                Die(
"redefining abbreviation $short_name for $long_name; already used for $existing_name\n"
                );
            }
            $expansion{$short_name} = [$long_name];
            if ( $flag eq '!' ) {
                my $nshort_name = 'n' . $short_name;
                my $nolong_name = 'no' . $long_name;
                if ( $expansion{$nshort_name} ) {
                    my $existing_name = $expansion{$nshort_name}[0];
                    Die(
"attempting to redefine abbreviation $nshort_name for $nolong_name; already used for $existing_name\n"
                    );
                }
                $expansion{$nshort_name} = [$nolong_name];
            }
        }
    };

    # Install long option names which have a simple abbreviation.
    # Options with code '!' get standard negation ('no' for long names,
    # 'n' for abbreviations).  Categories follow the manual.

    ###########################
    $category = 0;    # I/O_Control
    ###########################
    $add_option->( 'backup-and-modify-in-place', 'b',     '!' );
    $add_option->( 'backup-file-extension',      'bext',  '=s' );
    $add_option->( 'force-read-binary',          'f',     '!' );
    $add_option->( 'format',                     'fmt',   '=s' );
    $add_option->( 'iterations',                 'it',    '=i' );
    $add_option->( 'logfile',                    'log',   '!' );
    $add_option->( 'logfile-gap',                'g',     ':i' );
    $add_option->( 'outfile',                    'o',     '=s' );
    $add_option->( 'output-file-extension',      'oext',  '=s' );
    $add_option->( 'output-path',                'opath', '=s' );
    $add_option->( 'profile',                    'pro',   '=s' );
    $add_option->( 'quiet',                      'q',     '!' );
    $add_option->( 'standard-error-output',      'se',    '!' );
    $add_option->( 'standard-output',            'st',    '!' );
    $add_option->( 'warning-output',             'w',     '!' );
    $add_option->( 'character-encoding',         'enc',   '=s' );

    # options which are both toggle switches and values moved here
    # to hide from tidyview (which does not show category 0 flags):
    # -ole moved here from category 1
    # -sil moved here from category 2
    $add_option->( 'output-line-ending',         'ole', '=s' );
    $add_option->( 'starting-indentation-level', 'sil', '=i' );

    ########################################
    $category = 1;    # Basic formatting options
    ########################################
    $add_option->( 'check-syntax',                 'syn',  '!' );
    $add_option->( 'entab-leading-whitespace',     'et',   '=i' );
    $add_option->( 'indent-columns',               'i',    '=i' );
    $add_option->( 'maximum-line-length',          'l',    '=i' );
    $add_option->( 'variable-maximum-line-length', 'vmll', '!' );
    $add_option->( 'whitespace-cycle',             'wc',   '=i' );
    $add_option->( 'perl-syntax-check-flags',      'pscf', '=s' );
    $add_option->( 'preserve-line-endings',        'ple',  '!' );
    $add_option->( 'tabs',                         't',    '!' );
    $add_option->( 'default-tabsize',              'dt',   '=i' );
    $add_option->( 'extended-syntax',              'xs',   '!' );

    ########################################
    $category = 2;    # Code indentation control
    ########################################
    $add_option->( 'continuation-indentation',           'ci',   '=i' );
    $add_option->( 'line-up-parentheses',                'lp',   '!' );
    $add_option->( 'outdent-keyword-list',               'okwl', '=s' );
    $add_option->( 'outdent-keywords',                   'okw',  '!' );
    $add_option->( 'outdent-labels',                     'ola',  '!' );
    $add_option->( 'outdent-long-quotes',                'olq',  '!' );
    $add_option->( 'indent-closing-brace',               'icb',  '!' );
    $add_option->( 'closing-token-indentation',          'cti',  '=i' );
    $add_option->( 'closing-paren-indentation',          'cpi',  '=i' );
    $add_option->( 'closing-brace-indentation',          'cbi',  '=i' );
    $add_option->( 'closing-square-bracket-indentation', 'csbi', '=i' );
    $add_option->( 'brace-left-and-indent',              'bli',  '!' );
    $add_option->( 'brace-left-and-indent-list',         'blil', '=s' );

    ########################################
    $category = 3;    # Whitespace control
    ########################################
    $add_option->( 'add-semicolons',                            'asc',   '!' );
    $add_option->( 'add-whitespace',                            'aws',   '!' );
    $add_option->( 'block-brace-tightness',                     'bbt',   '=i' );
    $add_option->( 'brace-tightness',                           'bt',    '=i' );
    $add_option->( 'delete-old-whitespace',                     'dws',   '!' );
    $add_option->( 'delete-semicolons',                         'dsm',   '!' );
    $add_option->( 'nospace-after-keyword',                     'nsak',  '=s' );
    $add_option->( 'nowant-left-space',                         'nwls',  '=s' );
    $add_option->( 'nowant-right-space',                        'nwrs',  '=s' );
    $add_option->( 'paren-tightness',                           'pt',    '=i' );
    $add_option->( 'space-after-keyword',                       'sak',   '=s' );
    $add_option->( 'space-for-semicolon',                       'sfs',   '!' );
    $add_option->( 'space-function-paren',                      'sfp',   '!' );
    $add_option->( 'space-keyword-paren',                       'skp',   '!' );
    $add_option->( 'space-terminal-semicolon',                  'sts',   '!' );
    $add_option->( 'square-bracket-tightness',                  'sbt',   '=i' );
    $add_option->( 'square-bracket-vertical-tightness',         'sbvt',  '=i' );
    $add_option->( 'square-bracket-vertical-tightness-closing', 'sbvtc', '=i' );
    $add_option->( 'tight-secret-operators',                    'tso',   '!' );
    $add_option->( 'trim-qw',                                   'tqw',   '!' );
    $add_option->( 'trim-pod',                                  'trp',   '!' );
    $add_option->( 'want-left-space',                           'wls',   '=s' );
    $add_option->( 'want-right-space',                          'wrs',   '=s' );

    ########################################
    $category = 4;    # Comment controls
    ########################################
    $add_option->( 'closing-side-comment-else-flag',    'csce', '=i' );
    $add_option->( 'closing-side-comment-interval',     'csci', '=i' );
    $add_option->( 'closing-side-comment-list',         'cscl', '=s' );
    $add_option->( 'closing-side-comment-maximum-text', 'csct', '=i' );
    $add_option->( 'closing-side-comment-prefix',       'cscp', '=s' );
    $add_option->( 'closing-side-comment-warnings',     'cscw', '!' );
    $add_option->( 'closing-side-comments',             'csc',  '!' );
    $add_option->( 'closing-side-comments-balanced',    'cscb', '!' );
    $add_option->( 'format-skipping',                   'fs',   '!' );
    $add_option->( 'format-skipping-begin',             'fsb',  '=s' );
    $add_option->( 'format-skipping-end',               'fse',  '=s' );
    $add_option->( 'hanging-side-comments',             'hsc',  '!' );
    $add_option->( 'indent-block-comments',             'ibc',  '!' );
    $add_option->( 'indent-spaced-block-comments',      'isbc', '!' );
    $add_option->( 'fixed-position-side-comment',       'fpsc', '=i' );
    $add_option->( 'minimum-space-to-comment',          'msc',  '=i' );
    $add_option->( 'outdent-long-comments',             'olc',  '!' );
    $add_option->( 'outdent-static-block-comments',     'osbc', '!' );
    $add_option->( 'static-block-comment-prefix',       'sbcp', '=s' );
    $add_option->( 'static-block-comments',             'sbc',  '!' );
    $add_option->( 'static-side-comment-prefix',        'sscp', '=s' );
    $add_option->( 'static-side-comments',              'ssc',  '!' );
    $add_option->( 'ignore-side-comment-lengths',       'iscl', '!' );

    ########################################
    $category = 5;    # Linebreak controls
    ########################################
    $add_option->( 'add-newlines',                            'anl',   '!' );
    $add_option->( 'block-brace-vertical-tightness',          'bbvt',  '=i' );
    $add_option->( 'block-brace-vertical-tightness-list',     'bbvtl', '=s' );
    $add_option->( 'brace-vertical-tightness',                'bvt',   '=i' );
    $add_option->( 'brace-vertical-tightness-closing',        'bvtc',  '=i' );
    $add_option->( 'cuddled-else',                            'ce',    '!' );
    $add_option->( 'cuddled-block-list',                      'cbl',   '=s' );
    $add_option->( 'cuddled-block-list-exclusive',            'cblx',  '!' );
    $add_option->( 'cuddled-break-option',                    'cbo',   '=i' );
    $add_option->( 'delete-old-newlines',                     'dnl',   '!' );
    $add_option->( 'opening-brace-always-on-right',           'bar',   '!' );
    $add_option->( 'opening-brace-on-new-line',               'bl',    '!' );
    $add_option->( 'opening-hash-brace-right',                'ohbr',  '!' );
    $add_option->( 'opening-paren-right',                     'opr',   '!' );
    $add_option->( 'opening-square-bracket-right',            'osbr',  '!' );
    $add_option->( 'opening-anonymous-sub-brace-on-new-line', 'asbl',  '!' );
    $add_option->( 'opening-sub-brace-on-new-line',           'sbl',   '!' );
    $add_option->( 'paren-vertical-tightness',                'pvt',   '=i' );
    $add_option->( 'paren-vertical-tightness-closing',        'pvtc',  '=i' );
    $add_option->( 'weld-nested-containers',                  'wn',    '!' );
    $add_option->( 'space-backslash-quote',                   'sbq',   '=i' );
    $add_option->( 'stack-closing-block-brace',               'scbb',  '!' );
    $add_option->( 'stack-closing-hash-brace',                'schb',  '!' );
    $add_option->( 'stack-closing-paren',                     'scp',   '!' );
    $add_option->( 'stack-closing-square-bracket',            'scsb',  '!' );
    $add_option->( 'stack-opening-block-brace',               'sobb',  '!' );
    $add_option->( 'stack-opening-hash-brace',                'sohb',  '!' );
    $add_option->( 'stack-opening-paren',                     'sop',   '!' );
    $add_option->( 'stack-opening-square-bracket',            'sosb',  '!' );
    $add_option->( 'vertical-tightness',                      'vt',    '=i' );
    $add_option->( 'vertical-tightness-closing',              'vtc',   '=i' );
    $add_option->( 'want-break-after',                        'wba',   '=s' );
    $add_option->( 'want-break-before',                       'wbb',   '=s' );
    $add_option->( 'break-after-all-operators',               'baao',  '!' );
    $add_option->( 'break-before-all-operators',              'bbao',  '!' );
    $add_option->( 'keep-interior-semicolons',                'kis',   '!' );

    ########################################
    $category = 6;    # Controlling list formatting
    ########################################
    $add_option->( 'break-at-old-comma-breakpoints', 'boc', '!' );
    $add_option->( 'comma-arrow-breakpoints',        'cab', '=i' );
    $add_option->( 'maximum-fields-per-table',       'mft', '=i' );

    ########################################
    $category = 7;    # Retaining or ignoring existing line breaks
    ########################################
    $add_option->( 'break-at-old-keyword-breakpoints',   'bok', '!' );
    $add_option->( 'break-at-old-logical-breakpoints',   'bol', '!' );
    $add_option->( 'break-at-old-ternary-breakpoints',   'bot', '!' );
    $add_option->( 'break-at-old-attribute-breakpoints', 'boa', '!' );
    $add_option->( 'ignore-old-breakpoints',             'iob', '!' );

    ########################################
    $category = 8;    # Blank line control
    ########################################
    $add_option->( 'blanks-before-blocks',            'bbb',  '!' );
    $add_option->( 'blanks-before-comments',          'bbc',  '!' );
    $add_option->( 'blank-lines-before-subs',         'blbs', '=i' );
    $add_option->( 'blank-lines-before-packages',     'blbp', '=i' );
    $add_option->( 'long-block-line-count',           'lbl',  '=i' );
    $add_option->( 'maximum-consecutive-blank-lines', 'mbl',  '=i' );
    $add_option->( 'keep-old-blank-lines',            'kbl',  '=i' );

    $add_option->( 'blank-lines-after-opening-block',       'blao',  '=i' );
    $add_option->( 'blank-lines-before-closing-block',      'blbc',  '=i' );
    $add_option->( 'blank-lines-after-opening-block-list',  'blaol', '=s' );
    $add_option->( 'blank-lines-before-closing-block-list', 'blbcl', '=s' );

    ########################################
    $category = 9;    # Other controls
    ########################################
    $add_option->( 'delete-block-comments',        'dbc',  '!' );
    $add_option->( 'delete-closing-side-comments', 'dcsc', '!' );
    $add_option->( 'delete-pod',                   'dp',   '!' );
    $add_option->( 'delete-side-comments',         'dsc',  '!' );
    $add_option->( 'tee-block-comments',           'tbc',  '!' );
    $add_option->( 'tee-pod',                      'tp',   '!' );
    $add_option->( 'tee-side-comments',            'tsc',  '!' );
    $add_option->( 'look-for-autoloader',          'lal',  '!' );
    $add_option->( 'look-for-hash-bang',           'x',    '!' );
    $add_option->( 'look-for-selfloader',          'lsl',  '!' );
    $add_option->( 'pass-version-line',            'pvl',  '!' );

    ########################################
    $category = 13;    # Debugging
    ########################################
##  $add_option->( 'DIAGNOSTICS',                     'I',    '!' );
    $add_option->( 'DEBUG',                           'D',    '!' );
    $add_option->( 'dump-cuddled-block-list',         'dcbl', '!' );
    $add_option->( 'dump-defaults',                   'ddf',  '!' );
    $add_option->( 'dump-long-names',                 'dln',  '!' );
    $add_option->( 'dump-options',                    'dop',  '!' );
    $add_option->( 'dump-profile',                    'dpro', '!' );
    $add_option->( 'dump-short-names',                'dsn',  '!' );
    $add_option->( 'dump-token-types',                'dtt',  '!' );
    $add_option->( 'dump-want-left-space',            'dwls', '!' );
    $add_option->( 'dump-want-right-space',           'dwrs', '!' );
    $add_option->( 'fuzzy-line-length',               'fll',  '!' );
    $add_option->( 'help',                            'h',    '' );
    $add_option->( 'short-concatenation-item-length', 'scl',  '=i' );
    $add_option->( 'show-options',                    'opt',  '!' );
    $add_option->( 'timestamp',                       'ts',   '!' );
    $add_option->( 'version',                         'v',    '' );
    $add_option->( 'memoize',                         'mem',  '!' );
    $add_option->( 'file-size-order',                 'fso',  '!' );

    #---------------------------------------------------------------------

    # The Perl::Tidy::HtmlWriter will add its own options to the string
    Perl::Tidy::HtmlWriter->make_getopt_long_names( \@option_string );

    ########################################
    # Set categories 10, 11, 12
    ########################################
    # Based on their known order
    $category = 12;    # HTML properties
    foreach my $opt (@option_string) {
        my $long_name = $opt;
        $long_name =~ s/(!|=.*|:.*)$//;
        unless ( defined( $option_category{$long_name} ) ) {
            if ( $long_name =~ /^html-linked/ ) {
                $category = 10;    # HTML options
            }
            elsif ( $long_name =~ /^pod2html/ ) {
                $category = 11;    # Pod2html
            }
            $option_category{$long_name} = $category_name[$category];
        }
    }

    #---------------------------------------------------------------
    # Assign valid ranges to certain options
    #---------------------------------------------------------------
    # In the future, these may be used to make preliminary checks
    # hash keys are long names
    # If key or value is undefined:
    #   strings may have any value
    #   integer ranges are >=0
    # If value is defined:
    #   value is [qw(any valid words)] for strings
    #   value is [min, max] for integers
    #   if min is undefined, there is no lower limit
    #   if max is undefined, there is no upper limit
    # Parameters not listed here have defaults
    %option_range = (
        'format'             => [ 'tidy', 'html', 'user' ],
        'output-line-ending' => [ 'dos',  'win',  'mac', 'unix' ],
        'character-encoding' => [ 'none', 'utf8' ],

        'space-backslash-quote' => [ 0, 2 ],

        'block-brace-tightness'    => [ 0, 2 ],
        'brace-tightness'          => [ 0, 2 ],
        'paren-tightness'          => [ 0, 2 ],
        'square-bracket-tightness' => [ 0, 2 ],

        'block-brace-vertical-tightness'            => [ 0, 2 ],
        'brace-vertical-tightness'                  => [ 0, 2 ],
        'brace-vertical-tightness-closing'          => [ 0, 2 ],
        'paren-vertical-tightness'                  => [ 0, 2 ],
        'paren-vertical-tightness-closing'          => [ 0, 2 ],
        'square-bracket-vertical-tightness'         => [ 0, 2 ],
        'square-bracket-vertical-tightness-closing' => [ 0, 2 ],
        'vertical-tightness'                        => [ 0, 2 ],
        'vertical-tightness-closing'                => [ 0, 2 ],

        'closing-brace-indentation'          => [ 0, 3 ],
        'closing-paren-indentation'          => [ 0, 3 ],
        'closing-square-bracket-indentation' => [ 0, 3 ],
        'closing-token-indentation'          => [ 0, 3 ],

        'closing-side-comment-else-flag' => [ 0, 2 ],
        'comma-arrow-breakpoints'        => [ 0, 5 ],
    );

    # Note: we could actually allow negative ci if someone really wants it:
    # $option_range{'continuation-indentation'} = [ undef, undef ];

    #---------------------------------------------------------------
    # Assign default values to the above options here, except
    # for 'outfile' and 'help'.
    # These settings should approximate the perlstyle(1) suggestions.
    #---------------------------------------------------------------
    my @defaults = qw(
      add-newlines
      add-semicolons
      add-whitespace
      blanks-before-blocks
      blanks-before-comments
      blank-lines-before-subs=1
      blank-lines-before-packages=1
      block-brace-tightness=0
      block-brace-vertical-tightness=0
      brace-tightness=1
      brace-vertical-tightness-closing=0
      brace-vertical-tightness=0
      break-at-old-logical-breakpoints
      break-at-old-ternary-breakpoints
      break-at-old-attribute-breakpoints
      break-at-old-keyword-breakpoints
      comma-arrow-breakpoints=5
      nocheck-syntax
      closing-side-comment-interval=6
      closing-side-comment-maximum-text=20
      closing-side-comment-else-flag=0
      closing-side-comments-balanced
      closing-paren-indentation=0
      closing-brace-indentation=0
      closing-square-bracket-indentation=0
      continuation-indentation=2
      cuddled-break-option=1
      delete-old-newlines
      delete-semicolons
      extended-syntax
      fuzzy-line-length
      hanging-side-comments
      indent-block-comments
      indent-columns=4
      iterations=1
      keep-old-blank-lines=1
      long-block-line-count=8
      look-for-autoloader
      look-for-selfloader
      maximum-consecutive-blank-lines=1
      maximum-fields-per-table=0
      maximum-line-length=80
      memoize
      minimum-space-to-comment=4
      nobrace-left-and-indent
      nocuddled-else
      nodelete-old-whitespace
      nohtml
      nologfile
      noquiet
      noshow-options
      nostatic-side-comments
      notabs
      nowarning-output
      character-encoding=none
      outdent-labels
      outdent-long-quotes
      outdent-long-comments
      paren-tightness=1
      paren-vertical-tightness-closing=0
      paren-vertical-tightness=0
      pass-version-line
      noweld-nested-containers
      recombine
      valign
      short-concatenation-item-length=8
      space-for-semicolon
      space-backslash-quote=1
      square-bracket-tightness=1
      square-bracket-vertical-tightness-closing=0
      square-bracket-vertical-tightness=0
      static-block-comments
      timestamp
      trim-qw
      format=tidy
      backup-file-extension=bak
      format-skipping
      default-tabsize=8

      pod2html
      html-table-of-contents
      html-entities
    );

    push @defaults, "perl-syntax-check-flags=-c -T";

    #---------------------------------------------------------------
    # Define abbreviations which will be expanded into the above primitives.
    # These may be defined recursively.
    #---------------------------------------------------------------
    %expansion = (
        %expansion,
        'freeze-newlines'   => [qw(noadd-newlines nodelete-old-newlines)],
        'fnl'               => [qw(freeze-newlines)],
        'freeze-whitespace' => [qw(noadd-whitespace nodelete-old-whitespace)],
        'fws'               => [qw(freeze-whitespace)],
        'freeze-blank-lines' =>
          [qw(maximum-consecutive-blank-lines=0 keep-old-blank-lines=2)],
        'fbl'                => [qw(freeze-blank-lines)],
        'indent-only'        => [qw(freeze-newlines freeze-whitespace)],
        'outdent-long-lines' => [qw(outdent-long-quotes outdent-long-comments)],
        'nooutdent-long-lines' =>
          [qw(nooutdent-long-quotes nooutdent-long-comments)],
        'noll' => [qw(nooutdent-long-lines)],
        'io'   => [qw(indent-only)],
        'delete-all-comments' =>
          [qw(delete-block-comments delete-side-comments delete-pod)],
        'nodelete-all-comments' =>
          [qw(nodelete-block-comments nodelete-side-comments nodelete-pod)],
        'dac'  => [qw(delete-all-comments)],
        'ndac' => [qw(nodelete-all-comments)],
        'gnu'  => [qw(gnu-style)],
        'pbp'  => [qw(perl-best-practices)],
        'tee-all-comments' =>
          [qw(tee-block-comments tee-side-comments tee-pod)],
        'notee-all-comments' =>
          [qw(notee-block-comments notee-side-comments notee-pod)],
        'tac'   => [qw(tee-all-comments)],
        'ntac'  => [qw(notee-all-comments)],
        'html'  => [qw(format=html)],
        'nhtml' => [qw(format=tidy)],
        'tidy'  => [qw(format=tidy)],

        # -cb is now a synonym for -ce
        'cb'             => [qw(cuddled-else)],
        'cuddled-blocks' => [qw(cuddled-else)],

        'utf8' => [qw(character-encoding=utf8)],
        'UTF8' => [qw(character-encoding=utf8)],

        'swallow-optional-blank-lines'   => [qw(kbl=0)],
        'noswallow-optional-blank-lines' => [qw(kbl=1)],
        'sob'                            => [qw(kbl=0)],
        'nsob'                           => [qw(kbl=1)],

        'break-after-comma-arrows'   => [qw(cab=0)],
        'nobreak-after-comma-arrows' => [qw(cab=1)],
        'baa'                        => [qw(cab=0)],
        'nbaa'                       => [qw(cab=1)],

        'blanks-before-subs'   => [qw(blbs=1 blbp=1)],
        'bbs'                  => [qw(blbs=1 blbp=1)],
        'noblanks-before-subs' => [qw(blbs=0 blbp=0)],
        'nbbs'                 => [qw(blbs=0 blbp=0)],

        'break-at-old-trinary-breakpoints' => [qw(bot)],

        'cti=0' => [qw(cpi=0 cbi=0 csbi=0)],
        'cti=1' => [qw(cpi=1 cbi=1 csbi=1)],
        'cti=2' => [qw(cpi=2 cbi=2 csbi=2)],
        'icp'   => [qw(cpi=2 cbi=2 csbi=2)],
        'nicp'  => [qw(cpi=0 cbi=0 csbi=0)],

        'closing-token-indentation=0' => [qw(cpi=0 cbi=0 csbi=0)],
        'closing-token-indentation=1' => [qw(cpi=1 cbi=1 csbi=1)],
        'closing-token-indentation=2' => [qw(cpi=2 cbi=2 csbi=2)],
        'indent-closing-paren'        => [qw(cpi=2 cbi=2 csbi=2)],
        'noindent-closing-paren'      => [qw(cpi=0 cbi=0 csbi=0)],

        'vt=0' => [qw(pvt=0 bvt=0 sbvt=0)],
        'vt=1' => [qw(pvt=1 bvt=1 sbvt=1)],
        'vt=2' => [qw(pvt=2 bvt=2 sbvt=2)],

        'vertical-tightness=0' => [qw(pvt=0 bvt=0 sbvt=0)],
        'vertical-tightness=1' => [qw(pvt=1 bvt=1 sbvt=1)],
        'vertical-tightness=2' => [qw(pvt=2 bvt=2 sbvt=2)],

        'vtc=0' => [qw(pvtc=0 bvtc=0 sbvtc=0)],
        'vtc=1' => [qw(pvtc=1 bvtc=1 sbvtc=1)],
        'vtc=2' => [qw(pvtc=2 bvtc=2 sbvtc=2)],

        'vertical-tightness-closing=0' => [qw(pvtc=0 bvtc=0 sbvtc=0)],
        'vertical-tightness-closing=1' => [qw(pvtc=1 bvtc=1 sbvtc=1)],
        'vertical-tightness-closing=2' => [qw(pvtc=2 bvtc=2 sbvtc=2)],

        'otr'                   => [qw(opr ohbr osbr)],
        'opening-token-right'   => [qw(opr ohbr osbr)],
        'notr'                  => [qw(nopr nohbr nosbr)],
        'noopening-token-right' => [qw(nopr nohbr nosbr)],

        'sot'                    => [qw(sop sohb sosb)],
        'nsot'                   => [qw(nsop nsohb nsosb)],
        'stack-opening-tokens'   => [qw(sop sohb sosb)],
        'nostack-opening-tokens' => [qw(nsop nsohb nsosb)],

        'sct'                    => [qw(scp schb scsb)],
        'stack-closing-tokens'   => => [qw(scp schb scsb)],
        'nsct'                   => [qw(nscp nschb nscsb)],
        'nostack-closing-tokens' => [qw(nscp nschb nscsb)],

        'sac'                    => [qw(sot sct)],
        'nsac'                   => [qw(nsot nsct)],
        'stack-all-containers'   => [qw(sot sct)],
        'nostack-all-containers' => [qw(nsot nsct)],

        'act=0'                      => [qw(pt=0 sbt=0 bt=0 bbt=0)],
        'act=1'                      => [qw(pt=1 sbt=1 bt=1 bbt=1)],
        'act=2'                      => [qw(pt=2 sbt=2 bt=2 bbt=2)],
        'all-containers-tightness=0' => [qw(pt=0 sbt=0 bt=0 bbt=0)],
        'all-containers-tightness=1' => [qw(pt=1 sbt=1 bt=1 bbt=1)],
        'all-containers-tightness=2' => [qw(pt=2 sbt=2 bt=2 bbt=2)],

        'stack-opening-block-brace'   => [qw(bbvt=2 bbvtl=*)],
        'sobb'                        => [qw(bbvt=2 bbvtl=*)],
        'nostack-opening-block-brace' => [qw(bbvt=0)],
        'nsobb'                       => [qw(bbvt=0)],

        'converge'   => [qw(it=4)],
        'noconverge' => [qw(it=1)],
        'conv'       => [qw(it=4)],
        'nconv'      => [qw(it=1)],

        # 'mangle' originally deleted pod and comments, but to keep it
        # reversible, it no longer does.  But if you really want to
        # delete them, just use:
        #   -mangle -dac

        # An interesting use for 'mangle' is to do this:
        #    perltidy -mangle myfile.pl -st | perltidy -o myfile.pl.new
        # which will form as many one-line blocks as possible

        'mangle' => [
            qw(
              check-syntax
              keep-old-blank-lines=0
              delete-old-newlines
              delete-old-whitespace
              delete-semicolons
              indent-columns=0
              maximum-consecutive-blank-lines=0
              maximum-line-length=100000
              noadd-newlines
              noadd-semicolons
              noadd-whitespace
              noblanks-before-blocks
              blank-lines-before-subs=0
              blank-lines-before-packages=0
              notabs
              )
        ],

        # 'extrude' originally deleted pod and comments, but to keep it
        # reversible, it no longer does.  But if you really want to
        # delete them, just use
        #   extrude -dac
        #
        # An interesting use for 'extrude' is to do this:
        #    perltidy -extrude myfile.pl -st | perltidy -o myfile.pl.new
        # which will break up all one-line blocks.
        #
        # Removed 'check-syntax' option, which is unsafe because it may execute
        # code in BEGIN blocks.  Example 'Moose/debugger-duck_type.t'.

        'extrude' => [
            qw(
              ci=0
              delete-old-newlines
              delete-old-whitespace
              delete-semicolons
              indent-columns=0
              maximum-consecutive-blank-lines=0
              maximum-line-length=1
              noadd-semicolons
              noadd-whitespace
              noblanks-before-blocks
              blank-lines-before-subs=0
              blank-lines-before-packages=0
              nofuzzy-line-length
              notabs
              norecombine
              )
        ],

        # this style tries to follow the GNU Coding Standards (which do
        # not really apply to perl but which are followed by some perl
        # programmers).
        'gnu-style' => [
            qw(
              lp bl noll pt=2 bt=2 sbt=2 cpi=1 csbi=1 cbi=1
              )
        ],

        # Style suggested in Damian Conway's Perl Best Practices
        'perl-best-practices' => [
            qw(l=78 i=4 ci=4 st se vt=2 cti=0 pt=1 bt=1 sbt=1 bbt=1 nsfs nolq),
q(wbb=% + - * / x != == >= <= =~ !~ < > | & = **= += *= &= <<= &&= -= /= |= >>= ||= //= .= %= ^= x=)
        ],

        # Additional styles can be added here
    );

    Perl::Tidy::HtmlWriter->make_abbreviated_names( \%expansion );

    # Uncomment next line to dump all expansions for debugging:
    # dump_short_names(\%expansion);
    return (
        \@option_string,   \@defaults, \%expansion,
        \%option_category, \%option_range
    );

}    # end of generate_options

# Memoize process_command_line. Given same @ARGV passed in, return same
# values and same @ARGV back.
# This patch was supplied by Jonathan Swartz Nov 2012 and significantly speeds
# up masontidy (https://metacpan.org/module/masontidy)

my %process_command_line_cache;

sub process_command_line {

    my @q = @_;
    my (
        $perltidyrc_stream,  $is_Windows, $Windows_type,
        $rpending_complaint, $dump_options_type
    ) = @q;

    my $use_cache = !defined($perltidyrc_stream) && !$dump_options_type;
    if ($use_cache) {
        my $cache_key = join( chr(28), @ARGV );
        if ( my $result = $process_command_line_cache{$cache_key} ) {
            my ( $argv, @retvals ) = @{$result};
            @ARGV = @{$argv};
            return @retvals;
        }
        else {
            my @retvals = _process_command_line(@q);
            $process_command_line_cache{$cache_key} = [ \@ARGV, @retvals ]
              if $retvals[0]->{'memoize'};
            return @retvals;
        }
    }
    else {
        return _process_command_line(@q);
    }
}

# This is the original coding, which worked,
# but I've rewritten it (above) to keep Perl-Critic from complaining
# Keep for awhile.

=pod
sub process_command_line {

    my (
        $perltidyrc_stream,  $is_Windows, $Windows_type,
        $rpending_complaint, $dump_options_type
    ) = @_;

    my $use_cache = !defined($perltidyrc_stream) && !$dump_options_type;
    if ($use_cache) {
        my $cache_key = join( chr(28), @ARGV );
        if ( my $result = $process_command_line_cache{$cache_key} ) {
            my ( $argv, @retvals ) = @{$result};
            @ARGV = @{$argv};
            return @retvals;
        }
        else {
            my @retvals = _process_command_line(@_);
            $process_command_line_cache{$cache_key} = [ \@ARGV, @retvals ]
              if $retvals[0]->{'memoize'};
            return @retvals;
        }
    }
    else {
        return _process_command_line(@_);
    }
}
=cut

# (note the underscore here)
sub _process_command_line {

    my (
        $perltidyrc_stream,  $is_Windows, $Windows_type,
        $rpending_complaint, $dump_options_type
    ) = @_;

    use Getopt::Long;

    # Save any current Getopt::Long configuration
    # and set to Getopt::Long defaults.  Use eval to avoid
    # breaking old versions of Perl without these routines.
    # Previous configuration is reset at the exit of this routine.
    my $glc;
    eval { $glc = Getopt::Long::Configure() };
    unless ($@) {
        eval { Getopt::Long::ConfigDefaults() };
    }
    else { $glc = undef }

    my (
        $roption_string,   $rdefaults, $rexpansion,
        $roption_category, $roption_range
    ) = generate_options();

    #---------------------------------------------------------------
    # set the defaults by passing the above list through GetOptions
    #---------------------------------------------------------------
    my %Opts = ();
    {
        local @ARGV = ();

        # do not load the defaults if we are just dumping perltidyrc
        unless ( $dump_options_type eq 'perltidyrc' ) {
            for my $i ( @{$rdefaults} ) { push @ARGV, "--" . $i }
        }
        if ( !GetOptions( \%Opts, @{$roption_string} ) ) {
            Die(
"Programming Bug reported by 'GetOptions': error in setting default options"
            );
        }
    }

    my $word;
    my @raw_options        = ();
    my $config_file        = "";
    my $saw_ignore_profile = 0;
    my $saw_dump_profile   = 0;

    #---------------------------------------------------------------
    # Take a first look at the command-line parameters.  Do as many
    # immediate dumps as possible, which can avoid confusion if the
    # perltidyrc file has an error.
    #---------------------------------------------------------------
    foreach my $i (@ARGV) {

        $i =~ s/^--/-/;
        if ( $i =~ /^-(npro|noprofile|no-profile)$/ ) {
            $saw_ignore_profile = 1;
        }

        # note: this must come before -pro and -profile, below:
        elsif ( $i =~ /^-(dump-profile|dpro)$/ ) {
            $saw_dump_profile = 1;
        }
        elsif ( $i =~ /^-(pro|profile)=(.+)/ ) {
            if ($config_file) {
                Warn(
"Only one -pro=filename allowed, using '$2' instead of '$config_file'\n"
                );
            }
            $config_file = $2;

            # resolve <dir>/.../<file>, meaning look upwards from directory
            if ( defined($config_file) ) {
                if ( my ( $start_dir, $search_file ) =
                    ( $config_file =~ m{^(.*)\.\.\./(.*)$} ) )
                {
                    $start_dir = '.' if !$start_dir;
                    $start_dir = Cwd::realpath($start_dir);
                    if ( my $found_file =
                        find_file_upwards( $start_dir, $search_file ) )
                    {
                        $config_file = $found_file;
                    }
                }
            }
            unless ( -e $config_file ) {
                Warn("cannot find file given with -pro=$config_file: $!\n");
                $config_file = "";
            }
        }
        elsif ( $i =~ /^-(pro|profile)=?$/ ) {
            Die("usage: -pro=filename or --profile=filename, no spaces\n");
        }
        elsif ( $i =~ /^-(help|h|HELP|H|\?)$/ ) {
            usage();
            Exit(0);
        }
        elsif ( $i =~ /^-(version|v)$/ ) {
            show_version();
            Exit(0);
        }
        elsif ( $i =~ /^-(dump-defaults|ddf)$/ ) {
            dump_defaults( @{$rdefaults} );
            Exit(0);
        }
        elsif ( $i =~ /^-(dump-long-names|dln)$/ ) {
            dump_long_names( @{$roption_string} );
            Exit(0);
        }
        elsif ( $i =~ /^-(dump-short-names|dsn)$/ ) {
            dump_short_names($rexpansion);
            Exit(0);
        }
        elsif ( $i =~ /^-(dump-token-types|dtt)$/ ) {
            Perl::Tidy::Tokenizer->dump_token_types(*STDOUT);
            Exit(0);
        }
    }

    if ( $saw_dump_profile && $saw_ignore_profile ) {
        Warn("No profile to dump because of -npro\n");
        Exit(1);
    }

    #---------------------------------------------------------------
    # read any .perltidyrc configuration file
    #---------------------------------------------------------------
    unless ($saw_ignore_profile) {

        # resolve possible conflict between $perltidyrc_stream passed
        # as call parameter to perltidy and -pro=filename on command
        # line.
        if ($perltidyrc_stream) {
            if ($config_file) {
                Warn(<<EOM);
 Conflict: a perltidyrc configuration file was specified both as this
 perltidy call parameter: $perltidyrc_stream 
 and with this -profile=$config_file.
 Using -profile=$config_file.
EOM
            }
            else {
                $config_file = $perltidyrc_stream;
            }
        }

        # look for a config file if we don't have one yet
        my $rconfig_file_chatter;
        ${$rconfig_file_chatter} = "";
        $config_file =
          find_config_file( $is_Windows, $Windows_type, $rconfig_file_chatter,
            $rpending_complaint )
          unless $config_file;

        # open any config file
        my $fh_config;
        if ($config_file) {
            ( $fh_config, $config_file ) =
              Perl::Tidy::streamhandle( $config_file, 'r' );
            unless ($fh_config) {
                ${$rconfig_file_chatter} .=
                  "# $config_file exists but cannot be opened\n";
            }
        }

        if ($saw_dump_profile) {
            dump_config_file( $fh_config, $config_file, $rconfig_file_chatter );
            Exit(0);
        }

        if ($fh_config) {

            my ( $rconfig_list, $death_message ) =
              read_config_file( $fh_config, $config_file, $rexpansion );
            Die($death_message) if ($death_message);

            # process any .perltidyrc parameters right now so we can
            # localize errors
            if ( @{$rconfig_list} ) {
                local @ARGV = @{$rconfig_list};

                expand_command_abbreviations( $rexpansion, \@raw_options,
                    $config_file );

                if ( !GetOptions( \%Opts, @{$roption_string} ) ) {
                    Die(
"Error in this config file: $config_file  \nUse -npro to ignore this file, -h for help'\n"
                    );
                }

                # Anything left in this local @ARGV is an error and must be
                # invalid bare words from the configuration file.  We cannot
                # check this earlier because bare words may have been valid
                # values for parameters.  We had to wait for GetOptions to have
                # a look at @ARGV.
                if (@ARGV) {
                    my $count = @ARGV;
                    my $str   = "\'" . pop(@ARGV) . "\'";
                    while ( my $param = pop(@ARGV) ) {
                        if ( length($str) < 70 ) {
                            $str .= ", '$param'";
                        }
                        else {
                            $str .= ", ...";
                            last;
                        }
                    }
                    Die(<<EOM);
There are $count unrecognized values in the configuration file '$config_file':
$str
Use leading dashes for parameters.  Use -npro to ignore this file.
EOM
                }

                # Undo any options which cause premature exit.  They are not
                # appropriate for a config file, and it could be hard to
                # diagnose the cause of the premature exit.
                foreach (
                    qw{
                    dump-cuddled-block-list
                    dump-defaults
                    dump-long-names
                    dump-options
                    dump-profile
                    dump-short-names
                    dump-token-types
                    dump-want-left-space
                    dump-want-right-space
                    help
                    stylesheet
                    version
                    }
                  )
                {

                    if ( defined( $Opts{$_} ) ) {
                        delete $Opts{$_};
                        Warn("ignoring --$_ in config file: $config_file\n");
                    }
                }
            }
        }
    }

    #---------------------------------------------------------------
    # now process the command line parameters
    #---------------------------------------------------------------
    expand_command_abbreviations( $rexpansion, \@raw_options, $config_file );

    local $SIG{'__WARN__'} = sub { Warn( $_[0] ) };
    if ( !GetOptions( \%Opts, @{$roption_string} ) ) {
        Die("Error on command line; for help try 'perltidy -h'\n");
    }

    # reset Getopt::Long configuration back to its previous value
    eval { Getopt::Long::Configure($glc) } if defined $glc;

    return ( \%Opts, $config_file, \@raw_options, $roption_string,
        $rexpansion, $roption_category, $roption_range );
}    # end of _process_command_line

sub check_options {

    my ( $rOpts, $is_Windows, $Windows_type, $rpending_complaint ) = @_;

    #---------------------------------------------------------------
    # check and handle any interactions among the basic options..
    #---------------------------------------------------------------

    # Since -vt, -vtc, and -cti are abbreviations, but under
    # msdos, an unquoted input parameter like vtc=1 will be
    # seen as 2 parameters, vtc and 1, so the abbreviations
    # won't be seen.  Therefore, we will catch them here if
    # they get through.

    if ( defined $rOpts->{'vertical-tightness'} ) {
        my $vt = $rOpts->{'vertical-tightness'};
        $rOpts->{'paren-vertical-tightness'}          = $vt;
        $rOpts->{'square-bracket-vertical-tightness'} = $vt;
        $rOpts->{'brace-vertical-tightness'}          = $vt;
    }

    if ( defined $rOpts->{'vertical-tightness-closing'} ) {
        my $vtc = $rOpts->{'vertical-tightness-closing'};
        $rOpts->{'paren-vertical-tightness-closing'}          = $vtc;
        $rOpts->{'square-bracket-vertical-tightness-closing'} = $vtc;
        $rOpts->{'brace-vertical-tightness-closing'}          = $vtc;
    }

    if ( defined $rOpts->{'closing-token-indentation'} ) {
        my $cti = $rOpts->{'closing-token-indentation'};
        $rOpts->{'closing-square-bracket-indentation'} = $cti;
        $rOpts->{'closing-brace-indentation'}          = $cti;
        $rOpts->{'closing-paren-indentation'}          = $cti;
    }

    # In quiet mode, there is no log file and hence no way to report
    # results of syntax check, so don't do it.
    if ( $rOpts->{'quiet'} ) {
        $rOpts->{'check-syntax'} = 0;
    }

    # can't check syntax if no output
    if ( $rOpts->{'format'} ne 'tidy' ) {
        $rOpts->{'check-syntax'} = 0;
    }

    # Never let Windows 9x/Me systems run syntax check -- this will prevent a
    # wide variety of nasty problems on these systems, because they cannot
    # reliably run backticks.  Don't even think about changing this!
    if (   $rOpts->{'check-syntax'}
        && $is_Windows
        && ( !$Windows_type || $Windows_type =~ /^(9|Me)/ ) )
    {
        $rOpts->{'check-syntax'} = 0;
    }

    # Added Dec 2017: Deactivating check-syntax for all systems for safety
    # because unexpected results can occur when code in BEGIN blocks is
    # executed.  This flag was included to help check for perltidy mistakes,
    # and may still be useful for debugging.  To activate for testing comment
    # out the next three lines.
    else {
        $rOpts->{'check-syntax'} = 0;
    }

    # It's really a bad idea to check syntax as root unless you wrote
    # the script yourself.  FIXME: not sure if this works with VMS
    unless ($is_Windows) {

        if ( $< == 0 && $rOpts->{'check-syntax'} ) {
            $rOpts->{'check-syntax'} = 0;
            ${$rpending_complaint} .=
"Syntax check deactivated for safety; you shouldn't run this as root\n";
        }
    }

    # check iteration count and quietly fix if necessary:
    # - iterations option only applies to code beautification mode
    # - the convergence check should stop most runs on iteration 2, and
    #   virtually all on iteration 3.  But we'll allow up to 6.
    if ( $rOpts->{'format'} ne 'tidy' ) {
        $rOpts->{'iterations'} = 1;
    }
    elsif ( defined( $rOpts->{'iterations'} ) ) {
        if    ( $rOpts->{'iterations'} <= 0 ) { $rOpts->{'iterations'} = 1 }
        elsif ( $rOpts->{'iterations'} > 6 )  { $rOpts->{'iterations'} = 6 }
    }
    else {
        $rOpts->{'iterations'} = 1;
    }

    my $check_blank_count = sub {
        my ( $key, $abbrev ) = @_;
        if ( $rOpts->{$key} ) {
            if ( $rOpts->{$key} < 0 ) {
                $rOpts->{$key} = 0;
                Warn("negative value of $abbrev, setting 0\n");
            }
            if ( $rOpts->{$key} > 100 ) {
                Warn("unreasonably large value of $abbrev, reducing\n");
                $rOpts->{$key} = 100;
            }
        }
    };

    # check for reasonable number of blank lines and fix to avoid problems
    $check_blank_count->( 'blank-lines-before-subs',          '-blbs' );
    $check_blank_count->( 'blank-lines-before-packages',      '-blbp' );
    $check_blank_count->( 'blank-lines-after-block-opening',  '-blao' );
    $check_blank_count->( 'blank-lines-before-block-closing', '-blbc' );

    # setting a non-negative logfile gap causes logfile to be saved
    if ( defined( $rOpts->{'logfile-gap'} ) && $rOpts->{'logfile-gap'} >= 0 ) {
        $rOpts->{'logfile'} = 1;
    }

    # set short-cut flag when only indentation is to be done.
    # Note that the user may or may not have already set the
    # indent-only flag.
    if (   !$rOpts->{'add-whitespace'}
        && !$rOpts->{'delete-old-whitespace'}
        && !$rOpts->{'add-newlines'}
        && !$rOpts->{'delete-old-newlines'} )
    {
        $rOpts->{'indent-only'} = 1;
    }

    # -isbc implies -ibc
    if ( $rOpts->{'indent-spaced-block-comments'} ) {
        $rOpts->{'indent-block-comments'} = 1;
    }

    # -bli flag implies -bl
    if ( $rOpts->{'brace-left-and-indent'} ) {
        $rOpts->{'opening-brace-on-new-line'} = 1;
    }

    if (   $rOpts->{'opening-brace-always-on-right'}
        && $rOpts->{'opening-brace-on-new-line'} )
    {
        Warn(<<EOM);
 Conflict: you specified both 'opening-brace-always-on-right' (-bar) and 
  'opening-brace-on-new-line' (-bl).  Ignoring -bl. 
EOM
        $rOpts->{'opening-brace-on-new-line'} = 0;
    }

    # it simplifies things if -bl is 0 rather than undefined
    if ( !defined( $rOpts->{'opening-brace-on-new-line'} ) ) {
        $rOpts->{'opening-brace-on-new-line'} = 0;
    }

    # -sbl defaults to -bl if not defined
    if ( !defined( $rOpts->{'opening-sub-brace-on-new-line'} ) ) {
        $rOpts->{'opening-sub-brace-on-new-line'} =
          $rOpts->{'opening-brace-on-new-line'};
    }

    if ( $rOpts->{'entab-leading-whitespace'} ) {
        if ( $rOpts->{'entab-leading-whitespace'} < 0 ) {
            Warn("-et=n must use a positive integer; ignoring -et\n");
            $rOpts->{'entab-leading-whitespace'} = undef;
        }

        # entab leading whitespace has priority over the older 'tabs' option
        if ( $rOpts->{'tabs'} ) { $rOpts->{'tabs'} = 0; }
    }

    # set a default tabsize to be used in guessing the starting indentation
    # level if and only if this run does not use tabs and the old code does
    # use tabs
    if ( $rOpts->{'default-tabsize'} ) {
        if ( $rOpts->{'default-tabsize'} < 0 ) {
            Warn("negative value of -dt, setting 0\n");
            $rOpts->{'default-tabsize'} = 0;
        }
        if ( $rOpts->{'default-tabsize'} > 20 ) {
            Warn("unreasonably large value of -dt, reducing\n");
            $rOpts->{'default-tabsize'} = 20;
        }
    }
    else {
        $rOpts->{'default-tabsize'} = 8;
    }

    # Define $tabsize, the number of spaces per tab for use in
    # guessing the indentation of source lines with leading tabs.
    # Assume same as for this run if tabs are used , otherwise assume
    # a default value, typically 8
    my $tabsize =
        $rOpts->{'entab-leading-whitespace'}
      ? $rOpts->{'entab-leading-whitespace'}
      : $rOpts->{'tabs'} ? $rOpts->{'indent-columns'}
      :                    $rOpts->{'default-tabsize'};
    return $tabsize;
}

sub find_file_upwards {
    my ( $search_dir, $search_file ) = @_;

    $search_dir  =~ s{/+$}{};
    $search_file =~ s{^/+}{};

    while (1) {
        my $try_path = "$search_dir/$search_file";
        if ( -f $try_path ) {
            return $try_path;
        }
        elsif ( $search_dir eq '/' ) {
            return;
        }
        else {
            $search_dir = dirname($search_dir);
        }
    }

    # This return is for Perl-Critic.
    # We shouldn't get out of the while loop without a return
    return;
}

sub expand_command_abbreviations {

    # go through @ARGV and expand any abbreviations

    my ( $rexpansion, $rraw_options, $config_file ) = @_;

    # set a pass limit to prevent an infinite loop;
    # 10 should be plenty, but it may be increased to allow deeply
    # nested expansions.
    my $max_passes = 10;
    my @new_argv   = ();

    # keep looping until all expansions have been converted into actual
    # dash parameters..
    foreach my $pass_count ( 0 .. $max_passes ) {
        my @new_argv     = ();
        my $abbrev_count = 0;

        # loop over each item in @ARGV..
        foreach my $word (@ARGV) {

            # convert any leading 'no-' to just 'no'
            if ( $word =~ /^(-[-]?no)-(.*)/ ) { $word = $1 . $2 }

            # if it is a dash flag (instead of a file name)..
            if ( $word =~ /^-[-]?([\w\-]+)(.*)/ ) {

                my $abr   = $1;
                my $flags = $2;

                # save the raw input for debug output in case of circular refs
                if ( $pass_count == 0 ) {
                    push( @{$rraw_options}, $word );
                }

                # recombine abbreviation and flag, if necessary,
                # to allow abbreviations with arguments such as '-vt=1'
                if ( $rexpansion->{ $abr . $flags } ) {
                    $abr   = $abr . $flags;
                    $flags = "";
                }

                # if we see this dash item in the expansion hash..
                if ( $rexpansion->{$abr} ) {
                    $abbrev_count++;

                    # stuff all of the words that it expands to into the
                    # new arg list for the next pass
                    foreach my $abbrev ( @{ $rexpansion->{$abr} } ) {
                        next unless $abbrev;    # for safety; shouldn't happen
                        push( @new_argv, '--' . $abbrev . $flags );
                    }
                }

                # not in expansion hash, must be actual long name
                else {
                    push( @new_argv, $word );
                }
            }

            # not a dash item, so just save it for the next pass
            else {
                push( @new_argv, $word );
            }
        }    # end of this pass

        # update parameter list @ARGV to the new one
        @ARGV = @new_argv;
        last unless ( $abbrev_count > 0 );

        # make sure we are not in an infinite loop
        if ( $pass_count == $max_passes ) {
            local $" = ')(';
            Warn(<<EOM);
I'm tired. We seem to be in an infinite loop trying to expand aliases.
Here are the raw options;
(rraw_options)
EOM
            my $num = @new_argv;
            if ( $num < 50 ) {
                Warn(<<EOM);
After $max_passes passes here is ARGV
(@new_argv)
EOM
            }
            else {
                Warn(<<EOM);
After $max_passes passes ARGV has $num entries
EOM
            }

            if ($config_file) {
                Die(<<"DIE");
Please check your configuration file $config_file for circular-references. 
To deactivate it, use -npro.
DIE
            }
            else {
                Die(<<'DIE');
Program bug - circular-references in the %expansion hash, probably due to
a recent program change.
DIE
            }
        }    # end of check for circular references
    }    # end of loop over all passes
    return;
}

# Debug routine -- this will dump the expansion hash
sub dump_short_names {
    my $rexpansion = shift;
    print STDOUT <<EOM;
List of short names.  This list shows how all abbreviations are
translated into other abbreviations and, eventually, into long names.
New abbreviations may be defined in a .perltidyrc file.  
For a list of all long names, use perltidy --dump-long-names (-dln).
--------------------------------------------------------------------------
EOM
    foreach my $abbrev ( sort keys %$rexpansion ) {
        my @list = @{ $rexpansion->{$abbrev} };
        print STDOUT "$abbrev --> @list\n";
    }
    return;
}

sub check_vms_filename {

    # given a valid filename (the perltidy input file)
    # create a modified filename and separator character
    # suitable for VMS.
    #
    # Contributed by Michael Cartmell
    #
    my $filename = shift;
    my ( $base, $path ) = fileparse($filename);

    # remove explicit ; version
    $base =~ s/;-?\d*$//

      # remove explicit . version ie two dots in filename NB ^ escapes a dot
      or $base =~ s/(          # begin capture $1
                  (?:^|[^^])\. # match a dot not preceded by a caret
                  (?:          # followed by nothing
                    |          # or
                    .*[^^]     # anything ending in a non caret
                  )
                )              # end capture $1
                \.-?\d*$       # match . version number
              /$1/x;

    # normalise filename, if there are no unescaped dots then append one
    $base .= '.' unless $base =~ /(?:^|[^^])\./;

    # if we don't already have an extension then we just append the extension
    my $separator = ( $base =~ /\.$/ ) ? "" : "_";
    return ( $path . $base, $separator );
}

sub Win_OS_Type {

    # TODO: are these more standard names?
    # Win32s Win95 Win98 WinMe WinNT3.51 WinNT4 Win2000 WinXP/.Net Win2003

    # Returns a string that determines what MS OS we are on.
    # Returns win32s,95,98,Me,NT3.51,NT4,2000,XP/.Net,Win2003
    # Returns blank string if not an MS system.
    # Original code contributed by: Yves Orton
    # We need to know this to decide where to look for config files

    my $rpending_complaint = shift;
    my $os                 = "";
    return $os unless $^O =~ /win32|dos/i;    # is it a MS box?

    # Systems built from Perl source may not have Win32.pm
    # But probably have Win32::GetOSVersion() anyway so the
    # following line is not 'required':
    # return $os unless eval('require Win32');

    # Use the standard API call to determine the version
    my ( $undef, $major, $minor, $build, $id );
    eval { ( $undef, $major, $minor, $build, $id ) = Win32::GetOSVersion() };

    #
    #    NAME                   ID   MAJOR  MINOR
    #    Windows NT 4           2      4       0
    #    Windows 2000           2      5       0
    #    Windows XP             2      5       1
    #    Windows Server 2003    2      5       2

    return "win32s" unless $id;    # If id==0 then its a win32s box.
    $os = {                        # Magic numbers from MSDN
                                   # documentation of GetOSVersion
        1 => {
            0  => "95",
            10 => "98",
            90 => "Me"
        },
        2 => {
            0  => "2000",          # or NT 4, see below
            1  => "XP/.Net",
            2  => "Win2003",
            51 => "NT3.51"
        }
    }->{$id}->{$minor};

    # If $os is undefined, the above code is out of date.  Suggested updates
    # are welcome.
    unless ( defined $os ) {
        $os = "";

        # Deactivated this message 20180322 because it was needlessly
        # causing some test scripts to fail.  Need help from someone
        # with expertise in Windows to decide what is possible with windows.
        ${$rpending_complaint} .= <<EOS if (0);
Error trying to discover Win_OS_Type: $id:$major:$minor Has no name of record!
We won't be able to look for a system-wide config file.
EOS
    }

    # Unfortunately the logic used for the various versions isn't so clever..
    # so we have to handle an outside case.
    return ( $os eq "2000" && $major != 5 ) ? "NT4" : $os;
}

sub is_unix {
    return
         ( $^O !~ /win32|dos/i )
      && ( $^O ne 'VMS' )
      && ( $^O ne 'OS2' )
      && ( $^O ne 'MacOS' );
}

sub look_for_Windows {

    # determine Windows sub-type and location of
    # system-wide configuration files
    my $rpending_complaint = shift;
    my $is_Windows         = ( $^O =~ /win32|dos/i );
    my $Windows_type;
    $Windows_type = Win_OS_Type($rpending_complaint) if $is_Windows;
    return ( $is_Windows, $Windows_type );
}

sub find_config_file {

    # look for a .perltidyrc configuration file
    # For Windows also look for a file named perltidy.ini
    my ( $is_Windows, $Windows_type, $rconfig_file_chatter,
        $rpending_complaint ) = @_;

    ${$rconfig_file_chatter} .= "# Config file search...system reported as:";
    if ($is_Windows) {
        ${$rconfig_file_chatter} .= "Windows $Windows_type\n";
    }
    else {
        ${$rconfig_file_chatter} .= " $^O\n";
    }

    # sub to check file existence and record all tests
    my $exists_config_file = sub {
        my $config_file = shift;
        return 0 unless $config_file;
        ${$rconfig_file_chatter} .= "# Testing: $config_file\n";
        return -f $config_file;
    };

    # Sub to search upward for config file
    my $resolve_config_file = sub {

        # resolve <dir>/.../<file>, meaning look upwards from directory
        my $config_file = shift;
        if ($config_file) {
            if ( my ( $start_dir, $search_file ) =
                ( $config_file =~ m{^(.*)\.\.\./(.*)$} ) )
            {
                ${$rconfig_file_chatter} .=
                  "# Searching Upward: $config_file\n";
                $start_dir = '.' if !$start_dir;
                $start_dir = Cwd::realpath($start_dir);
                if ( my $found_file =
                    find_file_upwards( $start_dir, $search_file ) )
                {
                    $config_file = $found_file;
                    ${$rconfig_file_chatter} .= "# Found: $config_file\n";
                }
            }
        }
        return $config_file;
    };

    my $config_file;

    # look in current directory first
    $config_file = ".perltidyrc";
    return $config_file if $exists_config_file->($config_file);
    if ($is_Windows) {
        $config_file = "perltidy.ini";
        return $config_file if $exists_config_file->($config_file);
    }

    # Default environment vars.
    my @envs = qw(PERLTIDY HOME);

    # Check the NT/2k/XP locations, first a local machine def, then a
    # network def
    push @envs, qw(USERPROFILE HOMESHARE) if $^O =~ /win32/i;

    # Now go through the environment ...
    foreach my $var (@envs) {
        ${$rconfig_file_chatter} .= "# Examining: \$ENV{$var}";
        if ( defined( $ENV{$var} ) ) {
            ${$rconfig_file_chatter} .= " = $ENV{$var}\n";

            # test ENV{ PERLTIDY } as file:
            if ( $var eq 'PERLTIDY' ) {
                $config_file = "$ENV{$var}";
                $config_file = $resolve_config_file->($config_file);
                return $config_file if $exists_config_file->($config_file);
            }

            # test ENV as directory:
            $config_file = catfile( $ENV{$var}, ".perltidyrc" );
            $config_file = $resolve_config_file->($config_file);
            return $config_file if $exists_config_file->($config_file);

            if ($is_Windows) {
                $config_file = catfile( $ENV{$var}, "perltidy.ini" );
                $config_file = $resolve_config_file->($config_file);
                return $config_file if $exists_config_file->($config_file);
            }
        }
        else {
            ${$rconfig_file_chatter} .= "\n";
        }
    }

    # then look for a system-wide definition
    # where to look varies with OS
    if ($is_Windows) {

        if ($Windows_type) {
            my ( $os, $system, $allusers ) =
              Win_Config_Locs( $rpending_complaint, $Windows_type );

            # Check All Users directory, if there is one.
            # i.e. C:\Documents and Settings\User\perltidy.ini
            if ($allusers) {

                $config_file = catfile( $allusers, ".perltidyrc" );
                return $config_file if $exists_config_file->($config_file);

                $config_file = catfile( $allusers, "perltidy.ini" );
                return $config_file if $exists_config_file->($config_file);
            }

            # Check system directory.
            # retain old code in case someone has been able to create
            # a file with a leading period.
            $config_file = catfile( $system, ".perltidyrc" );
            return $config_file if $exists_config_file->($config_file);

            $config_file = catfile( $system, "perltidy.ini" );
            return $config_file if $exists_config_file->($config_file);
        }
    }

    # Place to add customization code for other systems
    elsif ( $^O eq 'OS2' ) {
    }
    elsif ( $^O eq 'MacOS' ) {
    }
    elsif ( $^O eq 'VMS' ) {
    }

    # Assume some kind of Unix
    else {

        $config_file = "/usr/local/etc/perltidyrc";
        return $config_file if $exists_config_file->($config_file);

        $config_file = "/etc/perltidyrc";
        return $config_file if $exists_config_file->($config_file);
    }

    # Couldn't find a config file
    return;
}

sub Win_Config_Locs {

    # In scalar context returns the OS name (95 98 ME NT3.51 NT4 2000 XP),
    # or undef if its not a win32 OS.  In list context returns OS, System
    # Directory, and All Users Directory.  All Users will be empty on a
    # 9x/Me box.  Contributed by: Yves Orton.

    # Original coding:
    # my $rpending_complaint = shift;
    # my $os = (@_) ? shift : Win_OS_Type();

    my ( $rpending_complaint, $os ) = @_;
    if ( !$os ) { $os = Win_OS_Type(); }

    return unless $os;

    my $system   = "";
    my $allusers = "";

    if ( $os =~ /9[58]|Me/ ) {
        $system = "C:/Windows";
    }
    elsif ( $os =~ /NT|XP|200?/ ) {
        $system = ( $os =~ /XP/ ) ? "C:/Windows/" : "C:/WinNT/";
        $allusers =
          ( $os =~ /NT/ )
          ? "C:/WinNT/profiles/All Users/"
          : "C:/Documents and Settings/All Users/";
    }
    else {

        # This currently would only happen on a win32s computer.  I don't have
        # one to test, so I am unsure how to proceed.  Suggestions welcome!
        ${$rpending_complaint} .=
"I dont know a sensible place to look for config files on an $os system.\n";
        return;
    }
    return wantarray ? ( $os, $system, $allusers ) : $os;
}

sub dump_config_file {
    my ( $fh, $config_file, $rconfig_file_chatter ) = @_;
    print STDOUT "$$rconfig_file_chatter";
    if ($fh) {
        print STDOUT "# Dump of file: '$config_file'\n";
        while ( my $line = $fh->getline() ) { print STDOUT $line }
        eval { $fh->close() };
    }
    else {
        print STDOUT "# ...no config file found\n";
    }
    return;
}

sub read_config_file {

    my ( $fh, $config_file, $rexpansion ) = @_;
    my @config_list = ();

    # file is bad if non-empty $death_message is returned
    my $death_message = "";

    my $name = undef;
    my $line_no;
    my $opening_brace_line;
    while ( my $line = $fh->getline() ) {
        $line_no++;
        chomp $line;
        ( $line, $death_message ) =
          strip_comment( $line, $config_file, $line_no );
        last if ($death_message);
        next unless $line;
        $line =~ s/^\s*(.*?)\s*$/$1/;    # trim both ends
        next unless $line;

        my $body = $line;

        # Look for complete or partial abbreviation definition of the form
        #     name { body }   or  name {   or    name { body
        # See rules in perltidy's perldoc page
        # Section: Other Controls - Creating a new abbreviation
        if ( $line =~ /^((\w+)\s*\{)(.*)?$/ ) {
            my $oldname = $name;
            ( $name, $body ) = ( $2, $3 );

            # Cannot start new abbreviation unless old abbreviation is complete
            last if ($opening_brace_line);

            $opening_brace_line = $line_no unless ( $body && $body =~ s/\}$// );

            # handle a new alias definition
            if ( ${$rexpansion}{$name} ) {
                local $" = ')(';
                my @names = sort keys %$rexpansion;
                $death_message =
                    "Here is a list of all installed aliases\n(@names)\n"
                  . "Attempting to redefine alias ($name) in config file $config_file line $.\n";
                last;
            }
            ${$rexpansion}{$name} = [];
        }

        # leading opening braces not allowed
        elsif ( $line =~ /^{/ ) {
            $opening_brace_line = undef;
            $death_message =
              "Unexpected '{' at line $line_no in config file '$config_file'\n";
            last;
        }

        # Look for abbreviation closing:    body }   or    }
        elsif ( $line =~ /^(.*)?\}$/ ) {
            $body = $1;
            if ($opening_brace_line) {
                $opening_brace_line = undef;
            }
            else {
                $death_message =
"Unexpected '}' at line $line_no in config file '$config_file'\n";
                last;
            }
        }

        # Now store any parameters
        if ($body) {

            my ( $rbody_parts, $msg ) = parse_args($body);
            if ($msg) {
                $death_message = <<EOM;
Error reading file '$config_file' at line number $line_no.
$msg
Please fix this line or use -npro to avoid reading this file
EOM
                last;
            }

            if ($name) {

                # remove leading dashes if this is an alias
                foreach ( @{$rbody_parts} ) { s/^\-+//; }
                push @{ ${$rexpansion}{$name} }, @{$rbody_parts};
            }
            else {
                push( @config_list, @{$rbody_parts} );
            }
        }
    }

    if ($opening_brace_line) {
        $death_message =
"Didn't see a '}' to match the '{' at line $opening_brace_line in config file '$config_file'\n";
    }
    eval { $fh->close() };
    return ( \@config_list, $death_message );
}

sub strip_comment {

    # Strip any comment from a command line
    my ( $instr, $config_file, $line_no ) = @_;
    my $msg = "";

    # check for full-line comment
    if ( $instr =~ /^\s*#/ ) {
        return ( "", $msg );
    }

    # nothing to do if no comments
    if ( $instr !~ /#/ ) {
        return ( $instr, $msg );
    }

    # handle case of no quotes
    elsif ( $instr !~ /['"]/ ) {

        # We now require a space before the # of a side comment
        # this allows something like:
        #    -sbcp=#
        # Otherwise, it would have to be quoted:
        #    -sbcp='#'
        $instr =~ s/\s+\#.*$//;
        return ( $instr, $msg );
    }

    # handle comments and quotes
    my $outstr     = "";
    my $quote_char = "";
    while (1) {

        # looking for ending quote character
        if ($quote_char) {
            if ( $instr =~ /\G($quote_char)/gc ) {
                $quote_char = "";
                $outstr .= $1;
            }
            elsif ( $instr =~ /\G(.)/gc ) {
                $outstr .= $1;
            }

            # error..we reached the end without seeing the ending quote char
            else {
                $msg = <<EOM;
Error reading file $config_file at line number $line_no.
Did not see ending quote character <$quote_char> in this text:
$instr
Please fix this line or use -npro to avoid reading this file
EOM
                last;
            }
        }

        # accumulating characters and looking for start of a quoted string
        else {
            if ( $instr =~ /\G([\"\'])/gc ) {
                $outstr .= $1;
                $quote_char = $1;
            }

            # Note: not yet enforcing the space-before-hash rule for side
            # comments if the parameter is quoted.
            elsif ( $instr =~ /\G#/gc ) {
                last;
            }
            elsif ( $instr =~ /\G(.)/gc ) {
                $outstr .= $1;
            }
            else {
                last;
            }
        }
    }
    return ( $outstr, $msg );
}

sub parse_args {

    # Parse a command string containing multiple string with possible
    # quotes, into individual commands.  It might look like this, for example:
    #
    #    -wba=" + - "  -some-thing -wbb='. && ||'
    #
    # There is no need, at present, to handle escaped quote characters.
    # (They are not perltidy tokens, so needn't be in strings).

    my ($body)     = @_;
    my @body_parts = ();
    my $quote_char = "";
    my $part       = "";
    my $msg        = "";
    while (1) {

        # looking for ending quote character
        if ($quote_char) {
            if ( $body =~ /\G($quote_char)/gc ) {
                $quote_char = "";
            }
            elsif ( $body =~ /\G(.)/gc ) {
                $part .= $1;
            }

            # error..we reached the end without seeing the ending quote char
            else {
                if ( length($part) ) { push @body_parts, $part; }
                $msg = <<EOM;
Did not see ending quote character <$quote_char> in this text:
$body
EOM
                last;
            }
        }

        # accumulating characters and looking for start of a quoted string
        else {
            if ( $body =~ /\G([\"\'])/gc ) {
                $quote_char = $1;
            }
            elsif ( $body =~ /\G(\s+)/gc ) {
                if ( length($part) ) { push @body_parts, $part; }
                $part = "";
            }
            elsif ( $body =~ /\G(.)/gc ) {
                $part .= $1;
            }
            else {
                if ( length($part) ) { push @body_parts, $part; }
                last;
            }
        }
    }
    return ( \@body_parts, $msg );
}

sub dump_long_names {

    my @names = @_;
    print STDOUT <<EOM;
# Command line long names (passed to GetOptions)
#---------------------------------------------------------------
# here is a summary of the Getopt codes:
# <none> does not take an argument
# =s takes a mandatory string
# :s takes an optional string
# =i takes a mandatory integer
# :i takes an optional integer
# ! does not take an argument and may be negated
#  i.e., -foo and -nofoo are allowed
# a double dash signals the end of the options list
#
#---------------------------------------------------------------
EOM

    foreach my $name ( sort @names ) { print STDOUT "$name\n" }
    return;
}

sub dump_defaults {
    my @defaults = @_;
    print STDOUT "Default command line options:\n";
    foreach my $line ( sort @defaults ) { print STDOUT "$line\n" }
    return;
}

sub readable_options {

    # return options for this run as a string which could be
    # put in a perltidyrc file
    my ( $rOpts, $roption_string ) = @_;
    my %Getopt_flags;
    my $rGetopt_flags    = \%Getopt_flags;
    my $readable_options = "# Final parameter set for this run.\n";
    $readable_options .=
      "# See utility 'perltidyrc_dump.pl' for nicer formatting.\n";
    foreach my $opt ( @{$roption_string} ) {
        my $flag = "";
        if ( $opt =~ /(.*)(!|=.*)$/ ) {
            $opt  = $1;
            $flag = $2;
        }
        if ( defined( $rOpts->{$opt} ) ) {
            $rGetopt_flags->{$opt} = $flag;
        }
    }
    foreach my $key ( sort keys %{$rOpts} ) {
        my $flag   = $rGetopt_flags->{$key};
        my $value  = $rOpts->{$key};
        my $prefix = '--';
        my $suffix = "";
        if ($flag) {
            if ( $flag =~ /^=/ ) {
                if ( $value !~ /^\d+$/ ) { $value = '"' . $value . '"' }
                $suffix = "=" . $value;
            }
            elsif ( $flag =~ /^!/ ) {
                $prefix .= "no" unless ($value);
            }
            else {

                # shouldn't happen
                $readable_options .=
                  "# ERROR in dump_options: unrecognized flag $flag for $key\n";
            }
        }
        $readable_options .= $prefix . $key . $suffix . "\n";
    }
    return $readable_options;
}

sub show_version {
    print STDOUT <<"EOM";
This is perltidy, v$VERSION 

Copyright 2000-2018, Steve Hancock

Perltidy is free software and may be copied under the terms of the GNU
General Public License, which is included in the distribution files.

Complete documentation for perltidy can be found using 'man perltidy'
or on the internet at http://perltidy.sourceforge.net.
EOM
    return;
}

sub usage {

    print STDOUT <<EOF;
This is perltidy version $VERSION, a perl script indenter.  Usage:

    perltidy [ options ] file1 file2 file3 ...
            (output goes to file1.tdy, file2.tdy, file3.tdy, ...)
    perltidy [ options ] file1 -o outfile
    perltidy [ options ] file1 -st >outfile
    perltidy [ options ] <infile >outfile

Options have short and long forms. Short forms are shown; see
man pages for long forms.  Note: '=s' indicates a required string,
and '=n' indicates a required integer.

I/O control
 -h      show this help
 -o=file name of the output file (only if single input file)
 -oext=s change output extension from 'tdy' to s
 -opath=path  change path to be 'path' for output files
 -b      backup original to .bak and modify file in-place
 -bext=s change default backup extension from 'bak' to s
 -q      deactivate error messages (for running under editor)
 -w      include non-critical warning messages in the .ERR error output
 -syn    run perl -c to check syntax (default under unix systems)
 -log    save .LOG file, which has useful diagnostics
 -f      force perltidy to read a binary file
 -g      like -log but writes more detailed .LOG file, for debugging scripts
 -opt    write the set of options actually used to a .LOG file
 -npro   ignore .perltidyrc configuration command file 
 -pro=file   read configuration commands from file instead of .perltidyrc 
 -st     send output to standard output, STDOUT
 -se     send all error output to standard error output, STDERR
 -v      display version number to standard output and quit

Basic Options:
 -i=n    use n columns per indentation level (default n=4)
 -t      tabs: use one tab character per indentation level, not recommeded
 -nt     no tabs: use n spaces per indentation level (default)
 -et=n   entab leading whitespace n spaces per tab; not recommended
 -io     "indent only": just do indentation, no other formatting.
 -sil=n  set starting indentation level to n;  use if auto detection fails
 -ole=s  specify output line ending (s=dos or win, mac, unix)
 -ple    keep output line endings same as input (input must be filename)

Whitespace Control
 -fws    freeze whitespace; this disables all whitespace changes
           and disables the following switches:
 -bt=n   sets brace tightness,  n= (0 = loose, 1=default, 2 = tight)
 -bbt    same as -bt but for code block braces; same as -bt if not given
 -bbvt   block braces vertically tight; use with -bl or -bli
 -bbvtl=s  make -bbvt to apply to selected list of block types
 -pt=n   paren tightness (n=0, 1 or 2)
 -sbt=n  square bracket tightness (n=0, 1, or 2)
 -bvt=n  brace vertical tightness, 
         n=(0=open, 1=close unless multiple steps on a line, 2=always close)
 -pvt=n  paren vertical tightness (see -bvt for n)
 -sbvt=n square bracket vertical tightness (see -bvt for n)
 -bvtc=n closing brace vertical tightness: 
         n=(0=open, 1=sometimes close, 2=always close)
 -pvtc=n closing paren vertical tightness, see -bvtc for n.
 -sbvtc=n closing square bracket vertical tightness, see -bvtc for n.
 -ci=n   sets continuation indentation=n,  default is n=2 spaces
 -lp     line up parentheses, brackets, and non-BLOCK braces
 -sfs    add space before semicolon in for( ; ; )
 -aws    allow perltidy to add whitespace (default)
 -dws    delete all old non-essential whitespace 
 -icb    indent closing brace of a code block
 -cti=n  closing indentation of paren, square bracket, or non-block brace: 
         n=0 none, =1 align with opening, =2 one full indentation level
 -icp    equivalent to -cti=2
 -wls=s  want space left of tokens in string; i.e. -nwls='+ - * /'
 -wrs=s  want space right of tokens in string;
 -sts    put space before terminal semicolon of a statement
 -sak=s  put space between keywords given in s and '(';
 -nsak=s no space between keywords in s and '('; i.e. -nsak='my our local'

Line Break Control
 -fnl    freeze newlines; this disables all line break changes
            and disables the following switches:
 -anl    add newlines;  ok to introduce new line breaks
 -bbs    add blank line before subs and packages
 -bbc    add blank line before block comments
 -bbb    add blank line between major blocks
 -kbl=n  keep old blank lines? 0=no, 1=some, 2=all
 -mbl=n  maximum consecutive blank lines to output (default=1)
 -ce     cuddled else; use this style: '} else {'
 -cb     cuddled blocks (other than 'if-elsif-else')
 -cbl=s  list of blocks to cuddled, default 'try-catch-finally'
 -dnl    delete old newlines (default)
 -l=n    maximum line length;  default n=80
 -bl     opening brace on new line 
 -sbl    opening sub brace on new line.  value of -bl is used if not given.
 -bli    opening brace on new line and indented
 -bar    opening brace always on right, even for long clauses
 -vt=n   vertical tightness (requires -lp); n controls break after opening
         token: 0=never  1=no break if next line balanced   2=no break
 -vtc=n  vertical tightness of closing container; n controls if closing
         token starts new line: 0=always  1=not unless list  1=never
 -wba=s  want break after tokens in string; i.e. wba=': .'
 -wbb=s  want break before tokens in string
 -wn     weld nested: combines opening and closing tokens when both are adjacent

Following Old Breakpoints
 -kis    keep interior semicolons.  Allows multiple statements per line.
 -boc    break at old comma breaks: turns off all automatic list formatting
 -bol    break at old logical breakpoints: or, and, ||, && (default)
 -bok    break at old list keyword breakpoints such as map, sort (default)
 -bot    break at old conditional (ternary ?:) operator breakpoints (default)
 -boa    break at old attribute breakpoints 
 -cab=n  break at commas after a comma-arrow (=>):
         n=0 break at all commas after =>
         n=1 stable: break unless this breaks an existing one-line container
         n=2 break only if a one-line container cannot be formed
         n=3 do not treat commas after => specially at all

Comment controls
 -ibc    indent block comments (default)
 -isbc   indent spaced block comments; may indent unless no leading space
 -msc=n  minimum desired spaces to side comment, default 4
 -fpsc=n fix position for side comments; default 0;
 -csc    add or update closing side comments after closing BLOCK brace
 -dcsc   delete closing side comments created by a -csc command
 -cscp=s change closing side comment prefix to be other than '## end'
 -cscl=s change closing side comment to apply to selected list of blocks
 -csci=n minimum number of lines needed to apply a -csc tag, default n=6
 -csct=n maximum number of columns of appended text, default n=20 
 -cscw   causes warning if old side comment is overwritten with -csc

 -sbc    use 'static block comments' identified by leading '##' (default)
 -sbcp=s change static block comment identifier to be other than '##'
 -osbc   outdent static block comments

 -ssc    use 'static side comments' identified by leading '##' (default)
 -sscp=s change static side comment identifier to be other than '##'

Delete selected text
 -dac    delete all comments AND pod
 -dbc    delete block comments     
 -dsc    delete side comments  
 -dp     delete pod

Send selected text to a '.TEE' file
 -tac    tee all comments AND pod
 -tbc    tee block comments       
 -tsc    tee side comments       
 -tp     tee pod           

Outdenting
 -olq    outdent long quoted strings (default) 
 -olc    outdent a long block comment line
 -ola    outdent statement labels
 -okw    outdent control keywords (redo, next, last, goto, return)
 -okwl=s specify alternative keywords for -okw command

Other controls
 -mft=n  maximum fields per table; default n=40
 -x      do not format lines before hash-bang line (i.e., for VMS)
 -asc    allows perltidy to add a ';' when missing (default)
 -dsm    allows perltidy to delete an unnecessary ';'  (default)

Combinations of other parameters
 -gnu     attempt to follow GNU Coding Standards as applied to perl
 -mangle  remove as many newlines as possible (but keep comments and pods)
 -extrude  insert as many newlines as possible

Dump and die, debugging
 -dop    dump options used in this run to standard output and quit
 -ddf    dump default options to standard output and quit
 -dsn    dump all option short names to standard output and quit
 -dln    dump option long names to standard output and quit
 -dpro   dump whatever configuration file is in effect to standard output
 -dtt    dump all token types to standard output and quit

HTML
 -html write an html file (see 'man perl2web' for many options)
       Note: when -html is used, no indentation or formatting are done.
       Hint: try perltidy -html -css=mystyle.css filename.pl
       and edit mystyle.css to change the appearance of filename.html.
       -nnn gives line numbers
       -pre only writes out <pre>..</pre> code section
       -toc places a table of contents to subs at the top (default)
       -pod passes pod text through pod2html (default)
       -frm write html as a frame (3 files)
       -text=s extra extension for table of contents if -frm, default='toc'
       -sext=s extra extension for file content if -frm, default='src'

A prefix of "n" negates short form toggle switches, and a prefix of "no"
negates the long forms.  For example, -nasc means don't add missing
semicolons.  

If you are unable to see this entire text, try "perltidy -h | more"
For more detailed information, and additional options, try "man perltidy",
or go to the perltidy home page at http://perltidy.sourceforge.net
EOF

    return;
}

sub process_this_file {

    my ( $tokenizer, $formatter ) = @_;

    while ( my $line = $tokenizer->get_line() ) {
        $formatter->write_line($line);
    }
    my $severe_error = $tokenizer->report_tokenization_errors();
    eval { $formatter->finish_formatting($severe_error) };

    return;
}

sub check_syntax {

    # Use 'perl -c' to make sure that we did not create bad syntax
    # This is a very good independent check for programming errors
    #
    # Given names of the input and output files, ($istream, $ostream),
    # we do the following:
    # - check syntax of the input file
    # - if bad, all done (could be an incomplete code snippet)
    # - if infile syntax ok, then check syntax of the output file;
    #   - if outfile syntax bad, issue warning; this implies a code bug!
    # - set and return flag "infile_syntax_ok" : =-1 bad 0 unknown 1 good

    my ( $istream, $ostream, $logger_object, $rOpts ) = @_;
    my $infile_syntax_ok = 0;
    my $line_of_dashes   = '-' x 42 . "\n";

    my $flags = $rOpts->{'perl-syntax-check-flags'};

    # be sure we invoke perl with -c
    # note: perl will accept repeated flags like '-c -c'.  It is safest
    # to append another -c than try to find an interior bundled c, as
    # in -Tc, because such a 'c' might be in a quoted string, for example.
    if ( $flags !~ /(^-c|\s+-c)/ ) { $flags .= " -c" }

    # be sure we invoke perl with -x if requested
    # same comments about repeated parameters applies
    if ( $rOpts->{'look-for-hash-bang'} ) {
        if ( $flags !~ /(^-x|\s+-x)/ ) { $flags .= " -x" }
    }

    # this shouldn't happen unless a temporary file couldn't be made
    if ( $istream eq '-' ) {
        $logger_object->write_logfile_entry(
            "Cannot run perl -c on STDIN and STDOUT\n");
        return $infile_syntax_ok;
    }

    $logger_object->write_logfile_entry(
        "checking input file syntax with perl $flags\n");

    # Not all operating systems/shells support redirection of the standard
    # error output.
    my $error_redirection = ( $^O eq 'VMS' ) ? "" : '2>&1';

    my ( $istream_filename, $perl_output ) =
      do_syntax_check( $istream, $flags, $error_redirection );
    $logger_object->write_logfile_entry(
        "Input stream passed to Perl as file $istream_filename\n");
    $logger_object->write_logfile_entry($line_of_dashes);
    $logger_object->write_logfile_entry("$perl_output\n");

    if ( $perl_output =~ /syntax\s*OK/ ) {
        $infile_syntax_ok = 1;
        $logger_object->write_logfile_entry($line_of_dashes);
        $logger_object->write_logfile_entry(
            "checking output file syntax with perl $flags ...\n");
        my ( $ostream_filename, $perl_output ) =
          do_syntax_check( $ostream, $flags, $error_redirection );
        $logger_object->write_logfile_entry(
            "Output stream passed to Perl as file $ostream_filename\n");
        $logger_object->write_logfile_entry($line_of_dashes);
        $logger_object->write_logfile_entry("$perl_output\n");

        unless ( $perl_output =~ /syntax\s*OK/ ) {
            $logger_object->write_logfile_entry($line_of_dashes);
            $logger_object->warning(
"The output file has a syntax error when tested with perl $flags $ostream !\n"
            );
            $logger_object->warning(
                "This implies an error in perltidy; the file $ostream is bad\n"
            );
            $logger_object->report_definite_bug();

            # the perl version number will be helpful for diagnosing the problem
            $logger_object->write_logfile_entry( $^V . "\n" );
            ##qx/perl -v $error_redirection/ . "\n" );
        }
    }
    else {

        # Only warn of perl -c syntax errors.  Other messages,
        # such as missing modules, are too common.  They can be
        # seen by running with perltidy -w
        $logger_object->complain("A syntax check using perl $flags\n");
        $logger_object->complain(
            "for the output in file $istream_filename gives:\n");
        $logger_object->complain($line_of_dashes);
        $logger_object->complain("$perl_output\n");
        $logger_object->complain($line_of_dashes);
        $infile_syntax_ok = -1;
        $logger_object->write_logfile_entry($line_of_dashes);
        $logger_object->write_logfile_entry(
"The output file will not be checked because of input file problems\n"
        );
    }
    return $infile_syntax_ok;
}

sub do_syntax_check {

    # This should not be called; the syntax check is deactivated
    Die("Unexpected call for syntax check-shouldn't happen\n");
    return;
}

=pod
sub do_syntax_check {
    my ( $stream, $flags, $error_redirection ) = @_;

    ############################################################
    # This code is not reachable because syntax check is deactivated,
    # but it is retained for reference.
    ############################################################

    # We need a named input file for executing perl
    my ( $stream_filename, $is_tmpfile ) = get_stream_as_named_file($stream);

    # TODO: Need to add name of file to log somewhere
    # otherwise Perl output is hard to read
    if ( !$stream_filename ) { return $stream_filename, "" }

    # We have to quote the filename in case it has unusual characters
    # or spaces.  Example: this filename #CM11.pm# gives trouble.
    my $quoted_stream_filename = '"' . $stream_filename . '"';

    # Under VMS something like -T will become -t (and an error) so we
    # will put quotes around the flags.  Double quotes seem to work on
    # Unix/Windows/VMS, but this may not work on all systems.  (Single
    # quotes do not work under Windows).  It could become necessary to
    # put double quotes around each flag, such as:  -"c"  -"T"
    # We may eventually need some system-dependent coding here.
    $flags = '"' . $flags . '"';

    # now wish for luck...
    my $msg = qx/perl $flags $quoted_stream_filename $error_redirection/; 

    if ($is_tmpfile) {
        unlink $stream_filename
          or Perl::Tidy::Die("couldn't unlink stream $stream_filename: $!\n");
    }
    return $stream_filename, $msg;
}
=cut

1;

