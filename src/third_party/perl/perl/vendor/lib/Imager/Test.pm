package Imager::Test;
use strict;
use Test::Builder;
require Exporter;
use vars qw(@ISA @EXPORT_OK $VERSION);
use Carp qw(croak);

$VERSION = "1.000";

@ISA = qw(Exporter);
@EXPORT_OK = 
  qw(
     diff_text_with_nul 
     test_image_raw
     test_image_16
     test_image
     test_image_double 
     test_image_mono
     test_image_gray
     test_image_gray_16
     test_image_named
     is_color1
     is_color3
     is_color4
     is_color_close3
     is_fcolor1
     is_fcolor3
     is_fcolor4
     color_cmp
     is_image
     is_imaged
     is_image_similar
     isnt_image
     image_bounds_checks
     mask_tests
     test_colorf_gpix
     test_color_gpix
     test_colorf_glin);

sub diff_text_with_nul {
  my ($desc, $text1, $text2, @params) = @_;

  my $builder = Test::Builder->new;

  print "# $desc\n";
  my $imbase = Imager->new(xsize => 100, ysize => 100);
  my $imcopy = Imager->new(xsize => 100, ysize => 100);

  $builder->ok($imbase->string(x => 5, 'y' => 50, size => 20,
			       string => $text1,
			       @params), "$desc - draw text1");
  $builder->ok($imcopy->string(x => 5, 'y' => 50, size => 20,
			       string => $text2,
			       @params), "$desc - draw text2");
  $builder->isnt_num(Imager::i_img_diff($imbase->{IMG}, $imcopy->{IMG}), 0,
		     "$desc - check result different");
}

sub is_color3($$$$$) {
  my ($color, $red, $green, $blue, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb) = $color->rgba;
  unless ($builder->ok($cr == $red && $cg == $green && $cb == $blue, $comment)) {
    print <<END_DIAG;
Color mismatch:
  Red: $red vs $cr
Green: $green vs $cg
 Blue: $blue vs $cb
END_DIAG
    return;
  }

  return 1;
}

sub is_color_close3($$$$$$) {
  my ($color, $red, $green, $blue, $tolerance, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb) = $color->rgba;
  unless ($builder->ok(abs($cr - $red) <= $tolerance
		       && abs($cg - $green) <= $tolerance
		       && abs($cb - $blue) <= $tolerance, $comment)) {
    $builder->diag(<<END_DIAG);
Color out of tolerance ($tolerance):
  Red: expected $red vs received $cr
Green: expected $green vs received $cg
 Blue: expected $blue vs received $cb
END_DIAG
    return;
  }

  return 1;
}

sub is_color4($$$$$$) {
  my ($color, $red, $green, $blue, $alpha, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb, $ca) = $color->rgba;
  unless ($builder->ok($cr == $red && $cg == $green && $cb == $blue 
		       && $ca == $alpha, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Red: $cr vs $red
Green: $cg vs $green
 Blue: $cb vs $blue
Alpha: $ca vs $alpha
END_DIAG
    return;
  }

  return 1;
}

sub is_fcolor4($$$$$$;$) {
  my ($color, $red, $green, $blue, $alpha, $comment_or_diff, $comment_or_undef) = @_;
  my ($comment, $mindiff);
  if (defined $comment_or_undef) {
    ( $mindiff, $comment ) = ( $comment_or_diff, $comment_or_undef )
  }
  else {
    ( $mindiff, $comment ) = ( 0.001, $comment_or_diff )
  }

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb, $ca) = $color->rgba;
  unless ($builder->ok(abs($cr - $red) <= $mindiff
		       && abs($cg - $green) <= $mindiff
		       && abs($cb - $blue) <= $mindiff
		       && abs($ca - $alpha) <= $mindiff, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Red: $cr vs $red
Green: $cg vs $green
 Blue: $cb vs $blue
Alpha: $ca vs $alpha
END_DIAG
    return;
  }

  return 1;
}

sub is_fcolor1($$$;$) {
  my ($color, $grey, $comment_or_diff, $comment_or_undef) = @_;
  my ($comment, $mindiff);
  if (defined $comment_or_undef) {
    ( $mindiff, $comment ) = ( $comment_or_diff, $comment_or_undef )
  }
  else {
    ( $mindiff, $comment ) = ( 0.001, $comment_or_diff )
  }

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cgrey) = $color->rgba;
  unless ($builder->ok(abs($cgrey - $grey) <= $mindiff, $comment)) {
    print <<END_DIAG;
Color mismatch:
  Gray: $cgrey vs $grey
END_DIAG
    return;
  }

  return 1;
}

sub is_fcolor3($$$$$;$) {
  my ($color, $red, $green, $blue, $comment_or_diff, $comment_or_undef) = @_;
  my ($comment, $mindiff);
  if (defined $comment_or_undef) {
    ( $mindiff, $comment ) = ( $comment_or_diff, $comment_or_undef )
  }
  else {
    ( $mindiff, $comment ) = ( 0.001, $comment_or_diff )
  }

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cr, $cg, $cb) = $color->rgba;
  unless ($builder->ok(abs($cr - $red) <= $mindiff
		       && abs($cg - $green) <= $mindiff
		       && abs($cb - $blue) <= $mindiff, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Red: $cr vs $red
Green: $cg vs $green
 Blue: $cb vs $blue
END_DIAG
    return;
  }

  return 1;
}

sub is_color1($$$) {
  my ($color, $grey, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $color) {
    $builder->ok(0, $comment);
    $builder->diag("color is undef");
    return;
  }
  unless ($color->can('rgba')) {
    $builder->ok(0, $comment);
    $builder->diag("color is not a color object");
    return;
  }

  my ($cgrey) = $color->rgba;
  unless ($builder->ok($cgrey == $grey, $comment)) {
    $builder->diag(<<END_DIAG);
Color mismatch:
  Grey: $grey vs $cgrey
END_DIAG
    return;
  }

  return 1;
}

sub test_image_raw {
  my $green=Imager::i_color_new(0,255,0,255);
  my $blue=Imager::i_color_new(0,0,255,255);
  my $red=Imager::i_color_new(255,0,0,255);
  
  my $img=Imager::ImgRaw::new(150,150,3);
  
  Imager::i_box_filled($img,70,25,130,125,$green);
  Imager::i_box_filled($img,20,25,80,125,$blue);
  Imager::i_arc($img,75,75,30,0,361,$red);
  Imager::i_conv($img,[0.1, 0.2, 0.4, 0.2, 0.1]);

  $img;
}

sub test_image {
  my $green = Imager::Color->new(0, 255, 0, 255);
  my $blue  = Imager::Color->new(0, 0, 255, 255);
  my $red   = Imager::Color->new(255, 0, 0, 255);
  my $img = Imager->new(xsize => 150, ysize => 150);
  $img->box(filled => 1, color => $green, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $blue,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $red);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  $img;
}

sub test_image_16 {
  my $green = Imager::Color->new(0, 255, 0, 255);
  my $blue  = Imager::Color->new(0, 0, 255, 255);
  my $red   = Imager::Color->new(255, 0, 0, 255);
  my $img = Imager->new(xsize => 150, ysize => 150, bits => 16);
  $img->box(filled => 1, color => $green, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $blue,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $red);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  $img;
}

sub test_image_double {
  my $green = Imager::Color->new(0, 255, 0, 255);
  my $blue  = Imager::Color->new(0, 0, 255, 255);
  my $red   = Imager::Color->new(255, 0, 0, 255);
  my $img = Imager->new(xsize => 150, ysize => 150, bits => 'double');
  $img->box(filled => 1, color => $green, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $blue,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $red);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  $img;
}

sub test_image_gray {
  my $g50 = Imager::Color->new(128, 128, 128);
  my $g30  = Imager::Color->new(76, 76, 76);
  my $g70   = Imager::Color->new(178, 178, 178);
  my $img = Imager->new(xsize => 150, ysize => 150, channels => 1);
  $img->box(filled => 1, color => $g50, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $g30,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $g70);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  return $img;
}

sub test_image_gray_16 {
  my $g50 = Imager::Color->new(128, 128, 128);
  my $g30  = Imager::Color->new(76, 76, 76);
  my $g70   = Imager::Color->new(178, 178, 178);
  my $img = Imager->new(xsize => 150, ysize => 150, channels => 1, bits => 16);
  $img->box(filled => 1, color => $g50, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $g30,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $g70);
  $img->filter(type => 'conv', coef => [ 0.1, 0.2, 0.4, 0.2, 0.1 ]);

  return $img;
}

sub test_image_mono {
  require Imager::Fill;
  my $fh = Imager::Fill->new(hatch => 'check1x1');
  my $img = Imager->new(xsize => 150, ysize => 150, type => "paletted");
  my $black = Imager::Color->new(0, 0, 0);
  my $white = Imager::Color->new(255, 255, 255);
  $img->addcolors(colors => [ $black, $white ]);
  $img->box(fill => $fh, box => [ 70, 24, 130, 124 ]);
  $img->box(filled => 1, color => $white,  box => [ 20, 26, 80, 126 ]);
  $img->arc(x => 75, y => 75, r => 30, color => $black, aa => 0);

  return $img;
}

my %name_to_sub =
  (
   basic => \&test_image,
   basic16 => \&test_image_16,
   basic_double => \&test_image_double,
   gray => \&test_image_gray,
   gray16 => \&test_image_gray_16,
   mono => \&test_image_mono,
  );

sub test_image_named {
  my $name = shift
    or croak("No name supplied to test_image_named()");
  my $sub = $name_to_sub{$name}
    or croak("Unknown name $name supplied to test_image_named()");

  return $sub->();
}

sub _low_image_diff_check {
  my ($left, $right, $comment) = @_;

  my $builder = Test::Builder->new;

  unless (defined $left) {
    $builder->ok(0, $comment);
    $builder->diag("left is undef");
    return;
  } 
  unless (defined $right) {
    $builder->ok(0, $comment);
    $builder->diag("right is undef");
    return;
  }
  unless ($left->{IMG}) {
    $builder->ok(0, $comment);
    $builder->diag("left image has no low level object");
    return;
  }
  unless ($right->{IMG}) {
    $builder->ok(0, $comment);
    $builder->diag("right image has no low level object");
    return;
  }
  unless ($left->getwidth == $right->getwidth) {
    $builder->ok(0, $comment);
    $builder->diag("left width " . $left->getwidth . " vs right width " 
                   . $right->getwidth);
    return;
  }
  unless ($left->getheight == $right->getheight) {
    $builder->ok(0, $comment);
    $builder->diag("left height " . $left->getheight . " vs right height " 
                   . $right->getheight);
    return;
  }
  unless ($left->getchannels == $right->getchannels) {
    $builder->ok(0, $comment);
    $builder->diag("left channels " . $left->getchannels . " vs right channels " 
                   . $right->getchannels);
    return;
  }

  return 1;
}

sub is_image_similar($$$$) {
  my ($left, $right, $limit, $comment) = @_;

  {
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    _low_image_diff_check($left, $right, $comment)
      or return;
  }

  my $builder = Test::Builder->new;

  my $diff = Imager::i_img_diff($left->{IMG}, $right->{IMG});
  if ($diff > $limit) {
    $builder->ok(0, $comment);
    $builder->diag("image data difference > $limit - $diff");
   
    if ($limit == 0) {
      # find the first mismatch
      PIXELS:
      for my $y (0 .. $left->getheight()-1) {
	for my $x (0.. $left->getwidth()-1) {
	  my @lsamples = $left->getsamples(x => $x, y => $y, width => 1);
	  my @rsamples = $right->getsamples(x => $x, y => $y, width => 1);
          if ("@lsamples" ne "@rsamples") {
            $builder->diag("first mismatch at ($x, $y) - @lsamples vs @rsamples");
            last PIXELS;
          }
	}
      }
    }

    return;
  }
  
  return $builder->ok(1, $comment);
}

sub is_image($$$) {
  my ($left, $right, $comment) = @_;

  local $Test::Builder::Level = $Test::Builder::Level + 1;

  return is_image_similar($left, $right, 0, $comment);
}

sub is_imaged($$$;$) {
  my $epsilon = Imager::i_img_epsilonf();
  if (@_ > 3) {
    ($epsilon) = splice @_, 2, 1;
  }

  my ($left, $right, $comment) = @_;

  {
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    _low_image_diff_check($left, $right, $comment)
      or return;
  }

  my $builder = Test::Builder->new;

  my $same = Imager::i_img_samef($left->{IMG}, $right->{IMG}, $epsilon, $comment);
  if (!$same) {
    $builder->ok(0, $comment);
    $builder->diag("images different");

    # find the first mismatch
  PIXELS:
    for my $y (0 .. $left->getheight()-1) {
      for my $x (0.. $left->getwidth()-1) {
	my @lsamples = $left->getsamples(x => $x, y => $y, width => 1, type => "float");
	my @rsamples = $right->getsamples(x => $x, y => $y, width => 1, type => "float");
	if ("@lsamples" ne "@rsamples") {
	  $builder->diag("first mismatch at ($x, $y) - @lsamples vs @rsamples");
	  last PIXELS;
	}
      }
    }

    return;
  }
  
  return $builder->ok(1, $comment);
}

sub isnt_image {
  my ($left, $right, $comment) = @_;

  my $builder = Test::Builder->new;

  my $diff = Imager::i_img_diff($left->{IMG}, $right->{IMG});

  return $builder->ok($diff, "$comment");
}

sub image_bounds_checks {
  my $im = shift;

  my $builder = Test::Builder->new;

  $builder->ok(!$im->getpixel(x => -1, y => 0), 'bounds check get (-1, 0)');
  $builder->ok(!$im->getpixel(x => 10, y => 0), 'bounds check get (10, 0)');
  $builder->ok(!$im->getpixel(x => 0, y => -1), 'bounds check get (0, -1)');
  $builder->ok(!$im->getpixel(x => 0, y => 10), 'bounds check get (0, 10)');
  $builder->ok(!$im->getpixel(x => -1, y => 0), 'bounds check get (-1, 0) float');
  $builder->ok(!$im->getpixel(x => 10, y => 0), 'bounds check get (10, 0) float');
  $builder->ok(!$im->getpixel(x => 0, y => -1), 'bounds check get (0, -1) float');
  $builder->ok(!$im->getpixel(x => 0, y => 10), 'bounds check get (0, 10) float');
  my $black = Imager::Color->new(0, 0, 0);
  require Imager::Color::Float;
  my $blackf = Imager::Color::Float->new(0, 0, 0);
  $builder->ok(!$im->setpixel(x => -1, y => 0, color => $black), 'bounds check set (-1, 0)');
  $builder->ok(!$im->setpixel(x => 10, y => 0, color => $black), 'bounds check set (10, 0)');
  $builder->ok(!$im->setpixel(x => 0, y => -1, color => $black), 'bounds check set (0, -1)');
  $builder->ok(!$im->setpixel(x => 0, y => 10, color => $black), 'bounds check set (0, 10)');
  $builder->ok(!$im->setpixel(x => -1, y => 0, color => $blackf), 'bounds check set (-1, 0) float');
  $builder->ok(!$im->setpixel(x => 10, y => 0, color => $blackf), 'bounds check set (10, 0) float');
  $builder->ok(!$im->setpixel(x => 0, y => -1, color => $blackf), 'bounds check set (0, -1) float');
  $builder->ok(!$im->setpixel(x => 0, y => 10, color => $blackf), 'bounds check set (0, 10) float');
}

sub test_colorf_gpix {
  my ($im, $x, $y, $expected, $epsilon, $comment) = @_;

  my $builder = Test::Builder->new;
  
  defined $comment or $comment = '';

  my $c = Imager::i_gpixf($im, $x, $y);
  unless ($c) {
    $builder->ok(0, "$comment - retrieve color at ($x,$y)");
    return;
  }
  unless ($builder->ok(colorf_cmp($c, $expected, $epsilon) == 0,
	     "$comment - got right color ($x, $y)")) {
    my @c = $c->rgba;
    my @exp = $expected->rgba;
    $builder->diag(<<EOS);
# got: ($c[0], $c[1], $c[2])
# expected: ($exp[0], $exp[1], $exp[2])
EOS
  }
  1;
}

sub test_color_gpix {
  my ($im, $x, $y, $expected, $comment) = @_;

  my $builder = Test::Builder->new;
  
  defined $comment or $comment = '';
  my $c = Imager::i_get_pixel($im, $x, $y);
  unless ($c) {
    $builder->ok(0, "$comment - retrieve color at ($x,$y)");
    return;
  }
  unless ($builder->ok(color_cmp($c, $expected) == 0,
     "got right color ($x, $y)")) {
    my @c = $c->rgba;
    my @exp = $expected->rgba;
    $builder->diag(<<EOS);
# got: ($c[0], $c[1], $c[2])
# expected: ($exp[0], $exp[1], $exp[2])
EOS
    return;
  }

  return 1;
}

sub test_colorf_glin {
  my ($im, $x, $y, $pels, $comment) = @_;

  my $builder = Test::Builder->new;
  
  my @got = Imager::i_glinf($im, $x, $x+@$pels, $y);
  @got == @$pels
    or return $builder->is_num(scalar(@got), scalar(@$pels), "$comment - pixels retrieved");
  
  return $builder->ok(!grep(colorf_cmp($pels->[$_], $got[$_], 0.005), 0..$#got),
     "$comment - check colors ($x, $y)");
}

sub colorf_cmp {
  my ($c1, $c2, $epsilon) = @_;

  defined $epsilon or $epsilon = 0;

  my @s1 = $c1->rgba;
  my @s2 = $c2->rgba;

  # print "# (",join(",", @s1[0..2]),") <=> (",join(",", @s2[0..2]),")\n";
  return abs($s1[0]-$s2[0]) >= $epsilon && $s1[0] <=> $s2[0] 
    || abs($s1[1]-$s2[1]) >= $epsilon && $s1[1] <=> $s2[1]
      || abs($s1[2]-$s2[2]) >= $epsilon && $s1[2] <=> $s2[2];
}

sub color_cmp {
  my ($c1, $c2) = @_;

  my @s1 = $c1->rgba;
  my @s2 = $c2->rgba;

  return $s1[0] <=> $s2[0] 
    || $s1[1] <=> $s2[1]
      || $s1[2] <=> $s2[2];
}

# these test the action of the channel mask on the image supplied
# which should be an OO image.
sub mask_tests {
  my ($im, $epsilon) = @_;

  my $builder = Test::Builder->new;

  defined $epsilon or $epsilon = 0;

  # we want to check all four of ppix() and plin(), ppix() and plinf()
  # basic test procedure:
  #   first using default/all 1s mask, set to white
  #   make sure we got white
  #   set mask to skip a channel, set to grey
  #   make sure only the right channels set

  print "# channel mask tests\n";
  # 8-bit color tests
  my $white = Imager::NC(255, 255, 255);
  my $grey = Imager::NC(128, 128, 128);
  my $white_grey = Imager::NC(128, 255, 128);

  print "# with ppix\n";
  $builder->ok($im->setmask(mask=>~0), "set to default mask");
  $builder->ok($im->setpixel(x=>0, 'y'=>0, color=>$white), "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 0, $white, "ppix");
  $builder->ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  $builder->ok($im->setpixel(x=>0, 'y'=>0, color=>$grey), "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 0, $white_grey, "ppix masked");

  print "# with plin\n";
  $builder->ok($im->setmask(mask=>~0), "set to default mask");
  $builder->ok($im->setscanline(x=>0, 'y'=>1, pixels => [$white]), 
     "set to white all channels");
  test_color_gpix($im->{IMG}, 0, 1, $white, "plin");
  $builder->ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  $builder->ok($im->setscanline(x=>0, 'y'=>1, pixels=>[$grey]), 
     "set to grey, no channel 2");
  test_color_gpix($im->{IMG}, 0, 1, $white_grey, "plin masked");

  # float color tests
  my $whitef = Imager::NCF(1.0, 1.0, 1.0);
  my $greyf = Imager::NCF(0.5, 0.5, 0.5);
  my $white_greyf = Imager::NCF(0.5, 1.0, 0.5);

  print "# with ppixf\n";
  $builder->ok($im->setmask(mask=>~0), "set to default mask");
  $builder->ok($im->setpixel(x=>0, 'y'=>2, color=>$whitef), "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 2, $whitef, $epsilon, "ppixf");
  $builder->ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  $builder->ok($im->setpixel(x=>0, 'y'=>2, color=>$greyf), "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 2, $white_greyf, $epsilon, "ppixf masked");

  print "# with plinf\n";
  $builder->ok($im->setmask(mask=>~0), "set to default mask");
  $builder->ok($im->setscanline(x=>0, 'y'=>3, pixels => [$whitef]), 
     "set to white all channels");
  test_colorf_gpix($im->{IMG}, 0, 3, $whitef, $epsilon, "plinf");
  $builder->ok($im->setmask(mask=>0xF-0x2), "set channel to exclude channel1");
  $builder->ok($im->setscanline(x=>0, 'y'=>3, pixels=>[$greyf]), 
     "set to grey, no channel 2");
  test_colorf_gpix($im->{IMG}, 0, 3, $white_greyf, $epsilon, "plinf masked");

}

1;

__END__

=head1 NAME

Imager::Test - common functions used in testing Imager

=head1 SYNOPSIS

  use Imager::Test 'diff_text_with_nul';
  diff_text_with_nul($test_name, $text1, $text2, @string_options);

=head1 DESCRIPTION

This is a repository of functions used in testing Imager.

Some functions will only be useful in testing Imager itself, while
others should be useful in testing modules that use Imager.

No functions are exported by default.

=head1 FUNCTIONS

=head2 Test functions

=for stopwords OO

=over

=item is_color1($color, $grey, $comment)

Tests if the first channel of $color matches $grey.

=item is_color3($color, $red, $green, $blue, $comment)

Tests if $color matches the given ($red, $green, $blue)

=item is_color4($color, $red, $green, $blue, $alpha, $comment)

Tests if $color matches the given ($red, $green, $blue, $alpha)

=item is_fcolor1($fcolor, $grey, $comment)

=item is_fcolor1($fcolor, $grey, $epsilon, $comment)

Tests if $fcolor's first channel is within $epsilon of ($grey).  For
the first form $epsilon is taken as 0.001.

=item is_fcolor3($fcolor, $red, $green, $blue, $comment)

=item is_fcolor3($fcolor, $red, $green, $blue, $epsilon, $comment)

Tests if $fcolor's channels are within $epsilon of ($red, $green,
$blue).  For the first form $epsilon is taken as 0.001.

=item is_fcolor4($fcolor, $red, $green, $blue, $alpha, $comment)

=item is_fcolor4($fcolor, $red, $green, $blue, $alpha, $epsilon, $comment)

Tests if $fcolor's channels are within $epsilon of ($red, $green,
$blue, $alpha).  For the first form $epsilon is taken as 0.001.

=item is_image($im1, $im2, $comment)

Tests if the 2 images have the same content.  Both images must be
defined, have the same width, height, channels and the same color in
each pixel.  The color comparison is done at 8-bits per pixel.  The
color representation such as direct vs paletted, bits per sample are
not checked.  Equivalent to is_image_similar($im1, $im2, 0, $comment).

=item is_imaged($im, $im2, $comment)

=item is_imaged($im, $im2, $epsilon, $comment)

Tests if the two images have the same content at the double/sample
level.  C<$epsilon> defaults to the platform DBL_EPSILON multiplied by
four.

=item is_image_similar($im1, $im2, $maxdiff, $comment)

Tests if the 2 images have similar content.  Both images must be
defined, have the same width, height and channels.  The cum of the
squares of the differences of each sample are calculated and must be
less than or equal to I<$maxdiff> for the test to pass.  The color
comparison is done at 8-bits per pixel.  The color representation such
as direct vs paletted, bits per sample are not checked.

=item isnt_image($im1, $im2, $comment)

Tests that the two images are different.  For regressions tests where
something (like text output of "0") produced no change, but should
have produced a change.

=item test_colorf_gpix($im, $x, $y, $expected, $epsilon, $comment)

Retrieves the pixel ($x,$y) from the low-level image $im and compares
it to the floating point color $expected, with a tolerance of epsilon.

=item test_color_gpix($im, $x, $y, $expected, $comment)

Retrieves the pixel ($x,$y) from the low-level image $im and compares
it to the floating point color $expected.

=item test_colorf_glin($im, $x, $y, $pels, $comment)

Retrieves the floating point pixels ($x, $y)-[$x+@$pels, $y] from the
low level image $im and compares them against @$pels.

=item is_color_close3($color, $red, $green, $blue, $tolerance, $comment)

Tests if $color's first three channels are within $tolerance of ($red,
$green, $blue).

=back

=head2 Test suite functions

Functions that perform one or more tests, typically used to test
various parts of Imager's implementation.

=over

=item image_bounds_checks($im)

Attempts to write to various pixel positions outside the edge of the
image to ensure that it fails in those locations.

Any new image type should pass these tests.  Does 16 separate tests.

=item mask_tests($im, $epsilon)

Perform a standard set of mask tests on the OO image $im.  Does 24
separate tests.

=item diff_text_with_nul($test_name, $text1, $text2, @options)

Creates 2 test images and writes $text1 to the first image and $text2
to the second image with the string() method.  Each call adds 3
C<ok>/C<not ok> to the output of the test script.

Extra options that should be supplied include the font and either a
color or channel parameter.

This was explicitly created for regression tests on #21770.

=back

=head2 Helper functions

=over

=item test_image_raw()

Returns a 150x150x3 Imager::ImgRaw test image.

=item test_image()

Returns a 150x150x3 8-bit/sample OO test image. Name: C<basic>.

=item test_image_16()

Returns a 150x150x3 16-bit/sample OO test image. Name: C<basic16>

=item test_image_double()

Returns a 150x150x3 double/sample OO test image. Name: C<basic_double>.

=item test_image_gray()

Returns a 150x150 single channel OO test image. Name: C<gray>.

=item test_image_gray_16()

Returns a 150x150 16-bit/sample single channel OO test image. Name:
C<gray16>.

=item test_image_mono()

Returns a 150x150 bilevel image that passes the is_bilevel() test.
Name: C<mono>.

=item test_image_named($name)

Return one of the other test images above based on name.

=item color_cmp($c1, $c2)

Performs an ordering of 3-channel colors (like <=>).

=item colorf_cmp($c1, $c2)

Performs an ordering of 3-channel floating point colors (like <=>).

=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
