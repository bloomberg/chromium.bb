#####################################################################
#
# The Perl::Tidy::HtmlWriter class writes a copy of the input stream in html
#
#####################################################################

package Perl::Tidy::HtmlWriter;
use strict;
use warnings;
our $VERSION = '20181120';

use File::Basename;

# class variables
use vars qw{
  %html_color
  %html_bold
  %html_italic
  %token_short_names
  %short_to_long_names
  $rOpts
  $css_filename
  $css_linkname
  $missing_html_entities
  $missing_pod_html
};

# replace unsafe characters with HTML entity representation if HTML::Entities
# is available
#{ eval "use HTML::Entities"; $missing_html_entities = $@; }

BEGIN {
    if ( !eval { require HTML::Entities; 1 } ) {
        $missing_html_entities = $@ ? $@ : 1;
    }
    if ( !eval { require Pod::Html; 1 } ) {
        $missing_pod_html = $@ ? $@ : 1;
    }
}

sub new {

    my ( $class, $input_file, $html_file, $extension, $html_toc_extension,
        $html_src_extension )
      = @_;

    my $html_file_opened = 0;
    my $html_fh;
    ( $html_fh, my $html_filename ) =
      Perl::Tidy::streamhandle( $html_file, 'w' );
    unless ($html_fh) {
        Perl::Tidy::Warn("can't open $html_file: $!\n");
        return;
    }
    $html_file_opened = 1;

    if ( !$input_file || $input_file eq '-' || ref($input_file) ) {
        $input_file = "NONAME";
    }

    # write the table of contents to a string
    my $toc_string;
    my $html_toc_fh = Perl::Tidy::IOScalar->new( \$toc_string, 'w' );

    my $html_pre_fh;
    my @pre_string_stack;
    if ( $rOpts->{'html-pre-only'} ) {

        # pre section goes directly to the output stream
        $html_pre_fh = $html_fh;
        $html_pre_fh->print( <<"PRE_END");
<pre>
PRE_END
    }
    else {

        # pre section go out to a temporary string
        my $pre_string;
        $html_pre_fh = Perl::Tidy::IOScalar->new( \$pre_string, 'w' );
        push @pre_string_stack, \$pre_string;
    }

    # pod text gets diverted if the 'pod2html' is used
    my $html_pod_fh;
    my $pod_string;
    if ( $rOpts->{'pod2html'} ) {
        if ( $rOpts->{'html-pre-only'} ) {
            undef $rOpts->{'pod2html'};
        }
        else {
            ##eval "use Pod::Html";
            #if ($@) {
            if ($missing_pod_html) {
                Perl::Tidy::Warn(
"unable to find Pod::Html; cannot use pod2html\n-npod disables this message\n"
                );
                undef $rOpts->{'pod2html'};
            }
            else {
                $html_pod_fh = Perl::Tidy::IOScalar->new( \$pod_string, 'w' );
            }
        }
    }

    my $toc_filename;
    my $src_filename;
    if ( $rOpts->{'frames'} ) {
        unless ($extension) {
            Perl::Tidy::Warn(
"cannot use frames without a specified output extension; ignoring -frm\n"
            );
            undef $rOpts->{'frames'};
        }
        else {
            $toc_filename = $input_file . $html_toc_extension . $extension;
            $src_filename = $input_file . $html_src_extension . $extension;
        }
    }

    # ----------------------------------------------------------
    # Output is now directed as follows:
    # html_toc_fh <-- table of contents items
    # html_pre_fh <-- the <pre> section of formatted code, except:
    # html_pod_fh <-- pod goes here with the pod2html option
    # ----------------------------------------------------------

    my $title = $rOpts->{'title'};
    unless ($title) {
        ( $title, my $path ) = fileparse($input_file);
    }
    my $toc_item_count = 0;
    my $in_toc_package = "";
    my $last_level     = 0;
    return bless {
        _input_file        => $input_file,          # name of input file
        _title             => $title,               # title, unescaped
        _html_file         => $html_file,           # name of .html output file
        _toc_filename      => $toc_filename,        # for frames option
        _src_filename      => $src_filename,        # for frames option
        _html_file_opened  => $html_file_opened,    # a flag
        _html_fh           => $html_fh,             # the output stream
        _html_pre_fh       => $html_pre_fh,         # pre section goes here
        _rpre_string_stack => \@pre_string_stack,   # stack of pre sections
        _html_pod_fh       => $html_pod_fh,         # pod goes here if pod2html
        _rpod_string       => \$pod_string,         # string holding pod
        _pod_cut_count     => 0,                    # how many =cut's?
        _html_toc_fh       => $html_toc_fh,         # fh for table of contents
        _rtoc_string       => \$toc_string,         # string holding toc
        _rtoc_item_count   => \$toc_item_count,     # how many toc items
        _rin_toc_package   => \$in_toc_package,     # package name
        _rtoc_name_count   => {},                   # hash to track unique names
        _rpackage_stack    => [],                   # stack to check for package
                                                    # name changes
        _rlast_level       => \$last_level,         # brace indentation level
    }, $class;
}

sub close_object {
    my ($object) = @_;

    # returns true if close works, false if not
    # failure probably means there is no close method
    return eval { $object->close(); 1 };
}

sub add_toc_item {

    # Add an item to the html table of contents.
    # This is called even if no table of contents is written,
    # because we still want to put the anchors in the <pre> text.
    # We are given an anchor name and its type; types are:
    #      'package', 'sub', '__END__', '__DATA__', 'EOF'
    # There must be an 'EOF' call at the end to wrap things up.
    my ( $self, $name, $type ) = @_;
    my $html_toc_fh     = $self->{_html_toc_fh};
    my $html_pre_fh     = $self->{_html_pre_fh};
    my $rtoc_name_count = $self->{_rtoc_name_count};
    my $rtoc_item_count = $self->{_rtoc_item_count};
    my $rlast_level     = $self->{_rlast_level};
    my $rin_toc_package = $self->{_rin_toc_package};
    my $rpackage_stack  = $self->{_rpackage_stack};

    # packages contain sublists of subs, so to avoid errors all package
    # items are written and finished with the following routines
    my $end_package_list = sub {
        if ( ${$rin_toc_package} ) {
            $html_toc_fh->print("</ul>\n</li>\n");
            ${$rin_toc_package} = "";
        }
    };

    my $start_package_list = sub {
        my ( $unique_name, $package ) = @_;
        if ( ${$rin_toc_package} ) { $end_package_list->() }
        $html_toc_fh->print(<<EOM);
<li><a href=\"#$unique_name\">package $package</a>
<ul>
EOM
        ${$rin_toc_package} = $package;
    };

    # start the table of contents on the first item
    unless ( ${$rtoc_item_count} ) {

        # but just quit if we hit EOF without any other entries
        # in this case, there will be no toc
        return if ( $type eq 'EOF' );
        $html_toc_fh->print( <<"TOC_END");
<!-- BEGIN CODE INDEX --><a name="code-index"></a>
<ul>
TOC_END
    }
    ${$rtoc_item_count}++;

    # make a unique anchor name for this location:
    #   - packages get a 'package-' prefix
    #   - subs use their names
    my $unique_name = $name;
    if ( $type eq 'package' ) { $unique_name = "package-$name" }

    # append '-1', '-2', etc if necessary to make unique; this will
    # be unique because subs and packages cannot have a '-'
    if ( my $count = $rtoc_name_count->{ lc $unique_name }++ ) {
        $unique_name .= "-$count";
    }

    #   - all names get terminal '-' if pod2html is used, to avoid
    #     conflicts with anchor names created by pod2html
    if ( $rOpts->{'pod2html'} ) { $unique_name .= '-' }

    # start/stop lists of subs
    if ( $type eq 'sub' ) {
        my $package = $rpackage_stack->[ ${$rlast_level} ];
        unless ($package) { $package = 'main' }

        # if we're already in a package/sub list, be sure its the right
        # package or else close it
        if ( ${$rin_toc_package} && ${$rin_toc_package} ne $package ) {
            $end_package_list->();
        }

        # start a package/sub list if necessary
        unless ( ${$rin_toc_package} ) {
            $start_package_list->( $unique_name, $package );
        }
    }

    # now write an entry in the toc for this item
    if ( $type eq 'package' ) {
        $start_package_list->( $unique_name, $name );
    }
    elsif ( $type eq 'sub' ) {
        $html_toc_fh->print("<li><a href=\"#$unique_name\">$name</a></li>\n");
    }
    else {
        $end_package_list->();
        $html_toc_fh->print("<li><a href=\"#$unique_name\">$name</a></li>\n");
    }

    # write the anchor in the <pre> section
    $html_pre_fh->print("<a name=\"$unique_name\"></a>");

    # end the table of contents, if any, on the end of file
    if ( $type eq 'EOF' ) {
        $html_toc_fh->print( <<"TOC_END");
</ul>
<!-- END CODE INDEX -->
TOC_END
    }
    return;
}

BEGIN {

    # This is the official list of tokens which may be identified by the
    # user.  Long names are used as getopt keys.  Short names are
    # convenient short abbreviations for specifying input.  Short names
    # somewhat resemble token type characters, but are often different
    # because they may only be alphanumeric, to allow command line
    # input.  Also, note that because of case insensitivity of html,
    # this table must be in a single case only (I've chosen to use all
    # lower case).
    # When adding NEW_TOKENS: update this hash table
    # short names => long names
    %short_to_long_names = (
        'n'  => 'numeric',
        'p'  => 'paren',
        'q'  => 'quote',
        's'  => 'structure',
        'c'  => 'comment',
        'v'  => 'v-string',
        'cm' => 'comma',
        'w'  => 'bareword',
        'co' => 'colon',
        'pu' => 'punctuation',
        'i'  => 'identifier',
        'j'  => 'label',
        'h'  => 'here-doc-target',
        'hh' => 'here-doc-text',
        'k'  => 'keyword',
        'sc' => 'semicolon',
        'm'  => 'subroutine',
        'pd' => 'pod-text',
    );

    # Now we have to map actual token types into one of the above short
    # names; any token types not mapped will get 'punctuation'
    # properties.

    # The values of this hash table correspond to the keys of the
    # previous hash table.
    # The keys of this hash table are token types and can be seen
    # by running with --dump-token-types (-dtt).

    # When adding NEW_TOKENS: update this hash table
    # $type => $short_name
    %token_short_names = (
        '#'  => 'c',
        'n'  => 'n',
        'v'  => 'v',
        'k'  => 'k',
        'F'  => 'k',
        'Q'  => 'q',
        'q'  => 'q',
        'J'  => 'j',
        'j'  => 'j',
        'h'  => 'h',
        'H'  => 'hh',
        'w'  => 'w',
        ','  => 'cm',
        '=>' => 'cm',
        ';'  => 'sc',
        ':'  => 'co',
        'f'  => 'sc',
        '('  => 'p',
        ')'  => 'p',
        'M'  => 'm',
        'P'  => 'pd',
        'A'  => 'co',
    );

    # These token types will all be called identifiers for now
    # FIXME: could separate user defined modules as separate type
    my @identifier = qw< i t U C Y Z G :: CORE::>;
    @token_short_names{@identifier} = ('i') x scalar(@identifier);

    # These token types will be called 'structure'
    my @structure = qw< { } >;
    @token_short_names{@structure} = ('s') x scalar(@structure);

    # OLD NOTES: save for reference
    # Any of these could be added later if it would be useful.
    # For now, they will by default become punctuation
    #    my @list = qw< L R [ ] >;
    #    @token_long_names{@list} = ('non-structure') x scalar(@list);
    #
    #    my @list = qw"
    #      / /= * *= ** **= + += - -= % %= = ++ -- << <<= >> >>= pp p m mm
    #      ";
    #    @token_long_names{@list} = ('math') x scalar(@list);
    #
    #    my @list = qw" & &= ~ ~= ^ ^= | |= ";
    #    @token_long_names{@list} = ('bit') x scalar(@list);
    #
    #    my @list = qw" == != < > <= <=> ";
    #    @token_long_names{@list} = ('numerical-comparison') x scalar(@list);
    #
    #    my @list = qw" && || ! &&= ||= //= ";
    #    @token_long_names{@list} = ('logical') x scalar(@list);
    #
    #    my @list = qw" . .= =~ !~ x x= ";
    #    @token_long_names{@list} = ('string-operators') x scalar(@list);
    #
    #    # Incomplete..
    #    my @list = qw" .. -> <> ... \ ? ";
    #    @token_long_names{@list} = ('misc-operators') x scalar(@list);

}

sub make_getopt_long_names {
    my ( $class, $rgetopt_names ) = @_;
    while ( my ( $short_name, $name ) = each %short_to_long_names ) {
        push @{$rgetopt_names}, "html-color-$name=s";
        push @{$rgetopt_names}, "html-italic-$name!";
        push @{$rgetopt_names}, "html-bold-$name!";
    }
    push @{$rgetopt_names}, "html-color-background=s";
    push @{$rgetopt_names}, "html-linked-style-sheet=s";
    push @{$rgetopt_names}, "nohtml-style-sheets";
    push @{$rgetopt_names}, "html-pre-only";
    push @{$rgetopt_names}, "html-line-numbers";
    push @{$rgetopt_names}, "html-entities!";
    push @{$rgetopt_names}, "stylesheet";
    push @{$rgetopt_names}, "html-table-of-contents!";
    push @{$rgetopt_names}, "pod2html!";
    push @{$rgetopt_names}, "frames!";
    push @{$rgetopt_names}, "html-toc-extension=s";
    push @{$rgetopt_names}, "html-src-extension=s";

    # Pod::Html parameters:
    push @{$rgetopt_names}, "backlink=s";
    push @{$rgetopt_names}, "cachedir=s";
    push @{$rgetopt_names}, "htmlroot=s";
    push @{$rgetopt_names}, "libpods=s";
    push @{$rgetopt_names}, "podpath=s";
    push @{$rgetopt_names}, "podroot=s";
    push @{$rgetopt_names}, "title=s";

    # Pod::Html parameters with leading 'pod' which will be removed
    # before the call to Pod::Html
    push @{$rgetopt_names}, "podquiet!";
    push @{$rgetopt_names}, "podverbose!";
    push @{$rgetopt_names}, "podrecurse!";
    push @{$rgetopt_names}, "podflush";
    push @{$rgetopt_names}, "podheader!";
    push @{$rgetopt_names}, "podindex!";
    return;
}

sub make_abbreviated_names {

    # We're appending things like this to the expansion list:
    #      'hcc'    => [qw(html-color-comment)],
    #      'hck'    => [qw(html-color-keyword)],
    #  etc
    my ( $class, $rexpansion ) = @_;

    # abbreviations for color/bold/italic properties
    while ( my ( $short_name, $long_name ) = each %short_to_long_names ) {
        ${$rexpansion}{"hc$short_name"}  = ["html-color-$long_name"];
        ${$rexpansion}{"hb$short_name"}  = ["html-bold-$long_name"];
        ${$rexpansion}{"hi$short_name"}  = ["html-italic-$long_name"];
        ${$rexpansion}{"nhb$short_name"} = ["nohtml-bold-$long_name"];
        ${$rexpansion}{"nhi$short_name"} = ["nohtml-italic-$long_name"];
    }

    # abbreviations for all other html options
    ${$rexpansion}{"hcbg"}  = ["html-color-background"];
    ${$rexpansion}{"pre"}   = ["html-pre-only"];
    ${$rexpansion}{"toc"}   = ["html-table-of-contents"];
    ${$rexpansion}{"ntoc"}  = ["nohtml-table-of-contents"];
    ${$rexpansion}{"nnn"}   = ["html-line-numbers"];
    ${$rexpansion}{"hent"}  = ["html-entities"];
    ${$rexpansion}{"nhent"} = ["nohtml-entities"];
    ${$rexpansion}{"css"}   = ["html-linked-style-sheet"];
    ${$rexpansion}{"nss"}   = ["nohtml-style-sheets"];
    ${$rexpansion}{"ss"}    = ["stylesheet"];
    ${$rexpansion}{"pod"}   = ["pod2html"];
    ${$rexpansion}{"npod"}  = ["nopod2html"];
    ${$rexpansion}{"frm"}   = ["frames"];
    ${$rexpansion}{"nfrm"}  = ["noframes"];
    ${$rexpansion}{"text"}  = ["html-toc-extension"];
    ${$rexpansion}{"sext"}  = ["html-src-extension"];
    return;
}

sub check_options {

    # This will be called once after options have been parsed
    # Note that we are defining the package variable $rOpts here:
    ( my $class, $rOpts ) = @_;

    # X11 color names for default settings that seemed to look ok
    # (these color names are only used for programming clarity; the hex
    # numbers are actually written)
    use constant ForestGreen   => "#228B22";
    use constant SaddleBrown   => "#8B4513";
    use constant magenta4      => "#8B008B";
    use constant IndianRed3    => "#CD5555";
    use constant DeepSkyBlue4  => "#00688B";
    use constant MediumOrchid3 => "#B452CD";
    use constant black         => "#000000";
    use constant white         => "#FFFFFF";
    use constant red           => "#FF0000";

    # set default color, bold, italic properties
    # anything not listed here will be given the default (punctuation) color --
    # these types currently not listed and get default: ws pu s sc cm co p
    # When adding NEW_TOKENS: add an entry here if you don't want defaults

    # set_default_properties( $short_name, default_color, bold?, italic? );
    set_default_properties( 'c',  ForestGreen,   0, 0 );
    set_default_properties( 'pd', ForestGreen,   0, 1 );
    set_default_properties( 'k',  magenta4,      1, 0 );    # was SaddleBrown
    set_default_properties( 'q',  IndianRed3,    0, 0 );
    set_default_properties( 'hh', IndianRed3,    0, 1 );
    set_default_properties( 'h',  IndianRed3,    1, 0 );
    set_default_properties( 'i',  DeepSkyBlue4,  0, 0 );
    set_default_properties( 'w',  black,         0, 0 );
    set_default_properties( 'n',  MediumOrchid3, 0, 0 );
    set_default_properties( 'v',  MediumOrchid3, 0, 0 );
    set_default_properties( 'j',  IndianRed3,    1, 0 );
    set_default_properties( 'm',  red,           1, 0 );

    set_default_color( 'html-color-background',  white );
    set_default_color( 'html-color-punctuation', black );

    # setup property lookup tables for tokens based on their short names
    # every token type has a short name, and will use these tables
    # to do the html markup
    while ( my ( $short_name, $long_name ) = each %short_to_long_names ) {
        $html_color{$short_name}  = $rOpts->{"html-color-$long_name"};
        $html_bold{$short_name}   = $rOpts->{"html-bold-$long_name"};
        $html_italic{$short_name} = $rOpts->{"html-italic-$long_name"};
    }

    # write style sheet to STDOUT and die if requested
    if ( defined( $rOpts->{'stylesheet'} ) ) {
        write_style_sheet_file('-');
        Perl::Tidy::Exit(0);
    }

    # make sure user gives a file name after -css
    if ( defined( $rOpts->{'html-linked-style-sheet'} ) ) {
        $css_linkname = $rOpts->{'html-linked-style-sheet'};
        if ( $css_linkname =~ /^-/ ) {
            Perl::Tidy::Die("You must specify a valid filename after -css\n");
        }
    }

    # check for conflict
    if ( $css_linkname && $rOpts->{'nohtml-style-sheets'} ) {
        $rOpts->{'nohtml-style-sheets'} = 0;
        warning("You can't specify both -css and -nss; -nss ignored\n");
    }

    # write a style sheet file if necessary
    if ($css_linkname) {

        # if the selected filename exists, don't write, because user may
        # have done some work by hand to create it; use backup name instead
        # Also, this will avoid a potential disaster in which the user
        # forgets to specify the style sheet, like this:
        #    perltidy -html -css myfile1.pl myfile2.pl
        # This would cause myfile1.pl to parsed as the style sheet by GetOpts
        my $css_filename = $css_linkname;
        unless ( -e $css_filename ) {
            write_style_sheet_file($css_filename);
        }
    }
    $missing_html_entities = 1 unless $rOpts->{'html-entities'};
    return;
}

sub write_style_sheet_file {

    my $css_filename = shift;
    my $fh;
    unless ( $fh = IO::File->new("> $css_filename") ) {
        Perl::Tidy::Die("can't open $css_filename: $!\n");
    }
    write_style_sheet_data($fh);
    close_object($fh);
    return;
}

sub write_style_sheet_data {

    # write the style sheet data to an open file handle
    my $fh = shift;

    my $bg_color   = $rOpts->{'html-color-background'};
    my $text_color = $rOpts->{'html-color-punctuation'};

    # pre-bgcolor is new, and may not be defined
    my $pre_bg_color = $rOpts->{'html-pre-color-background'};
    $pre_bg_color = $bg_color unless $pre_bg_color;

    $fh->print(<<"EOM");
/* default style sheet generated by perltidy */
body {background: $bg_color; color: $text_color}
pre { color: $text_color; 
      background: $pre_bg_color;
      font-family: courier;
    } 

EOM

    foreach my $short_name ( sort keys %short_to_long_names ) {
        my $long_name = $short_to_long_names{$short_name};

        my $abbrev = '.' . $short_name;
        if ( length($short_name) == 1 ) { $abbrev .= ' ' }    # for alignment
        my $color = $html_color{$short_name};
        if ( !defined($color) ) { $color = $text_color }
        $fh->print("$abbrev \{ color: $color;");

        if ( $html_bold{$short_name} ) {
            $fh->print(" font-weight:bold;");
        }

        if ( $html_italic{$short_name} ) {
            $fh->print(" font-style:italic;");
        }
        $fh->print("} /* $long_name */\n");
    }
    return;
}

sub set_default_color {

    # make sure that options hash $rOpts->{$key} contains a valid color
    my ( $key, $color ) = @_;
    if ( $rOpts->{$key} ) { $color = $rOpts->{$key} }
    $rOpts->{$key} = check_RGB($color);
    return;
}

sub check_RGB {

    # if color is a 6 digit hex RGB value, prepend a #, otherwise
    # assume that it is a valid ascii color name
    my ($color) = @_;
    if ( $color =~ /^[0-9a-fA-F]{6,6}$/ ) { $color = "#$color" }
    return $color;
}

sub set_default_properties {
    my ( $short_name, $color, $bold, $italic ) = @_;

    set_default_color( "html-color-$short_to_long_names{$short_name}", $color );
    my $key;
    $key = "html-bold-$short_to_long_names{$short_name}";
    $rOpts->{$key} = ( defined $rOpts->{$key} ) ? $rOpts->{$key} : $bold;
    $key = "html-italic-$short_to_long_names{$short_name}";
    $rOpts->{$key} = ( defined $rOpts->{$key} ) ? $rOpts->{$key} : $italic;
    return;
}

sub pod_to_html {

    # Use Pod::Html to process the pod and make the page
    # then merge the perltidy code sections into it.
    # return 1 if success, 0 otherwise
    my ( $self, $pod_string, $css_string, $toc_string, $rpre_string_stack ) =
      @_;
    my $input_file   = $self->{_input_file};
    my $title        = $self->{_title};
    my $success_flag = 0;

    # don't try to use pod2html if no pod
    unless ($pod_string) {
        return $success_flag;
    }

    # Pod::Html requires a real temporary filename
    my ( $fh_tmp, $tmpfile ) = File::Temp::tempfile();
    unless ($fh_tmp) {
        Perl::Tidy::Warn(
            "unable to open temporary file $tmpfile; cannot use pod2html\n");
        return $success_flag;
    }

    #------------------------------------------------------------------
    # Warning: a temporary file is open; we have to clean up if
    # things go bad.  From here on all returns should be by going to
    # RETURN so that the temporary file gets unlinked.
    #------------------------------------------------------------------

    # write the pod text to the temporary file
    $fh_tmp->print($pod_string);
    $fh_tmp->close();

    # Hand off the pod to pod2html.
    # Note that we can use the same temporary filename for input and output
    # because of the way pod2html works.
    {

        my @args;
        push @args, "--infile=$tmpfile", "--outfile=$tmpfile", "--title=$title";

        # Flags with string args:
        # "backlink=s", "cachedir=s", "htmlroot=s", "libpods=s",
        # "podpath=s", "podroot=s"
        # Note: -css=s is handled by perltidy itself
        foreach my $kw (qw(backlink cachedir htmlroot libpods podpath podroot))
        {
            if ( $rOpts->{$kw} ) { push @args, "--$kw=$rOpts->{$kw}" }
        }

        # Toggle switches; these have extra leading 'pod'
        # "header!", "index!", "recurse!", "quiet!", "verbose!"
        foreach my $kw (qw(podheader podindex podrecurse podquiet podverbose)) {
            my $kwd = $kw;    # allows us to strip 'pod'
            if ( $rOpts->{$kw} ) { $kwd =~ s/^pod//; push @args, "--$kwd" }
            elsif ( defined( $rOpts->{$kw} ) ) {
                $kwd =~ s/^pod//;
                push @args, "--no$kwd";
            }
        }

        # "flush",
        my $kw = 'podflush';
        if ( $rOpts->{$kw} ) { $kw =~ s/^pod//; push @args, "--$kw" }

        # Must clean up if pod2html dies (it can);
        # Be careful not to overwrite callers __DIE__ routine
        local $SIG{__DIE__} = sub {
            unlink $tmpfile if -e $tmpfile;
            Perl::Tidy::Die( $_[0] );
        };

        pod2html(@args);
    }
    $fh_tmp = IO::File->new( $tmpfile, 'r' );
    unless ($fh_tmp) {

        # this error shouldn't happen ... we just used this filename
        Perl::Tidy::Warn(
            "unable to open temporary file $tmpfile; cannot use pod2html\n");
        goto RETURN;
    }

    my $html_fh = $self->{_html_fh};
    my @toc;
    my $in_toc;
    my $ul_level = 0;
    my $no_print;

    # This routine will write the html selectively and store the toc
    my $html_print = sub {
        foreach (@_) {
            $html_fh->print($_) unless ($no_print);
            if ($in_toc) { push @toc, $_ }
        }
    };

    # loop over lines of html output from pod2html and merge in
    # the necessary perltidy html sections
    my ( $saw_body, $saw_index, $saw_body_end );

    my $timestamp = "";
    if ( $rOpts->{'timestamp'} ) {
        my $date = localtime;
        $timestamp = "on $date";
    }
    while ( my $line = $fh_tmp->getline() ) {

        if ( $line =~ /^\s*<html>\s*$/i ) {
            ##my $date = localtime;
            ##$html_print->("<!-- Generated by perltidy on $date -->\n");
            $html_print->("<!-- Generated by perltidy $timestamp -->\n");
            $html_print->($line);
        }

        # Copy the perltidy css, if any, after <body> tag
        elsif ( $line =~ /^\s*<body.*>\s*$/i ) {
            $saw_body = 1;
            $html_print->($css_string) if $css_string;
            $html_print->($line);

            # add a top anchor and heading
            $html_print->("<a name=\"-top-\"></a>\n");
            $title = escape_html($title);
            $html_print->("<h1>$title</h1>\n");
        }

        # check for start of index, old pod2html
        # before Pod::Html VERSION 1.15_02 it is delimited by comments as:
        #    <!-- INDEX BEGIN -->
        #    <ul>
        #     ...
        #    </ul>
        #    <!-- INDEX END -->
        #
        elsif ( $line =~ /^\s*<!-- INDEX BEGIN -->\s*$/i ) {
            $in_toc = 'INDEX';

            # when frames are used, an extra table of contents in the
            # contents panel is confusing, so don't print it
            $no_print = $rOpts->{'frames'}
              || !$rOpts->{'html-table-of-contents'};
            $html_print->("<h2>Doc Index:</h2>\n") if $rOpts->{'frames'};
            $html_print->($line);
        }

        # check for start of index, new pod2html
        # After Pod::Html VERSION 1.15_02 it is delimited as:
        # <ul id="index">
        # ...
        # </ul>
        elsif ( $line =~ /^\s*<ul\s+id="index">/i ) {
            $in_toc   = 'UL';
            $ul_level = 1;

            # when frames are used, an extra table of contents in the
            # contents panel is confusing, so don't print it
            $no_print = $rOpts->{'frames'}
              || !$rOpts->{'html-table-of-contents'};
            $html_print->("<h2>Doc Index:</h2>\n") if $rOpts->{'frames'};
            $html_print->($line);
        }

        # Check for end of index, old pod2html
        elsif ( $line =~ /^\s*<!-- INDEX END -->\s*$/i ) {
            $saw_index = 1;
            $html_print->($line);

            # Copy the perltidy toc, if any, after the Pod::Html toc
            if ($toc_string) {
                $html_print->("<hr />\n") if $rOpts->{'frames'};
                $html_print->("<h2>Code Index:</h2>\n");
                ##my @toc = map { $_ .= "\n" } split /\n/, $toc_string;
                my @toc = map { $_ . "\n" } split /\n/, $toc_string;
                $html_print->(@toc);
            }
            $in_toc   = "";
            $no_print = 0;
        }

        # must track <ul> depth level for new pod2html
        elsif ( $line =~ /\s*<ul>\s*$/i && $in_toc eq 'UL' ) {
            $ul_level++;
            $html_print->($line);
        }

        # Check for end of index, for new pod2html
        elsif ( $line =~ /\s*<\/ul>/i && $in_toc eq 'UL' ) {
            $ul_level--;
            $html_print->($line);

            # Copy the perltidy toc, if any, after the Pod::Html toc
            if ( $ul_level <= 0 ) {
                $saw_index = 1;
                if ($toc_string) {
                    $html_print->("<hr />\n") if $rOpts->{'frames'};
                    $html_print->("<h2>Code Index:</h2>\n");
                    ##my @toc = map { $_ .= "\n" } split /\n/, $toc_string;
                    my @toc = map { $_ . "\n" } split /\n/, $toc_string;
                    $html_print->(@toc);
                }
                $in_toc   = "";
                $ul_level = 0;
                $no_print = 0;
            }
        }

        # Copy one perltidy section after each marker
        elsif ( $line =~ /^(.*)<!-- pERLTIDY sECTION -->(.*)$/ ) {
            $line = $2;
            $html_print->($1) if $1;

            # Intermingle code and pod sections if we saw multiple =cut's.
            if ( $self->{_pod_cut_count} > 1 ) {
                my $rpre_string = shift( @{$rpre_string_stack} );
                if ( ${$rpre_string} ) {
                    $html_print->('<pre>');
                    $html_print->( ${$rpre_string} );
                    $html_print->('</pre>');
                }
                else {

                    # shouldn't happen: we stored a string before writing
                    # each marker.
                    Perl::Tidy::Warn(
"Problem merging html stream with pod2html; order may be wrong\n"
                    );
                }
                $html_print->($line);
            }

            # If didn't see multiple =cut lines, we'll put the pod out first
            # and then the code, because it's less confusing.
            else {

                # since we are not intermixing code and pod, we don't need
                # or want any <hr> lines which separated pod and code
                $html_print->($line) unless ( $line =~ /^\s*<hr>\s*$/i );
            }
        }

        # Copy any remaining code section before the </body> tag
        elsif ( $line =~ /^\s*<\/body>\s*$/i ) {
            $saw_body_end = 1;
            if ( @{$rpre_string_stack} ) {
                unless ( $self->{_pod_cut_count} > 1 ) {
                    $html_print->('<hr />');
                }
                while ( my $rpre_string = shift( @{$rpre_string_stack} ) ) {
                    $html_print->('<pre>');
                    $html_print->( ${$rpre_string} );
                    $html_print->('</pre>');
                }
            }
            $html_print->($line);
        }
        else {
            $html_print->($line);
        }
    }

    $success_flag = 1;
    unless ($saw_body) {
        Perl::Tidy::Warn("Did not see <body> in pod2html output\n");
        $success_flag = 0;
    }
    unless ($saw_body_end) {
        Perl::Tidy::Warn("Did not see </body> in pod2html output\n");
        $success_flag = 0;
    }
    unless ($saw_index) {
        Perl::Tidy::Warn("Did not find INDEX END in pod2html output\n");
        $success_flag = 0;
    }

  RETURN:
    close_object($html_fh);

    # note that we have to unlink tmpfile before making frames
    # because the tmpfile may be one of the names used for frames
    if ( -e $tmpfile ) {
        unless ( unlink($tmpfile) ) {
            Perl::Tidy::Warn("couldn't unlink temporary file $tmpfile: $!\n");
            $success_flag = 0;
        }
    }

    if ( $success_flag && $rOpts->{'frames'} ) {
        $self->make_frame( \@toc );
    }
    return $success_flag;
}

sub make_frame {

    # Make a frame with table of contents in the left panel
    # and the text in the right panel.
    # On entry:
    #  $html_filename contains the no-frames html output
    #  $rtoc is a reference to an array with the table of contents
    my ( $self, $rtoc ) = @_;
    my $input_file    = $self->{_input_file};
    my $html_filename = $self->{_html_file};
    my $toc_filename  = $self->{_toc_filename};
    my $src_filename  = $self->{_src_filename};
    my $title         = $self->{_title};
    $title = escape_html($title);

    # FUTURE input parameter:
    my $top_basename = "";

    # We need to produce 3 html files:
    # 1. - the table of contents
    # 2. - the contents (source code) itself
    # 3. - the frame which contains them

    # get basenames for relative links
    my ( $toc_basename, $toc_path ) = fileparse($toc_filename);
    my ( $src_basename, $src_path ) = fileparse($src_filename);

    # 1. Make the table of contents panel, with appropriate changes
    # to the anchor names
    my $src_frame_name = 'SRC';
    my $first_anchor =
      write_toc_html( $title, $toc_filename, $src_basename, $rtoc,
        $src_frame_name );

    # 2. The current .html filename is renamed to be the contents panel
    rename( $html_filename, $src_filename )
      or Perl::Tidy::Die("Cannot rename $html_filename to $src_filename:$!\n");

    # 3. Then use the original html filename for the frame
    write_frame_html(
        $title,        $html_filename, $top_basename,
        $toc_basename, $src_basename,  $src_frame_name
    );
    return;
}

sub write_toc_html {

    # write a separate html table of contents file for frames
    my ( $title, $toc_filename, $src_basename, $rtoc, $src_frame_name ) = @_;
    my $fh = IO::File->new( $toc_filename, 'w' )
      or Perl::Tidy::Die("Cannot open $toc_filename:$!\n");
    $fh->print(<<EOM);
<html>
<head>
<title>$title</title>
</head>
<body>
<h1><a href=\"$src_basename#-top-" target="$src_frame_name">$title</a></h1>
EOM

    my $first_anchor =
      change_anchor_names( $rtoc, $src_basename, "$src_frame_name" );
    $fh->print( join "", @{$rtoc} );

    $fh->print(<<EOM);
</body>
</html>
EOM

    return;
}

sub write_frame_html {

    # write an html file to be the table of contents frame
    my (
        $title,        $frame_filename, $top_basename,
        $toc_basename, $src_basename,   $src_frame_name
    ) = @_;

    my $fh = IO::File->new( $frame_filename, 'w' )
      or Perl::Tidy::Die("Cannot open $toc_basename:$!\n");

    $fh->print(<<EOM);
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Frameset//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd">
<?xml version="1.0" encoding="iso-8859-1" ?>
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>$title</title>
</head>
EOM

    # two left panels, one right, if master index file
    if ($top_basename) {
        $fh->print(<<EOM);
<frameset cols="20%,80%">
<frameset rows="30%,70%">
<frame src = "$top_basename" />
<frame src = "$toc_basename" />
</frameset>
EOM
    }

    # one left panels, one right, if no master index file
    else {
        $fh->print(<<EOM);
<frameset cols="20%,*">
<frame src = "$toc_basename" />
EOM
    }
    $fh->print(<<EOM);
<frame src = "$src_basename" name = "$src_frame_name" />
<noframes>
<body>
<p>If you see this message, you are using a non-frame-capable web client.</p>
<p>This document contains:</p>
<ul>
<li><a href="$toc_basename">A table of contents</a></li>
<li><a href="$src_basename">The source code</a></li>
</ul>
</body>
</noframes>
</frameset>
</html>
EOM
    return;
}

sub change_anchor_names {

    # add a filename and target to anchors
    # also return the first anchor
    my ( $rlines, $filename, $target ) = @_;
    my $first_anchor;
    foreach my $line ( @{$rlines} ) {

        #  We're looking for lines like this:
        #  <LI><A HREF="#synopsis">SYNOPSIS</A></LI>
        #  ----  -       --------  -----------------
        #  $1              $4            $5
        if ( $line =~ /^(.*)<a(.*)href\s*=\s*"([^#]*)#([^"]+)"[^>]*>(.*)$/i ) {
            my $pre  = $1;
            my $name = $4;
            my $post = $5;
            my $href = "$filename#$name";
            $line = "$pre<a href=\"$href\" target=\"$target\">$post\n";
            unless ($first_anchor) { $first_anchor = $href }
        }
    }
    return $first_anchor;
}

sub close_html_file {
    my $self = shift;
    return unless $self->{_html_file_opened};

    my $html_fh     = $self->{_html_fh};
    my $rtoc_string = $self->{_rtoc_string};

    # There are 3 basic paths to html output...

    # ---------------------------------
    # Path 1: finish up if in -pre mode
    # ---------------------------------
    if ( $rOpts->{'html-pre-only'} ) {
        $html_fh->print( <<"PRE_END");
</pre>
PRE_END
        close_object($html_fh);
        return;
    }

    # Finish the index
    $self->add_toc_item( 'EOF', 'EOF' );

    my $rpre_string_stack = $self->{_rpre_string_stack};

    # Patch to darken the <pre> background color in case of pod2html and
    # interleaved code/documentation.  Otherwise, the distinction
    # between code and documentation is blurred.
    if (   $rOpts->{pod2html}
        && $self->{_pod_cut_count} >= 1
        && $rOpts->{'html-color-background'} eq '#FFFFFF' )
    {
        $rOpts->{'html-pre-color-background'} = '#F0F0F0';
    }

    # put the css or its link into a string, if used
    my $css_string;
    my $fh_css = Perl::Tidy::IOScalar->new( \$css_string, 'w' );

    # use css linked to another file
    if ( $rOpts->{'html-linked-style-sheet'} ) {
        $fh_css->print(
            qq(<link rel="stylesheet" href="$css_linkname" type="text/css" />));
    }

    # use css embedded in this file
    elsif ( !$rOpts->{'nohtml-style-sheets'} ) {
        $fh_css->print( <<'ENDCSS');
<style type="text/css">
<!--
ENDCSS
        write_style_sheet_data($fh_css);
        $fh_css->print( <<"ENDCSS");
-->
</style>
ENDCSS
    }

    # -----------------------------------------------------------
    # path 2: use pod2html if requested
    #         If we fail for some reason, continue on to path 3
    # -----------------------------------------------------------
    if ( $rOpts->{'pod2html'} ) {
        my $rpod_string = $self->{_rpod_string};
        $self->pod_to_html(
            ${$rpod_string}, $css_string,
            ${$rtoc_string}, $rpre_string_stack
        ) && return;
    }

    # --------------------------------------------------
    # path 3: write code in html, with pod only in italics
    # --------------------------------------------------
    my $input_file = $self->{_input_file};
    my $title      = escape_html($input_file);
    my $timestamp  = "";
    if ( $rOpts->{'timestamp'} ) {
        my $date = localtime;
        $timestamp = "on $date";
    }
    $html_fh->print( <<"HTML_START");
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" 
   "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<!-- Generated by perltidy $timestamp -->
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>$title</title>
HTML_START

    # output the css, if used
    if ($css_string) {
        $html_fh->print($css_string);
        $html_fh->print( <<"ENDCSS");
</head>
<body>
ENDCSS
    }
    else {

        $html_fh->print( <<"HTML_START");
</head>
<body bgcolor=\"$rOpts->{'html-color-background'}\" text=\"$rOpts->{'html-color-punctuation'}\">
HTML_START
    }

    $html_fh->print("<a name=\"-top-\"></a>\n");
    $html_fh->print( <<"EOM");
<h1>$title</h1>
EOM

    # copy the table of contents
    if (   ${$rtoc_string}
        && !$rOpts->{'frames'}
        && $rOpts->{'html-table-of-contents'} )
    {
        $html_fh->print( ${$rtoc_string} );
    }

    # copy the pre section(s)
    my $fname_comment = $input_file;
    $fname_comment =~ s/--+/-/g;    # protect HTML comment tags
    $html_fh->print( <<"END_PRE");
<hr />
<!-- contents of filename: $fname_comment -->
<pre>
END_PRE

    foreach my $rpre_string ( @{$rpre_string_stack} ) {
        $html_fh->print( ${$rpre_string} );
    }

    # and finish the html page
    $html_fh->print( <<"HTML_END");
</pre>
</body>
</html>
HTML_END
    close_object($html_fh);

    if ( $rOpts->{'frames'} ) {
        ##my @toc = map { $_ .= "\n" } split /\n/, ${$rtoc_string};
        my @toc = map { $_ . "\n" } split /\n/, ${$rtoc_string};
        $self->make_frame( \@toc );
    }
    return;
}

sub markup_tokens {
    my ( $self, $rtokens, $rtoken_type, $rlevels ) = @_;
    my ( @colored_tokens, $type, $token, $level );
    my $rlast_level    = $self->{_rlast_level};
    my $rpackage_stack = $self->{_rpackage_stack};

    for ( my $j = 0 ; $j < @{$rtoken_type} ; $j++ ) {
        $type  = $rtoken_type->[$j];
        $token = $rtokens->[$j];
        $level = $rlevels->[$j];
        $level = 0 if ( $level < 0 );

        #-------------------------------------------------------
        # Update the package stack.  The package stack is needed to keep
        # the toc correct because some packages may be declared within
        # blocks and go out of scope when we leave the block.
        #-------------------------------------------------------
        if ( $level > ${$rlast_level} ) {
            unless ( $rpackage_stack->[ $level - 1 ] ) {
                $rpackage_stack->[ $level - 1 ] = 'main';
            }
            $rpackage_stack->[$level] = $rpackage_stack->[ $level - 1 ];
        }
        elsif ( $level < ${$rlast_level} ) {
            my $package = $rpackage_stack->[$level];
            unless ($package) { $package = 'main' }

            # if we change packages due to a nesting change, we
            # have to make an entry in the toc
            if ( $package ne $rpackage_stack->[ $level + 1 ] ) {
                $self->add_toc_item( $package, 'package' );
            }
        }
        ${$rlast_level} = $level;

        #-------------------------------------------------------
        # Intercept a sub name here; split it
        # into keyword 'sub' and sub name; and add an
        # entry in the toc
        #-------------------------------------------------------
        if ( $type eq 'i' && $token =~ /^(sub\s+)(\w.*)$/ ) {
            $token = $self->markup_html_element( $1, 'k' );
            push @colored_tokens, $token;
            $token = $2;
            $type  = 'M';

            # but don't include sub declarations in the toc;
            # these wlll have leading token types 'i;'
            my $signature = join "", @{$rtoken_type};
            unless ( $signature =~ /^i;/ ) {
                my $subname = $token;
                $subname =~ s/[\s\(].*$//; # remove any attributes and prototype
                $self->add_toc_item( $subname, 'sub' );
            }
        }

        #-------------------------------------------------------
        # Intercept a package name here; split it
        # into keyword 'package' and name; add to the toc,
        # and update the package stack
        #-------------------------------------------------------
        if ( $type eq 'i' && $token =~ /^(package\s+)(\w.*)$/ ) {
            $token = $self->markup_html_element( $1, 'k' );
            push @colored_tokens, $token;
            $token = $2;
            $type  = 'i';
            $self->add_toc_item( "$token", 'package' );
            $rpackage_stack->[$level] = $token;
        }

        $token = $self->markup_html_element( $token, $type );
        push @colored_tokens, $token;
    }
    return ( \@colored_tokens );
}

sub markup_html_element {
    my ( $self, $token, $type ) = @_;

    return $token if ( $type eq 'b' );         # skip a blank token
    return $token if ( $token =~ /^\s*$/ );    # skip a blank line
    $token = escape_html($token);

    # get the short abbreviation for this token type
    my $short_name = $token_short_names{$type};
    if ( !defined($short_name) ) {
        $short_name = "pu";                    # punctuation is default
    }

    # handle style sheets..
    if ( !$rOpts->{'nohtml-style-sheets'} ) {
        if ( $short_name ne 'pu' ) {
            $token = qq(<span class="$short_name">) . $token . "</span>";
        }
    }

    # handle no style sheets..
    else {
        my $color = $html_color{$short_name};

        if ( $color && ( $color ne $rOpts->{'html-color-punctuation'} ) ) {
            $token = qq(<font color="$color">) . $token . "</font>";
        }
        if ( $html_italic{$short_name} ) { $token = "<i>$token</i>" }
        if ( $html_bold{$short_name} )   { $token = "<b>$token</b>" }
    }
    return $token;
}

sub escape_html {

    my $token = shift;
    if ($missing_html_entities) {
        $token =~ s/\&/&amp;/g;
        $token =~ s/\</&lt;/g;
        $token =~ s/\>/&gt;/g;
        $token =~ s/\"/&quot;/g;
    }
    else {
        HTML::Entities::encode_entities($token);
    }
    return $token;
}

sub finish_formatting {

    # called after last line
    my $self = shift;
    $self->close_html_file();
    return;
}

sub write_line {

    my ( $self, $line_of_tokens ) = @_;
    return unless $self->{_html_file_opened};
    my $html_pre_fh = $self->{_html_pre_fh};
    my $line_type   = $line_of_tokens->{_line_type};
    my $input_line  = $line_of_tokens->{_line_text};
    my $line_number = $line_of_tokens->{_line_number};
    chomp $input_line;

    # markup line of code..
    my $html_line;
    if ( $line_type eq 'CODE' ) {
        my $rtoken_type = $line_of_tokens->{_rtoken_type};
        my $rtokens     = $line_of_tokens->{_rtokens};
        my $rlevels     = $line_of_tokens->{_rlevels};

        if ( $input_line =~ /(^\s*)/ ) {
            $html_line = $1;
        }
        else {
            $html_line = "";
        }
        my ($rcolored_tokens) =
          $self->markup_tokens( $rtokens, $rtoken_type, $rlevels );
        $html_line .= join '', @{$rcolored_tokens};
    }

    # markup line of non-code..
    else {
        my $line_character;
        if    ( $line_type eq 'HERE' )       { $line_character = 'H' }
        elsif ( $line_type eq 'HERE_END' )   { $line_character = 'h' }
        elsif ( $line_type eq 'FORMAT' )     { $line_character = 'H' }
        elsif ( $line_type eq 'FORMAT_END' ) { $line_character = 'h' }
        elsif ( $line_type eq 'SYSTEM' )     { $line_character = 'c' }
        elsif ( $line_type eq 'END_START' ) {
            $line_character = 'k';
            $self->add_toc_item( '__END__', '__END__' );
        }
        elsif ( $line_type eq 'DATA_START' ) {
            $line_character = 'k';
            $self->add_toc_item( '__DATA__', '__DATA__' );
        }
        elsif ( $line_type =~ /^POD/ ) {
            $line_character = 'P';
            if ( $rOpts->{'pod2html'} ) {
                my $html_pod_fh = $self->{_html_pod_fh};
                if ( $line_type eq 'POD_START' ) {

                    my $rpre_string_stack = $self->{_rpre_string_stack};
                    my $rpre_string       = $rpre_string_stack->[-1];

                    # if we have written any non-blank lines to the
                    # current pre section, start writing to a new output
                    # string
                    if ( ${$rpre_string} =~ /\S/ ) {
                        my $pre_string;
                        $html_pre_fh =
                          Perl::Tidy::IOScalar->new( \$pre_string, 'w' );
                        $self->{_html_pre_fh} = $html_pre_fh;
                        push @{$rpre_string_stack}, \$pre_string;

                        # leave a marker in the pod stream so we know
                        # where to put the pre section we just
                        # finished.
                        my $for_html = '=for html';    # don't confuse pod utils
                        $html_pod_fh->print(<<EOM);

$for_html
<!-- pERLTIDY sECTION -->

EOM
                    }

                    # otherwise, just clear the current string and start
                    # over
                    else {
                        ${$rpre_string} = "";
                        $html_pod_fh->print("\n");
                    }
                }
                $html_pod_fh->print( $input_line . "\n" );
                if ( $line_type eq 'POD_END' ) {
                    $self->{_pod_cut_count}++;
                    $html_pod_fh->print("\n");
                }
                return;
            }
        }
        else { $line_character = 'Q' }
        $html_line = $self->markup_html_element( $input_line, $line_character );
    }

    # add the line number if requested
    if ( $rOpts->{'html-line-numbers'} ) {
        my $extra_space =
            ( $line_number < 10 )   ? "   "
          : ( $line_number < 100 )  ? "  "
          : ( $line_number < 1000 ) ? " "
          :                           "";
        $html_line = $extra_space . $line_number . " " . $html_line;
    }

    # write the line
    $html_pre_fh->print("$html_line\n");
    return;
}
1;

