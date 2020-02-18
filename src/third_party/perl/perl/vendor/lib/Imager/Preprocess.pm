package Imager::Preprocess;
use strict;
require Exporter;
use vars qw(@ISA @EXPORT $VERSION);
use Getopt::Long;
use Text::ParseWords;

@EXPORT = qw(preprocess);
@ISA = qw(Exporter);

$VERSION = "1.001";

sub preprocess {
  unshift @ARGV, grep /^-/, shellwords($ENV{IMAGER_PREPROCESS_OPTS})
    if $ENV{IMAGER_PREPROCESS_OPTS};
  my $skip_lines = 0;
  GetOptions("l" => \$skip_lines)
    or usage();
  my $keep_lines = !$skip_lines;

  my $src = shift @ARGV;
  my $dest = shift @ARGV
    or usage();

  open SRC, "< $src"
  or die "Cannot open $src: $!\n";

  my $cond;
  my $cond_line;
  my $save_code;
  my @code;
  my $code_line;
  my @out;
  my $failed;

  push @out, 
    "#define IM_ROUND_8(x) ((int)((x)+0.5))\n",
    "#define IM_ROUND_double(x) (x)\n",
    "#define IM_LIMIT_8(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))\n",
    "#define IM_LIMIT_double(x) ((x) < 0.0 ? 0.0 : (x) > 1.0 ? 1.0 : (x))\n";
  push @out, "#line 1 \"$src\"\n" if $keep_lines;
  while (defined(my $line = <SRC>)) {
    if ($line =~ /^\#code\s+(\S.+)$/) {
      $save_code
	and do { warn "$src:$code_line:Unclosed #code block\n"; ++$failed; };
      
      $cond = $1;
      $cond_line = $.;
      $code_line = $. + 1;
      $save_code = 1;
    }
    elsif ($line =~ /^\#code\s*$/) {
      $save_code
	and do { warn "$src:$code_line:Unclosed #code block\n"; ++$failed; };
      
      $cond = '';
      $cond_line = 0;
      $code_line = $. + 1;
      $save_code = 1;
    }
    elsif ($line =~ /^\#\/code\s*$/) {
      $save_code
	or do { warn "$src:$.:#/code without #code\n"; ++$failed; next; };
      
      if ($cond) {
	push @out, "#line $cond_line \"$src\"\n" if $keep_lines;
	push @out, "  if ($cond) {\n";
      }
      push @out,
	"#undef IM_EIGHT_BIT\n",
	"#define IM_EIGHT_BIT 1\n",
	"#undef IM_FILL_COMBINE\n",
        "#define IM_FILL_COMBINE(fill) ((fill)->combine)\n",
	"#undef IM_FILL_FILLER\n",
        "#define IM_FILL_FILLER(fill) ((fill)->f_fill_with_color)\n";
      push @out, "#line $code_line \"$src\"\n" if $keep_lines;
      push @out, byte_samples(@code);
      push @out, "  }\n", "  else {\n"
	if $cond;
      push @out, 
	"#undef IM_EIGHT_BIT\n",
	"#undef IM_FILL_COMBINE\n",
        "#define IM_FILL_COMBINE(fill) ((fill)->combinef)\n",
	"#undef IM_FILL_FILLER\n",
        "#define IM_FILL_FILLER(fill) ((fill)->f_fill_with_fcolor)\n";
      push @out, "#line $code_line \"$src\"\n" if $keep_lines;
      push @out, double_samples(@code);
      push @out, "  }\n"
	if $cond;
      push @out, "#line ",$.+1," \"$src\"\n" if $keep_lines;
      @code = ();
      $save_code = 0;
    }
    elsif ($save_code) {
      push @code, $line;
    }
    else {
      push @out, $line;
    }
  }
  
  if ($save_code) {
    warn "$src:$code_line:#code block not closed by EOF\n";
    ++$failed;
  }

  close SRC;
  
  $failed 
    and die "Errors during parsing, aborting\n";
  
  open DEST, "> $dest"
    or die "Cannot open $dest: $!\n";
  print DEST @out;
  close DEST;
}
  
sub byte_samples {
  # important we make a copy
  my @lines = @_;
  
  for (@lines) {
    s/\bIM_GPIX\b/i_gpix/g;
    s/\bIM_GLIN\b/i_glin/g;
    s/\bIM_PPIX\b/i_ppix/g;
    s/\bIM_PLIN\b/i_plin/g;
    s/\bIM_GSAMP\b/i_gsamp/g;
    s/\bIM_PSAMP\b/i_psamp/g;
    s/\bIM_SAMPLE_MAX\b/255/g;
    s/\bIM_SAMPLE_MAX2\b/65025/g;
    s/\bIM_SAMPLE_T/i_sample_t/g;
    s/\bIM_COLOR\b/i_color/g;
    s/\bIM_WORK_T\b/int/g;
    s/\bIM_Sf\b/"%d"/g;
    s/\bIM_Wf\b/"%d"/g;
    s/\bIM_SUFFIX\((\w+)\)/$1_8/g;
    s/\bIM_ROUND\(/IM_ROUND_8(/g;
    s/\bIM_ADAPT_COLORS\(/i_adapt_colors(/g;
    s/\bIM_LIMIT\(/IM_LIMIT_8(/g;
    s/\bIM_RENDER_LINE\(/i_render_line(/g;
    s/\bIM_FILL_COMBINE_F\b/i_fill_combine_f/g;
  }
  
  @lines;
}

sub double_samples {
  # important we make a copy
  my @lines = @_;
  
  for (@lines) {
    s/\bIM_GPIX\b/i_gpixf/g;
    s/\bIM_GLIN\b/i_glinf/g;
    s/\bIM_PPIX\b/i_ppixf/g;
    s/\bIM_PLIN\b/i_plinf/g;
    s/\bIM_GSAMP\b/i_gsampf/g;
    s/\bIM_PSAMP\b/i_psampf/g;
    s/\bIM_SAMPLE_MAX\b/1.0/g;
    s/\bIM_SAMPLE_MAX2\b/1.0/g;
    s/\bIM_SAMPLE_T/i_fsample_t/g;
    s/\bIM_COLOR\b/i_fcolor/g;
    s/\bIM_WORK_T\b/double/g;
    s/\bIM_Sf\b/"%f"/g;
    s/\bIM_Wf\b/"%f"/g;
    s/\bIM_SUFFIX\((\w+)\)/$1_double/g;
    s/\bIM_ROUND\(/IM_ROUND_double(/g;
    s/\bIM_ADAPT_COLORS\(/i_adapt_fcolors(/g;
    s/\bIM_LIMIT\(/IM_LIMIT_double(/g;
    s/\bIM_RENDER_LINE\(/i_render_linef(/g;
    s/\bIM_FILL_COMBINE_F\b/i_fill_combinef_f/g;
  }

  @lines;
}

sub usage {
  die <<EOS;
Usage: perl -MImager::Preprocess -epreprocess [-l] infile outfile
  -l don't produce #line directives
  infile - input file
  outfile output file

See perldoc Imager::Preprocess for details.
EOS
}

1;

__END__

=head1 NAME

=for stopwords preprocessor

Imager::Preprocess - simple preprocessor for handling multiple sample sizes

=head1 SYNOPSIS

  /* in the source: */
  #code condition true to work with 8-bit samples
  ... code using preprocessor types/values ...
  #/code

  # process and make #line directives
  perl -MImager::Preprocess -epreprocess foo.im foo.c

  # process and no #line directives
  perl -MImager::Preprocess -epreprocess -l foo.im foo.c

=head1 DESCRIPTION

This is a simple preprocessor that aims to reduce duplication of
source code when implementing an algorithm both for 8-bit samples and
double samples in Imager.

Imager's C<Makefile.PL> currently scans the F<MANIFEST> for F<.im>
files and adds Makefile files to convert these to F<.c> files.

The beginning of a sample-independent section of code is preceded by:

  #code expression

where I<expression> should return true if processing should be done at
8-bits/sample.

You can also use a #code block around a function definition to produce
8-bit and double sample versions of a function.  In this case #code
has no expression and you will need to use IM_SUFFIX() to produce
different function names.

The end of a sample-independent section of code is terminated by:

  #/code

#code sections cannot be nested.

#/code without a starting #code is an error.

The following types and values are defined in a #code section:

=over

=item *

IM_GPIX(C<im>, C<x>, C<y>, C<&col>)

=item *

IM_GLIN(C<im>, C<l>, C<r>, C<y>, C<colors>)

=item *

IM_PPIX(C<im>, C<x>, C<y>, C<&col>)

=item *

IM_PLIN(C<im>, C<x>, C<y>, C<colors>)

=item *

IM_GSAMP(C<im>, C<l>, C<r>, C<y>, C<samples>, C<chans>, C<chan_count>)

These correspond to the appropriate image function, eg. IM_GPIX()
becomes i_gpix() or i_gpixf() as appropriate.

=item *

IM_ADAPT_COLORS(C<dest_channels>, C<src_channels>, C<colors>, C<count>)

Call i_adapt_colors() or i_adapt_fcolors().

=item *

IM_FILL_COMBINE(C<fill>) - retrieve the combine function from a fill
object.

=item *

IM_FILL_FILLER(C<fill>) - retrieve the fill_with_* function from a fill
object.

=item *

IM_SAMPLE_MAX - maximum value for a sample

=item *

IM_SAMPLE_MAX2 - maximum value for a sample, squared

=item *

IM_SAMPLE_T - type of a sample (i_sample_t or i_fsample_t)

=item *

IM_COLOR - color type, either i_color or i_fcolor.

=item *

IM_WORK_T - working sample type, either int or double.

=item *

IM_Sf - format string for the sample type, C<"%d"> or C<"%f">.

=item *

IM_Wf - format string for the work type, C<"%d"> or C<"%f">.

=item *

IM_SUFFIX(identifier) - adds _8 or _double onto the end of identifier.

=item *

IM_EIGHT_BIT - this is a macro defined only in 8-bit/sample code.

=back

Other types, functions and values may be added in the future.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=cut
