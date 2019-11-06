package Imager::Font::Wrap;
use strict;
use Imager;
use Imager::Font;
use vars qw($VERSION);

$VERSION = "1.003";

*_first = \&Imager::Font::_first;

# we can't accept the utf8 parameter, too hard at this level

# the %state contains:
#  font - the font
#  im - the image
#  x - the left position
#  w - the width
#  justify - fill, left, right or center

sub _format_line {
  my ($state, $spaces, $text, $fill) = @_;

  $text =~ s/ +$//;
  my $box = $state->{font}->bounding_box(string=>$text,
                                         size=>$state->{size});

  my $y = $state->{linepos} + $box->global_ascent;
  
  if ($state->{bottom} 
      && $state->{linepos} + $box->font_height > $state->{bottom}) {
    $state->{full} = 1;
    return 0;
  }

  if ($text =~ /\S/ && $state->{im}) {
    my $justify = $fill ? $state->{justify} : 
      $state->{justify} eq 'fill' ? 'left' : $state->{justify};
    if ($justify ne 'fill') {
      my $x = $state->{x};
      if ($justify eq 'right') {
        $x += $state->{w} - $box->advance_width;
      }
      elsif ($justify eq 'center') {
        $x += ($state->{w} - $box->advance_width) / 2;
      }
      $state->{font}->draw(image=>$state->{im}, string=>$text, 
                           x=>$x, 'y'=>$y,
                           size=>$state->{size}, %{$state->{input}});
    }
    else {
      (my $nospaces = $text) =~ tr/ //d;
      my $nospace_bbox = $state->{font}->bounding_box(string=>$nospaces,
                                                      size=>$state->{size});
      my $gap = $state->{w} - $nospace_bbox->advance_width;
      my $x = $state->{x};
      $spaces = $text =~ tr/ / /;
      while (length $text) {
        if ($text =~ s/^(\S+)//) {
          my $word = $1;
          my $bbox = $state->{font}->bounding_box(string=>$word,
                                                  size=>$state->{size});
          $state->{font}->draw(image=>$state->{im}, string=>$1,
                               x=>$x, 'y'=>$y,
                               size=>$state->{size}, %{$state->{input}});
          $x += $bbox->advance_width;
        }
        elsif ($text =~ s/^( +)//) {
          my $sep = $1;
          my $advance = int($gap * length($sep) / $spaces);
          $spaces -= length $sep;
          $gap -= $advance;
          $x += $advance;
        }
        else {
          die "This shouldn't happen\n";
        }
      }
    }
  }
  $state->{linepos} += $box->font_height + $state->{linegap};

  1;
}

sub wrap_text {
  my $class = shift;
  my %input = @_;

  # try to get something useful
  my $x = _first(delete $input{'x'}, 0);
  my $y = _first(delete $input{'y'}, 0);
  exists $input{image}
    or return Imager->_set_error('No image parameter supplied');
  my $im = delete $input{image};
  my $imerr = $im || 'Imager';
  my $width = delete $input{width};
  if (!defined $width) {
    defined $im && $im->getwidth > $x
      or return $imerr->_set_error("No width supplied and can't guess");
    $width = $im->getwidth - $x;
  }
  my $font = delete $input{font}
    or return $imerr->_set_error("No font parameter supplied");
  my $size = _first(delete $input{size}, $font->{size});
  defined $size
    or return $imerr->_set_error("No font size supplied");

  2 * $size < $width
    or return $imerr->_set_error("Width too small for font size");
  
  my $text = delete $input{string};
  defined $text
    or return $imerr->_set_error("No string parameter supplied");

  my $justify = _first($input{justify}, "left");

  my %state =
    (
     font => $font,
     im => $im,
     x => $x,
     w => $width,
     justify => $justify,
     'y' => $y,
     linepos=>$y,
     size=>$size,
     input => \%input,
     linegap => delete $input{linegap} || 0,
    );
  $state{height} = delete $input{height};
  if ($state{height}) {
    $state{bottom} = $y + $state{height};
  }
  my $line = '';
  my $spaces = 0;
  my $charpos = 0;
  my $linepos = 0;
  pos($text) = 0; # avoid a warning
  while (pos($text) < length($text)) {
    #print pos($text), "\n";
    if ($text =~ /\G( +)/gc) {
      #print "spaces\n";
      $line .= $1;
      $spaces += length($1);
    }
    elsif ($text =~ /\G(?:\x0D\x0A?|\x0A\x0D?)/gc) {
      #print "newline\n";
      _format_line(\%state, $spaces, $line, 0)
        or last;
      $line = '';
      $spaces = 0;
      $linepos = pos($text);
    }
    elsif ($text =~ /\G(\S+)/gc) {
      #print "word\n";
      my $word = $1;
      my $bbox = $font->bounding_box(string=>$line . $word, size=>$size);
      if ($bbox->advance_width > $width) {
        _format_line(\%state, $spaces, $line, 1)
          or last;
        $line = '';
        $spaces = 0;
        $linepos = pos($text) - length($word);
      }
      $line .= $word;
      # check for long words
      $bbox = $font->bounding_box(string=>$line, size=>$size);
      while ($bbox->advance_width > $width) {
        my $len = length($line) - 1;
        $bbox = $font->bounding_box(string=>substr($line, 0, $len),
                                    size=>$size);
        while ($bbox->advance_width > $width) {
          --$len;
          $bbox = $font->bounding_box(string=>substr($line, 0, $len),
                                      size=>$size);
        }
        _format_line(\%state, 0, substr($line, 0, $len), 0)
          or last;
        $line = substr($line, $len);
        $bbox = $font->bounding_box(string=>$line, size=>$size);
        $linepos = pos($text) - length($line);
      }
    }
    elsif ($text =~ /\G\s/gc) {
      # skip a single unrecognized whitespace char
      #print "skip\n";
      $linepos = pos($text);
    }
  }

  if (length $line && !$state{full}) {
    $linepos += length $line
      if _format_line(\%state, 0, $line, 0);
  }

  if ($input{savepos}) {
    ${$input{savepos}} = $linepos;
  }

  return ($x, $y, $x+$width, $state{linepos});
}

1;

__END__

=head1 NAME

  Imager::Font::Wrap - simple wrapped text output

=head1 SYNOPSIS

  use Imager::Font::Wrap;

  my $img = Imager->new(xsize=>$xsize, ysize=>$ysize);

  my $font = Imager::Font->new(file=>$fontfile);

  my $string = "..."; # text with or without newlines

  Imager::Font::Wrap->wrap_text( image  => $img,
                                 font   => $font,
                                 string => $string,
                                 x      => $left,
                                 y      => $top,
                                 width  => $width,
                                 .... );

=head1 DESCRIPTION

This is a simple text wrapper with options to control the layout of
text within the line.

You can control the position, width and height of the text with the
C<image>, C<x>, C<y>, C<width> and C<height> options.

You can simply calculate space usage by setting C<image> to C<undef>,
or set C<savepos> to see how much text can fit within the given
C<height>.

=over

=item wrap_text()

Draw word-wrapped text.

=over

=item *

C<x>, C<y> - The top-left corner of the rectangle the text is
formatted into.  Defaults to (0, 0).

=item *

C<width> - The width of the formatted text in pixels.  Defaults to the
horizontal gap between the top-left corner and the right edge of the
image.  If no image is supplied then this is required.

=item *

C<height> - The maximum height of the formatted text in pixels.  Not
required.

=item *

C<savepos> - The amount of text consumed (as a count of characters)
will be stored into the scalar this refers to.

  my $pagenum = 1;
  my $string = "...";
  my $font = ...;
  my $savepos;

  while (length $string) { 
    my $img = Imager->new(xsize=>$xsize, ysize=>$ysize);
    Imager::Font::Wrap->wrap_text(string=>$string, font=>$font, 
                                  image=>$img, savepos => \$savepos)
      or die $img->errstr;
    $savepos > 0
      or die "Could not fit any text on page\n";
    $string = substr($string, $savepos);
    $img->write(file=>"page$pagenum.ppm");
  }

=item *

C<image> - The image to render the text to.  Can be supplied as
C<undef> to simply calculate the bounding box.

=item *

C<font> - The font used to render the text.  Required.

=item *

C<size> - The size to render the font in.  Defaults to the size stored
in the font object.  Required if it isn't stored in the font object.

=item *

C<string> - The text to render.  This can contain non-white-space,
blanks (ASCII 0x20), and newlines.

Newlines must match /(?:\x0A\x0D?|\x0D\x0A?)/.  White-space other than
blanks and newlines are completely ignored.

=item *

C<justify>

The way text is formatted within each line.  Possible values include:

=over

=item *

C<left> - left aligned against the left edge of the text box.

=item *

C<right> - right aligned against the right edge of the text box.

=item *

C<center> - centered horizontally in the text box.

=item *

fill - all but the final line of the paragraph has spaces expanded so
that the line fills from the left to the right edge of the text box.

=back

=item *

C<linegap> - Gap between lines of text in pixels.  This is in addition
to the size from C<< $font->font_height >>.  Can be positive or
negative.  Default 0.

=back

Any other parameters are passed onto Imager::Font->draw().

Returns a list:

  ($left, $top, $right, $bottom)

which are the bounds of the space used to layout the text.

If C<height> is set then this is the space used within that height.

You can use this to calculate the space required to format the text
before doing it:

  my ($left, $top, $right, $bottom) =
    Imager::Font::Wrap->wrap_text(string => $string,
                                  font   => $font,
                                  width  => $xsize);
  my $img = Imager->new(xsize=>$xsize, ysize=>$bottom);
  Imager::Font::Wrap->wrap_text(string => $string,
                                font   => $font,
                                width  => $xsize,
                                image  => $image);

=back

=head1 BUGS

Imager::Font can handle UTF-8 encoded text itself, but this module
doesn't support that (and probably won't).  This could probably be
done with regex magic.

Currently ignores the C<sizew> parameter, if you supply one it will be
supplied to the draw() function and the text will be too short or too
long for the C<width>.

Uses a simplistic text model, which is why there's no hyphenation, and
no tabs.

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3), Imager::Font(3)

=cut
