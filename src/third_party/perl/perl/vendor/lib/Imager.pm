package Imager;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS %formats $DEBUG %filters %DSOs $ERRSTR %OPCODES $I2P $FORMATGUESS $warn_obsolete);
use IO::File;

use Imager::Color;
use Imager::Font;

@EXPORT_OK = qw(
		init
		init_log
		DSO_open
		DSO_close
		DSO_funclist
		DSO_call

		load_plugin
		unload_plugin

		i_list_formats

		i_color_new
		i_color_set
		i_color_info

		i_img_empty
		i_img_empty_ch
		i_img_exorcise
		i_img_destroy

		i_img_info

		i_img_setmask
		i_img_getmask

		i_line
		i_line_aa
		i_box
		i_box_filled
		i_arc
		i_circle_aa

		i_bezier_multi
		i_poly_aa
		i_poly_aa_cfill

		i_copyto
		i_rubthru
		i_scaleaxis
		i_scale_nn
		i_haar
		i_count_colors

		i_gaussian
		i_conv

		i_convert
		i_map

		i_img_diff

		i_tt_set_aa
		i_tt_cp
		i_tt_text
		i_tt_bbox

		i_readpnm_wiol
		i_writeppm_wiol

		i_readraw_wiol
		i_writeraw_wiol

		i_contrast
		i_hardinvert
		i_noise
		i_bumpmap
		i_postlevels
		i_mosaic
		i_watermark

		malloc_state

		list_formats

		i_gifquant

		newfont
		newcolor
		newcolour
		NC
		NF
                NCF
);

@EXPORT=qw(
	  );

%EXPORT_TAGS=
  (handy => [qw(
		newfont
		newcolor
		NF
		NC
                NCF
	       )],
   all => [@EXPORT_OK],
   default => [qw(
		  load_plugin
		  unload_plugin
		 )]);

# registered file readers
my %readers;

# registered file writers
my %writers;

# modules we attempted to autoload
my %attempted_to_load;

# errors from loading files
my %file_load_errors;

# what happened when we tried to load
my %reader_load_errors;
my %writer_load_errors;

# library keys that are image file formats
my %file_formats = map { $_ => 1 } qw/tiff pnm gif png jpeg raw bmp tga/;

# image pixel combine types
my @combine_types = 
  qw/none normal multiply dissolve add subtract diff lighten darken
     hue saturation value color/;
my %combine_types;
@combine_types{@combine_types} = 0 .. $#combine_types;
$combine_types{mult} = $combine_types{multiply};
$combine_types{'sub'}  = $combine_types{subtract};
$combine_types{sat}  = $combine_types{saturation};

# this will be used to store global defaults at some point
my %defaults;

BEGIN {
  require Exporter;
  my $ex_version = eval $Exporter::VERSION;
  if ($ex_version < 5.57) {
    @ISA = qw(Exporter);
  }
  $VERSION = '0.90';
  require XSLoader;
  XSLoader::load(Imager => $VERSION);
}

my %formats_low;
my %format_classes =
  (
   png => "Imager::File::PNG",
   gif => "Imager::File::GIF",
   tiff => "Imager::File::TIFF",
   jpeg => "Imager::File::JPEG",
   w32 => "Imager::Font::W32",
   ft2 => "Imager::Font::FT2",
   t1 => "Imager::Font::T1",
  );

tie %formats, "Imager::FORMATS", \%formats_low, \%format_classes;

BEGIN {
  for(i_list_formats()) { $formats_low{$_}++; }

  %OPCODES=(Add=>[0],Sub=>[1],Mult=>[2],Div=>[3],Parm=>[4],'sin'=>[5],'cos'=>[6],'x'=>[4,0],'y'=>[4,1]);

  $DEBUG=0;

  # the members of the subhashes under %filters are:
  #  callseq - a list of the parameters to the underlying filter in the
  #            order they are passed
  #  callsub - a code ref that takes a named parameter list and calls the
  #            underlying filter
  #  defaults - a hash of default values
  #  names - defines names for value of given parameters so if the names 
  #          field is foo=> { bar=>1 }, and the user supplies "bar" as the
  #          foo parameter, the filter will receive 1 for the foo
  #          parameter
  $filters{contrast}={
		      callseq => ['image','intensity'],
		      callsub => sub { my %hsh=@_; i_contrast($hsh{image},$hsh{intensity}); } 
		     };

  $filters{noise} ={
		    callseq => ['image', 'amount', 'subtype'],
		    defaults => { amount=>3,subtype=>0 },
		    callsub => sub { my %hsh=@_; i_noise($hsh{image},$hsh{amount},$hsh{subtype}); }
		   };

  $filters{hardinvert} ={
			 callseq => ['image'],
			 defaults => { },
			 callsub => sub { my %hsh=@_; i_hardinvert($hsh{image}); }
			};

  $filters{hardinvertall} =
    {
     callseq => ['image'],
     defaults => { },
     callsub => sub { my %hsh=@_; i_hardinvertall($hsh{image}); }
    };

  $filters{autolevels} ={
			 callseq => ['image','lsat','usat','skew'],
			 defaults => { lsat=>0.1,usat=>0.1,skew=>0.0 },
			 callsub => sub { my %hsh=@_; i_autolevels($hsh{image},$hsh{lsat},$hsh{usat},$hsh{skew}); }
			};

  $filters{turbnoise} ={
			callseq => ['image'],
			defaults => { xo=>0.0,yo=>0.0,scale=>10.0 },
			callsub => sub { my %hsh=@_; i_turbnoise($hsh{image},$hsh{xo},$hsh{yo},$hsh{scale}); }
		       };

  $filters{radnoise} ={
		       callseq => ['image'],
		       defaults => { xo=>100,yo=>100,ascale=>17.0,rscale=>0.02 },
		       callsub => sub { my %hsh=@_; i_radnoise($hsh{image},$hsh{xo},$hsh{yo},$hsh{rscale},$hsh{ascale}); }
		      };

  $filters{conv} =
    {
     callseq => ['image', 'coef'],
     defaults => { },
     callsub => 
     sub { 
       my %hsh=@_;
       i_conv($hsh{image},$hsh{coef})
	 or die Imager->_error_as_msg() . "\n";
     }
    };

  $filters{gradgen} =
    {
     callseq => ['image', 'xo', 'yo', 'colors', 'dist'],
     defaults => { dist => 0 },
     callsub => 
     sub { 
       my %hsh=@_;
       my @colors = @{$hsh{colors}};
       $_ = _color($_)
         for @colors;
       i_gradgen($hsh{image}, $hsh{xo}, $hsh{yo}, \@colors, $hsh{dist});
     }
    };

  $filters{nearest_color} =
    {
     callseq => ['image', 'xo', 'yo', 'colors', 'dist'],
     defaults => { },
     callsub => 
     sub { 
       my %hsh=@_; 
       # make sure the segments are specified with colors
       my @colors;
       for my $color (@{$hsh{colors}}) {
         my $new_color = _color($color) 
           or die $Imager::ERRSTR."\n";
         push @colors, $new_color;
       }

       i_nearest_color($hsh{image}, $hsh{xo}, $hsh{yo}, \@colors, 
                       $hsh{dist})
         or die Imager->_error_as_msg() . "\n";
     },
    };
  $filters{gaussian} = {
                        callseq => [ 'image', 'stddev' ],
                        defaults => { },
                        callsub => sub { my %hsh = @_; i_gaussian($hsh{image}, $hsh{stddev}); },
                       };
  $filters{mosaic} =
    {
     callseq => [ qw(image size) ],
     defaults => { size => 20 },
     callsub => sub { my %hsh = @_; i_mosaic($hsh{image}, $hsh{size}) },
    };
  $filters{bumpmap} =
    {
     callseq => [ qw(image bump elevation lightx lighty st) ],
     defaults => { elevation=>0, st=> 2 },
     callsub => sub {
       my %hsh = @_;
       i_bumpmap($hsh{image}, $hsh{bump}{IMG}, $hsh{elevation},
                 $hsh{lightx}, $hsh{lighty}, $hsh{st});
     },
    };
  $filters{bumpmap_complex} =
    {
     callseq => [ qw(image bump channel tx ty Lx Ly Lz cd cs n Ia Il Is) ],
     defaults => {
		  channel => 0,
		  tx => 0,
		  ty => 0,
		  Lx => 0.2,
		  Ly => 0.4,
		  Lz => -1.0,
		  cd => 1.0,
		  cs => 40,
		  n => 1.3,
		  Ia => [0,0,0],
		  Il => [255,255,255],
		  Is => [255,255,255],
		 },
     callsub => sub {
       my %hsh = @_;
       for my $cname (qw/Ia Il Is/) {
	 my $old = $hsh{$cname};
	 my $new_color = _color($old)
	   or die $Imager::ERRSTR, "\n";
	 $hsh{$cname} = $new_color;
       }
       i_bumpmap_complex($hsh{image}, $hsh{bump}{IMG}, $hsh{channel},
                 $hsh{tx}, $hsh{ty}, $hsh{Lx}, $hsh{Ly}, $hsh{Lz},
		 $hsh{cd}, $hsh{cs}, $hsh{n}, $hsh{Ia}, $hsh{Il},
		 $hsh{Is});
     },
    };
  $filters{postlevels} =
    {
     callseq  => [ qw(image levels) ],
     defaults => { levels => 10 },
     callsub  => sub { my %hsh = @_; i_postlevels($hsh{image}, $hsh{levels}); },
    };
  $filters{watermark} =
    {
     callseq  => [ qw(image wmark tx ty pixdiff) ],
     defaults => { pixdiff=>10, tx=>0, ty=>0 },
     callsub  => 
     sub { 
       my %hsh = @_; 
       i_watermark($hsh{image}, $hsh{wmark}{IMG}, $hsh{tx}, $hsh{ty}, 
                   $hsh{pixdiff}); 
     },
    };
  $filters{fountain} =
    {
     callseq  => [ qw(image xa ya xb yb ftype repeat combine super_sample ssample_param segments) ],
     names    => {
                  ftype => { linear         => 0,
                             bilinear       => 1,
                             radial         => 2,
                             radial_square  => 3,
                             revolution     => 4,
                             conical        => 5 },
                  repeat => { none      => 0,
                              sawtooth  => 1,
                              triangle  => 2,
                              saw_both  => 3,
                              tri_both  => 4,
                            },
                  super_sample => {
                                   none    => 0,
                                   grid    => 1,
                                   random  => 2,
                                   circle  => 3,
                                  },
                  combine => {
                              none      => 0,
                              normal    => 1,
                              multiply  => 2, mult => 2,
                              dissolve  => 3,
                              add       => 4,
                              subtract  => 5, 'sub' => 5,
                              diff      => 6,
                              lighten   => 7,
                              darken    => 8,
                              hue       => 9,
                              sat       => 10,
                              value     => 11,
                              color     => 12,
                             },
                 },
     defaults => { ftype => 0, repeat => 0, combine => 0,
                   super_sample => 0, ssample_param => 4,
                   segments=>[ 
                              [ 0, 0.5, 1,
                                [0,0,0],
                                [255, 255, 255],
                                0, 0,
                              ],
                             ],
                 },
     callsub  => 
     sub {
       my %hsh = @_;

       # make sure the segments are specified with colors
       my @segments;
       for my $segment (@{$hsh{segments}}) {
         my @new_segment = @$segment;
         
         $_ = _color($_) or die $Imager::ERRSTR."\n" for @new_segment[3,4];
         push @segments, \@new_segment;
       }

       i_fountain($hsh{image}, $hsh{xa}, $hsh{ya}, $hsh{xb}, $hsh{yb},
                  $hsh{ftype}, $hsh{repeat}, $hsh{combine}, $hsh{super_sample},
                  $hsh{ssample_param}, \@segments)
         or die Imager->_error_as_msg() . "\n";
     },
    };
  $filters{unsharpmask} =
    {
     callseq => [ qw(image stddev scale) ],
     defaults => { stddev=>2.0, scale=>1.0 },
     callsub => 
     sub { 
       my %hsh = @_;
       i_unsharp_mask($hsh{image}, $hsh{stddev}, $hsh{scale});
     },
    };

  $FORMATGUESS=\&def_guess_type;

  $warn_obsolete = 1;
}

#
# Non methods
#

# initlize Imager
# NOTE: this might be moved to an import override later on

sub import {
  my $i = 1;
  while ($i < @_) {
    if ($_[$i] eq '-log-stderr') {
      init_log(undef, 4);
      splice(@_, $i, 1);
    }
    else {
      ++$i;
    }
  }
  goto &Exporter::import;
}

sub init_log {
  Imager->open_log(log => $_[0], level => $_[1]);
}


sub init {
  my %parms=(loglevel=>1,@_);

  if (exists $parms{'warn_obsolete'}) {
    $warn_obsolete = $parms{'warn_obsolete'};
  }

  if ($parms{'log'}) {
    Imager->open_log(log => $parms{log}, level => $parms{loglevel})
      or return;
  }

  if (exists $parms{'t1log'}) {
    if ($formats{t1}) {
      if (Imager::Font::T1::i_init_t1($parms{'t1log'})) {
	Imager->_set_error(Imager->_error_as_msg);
	return;
      }
    }
  }

  return 1;
}

{
  my $is_logging = 0;

  sub open_log {
    my $class = shift;
    my (%opts) = ( loglevel => 1, @_ );

    $is_logging = i_init_log($opts{log}, $opts{loglevel});
    unless ($is_logging) {
      Imager->_set_error(Imager->_error_as_msg());
      return;
    }

    Imager->log("Imager $VERSION starting\n", 1);

    return $is_logging;
  }

  sub close_log {
    i_init_log(undef, -1);
    $is_logging = 0;
  }

  sub log {
    my ($class, $message, $level) = @_;

    defined $level or $level = 1;

    i_log_entry($message, $level);
  }

  sub is_logging {
    return $is_logging;
  }
}

END {
  if ($DEBUG) {
    print "shutdown code\n";
    #	for(keys %instances) { $instances{$_}->DESTROY(); }
    malloc_state(); # how do decide if this should be used? -- store something from the import
    print "Imager exiting\n";
  }
}

# Load a filter plugin 

sub load_plugin {
  my ($filename)=@_;
  my $i;
  my ($DSO_handle,$str)=DSO_open($filename);
  if (!defined($DSO_handle)) { $Imager::ERRSTR="Couldn't load plugin '$filename'\n"; return undef; }
  my %funcs=DSO_funclist($DSO_handle);
  if ($DEBUG) { print "loading module $filename\n"; $i=0; for(keys %funcs) { printf("  %2d: %s\n",$i++,$_); } }
  $i=0;
  for(keys %funcs) { if ($filters{$_}) { $ERRSTR="filter '$_' already exists\n"; DSO_close($DSO_handle); return undef; } }

  $DSOs{$filename}=[$DSO_handle,\%funcs];

  for(keys %funcs) { 
    my $evstr="\$filters{'".$_."'}={".$funcs{$_}.'};';
    $DEBUG && print "eval string:\n",$evstr,"\n";
    eval $evstr;
    print $@ if $@;
  }
  return 1;
}

# Unload a plugin

sub unload_plugin {
  my ($filename)=@_;

  if (!$DSOs{$filename}) { $ERRSTR="plugin '$filename' not loaded."; return undef; }
  my ($DSO_handle,$funcref)=@{$DSOs{$filename}};
  for(keys %{$funcref}) {
    delete $filters{$_};
    $DEBUG && print "unloading: $_\n";
  }
  my $rc=DSO_close($DSO_handle);
  if (!defined($rc)) { $ERRSTR="unable to unload plugin '$filename'."; return undef; }
  return 1;
}

# take the results of i_error() and make a message out of it
sub _error_as_msg {
  return join(": ", map $_->[0], i_errors());
}

# this function tries to DWIM for color parameters
#  color objects are used as is
#  simple scalars are simply treated as single parameters to Imager::Color->new
#  hashrefs are treated as named argument lists to Imager::Color->new
#  arrayrefs are treated as list arguments to Imager::Color->new iff any
#    parameter is > 1
#  other arrayrefs are treated as list arguments to Imager::Color::Float

sub _color {
  my $arg = shift;
  # perl 5.6.0 seems to do weird things to $arg if we don't make an 
  # explicitly stringified copy
  # I vaguely remember a bug on this on p5p, but couldn't find it
  # through bugs.perl.org (I had trouble getting it to find any bugs)
  my $copy = $arg . "";
  my $result;

  if (ref $arg) {
    if (UNIVERSAL::isa($arg, "Imager::Color")
        || UNIVERSAL::isa($arg, "Imager::Color::Float")) {
      $result = $arg;
    }
    else {
      if ($copy =~ /^HASH\(/) {
        $result = Imager::Color->new(%$arg);
      }
      elsif ($copy =~ /^ARRAY\(/) {
	$result = Imager::Color->new(@$arg);
      }
      else {
        $Imager::ERRSTR = "Not a color";
      }
    }
  }
  else {
    # assume Imager::Color::new knows how to handle it
    $result = Imager::Color->new($arg);
  }

  return $result;
}

sub _combine {
  my ($self, $combine, $default) = @_;

  if (!defined $combine && ref $self) {
    $combine = $self->{combine};
  }
  defined $combine or $combine = $defaults{combine};
  defined $combine or $combine = $default;

  if (exists $combine_types{$combine}) {
    $combine = $combine_types{$combine};
  }
  
  return $combine;
}

sub _valid_image {
  my ($self, $method) = @_;

  $self->{IMG} and return 1;

  my $msg = 'empty input image';
  $msg = "$method: $msg" if $method;
  $self->_set_error($msg);

  return;
}

# returns first defined parameter
sub _first {
  for (@_) {
    return $_ if defined $_;
  }
  return undef;
}

#
# Methods to be called on objects.
#

# Create a new Imager object takes very few parameters.
# usually you call this method and then call open from
# the resulting object

sub new {
  my $class = shift;
  my $self ={};
  my %hsh=@_;
  bless $self,$class;
  $self->{IMG}=undef;    # Just to indicate what exists
  $self->{ERRSTR}=undef; #
  $self->{DEBUG}=$DEBUG;
  $self->{DEBUG} and print "Initialized Imager\n";
  if (defined $hsh{xsize} || defined $hsh{ysize}) { 
    unless ($self->img_set(%hsh)) {
      $Imager::ERRSTR = $self->{ERRSTR};
      return;
    }
  }
  elsif (defined $hsh{file} || 
	 defined $hsh{fh} ||
	 defined $hsh{fd} ||
	 defined $hsh{callback} ||
	 defined $hsh{readcb} ||
	 defined $hsh{data}) {
    # allow $img = Imager->new(file => $filename)
    my %extras;
    
    # type is already used as a parameter to new(), rename it for the
    # call to read()
    if ($hsh{filetype}) {
      $extras{type} = $hsh{filetype};
    }
    unless ($self->read(%hsh, %extras)) {
      $Imager::ERRSTR = $self->{ERRSTR};
      return;
    }
  }

  return $self;
}

# Copy an entire image with no changes 
# - if an image has magic the copy of it will not be magical

sub copy {
  my $self = shift;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  unless (defined wantarray) {
    my @caller = caller;
    warn "copy() called in void context - copy() returns the copied image at $caller[1] line $caller[2]\n";
    return;
  }

  my $newcopy=Imager->new();
  $newcopy->{IMG} = i_copy($self->{IMG});
  return $newcopy;
}

# Paste a region

sub paste {
  my $self = shift;

  unless ($self->{IMG}) { 
    $self->_set_error('empty input image');
    return;
  }
  my %input=(left=>0, top=>0, src_minx => 0, src_miny => 0, @_);
  my $src = $input{img} || $input{src};
  unless($src) {
    $self->_set_error("no source image");
    return;
  }
  $input{left}=0 if $input{left} <= 0;
  $input{top}=0 if $input{top} <= 0;

  my($r,$b)=i_img_info($src->{IMG});
  my ($src_left, $src_top) = @input{qw/src_minx src_miny/};
  my ($src_right, $src_bottom);
  if ($input{src_coords}) {
    ($src_left, $src_top, $src_right, $src_bottom) = @{$input{src_coords}}
  }
  else {
    if (defined $input{src_maxx}) {
      $src_right = $input{src_maxx};
    }
    elsif (defined $input{width}) {
      if ($input{width} <= 0) {
        $self->_set_error("paste: width must me positive");
        return;
      }
      $src_right = $src_left + $input{width};
    }
    else {
      $src_right = $r;
    }
    if (defined $input{src_maxy}) {
      $src_bottom = $input{src_maxy};
    }
    elsif (defined $input{height}) {
      if ($input{height} < 0) {
        $self->_set_error("paste: height must be positive");
        return;
      }
      $src_bottom = $src_top + $input{height};
    }
    else {
      $src_bottom = $b;
    }
  }

  $src_right > $r and $src_right = $r;
  $src_bottom > $b and $src_bottom = $b;

  if ($src_right <= $src_left
      || $src_bottom < $src_top) {
    $self->_set_error("nothing to paste");
    return;
  }

  i_copyto($self->{IMG}, $src->{IMG}, 
	   $src_left, $src_top, $src_right, $src_bottom, 
           $input{left}, $input{top});

  return $self;  # What should go here??
}

# Crop an image - i.e. return a new image that is smaller

sub crop {
  my $self=shift;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }
  
  unless (defined wantarray) {
    my @caller = caller;
    warn "crop() called in void context - crop() returns the cropped image at $caller[1] line $caller[2]\n";
    return;
  }

  my %hsh=@_;

  my ($w, $h, $l, $r, $b, $t) =
    @hsh{qw(width height left right bottom top)};

  # work through the various possibilities
  if (defined $l) {
    if (defined $w) {
      $r = $l + $w;
    }
    elsif (!defined $r) {
      $r = $self->getwidth;
    }
  }
  elsif (defined $r) {
    if (defined $w) {
      $l = $r - $w;
    }
    else {
      $l = 0;
    }
  }
  elsif (defined $w) {
    $l = int(0.5+($self->getwidth()-$w)/2);
    $r = $l + $w;
  }
  else {
    $l = 0;
    $r = $self->getwidth;
  }
  if (defined $t) {
    if (defined $h) {
      $b = $t + $h;
    }
    elsif (!defined $b) {
      $b = $self->getheight;
    }
  }
  elsif (defined $b) {
    if (defined $h) {
      $t = $b - $h;
    }
    else {
      $t = 0;
    }
  }
  elsif (defined $h) {
    $t=int(0.5+($self->getheight()-$h)/2);
    $b=$t+$h;
  }
  else {
    $t = 0;
    $b = $self->getheight;
  }

  ($l,$r)=($r,$l) if $l>$r;
  ($t,$b)=($b,$t) if $t>$b;

  $l < 0 and $l = 0;
  $r > $self->getwidth and $r = $self->getwidth;
  $t < 0 and $t = 0;
  $b > $self->getheight and $b = $self->getheight;

  if ($l == $r || $t == $b) {
    $self->_set_error("resulting image would have no content");
    return;
  }
  if( $r < $l or $b < $t ) {
    $self->_set_error("attempting to crop outside of the image");
    return;
  }
  my $dst = $self->_sametype(xsize=>$r-$l, ysize=>$b-$t);

  i_copyto($dst->{IMG},$self->{IMG},$l,$t,$r,$b,0,0);
  return $dst;
}

sub _sametype {
  my ($self, %opts) = @_;

  $self->{IMG} or return $self->_set_error("Not a valid image");

  my $x = $opts{xsize} || $self->getwidth;
  my $y = $opts{ysize} || $self->getheight;
  my $channels = $opts{channels} || $self->getchannels;
  
  my $out = Imager->new;
  if ($channels == $self->getchannels) {
    $out->{IMG} = i_sametype($self->{IMG}, $x, $y);
  }
  else {
    $out->{IMG} = i_sametype_chans($self->{IMG}, $x, $y, $channels);
  }
  unless ($out->{IMG}) {
    $self->{ERRSTR} = $self->_error_as_msg;
    return;
  }
  
  return $out;
}

# Sets an image to a certain size and channel number
# if there was previously data in the image it is discarded

sub img_set {
  my $self=shift;

  my %hsh=(xsize=>100, ysize=>100, channels=>3, bits=>8, type=>'direct', @_);

  if (defined($self->{IMG})) {
    # let IIM_DESTROY destroy it, it's possible this image is
    # referenced from a virtual image (like masked)
    #i_img_destroy($self->{IMG});
    undef($self->{IMG});
  }

  if ($hsh{type} eq 'paletted' || $hsh{type} eq 'pseudo') {
    $self->{IMG} = i_img_pal_new($hsh{xsize}, $hsh{ysize}, $hsh{channels},
                                 $hsh{maxcolors} || 256);
  }
  elsif ($hsh{bits} eq 'double') {
    $self->{IMG} = i_img_double_new($hsh{xsize}, $hsh{ysize}, $hsh{channels});
  }
  elsif ($hsh{bits} == 16) {
    $self->{IMG} = i_img_16_new($hsh{xsize}, $hsh{ysize}, $hsh{channels});
  }
  else {
    $self->{IMG}=Imager::ImgRaw::new($hsh{'xsize'}, $hsh{'ysize'},
                                     $hsh{'channels'});
  }

  unless ($self->{IMG}) {
    $self->{ERRSTR} = Imager->_error_as_msg();
    return;
  }

  $self;
}

# created a masked version of the current image
sub masked {
  my $self = shift;

  $self or return undef;
  my %opts = (left    => 0, 
              top     => 0, 
              right   => $self->getwidth, 
              bottom  => $self->getheight,
              @_);
  my $mask = $opts{mask} ? $opts{mask}{IMG} : undef;

  my $result = Imager->new;
  $result->{IMG} = i_img_masked_new($self->{IMG}, $mask, $opts{left}, 
                                    $opts{top}, $opts{right} - $opts{left},
                                    $opts{bottom} - $opts{top});
  unless ($result->{IMG}) {
    $self->_set_error(Imager->_error_as_msg);
    return;
  }

  # keep references to the mask and base images so they don't
  # disappear on us
  $result->{DEPENDS} = [ $self->{IMG}, $mask ];

  return $result;
}

# convert an RGB image into a paletted image
sub to_paletted {
  my $self = shift;
  my $opts;
  if (@_ != 1 && !ref $_[0]) {
    $opts = { @_ };
  }
  else {
    $opts = shift;
  }

  unless (defined wantarray) {
    my @caller = caller;
    warn "to_paletted() called in void context - to_paletted() returns the converted image at $caller[1] line $caller[2]\n";
    return;
  }

  $self->_valid_image
    or return;

  my $result = Imager->new;
  unless ($result->{IMG} = i_img_to_pal($self->{IMG}, $opts)) {
    $self->_set_error(Imager->_error_as_msg);
    return;
  }

  return $result;
}

sub make_palette {
  my ($class, $quant, @images) = @_;

  unless (@images) {
    Imager->_set_error("make_palette: supply at least one image");
    return;
  }
  my $index = 1;
  for my $img (@images) {
    unless ($img->{IMG}) {
      Imager->_set_error("make_palette: image $index is empty");
      return;
    }
    ++$index;
  }

  return i_img_make_palette($quant, map $_->{IMG}, @images);
}

# convert a paletted (or any image) to an 8-bit/channel RGB image
sub to_rgb8 {
  my $self = shift;

  unless (defined wantarray) {
    my @caller = caller;
    warn "to_rgb8() called in void context - to_rgb8() returns the converted image at $caller[1] line $caller[2]\n";
    return;
  }

  $self->_valid_image
    or return;

  my $result = Imager->new;
  unless ($result->{IMG} = i_img_to_rgb($self->{IMG})) {
    $self->_set_error(Imager->_error_as_msg());
    return;
  }

  return $result;
}

# convert a paletted (or any image) to a 16-bit/channel RGB image
sub to_rgb16 {
  my $self = shift;

  unless (defined wantarray) {
    my @caller = caller;
    warn "to_rgb16() called in void context - to_rgb16() returns the converted image at $caller[1] line $caller[2]\n";
    return;
  }

  $self->_valid_image
    or return;

  my $result = Imager->new;
  unless ($result->{IMG} = i_img_to_rgb16($self->{IMG})) {
    $self->_set_error(Imager->_error_as_msg());
    return;
  }

  return $result;
}

# convert a paletted (or any image) to an double/channel RGB image
sub to_rgb_double {
  my $self = shift;

  unless (defined wantarray) {
    my @caller = caller;
    warn "to_rgb16() called in void context - to_rgb_double() returns the converted image at $caller[1] line $caller[2]\n";
    return;
  }

  $self->_valid_image
    or return;

  my $result = Imager->new;
  unless ($result->{IMG} = i_img_to_drgb($self->{IMG})) {
    $self->_set_error(Imager->_error_as_msg());
    return;
  }

  return $result;
}

sub addcolors {
  my $self = shift;
  my %opts = (colors=>[], @_);

  unless ($self->{IMG}) {
    $self->_set_error("empty input image");
    return;
  }

  my @colors = @{$opts{colors}}
    or return undef;

  for my $color (@colors) {
    $color = _color($color);
    unless ($color) {
      $self->_set_error($Imager::ERRSTR);
      return;
    }  
  }

  return i_addcolors($self->{IMG}, @colors);
}

sub setcolors {
  my $self = shift;
  my %opts = (start=>0, colors=>[], @_);

  unless ($self->{IMG}) {
    $self->_set_error("empty input image");
    return;
  }

  my @colors = @{$opts{colors}}
    or return undef;

  for my $color (@colors) {
    $color = _color($color);
    unless ($color) {
      $self->_set_error($Imager::ERRSTR);
      return;
    }  
  }

  return i_setcolors($self->{IMG}, $opts{start}, @colors);
}

sub getcolors {
  my $self = shift;
  my %opts = @_;
  if (!exists $opts{start} && !exists $opts{count}) {
    # get them all
    $opts{start} = 0;
    $opts{count} = $self->colorcount;
  }
  elsif (!exists $opts{count}) {
    $opts{count} = 1;
  }
  elsif (!exists $opts{start}) {
    $opts{start} = 0;
  }
  
  $self->{IMG} and 
    return i_getcolors($self->{IMG}, $opts{start}, $opts{count});
}

sub colorcount {
  i_colorcount($_[0]{IMG});
}

sub maxcolors {
  i_maxcolors($_[0]{IMG});
}

sub findcolor {
  my $self = shift;
  my %opts = @_;
  $opts{color} or return undef;

  $self->{IMG} and i_findcolor($self->{IMG}, $opts{color});
}

sub bits {
  my $self = shift;
  my $bits = $self->{IMG} && i_img_bits($self->{IMG});
  if ($bits && $bits == length(pack("d", 1)) * 8) {
    $bits = 'double';
  }
  $bits;
}

sub type {
  my $self = shift;
  if ($self->{IMG}) {
    return i_img_type($self->{IMG}) ? "paletted" : "direct";
  }
}

sub virtual {
  my $self = shift;
  $self->{IMG} and i_img_virtual($self->{IMG});
}

sub is_bilevel {
  my ($self) = @_;

  $self->{IMG} or return;

  return i_img_is_monochrome($self->{IMG});
}

sub tags {
  my ($self, %opts) = @_;

  $self->{IMG} or return;

  if (defined $opts{name}) {
    my @result;
    my $start = 0;
    my $found;
    while (defined($found = i_tags_find($self->{IMG}, $opts{name}, $start))) {
      push @result, (i_tags_get($self->{IMG}, $found))[1];
      $start = $found+1;
    }
    return wantarray ? @result : $result[0];
  }
  elsif (defined $opts{code}) {
    my @result;
    my $start = 0;
    my $found;
    while (defined($found = i_tags_findn($self->{IMG}, $opts{code}, $start))) {
      push @result, (i_tags_get($self->{IMG}, $found))[1];
      $start = $found+1;
    }
    return @result;
  }
  else {
    if (wantarray) {
      return map { [ i_tags_get($self->{IMG}, $_) ] } 0.. i_tags_count($self->{IMG})-1;
    }
    else {
      return i_tags_count($self->{IMG});
    }
  }
}

sub addtag {
  my $self = shift;
  my %opts = @_;

  return -1 unless $self->{IMG};
  if ($opts{name}) {
    if (defined $opts{value}) {
      if ($opts{value} =~ /^\d+$/) {
        # add as a number
        return i_tags_addn($self->{IMG}, $opts{name}, 0, $opts{value});
      }
      else {
        return i_tags_add($self->{IMG}, $opts{name}, 0, $opts{value}, 0);
      }
    }
    elsif (defined $opts{data}) {
      # force addition as a string
      return i_tags_add($self->{IMG}, $opts{name}, 0, $opts{data}, 0);
    }
    else {
      $self->{ERRSTR} = "No value supplied";
      return undef;
    }
  }
  elsif ($opts{code}) {
    if (defined $opts{value}) {
      if ($opts{value} =~ /^\d+$/) {
        # add as a number
        return i_tags_addn($self->{IMG}, $opts{code}, 0, $opts{value});
      }
      else {
        return i_tags_add($self->{IMG}, $opts{code}, 0, $opts{value}, 0);
      }
    }
    elsif (defined $opts{data}) {
      # force addition as a string
      return i_tags_add($self->{IMG}, $opts{code}, 0, $opts{data}, 0);
    }
    else {
      $self->{ERRSTR} = "No value supplied";
      return undef;
    }
  }
  else {
    return undef;
  }
}

sub deltag {
  my $self = shift;
  my %opts = @_;

  return 0 unless $self->{IMG};

  if (defined $opts{'index'}) {
    return i_tags_delete($self->{IMG}, $opts{'index'});
  }
  elsif (defined $opts{name}) {
    return i_tags_delbyname($self->{IMG}, $opts{name});
  }
  elsif (defined $opts{code}) {
    return i_tags_delbycode($self->{IMG}, $opts{code});
  }
  else {
    $self->{ERRSTR} = "Need to supply index, name, or code parameter";
    return 0;
  }
}

sub settag {
  my ($self, %opts) = @_;

  if ($opts{name}) {
    $self->deltag(name=>$opts{name});
    return $self->addtag(name=>$opts{name}, value=>$opts{value});
  }
  elsif (defined $opts{code}) {
    $self->deltag(code=>$opts{code});
    return $self->addtag(code=>$opts{code}, value=>$opts{value});
  }
  else {
    return undef;
  }
}


sub _get_reader_io {
  my ($self, $input) = @_;

  if ($input->{io}) {
    return $input->{io}, undef;
  }
  elsif ($input->{fd}) {
    return io_new_fd($input->{fd});
  }
  elsif ($input->{fh}) {
    my $fd = fileno($input->{fh});
    unless (defined $fd) {
      $self->_set_error("Handle in fh option not opened");
      return;
    }
    return io_new_fd($fd);
  }
  elsif ($input->{file}) {
    my $file = IO::File->new($input->{file}, "r");
    unless ($file) {
      $self->_set_error("Could not open $input->{file}: $!");
      return;
    }
    binmode $file;
    return (io_new_fd(fileno($file)), $file);
  }
  elsif ($input->{data}) {
    return io_new_buffer($input->{data});
  }
  elsif ($input->{callback} || $input->{readcb}) {
    if (!$input->{seekcb}) {
      $self->_set_error("Need a seekcb parameter");
    }
    if ($input->{maxbuffer}) {
      return io_new_cb($input->{writecb},
                       $input->{callback} || $input->{readcb},
                       $input->{seekcb}, $input->{closecb},
                       $input->{maxbuffer});
    }
    else {
      return io_new_cb($input->{writecb},
                       $input->{callback} || $input->{readcb},
                       $input->{seekcb}, $input->{closecb});
    }
  }
  else {
    $self->_set_error("file/fd/fh/data/callback parameter missing");
    return;
  }
}

sub _get_writer_io {
  my ($self, $input) = @_;

  my $buffered = exists $input->{buffered} ? $input->{buffered} : 1;

  my $io;
  my @extras;
  if ($input->{io}) {
    $io = $input->{io};
  }
  elsif ($input->{fd}) {
    $io = io_new_fd($input->{fd});
  }
  elsif ($input->{fh}) {
    my $fd = fileno($input->{fh});
    unless (defined $fd) {
      $self->_set_error("Handle in fh option not opened");
      return;
    }
    # flush it
    my $oldfh = select($input->{fh});
    # flush anything that's buffered, and make sure anything else is flushed
    $| = 1;
    select($oldfh);
    $io = io_new_fd($fd);
  }
  elsif ($input->{file}) {
    my $fh = new IO::File($input->{file},"w+");
    unless ($fh) { 
      $self->_set_error("Could not open file $input->{file}: $!");
      return;
    }
    binmode($fh) or die;
    $io = io_new_fd(fileno($fh));
    push @extras, $fh;
  }
  elsif ($input->{data}) {
    $io = io_new_bufchain();
  }
  elsif ($input->{callback} || $input->{writecb}) {
    if ($input->{maxbuffer} && $input->{maxbuffer} == 1) {
      $buffered = 0;
    }
    $io = io_new_cb($input->{callback} || $input->{writecb},
		    $input->{readcb},
		    $input->{seekcb}, $input->{closecb});
  }
  else {
    $self->_set_error("file/fd/fh/data/callback parameter missing");
    return;
  }

  unless ($buffered) {
    $io->set_buffered(0);
  }

  return ($io, @extras);
}

# Read an image from file

sub read {
  my $self = shift;
  my %input=@_;

  if (defined($self->{IMG})) {
    # let IIM_DESTROY do the destruction, since the image may be
    # referenced from elsewhere
    #i_img_destroy($self->{IMG});
    undef($self->{IMG});
  }

  my ($IO, $fh) = $self->_get_reader_io(\%input) or return;

  my $type = $input{'type'};
  unless ($type) {
    $type = i_test_format_probe($IO, -1);
  }

  if ($input{file} && !$type) {
    # guess the type 
    $type = $FORMATGUESS->($input{file});
  }

  unless ($type) {
    my $msg = "type parameter missing and it couldn't be determined from the file contents";
    $input{file} and $msg .= " or file name";
    $self->_set_error($msg);
    return undef;
  }

  _reader_autoload($type);

  if ($readers{$type} && $readers{$type}{single}) {
    return $readers{$type}{single}->($self, $IO, %input);
  }

  unless ($formats_low{$type}) {
    my $read_types = join ', ', sort Imager->read_types();
    $self->_set_error("format '$type' not supported - formats $read_types available for reading - $reader_load_errors{$type}");
    return;
  }

  my $allow_incomplete = $input{allow_incomplete};
  defined $allow_incomplete or $allow_incomplete = 0;

  if ( $type eq 'pnm' ) {
    $self->{IMG}=i_readpnm_wiol( $IO, $allow_incomplete );
    if ( !defined($self->{IMG}) ) {
      $self->{ERRSTR}='unable to read pnm image: '._error_as_msg(); 
      return undef;
    }
    $self->{DEBUG} && print "loading a pnm file\n";
    return $self;
  }

  if ( $type eq 'bmp' ) {
    $self->{IMG}=i_readbmp_wiol( $IO, $allow_incomplete );
    if ( !defined($self->{IMG}) ) {
      $self->{ERRSTR}=$self->_error_as_msg();
      return undef;
    }
    $self->{DEBUG} && print "loading a bmp file\n";
  }

  if ( $type eq 'tga' ) {
    $self->{IMG}=i_readtga_wiol( $IO, -1 ); # Fixme, check if that length parameter is ever needed
    if ( !defined($self->{IMG}) ) {
      $self->{ERRSTR}=$self->_error_as_msg();
      return undef;
    }
    $self->{DEBUG} && print "loading a tga file\n";
  }

  if ( $type eq 'raw' ) {
    unless ( $input{xsize} && $input{ysize} ) {
      $self->_set_error('missing xsize or ysize parameter for raw');
      return undef;
    }

    my $interleave = _first($input{raw_interleave}, $input{interleave});
    unless (defined $interleave) {
      my @caller = caller;
      warn "read(type => 'raw') $caller[2] line $caller[1]: supply interleave or raw_interleave for future compatibility\n";
      $interleave = 1;
    }
    my $data_ch = _first($input{raw_datachannels}, $input{datachannels}, 3);
    my $store_ch = _first($input{raw_storechannels}, $input{storechannels}, 3);

    $self->{IMG} = i_readraw_wiol( $IO,
				   $input{xsize},
				   $input{ysize},
				   $data_ch,
				   $store_ch,
				   $interleave);
    if ( !defined($self->{IMG}) ) {
      $self->{ERRSTR}=$self->_error_as_msg();
      return undef;
    }
    $self->{DEBUG} && print "loading a raw file\n";
  }

  return $self;
}

sub register_reader {
  my ($class, %opts) = @_;

  defined $opts{type}
    or die "register_reader called with no type parameter\n";

  my $type = $opts{type};

  defined $opts{single} || defined $opts{multiple}
    or die "register_reader called with no single or multiple parameter\n";

  $readers{$type} = {  };
  if ($opts{single}) {
    $readers{$type}{single} = $opts{single};
  }
  if ($opts{multiple}) {
    $readers{$type}{multiple} = $opts{multiple};
  }

  return 1;
}

sub register_writer {
  my ($class, %opts) = @_;

  defined $opts{type}
    or die "register_writer called with no type parameter\n";

  my $type = $opts{type};

  defined $opts{single} || defined $opts{multiple}
    or die "register_writer called with no single or multiple parameter\n";

  $writers{$type} = {  };
  if ($opts{single}) {
    $writers{$type}{single} = $opts{single};
  }
  if ($opts{multiple}) {
    $writers{$type}{multiple} = $opts{multiple};
  }

  return 1;
}

sub read_types {
  my %types =
    (
     map { $_ => 1 }
     keys %readers,
     grep($file_formats{$_}, keys %formats),
     qw(ico sgi), # formats not handled directly, but supplied with Imager
    );

  return keys %types;
}

sub write_types {
  my %types =
    (
     map { $_ => 1 }
     keys %writers,
     grep($file_formats{$_}, keys %formats),
     qw(ico sgi), # formats not handled directly, but supplied with Imager
    );

  return keys %types;
}

sub _load_file {
  my ($file, $error) = @_;

  if ($attempted_to_load{$file}) {
    if ($file_load_errors{$file}) {
      $$error = $file_load_errors{$file};
      return 0;
    }
    else {
      return 1;
    }
  }
  else {
    local $SIG{__DIE__};
    my $loaded = eval {
      ++$attempted_to_load{$file};
      require $file;
      return 1;
    };
    if ($loaded) {
      return 1;
    }
    else {
      my $work = $@ || "Unknown error loading $file";
      chomp $work;
      $work =~ s/\n?Compilation failed in require at .*Imager\.pm line .*\z//m;
      $work =~ s/\n/\\n/g;
      $file_load_errors{$file} = $work;
      $$error = $work;
      return 0;
    }
  }
}

# probes for an Imager::File::whatever module
sub _reader_autoload {
  my $type = shift;

  return if $formats_low{$type} || $readers{$type};

  return unless $type =~ /^\w+$/;

  my $file = "Imager/File/\U$type\E.pm";

  my $error;
  my $loaded = _load_file($file, \$error);
  if (!$loaded && $error =~ /^Can't locate /) {
    my $filer = "Imager/File/\U$type\EReader.pm";
    $loaded = _load_file($filer, \$error);
    if ($error =~ /^Can't locate /) {
      $error = "Can't locate $file or $filer";
    }
  }
  unless ($loaded) {
    $reader_load_errors{$type} = $error;
  }
}

# probes for an Imager::File::whatever module
sub _writer_autoload {
  my $type = shift;

  return if $formats_low{$type} || $writers{$type};

  return unless $type =~ /^\w+$/;

  my $file = "Imager/File/\U$type\E.pm";

  my $error;
  my $loaded = _load_file($file, \$error);
  if (!$loaded && $error =~ /^Can't locate /) {
    my $filew = "Imager/File/\U$type\EWriter.pm";
    $loaded = _load_file($filew, \$error);
    if ($error =~ /^Can't locate /) {
      $error = "Can't locate $file or $filew";
    }
  }
  unless ($loaded) {
    $writer_load_errors{$type} = $error;
  }
}

sub _fix_gif_positions {
  my ($opts, $opt, $msg, @imgs) = @_;

  my $positions = $opts->{'gif_positions'};
  my $index = 0;
  for my $pos (@$positions) {
    my ($x, $y) = @$pos;
    my $img = $imgs[$index++];
    $img->settag(name=>'gif_left', value=>$x);
    $img->settag(name=>'gif_top', value=>$y) if defined $y;
  }
  $$msg .= "replaced with the gif_left and gif_top tags";
}

my %obsolete_opts =
  (
   gif_each_palette=>'gif_local_map',
   interlace       => 'gif_interlace',
   gif_delays => 'gif_delay',
   gif_positions => \&_fix_gif_positions,
   gif_loop_count => 'gif_loop',
  );

# options that should be converted to colors
my %color_opts = map { $_ => 1 } qw/i_background/;

sub _set_opts {
  my ($self, $opts, $prefix, @imgs) = @_;

  for my $opt (keys %$opts) {
    my $tagname = $opt;
    if ($obsolete_opts{$opt}) {
      my $new = $obsolete_opts{$opt};
      my $msg = "Obsolete option $opt ";
      if (ref $new) {
        $new->($opts, $opt, \$msg, @imgs);
      }
      else {
        $msg .= "replaced with the $new tag ";
        $tagname = $new;
      }
      $msg .= "line ".(caller(2))[2]." of file ".(caller(2))[1];
      warn $msg if $warn_obsolete && $^W;
    }
    next unless $tagname =~ /^\Q$prefix/;
    my $value = $opts->{$opt};
    if ($color_opts{$opt}) {
      $value = _color($value);
      unless ($value) {
	$self->_set_error($Imager::ERRSTR);
	return;
      }
    }
    if (ref $value) {
      if (UNIVERSAL::isa($value, "Imager::Color")) {
        my $tag = sprintf("color(%d,%d,%d,%d)", $value->rgba);
        for my $img (@imgs) {
          $img->settag(name=>$tagname, value=>$tag);
        }
      }
      elsif (ref($value) eq 'ARRAY') {
        for my $i (0..$#$value) {
          my $val = $value->[$i];
          if (ref $val) {
            if (UNIVERSAL::isa($val, "Imager::Color")) {
              my $tag = sprintf("color(%d,%d,%d,%d)", $value->rgba);
              $i < @imgs and
                $imgs[$i]->settag(name=>$tagname, value=>$tag);
            }
            else {
              $self->_set_error("Unknown reference type " . ref($value) . 
                                " supplied in array for $opt");
              return;
            }
          }
          else {
            $i < @imgs
              and $imgs[$i]->settag(name=>$tagname, value=>$val);
          }
        }
      }
      else {
        $self->_set_error("Unknown reference type " . ref($value) . 
                          " supplied for $opt");
        return;
      }
    }
    else {
      # set it as a tag for every image
      for my $img (@imgs) {
        $img->settag(name=>$tagname, value=>$value);
      }
    }
  }

  return 1;
}

# Write an image to file
sub write {
  my $self = shift;
  my %input=(jpegquality=>75,
	     gifquant=>'mc',
	     lmdither=>6.0,
	     lmfixed=>[],
	     idstring=>"",
	     compress=>1,
	     wierdpack=>0,
	     fax_fine=>1, @_);
  my $rc;

  $self->_set_opts(\%input, "i_", $self)
    or return undef;

  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  my $type = $input{'type'};
  if (!$type and $input{file}) { 
    $type = $FORMATGUESS->($input{file});
  }
  unless ($type) { 
    $self->{ERRSTR}='type parameter missing and not possible to guess from extension';
    return undef;
  }

  _writer_autoload($type);

  my ($IO, $fh);
  if ($writers{$type} && $writers{$type}{single}) {
    ($IO, $fh) = $self->_get_writer_io(\%input)
      or return undef;

    $writers{$type}{single}->($self, $IO, %input, type => $type)
      or return undef;
  }
  else {
    if (!$formats_low{$type}) { 
      my $write_types = join ', ', sort Imager->write_types();
      $self->_set_error("format '$type' not supported - formats $write_types available for writing - $writer_load_errors{$type}");
      return undef;
    }
    
    ($IO, $fh) = $self->_get_writer_io(\%input, $type)
      or return undef;
  
    if ( $type eq 'pnm' ) {
      $self->_set_opts(\%input, "pnm_", $self)
        or return undef;
      if ( ! i_writeppm_wiol($self->{IMG},$IO) ) {
        $self->{ERRSTR} = $self->_error_as_msg();
        return undef;
      }
      $self->{DEBUG} && print "writing a pnm file\n";
    }
    elsif ( $type eq 'raw' ) {
      $self->_set_opts(\%input, "raw_", $self)
        or return undef;
      if ( !i_writeraw_wiol($self->{IMG},$IO) ) {
        $self->{ERRSTR} = $self->_error_as_msg();
        return undef;
      }
      $self->{DEBUG} && print "writing a raw file\n";
    }
    elsif ( $type eq 'bmp' ) {
      $self->_set_opts(\%input, "bmp_", $self)
        or return undef;
      if ( !i_writebmp_wiol($self->{IMG}, $IO) ) {
	$self->{ERRSTR} = $self->_error_as_msg;
        return undef;
      }
      $self->{DEBUG} && print "writing a bmp file\n";
    }
    elsif ( $type eq 'tga' ) {
      $self->_set_opts(\%input, "tga_", $self)
        or return undef;
      
      if ( !i_writetga_wiol($self->{IMG}, $IO, $input{wierdpack}, $input{compress}, $input{idstring}) ) {
        $self->{ERRSTR}=$self->_error_as_msg();
        return undef;
      }
      $self->{DEBUG} && print "writing a tga file\n";
    }
  }

  if (exists $input{'data'}) {
    my $data = io_slurp($IO);
    if (!$data) {
      $self->{ERRSTR}='Could not slurp from buffer';
      return undef;
    }
    ${$input{data}} = $data;
  }
  return $self;
}

sub write_multi {
  my ($class, $opts, @images) = @_;

  my $type = $opts->{type};

  if (!$type && $opts->{'file'}) {
    $type = $FORMATGUESS->($opts->{'file'});
  }
  unless ($type) {
    $class->_set_error('type parameter missing and not possible to guess from extension');
    return;
  }
  # translate to ImgRaw
  if (grep !UNIVERSAL::isa($_, 'Imager') || !$_->{IMG}, @images) {
    $class->_set_error('Usage: Imager->write_multi({ options }, @images)');
    return 0;
  }
  $class->_set_opts($opts, "i_", @images)
    or return;
  my @work = map $_->{IMG}, @images;

  _writer_autoload($type);

  my ($IO, $file);
  if ($writers{$type} && $writers{$type}{multiple}) {
    ($IO, $file) = $class->_get_writer_io($opts, $type)
      or return undef;

    $writers{$type}{multiple}->($class, $IO, $opts, @images)
      or return undef;
  }
  else {
    if (!$formats{$type}) { 
      my $write_types = join ', ', sort Imager->write_types();
      $class->_set_error("format '$type' not supported - formats $write_types available for writing");
      return undef;
    }
    
    ($IO, $file) = $class->_get_writer_io($opts, $type)
      or return undef;
    
    if (0) { # eventually PNM in here, now that TIFF/GIF are elsewhere
    }
    else {
      if (@images == 1) {
	unless ($images[0]->write(%$opts, io => $IO, type => $type)) {
	  return 1;
	}
      }
      else {
	$ERRSTR = "Sorry, write_multi doesn't support $type yet";
	return 0;
      }
    }
  }

  if (exists $opts->{'data'}) {
    my $data = io_slurp($IO);
    if (!$data) {
      Imager->_set_error('Could not slurp from buffer');
      return undef;
    }
    ${$opts->{data}} = $data;
  }
  return 1;
}

# read multiple images from a file
sub read_multi {
  my ($class, %opts) = @_;

  my ($IO, $file) = $class->_get_reader_io(\%opts, $opts{'type'})
    or return;

  my $type = $opts{'type'};
  unless ($type) {
    $type = i_test_format_probe($IO, -1);
  }

  if ($opts{file} && !$type) {
    # guess the type 
    $type = $FORMATGUESS->($opts{file});
  }

  unless ($type) {
    my $msg = "type parameter missing and it couldn't be determined from the file contents";
    $opts{file} and $msg .= " or file name";
    Imager->_set_error($msg);
    return;
  }

  _reader_autoload($type);

  if ($readers{$type} && $readers{$type}{multiple}) {
    return $readers{$type}{multiple}->($IO, %opts);
  }

  unless ($formats{$type}) {
    my $read_types = join ', ', sort Imager->read_types();
    Imager->_set_error("format '$type' not supported - formats $read_types available for reading");
    return;
  }

  my @imgs;
  if ($type eq 'pnm') {
    @imgs = i_readpnm_multi_wiol($IO, $opts{allow_incomplete}||0);
  }
  else {
    my $img = Imager->new;
    if ($img->read(%opts, io => $IO, type => $type)) {
      return ( $img );
    }
    Imager->_set_error($img->errstr);
    return;
  }

  if (!@imgs) {
    $ERRSTR = _error_as_msg();
  return;
  }
  return map { 
        bless { IMG=>$_, DEBUG=>$DEBUG, ERRSTR=>undef }, 'Imager' 
      } @imgs;
}

# Destroy an Imager object

sub DESTROY {
  my $self=shift;
  #    delete $instances{$self};
  if (defined($self->{IMG})) {
    # the following is now handled by the XS DESTROY method for
    # Imager::ImgRaw object
    # Re-enabling this will break virtual images
    # tested for in t/t020masked.t
    # i_img_destroy($self->{IMG});
    undef($self->{IMG});
  } else {
#    print "Destroy Called on an empty image!\n"; # why did I put this here??
  }
}

# Perform an inplace filter of an image
# that is the image will be overwritten with the data

sub filter {
  my $self=shift;
  my %input=@_;
  my %hsh;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  if (!$input{'type'}) { $self->{ERRSTR}='type parameter missing'; return undef; }

  if ( (grep { $_ eq $input{'type'} } keys %filters) != 1) {
    $self->{ERRSTR}='type parameter not matching any filter'; return undef;
  }

  if ($filters{$input{'type'}}{names}) {
    my $names = $filters{$input{'type'}}{names};
    for my $name (keys %$names) {
      if (defined $input{$name} && exists $names->{$name}{$input{$name}}) {
        $input{$name} = $names->{$name}{$input{$name}};
      }
    }
  }
  if (defined($filters{$input{'type'}}{defaults})) {
    %hsh=( image => $self->{IMG},
           imager => $self,
           %{$filters{$input{'type'}}{defaults}},
           %input );
  } else {
    %hsh=( image => $self->{IMG},
           imager => $self,
           %input );
  }

  my @cs=@{$filters{$input{'type'}}{callseq}};

  for(@cs) {
    if (!defined($hsh{$_})) {
      $self->{ERRSTR}="missing parameter '$_' for filter ".$input{'type'}; return undef;
    }
  }

  eval {
    local $SIG{__DIE__}; # we don't want this processed by confess, etc
    &{$filters{$input{'type'}}{callsub}}(%hsh);
  };
  if ($@) {
    chomp($self->{ERRSTR} = $@);
    return;
  }

  my @b=keys %hsh;

  $self->{DEBUG} && print "callseq is: @cs\n";
  $self->{DEBUG} && print "matching callseq is: @b\n";

  return $self;
}

sub register_filter {
  my $class = shift;
  my %hsh = ( defaults => {}, @_ );

  defined $hsh{type}
    or die "register_filter() with no type\n";
  defined $hsh{callsub}
    or die "register_filter() with no callsub\n";
  defined $hsh{callseq}
    or die "register_filter() with no callseq\n";

  exists $filters{$hsh{type}}
    and return;

  $filters{$hsh{type}} = \%hsh;

  return 1;
}

sub scale_calculate {
  my $self = shift;

  my %opts = ('type'=>'max', @_);

  # none of these should be references
  for my $name (qw/xpixels ypixels xscalefactor yscalefactor width height/) {
    if (defined $opts{$name} && ref $opts{$name}) {
      $self->_set_error("scale_calculate: $name parameter cannot be a reference");
      return;
    }
  }

  my ($x_scale, $y_scale);
  my $width = $opts{width};
  my $height = $opts{height};
  if (ref $self) {
    defined $width or $width = $self->getwidth;
    defined $height or $height = $self->getheight;
  }
  else {
    unless (defined $width && defined $height) {
      $self->_set_error("scale_calculate: width and height parameters must be supplied when called as a class method");
      return;
    }
  }

  if ($opts{'xscalefactor'} && $opts{'yscalefactor'}) {
    $x_scale = $opts{'xscalefactor'};
    $y_scale = $opts{'yscalefactor'};
  }
  elsif ($opts{'xscalefactor'}) {
    $x_scale = $opts{'xscalefactor'};
    $y_scale = $opts{'scalefactor'} || $x_scale;
  }
  elsif ($opts{'yscalefactor'}) {
    $y_scale = $opts{'yscalefactor'};
    $x_scale = $opts{'scalefactor'} || $y_scale;
  }
  else {
    $x_scale = $y_scale = $opts{'scalefactor'} || 0.5;
  }

  # work out the scaling
  if ($opts{xpixels} and $opts{ypixels} and $opts{'type'}) {
    my ($xpix, $ypix)=( $opts{xpixels} / $width , 
			$opts{ypixels} / $height );
    if ($opts{'type'} eq 'min') { 
      $x_scale = $y_scale = _min($xpix,$ypix); 
    }
    elsif ($opts{'type'} eq 'max') {
      $x_scale = $y_scale = _max($xpix,$ypix);
    }
    elsif ($opts{'type'} eq 'nonprop' || $opts{'type'} eq 'non-proportional') {
      $x_scale = $xpix;
      $y_scale = $ypix;
    }
    else {
      $self->_set_error('invalid value for type parameter');
      return;
    }
  } elsif ($opts{xpixels}) { 
    $x_scale = $y_scale = $opts{xpixels} / $width;
  }
  elsif ($opts{ypixels}) { 
    $x_scale = $y_scale = $opts{ypixels}/$height;
  }
  elsif ($opts{constrain} && ref $opts{constrain}
	 && $opts{constrain}->can('constrain')) {
    # we've been passed an Image::Math::Constrain object or something
    # that looks like one
    my $scalefactor;
    (undef, undef, $scalefactor)
      = $opts{constrain}->constrain($self->getwidth, $self->getheight);
    unless ($scalefactor) {
      $self->_set_error('constrain method failed on constrain parameter');
      return;
    }
    $x_scale = $y_scale = $scalefactor;
  }

  my $new_width = int($x_scale * $width + 0.5);
  $new_width > 0 or $new_width = 1;
  my $new_height = int($y_scale * $height + 0.5);
  $new_height > 0 or $new_height = 1;

  return ($x_scale, $y_scale, $new_width, $new_height);
  
}

# Scale an image to requested size and return the scaled version

sub scale {
  my $self=shift;
  my %opts = (qtype=>'normal' ,@_);
  my $img = Imager->new();
  my $tmp = Imager->new();

  unless (defined wantarray) {
    my @caller = caller;
    warn "scale() called in void context - scale() returns the scaled image at $caller[1] line $caller[2]\n";
    return;
  }

  unless ($self->{IMG}) { 
    $self->_set_error('empty input image'); 
    return undef;
  }

  my ($x_scale, $y_scale, $new_width, $new_height) = 
    $self->scale_calculate(%opts)
      or return;

  if ($opts{qtype} eq 'normal') {
    $tmp->{IMG} = i_scaleaxis($self->{IMG}, $x_scale, 0);
    if ( !defined($tmp->{IMG}) ) { 
      $self->{ERRSTR} = 'unable to scale image: ' . $self->_error_as_msg;
      return undef;
    }
    $img->{IMG}=i_scaleaxis($tmp->{IMG}, $y_scale, 1);
    if ( !defined($img->{IMG}) ) { 
      $self->{ERRSTR}='unable to scale image: ' . $self->_error_as_msg; 
      return undef;
    }

    return $img;
  }
  elsif ($opts{'qtype'} eq 'preview') {
    $img->{IMG} = i_scale_nn($self->{IMG}, $x_scale, $y_scale); 
    if ( !defined($img->{IMG}) ) { 
      $self->{ERRSTR}='unable to scale image'; 
      return undef;
    }
    return $img;
  }
  elsif ($opts{'qtype'} eq 'mixing') {
    $img->{IMG} = i_scale_mixing($self->{IMG}, $new_width, $new_height);
    unless ($img->{IMG}) {
      $self->_set_error(Imager->_error_as_msg);
      return;
    }
    return $img;
  }
  else {
    $self->_set_error('invalid value for qtype parameter');
    return undef;
  }
}

# Scales only along the X axis

sub scaleX {
  my $self = shift;
  my %opts = ( scalefactor=>0.5, @_ );

  unless (defined wantarray) {
    my @caller = caller;
    warn "scaleX() called in void context - scaleX() returns the scaled image at $caller[1] line $caller[2]\n";
    return;
  }

  unless ($self->{IMG}) { 
    $self->{ERRSTR} = 'empty input image';
    return undef;
  }

  my $img = Imager->new();

  my $scalefactor = $opts{scalefactor};

  if ($opts{pixels}) { 
    $scalefactor = $opts{pixels} / $self->getwidth();
  }

  unless ($self->{IMG}) { 
    $self->{ERRSTR}='empty input image'; 
    return undef;
  }

  $img->{IMG} = i_scaleaxis($self->{IMG}, $scalefactor, 0);

  if ( !defined($img->{IMG}) ) { 
    $self->{ERRSTR} = 'unable to scale image'; 
    return undef;
  }

  return $img;
}

# Scales only along the Y axis

sub scaleY {
  my $self = shift;
  my %opts = ( scalefactor => 0.5, @_ );

  unless (defined wantarray) {
    my @caller = caller;
    warn "scaleY() called in void context - scaleY() returns the scaled image at $caller[1] line $caller[2]\n";
    return;
  }

  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  my $img = Imager->new();

  my $scalefactor = $opts{scalefactor};

  if ($opts{pixels}) { 
    $scalefactor = $opts{pixels} / $self->getheight();
  }

  unless ($self->{IMG}) { 
    $self->{ERRSTR} = 'empty input image'; 
    return undef;
  }
  $img->{IMG}=i_scaleaxis($self->{IMG}, $scalefactor, 1);

  if ( !defined($img->{IMG}) ) {
    $self->{ERRSTR} = 'unable to scale image';
    return undef;
  }

  return $img;
}

# Transform returns a spatial transformation of the input image
# this moves pixels to a new location in the returned image.
# NOTE - should make a utility function to check transforms for
# stack overruns

sub transform {
  my $self=shift;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }
  my %opts=@_;
  my (@op,@ropx,@ropy,$iop,$or,@parm,$expr,@xt,@yt,@pt,$numre);

#  print Dumper(\%opts);
#  xopcopdes

  if ( $opts{'xexpr'} and $opts{'yexpr'} ) {
    if (!$I2P) {
      eval ("use Affix::Infix2Postfix;");
      print $@;
      if ( $@ ) {
	$self->{ERRSTR}='transform: expr given and Affix::Infix2Postfix is not avaliable.'; 
	return undef;
      }
      $I2P=Affix::Infix2Postfix->new('ops'=>[{op=>'+',trans=>'Add'},
					     {op=>'-',trans=>'Sub'},
					     {op=>'*',trans=>'Mult'},
					     {op=>'/',trans=>'Div'},
					     {op=>'-','type'=>'unary',trans=>'u-'},
					     {op=>'**'},
					     {op=>'func','type'=>'unary'}],
				     'grouping'=>[qw( \( \) )],
				     'func'=>[qw( sin cos )],
				     'vars'=>[qw( x y )]
				    );
    }

    @xt=$I2P->translate($opts{'xexpr'});
    @yt=$I2P->translate($opts{'yexpr'});

    $numre=$I2P->{'numre'};
    @pt=(0,0);

    for(@xt) { if (/$numre/) { push(@pt,$_); push(@{$opts{'xopcodes'}},'Parm',$#pt); } else { push(@{$opts{'xopcodes'}},$_); } }
    for(@yt) { if (/$numre/) { push(@pt,$_); push(@{$opts{'yopcodes'}},'Parm',$#pt); } else { push(@{$opts{'yopcodes'}},$_); } }
    @{$opts{'parm'}}=@pt;
  }

#  print Dumper(\%opts);

  if ( !exists $opts{'xopcodes'} or @{$opts{'xopcodes'}}==0) {
    $self->{ERRSTR}='transform: no xopcodes given.';
    return undef;
  }

  @op=@{$opts{'xopcodes'}};
  for $iop (@op) { 
    if (!defined ($OPCODES{$iop}) and ($iop !~ /^\d+$/) ) {
      $self->{ERRSTR}="transform: illegal opcode '$_'.";
      return undef;
    }
    push(@ropx,(exists $OPCODES{$iop}) ? @{$OPCODES{$iop}} : $iop );
  }


# yopcopdes

  if ( !exists $opts{'yopcodes'} or @{$opts{'yopcodes'}}==0) {
    $self->{ERRSTR}='transform: no yopcodes given.';
    return undef;
  }

  @op=@{$opts{'yopcodes'}};
  for $iop (@op) { 
    if (!defined ($OPCODES{$iop}) and ($iop !~ /^\d+$/) ) {
      $self->{ERRSTR}="transform: illegal opcode '$_'.";
      return undef;
    }
    push(@ropy,(exists $OPCODES{$iop}) ? @{$OPCODES{$iop}} : $iop );
  }

#parameters

  if ( !exists $opts{'parm'}) {
    $self->{ERRSTR}='transform: no parameter arg given.';
    return undef;
  }

#  print Dumper(\@ropx);
#  print Dumper(\@ropy);
#  print Dumper(\@ropy);

  my $img = Imager->new();
  $img->{IMG}=i_transform($self->{IMG},\@ropx,\@ropy,$opts{'parm'});
  if ( !defined($img->{IMG}) ) { $self->{ERRSTR}='transform: failed'; return undef; }
  return $img;
}


sub transform2 {
  my ($opts, @imgs) = @_;
  
  require "Imager/Expr.pm";

  $opts->{variables} = [ qw(x y) ];
  my ($width, $height) = @{$opts}{qw(width height)};
  if (@imgs) {
    $width ||= $imgs[0]->getwidth();
    $height ||= $imgs[0]->getheight();
    my $img_num = 1;
    for my $img (@imgs) {
      $opts->{constants}{"w$img_num"} = $img->getwidth();
      $opts->{constants}{"h$img_num"} = $img->getheight();
      $opts->{constants}{"cx$img_num"} = $img->getwidth()/2;
      $opts->{constants}{"cy$img_num"} = $img->getheight()/2;
      ++$img_num;
    }
  }
  if ($width) {
    $opts->{constants}{w} = $width;
    $opts->{constants}{cx} = $width/2;
  }
  else {
    $Imager::ERRSTR = "No width supplied";
    return;
  }
  if ($height) {
    $opts->{constants}{h} = $height;
    $opts->{constants}{cy} = $height/2;
  }
  else {
    $Imager::ERRSTR = "No height supplied";
    return;
  }
  my $code = Imager::Expr->new($opts);
  if (!$code) {
    $Imager::ERRSTR = Imager::Expr::error();
    return;
  }
  my $channels = $opts->{channels} || 3;
  unless ($channels >= 1 && $channels <= 4) {
    return Imager->_set_error("channels must be an integer between 1 and 4");
  }

  my $img = Imager->new();
  $img->{IMG} = i_transform2($opts->{width}, $opts->{height}, 
			     $channels, $code->code(),
                             $code->nregs(), $code->cregs(),
                             [ map { $_->{IMG} } @imgs ]);
  if (!defined $img->{IMG}) {
    $Imager::ERRSTR = Imager->_error_as_msg();
    return;
  }

  return $img;
}

sub rubthrough {
  my $self=shift;
  my %opts= @_;

  unless ($self->{IMG}) { 
    $self->{ERRSTR}='empty input image'; 
    return undef;
  }
  unless ($opts{src} && $opts{src}->{IMG}) {
    $self->{ERRSTR}='empty input image for src'; 
    return undef;
  }

  %opts = (src_minx => 0,
	   src_miny => 0,
	   src_maxx => $opts{src}->getwidth(),
	   src_maxy => $opts{src}->getheight(),
	   %opts);

  my $tx = $opts{tx};
  defined $tx or $tx = $opts{left};
  defined $tx or $tx = 0;

  my $ty = $opts{ty};
  defined $ty or $ty = $opts{top};
  defined $ty or $ty = 0;

  unless (i_rubthru($self->{IMG}, $opts{src}->{IMG}, $tx, $ty,
                    $opts{src_minx}, $opts{src_miny}, 
                    $opts{src_maxx}, $opts{src_maxy})) {
    $self->_set_error($self->_error_as_msg());
    return undef;
  }

  return $self;
}

sub compose {
  my $self = shift;
  my %opts =
    ( 
     opacity => 1.0,
     mask_left => 0,
     mask_top => 0,
     @_
    );

  unless ($self->{IMG}) {
    $self->_set_error("compose: empty input image");
    return;
  }

  unless ($opts{src}) {
    $self->_set_error("compose: src parameter missing");
    return;
  }
  
  unless ($opts{src}{IMG}) {
    $self->_set_error("compose: src parameter empty image");
    return;
  }
  my $src = $opts{src};

  my $left = $opts{left};
  defined $left or $left = $opts{tx};
  defined $left or $left = 0;

  my $top = $opts{top};
  defined $top or $top = $opts{ty};
  defined $top or $top = 0;

  my $src_left = $opts{src_left};
  defined $src_left or $src_left = $opts{src_minx};
  defined $src_left or $src_left = 0;

  my $src_top = $opts{src_top};
  defined $src_top or $src_top = $opts{src_miny};
  defined $src_top or $src_top = 0;

  my $width = $opts{width};
  if (!defined $width && defined $opts{src_maxx}) {
    $width = $opts{src_maxx} - $src_left;
  }
  defined $width or $width = $src->getwidth() - $src_left;

  my $height = $opts{height};
  if (!defined $height && defined $opts{src_maxy}) {
    $height = $opts{src_maxy} - $src_top;
  }
  defined $height or $height = $src->getheight() - $src_top;

  my $combine = $self->_combine($opts{combine}, 'normal');

  if ($opts{mask}) {
    unless ($opts{mask}{IMG}) {
      $self->_set_error("compose: mask parameter empty image");
      return;
    }

    my $mask_left = $opts{mask_left};
    defined $mask_left or $mask_left = $opts{mask_minx};
    defined $mask_left or $mask_left = 0;
    
    my $mask_top = $opts{mask_top};
    defined $mask_top or $mask_top = $opts{mask_miny};
    defined $mask_top or $mask_top = 0;

    unless (i_compose_mask($self->{IMG}, $src->{IMG}, $opts{mask}{IMG}, 
		   $left, $top, $src_left, $src_top,
		   $mask_left, $mask_top, $width, $height, 
			   $combine, $opts{opacity})) {
      $self->_set_error(Imager->_error_as_msg);
      return;
    }
  }
  else {
    unless (i_compose($self->{IMG}, $src->{IMG}, $left, $top, $src_left, $src_top,
		      $width, $height, $combine, $opts{opacity})) {
      $self->_set_error(Imager->_error_as_msg);
      return;
    }
  }

  return $self;
}

sub flip {
  my $self  = shift;
  my %opts  = @_;
  my %xlate = (h=>0, v=>1, hv=>2, vh=>2);
  my $dir;
  return () unless defined $opts{'dir'} and defined $xlate{$opts{'dir'}};
  $dir = $xlate{$opts{'dir'}};
  return $self if i_flipxy($self->{IMG}, $dir);
  return ();
}

sub rotate {
  my $self = shift;
  my %opts = @_;

  unless (defined wantarray) {
    my @caller = caller;
    warn "rotate() called in void context - rotate() returns the rotated image at $caller[1] line $caller[2]\n";
    return;
  }

  if (defined $opts{right}) {
    my $degrees = $opts{right};
    if ($degrees < 0) {
      $degrees += 360 * int(((-$degrees)+360)/360);
    }
    $degrees = $degrees % 360;
    if ($degrees == 0) {
      return $self->copy();
    }
    elsif ($degrees == 90 || $degrees == 180 || $degrees == 270) {
      my $result = Imager->new();
      if ($result->{IMG} = i_rotate90($self->{IMG}, $degrees)) {
        return $result;
      }
      else {
        $self->{ERRSTR} = $self->_error_as_msg();
        return undef;
      }
    }
    else {
      $self->{ERRSTR} = "Parameter 'right' must be a multiple of 90 degrees";
      return undef;
    }
  }
  elsif (defined $opts{radians} || defined $opts{degrees}) {
    my $amount = $opts{radians} || $opts{degrees} * 3.1415926535 / 180;

    my $back = $opts{back};
    my $result = Imager->new;
    if ($back) {
      $back = _color($back);
      unless ($back) {
        $self->_set_error(Imager->errstr);
        return undef;
      }

      $result->{IMG} = i_rotate_exact($self->{IMG}, $amount, $back);
    }
    else {
      $result->{IMG} = i_rotate_exact($self->{IMG}, $amount);
    }
    if ($result->{IMG}) {
      return $result;
    }
    else {
      $self->{ERRSTR} = $self->_error_as_msg();
      return undef;
    }
  }
  else {
    $self->{ERRSTR} = "Only the 'right', 'radians' and 'degrees' parameters are available";
    return undef;
  }
}

sub matrix_transform {
  my $self = shift;
  my %opts = @_;

  unless (defined wantarray) {
    my @caller = caller;
    warn "copy() called in void context - copy() returns the copied image at $caller[1] line $caller[2]\n";
    return;
  }

  if ($opts{matrix}) {
    my $xsize = $opts{xsize} || $self->getwidth;
    my $ysize = $opts{ysize} || $self->getheight;

    my $result = Imager->new;
    if ($opts{back}) {
      $result->{IMG} = i_matrix_transform($self->{IMG}, $xsize, $ysize, 
					  $opts{matrix}, $opts{back})
	or return undef;
    }
    else {
      $result->{IMG} = i_matrix_transform($self->{IMG}, $xsize, $ysize, 
					  $opts{matrix})
	or return undef;
    }

    return $result;
  }
  else {
    $self->{ERRSTR} = "matrix parameter required";
    return undef;
  }
}

# blame Leolo :)
*yatf = \&matrix_transform;

# These two are supported for legacy code only

sub i_color_new {
  return Imager::Color->new(@_);
}

sub i_color_set {
  return Imager::Color::set(@_);
}

# Draws a box between the specified corner points.
sub box {
  my $self=shift;
  my $raw = $self->{IMG};

  unless ($raw) {
    $self->{ERRSTR}='empty input image';
    return undef;
  }

  my %opts = @_;

  my ($xmin, $ymin, $xmax, $ymax);
  if (exists $opts{'box'}) { 
    $xmin = _min($opts{'box'}->[0],$opts{'box'}->[2]);
    $xmax = _max($opts{'box'}->[0],$opts{'box'}->[2]);
    $ymin = _min($opts{'box'}->[1],$opts{'box'}->[3]);
    $ymax = _max($opts{'box'}->[1],$opts{'box'}->[3]);
  }
  else {
    defined($xmin = $opts{xmin}) or $xmin = 0;
    defined($xmax = $opts{xmax}) or $xmax = $self->getwidth()-1;
    defined($ymin = $opts{ymin}) or $ymin = 0;
    defined($ymax = $opts{ymax}) or $ymax = $self->getheight()-1;
  }

  if ($opts{filled}) { 
    my $color = $opts{'color'};

    if (defined $color) {
      unless (_is_color_object($color)) {
	$color = _color($color);
	unless ($color) { 
	  $self->{ERRSTR} = $Imager::ERRSTR; 
	  return;
	}
      }
    }
    else {
      $color = i_color_new(255,255,255,255);
    }

    if ($color->isa("Imager::Color")) {
      i_box_filled($raw, $xmin, $ymin,$xmax, $ymax, $color);
    }
    else {
      i_box_filledf($raw, $xmin, $ymin,$xmax, $ymax, $color);
    }
  }
  elsif ($opts{fill}) {
    unless (UNIVERSAL::isa($opts{fill}, 'Imager::Fill')) {
      # assume it's a hash ref
      require 'Imager/Fill.pm';
      unless ($opts{fill} = Imager::Fill->new(%{$opts{fill}})) {
        $self->{ERRSTR} = $Imager::ERRSTR;
        return undef;
      }
    }
    i_box_cfill($raw, $xmin, $ymin, $xmax, $ymax, $opts{fill}{fill});
  }
  else {
    my $color = $opts{'color'};
    if (defined $color) {
      unless (_is_color_object($color)) {
	$color = _color($color);
	unless ($color) { 
	  $self->{ERRSTR} = $Imager::ERRSTR;
	  return;
	}
      }
    }
    else {
      $color = i_color_new(255, 255, 255, 255);
    }
    unless ($color) { 
      $self->{ERRSTR} = $Imager::ERRSTR;
      return;
    }
    i_box($raw, $xmin, $ymin, $xmax, $ymax, $color);
  }

  return $self;
}

sub arc {
  my $self=shift;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }
  my $dflcl= [ 255, 255, 255, 255];
  my $good = 1;
  my %opts=
    (
     color=>$dflcl,
     'r'=>_min($self->getwidth(),$self->getheight())/3,
     'x'=>$self->getwidth()/2,
     'y'=>$self->getheight()/2,
     'd1'=>0, 'd2'=>361, 
     filled => 1,
     @_,
    );
  if ($opts{aa}) {
    if ($opts{fill}) {
      unless (UNIVERSAL::isa($opts{fill}, 'Imager::Fill')) {
	# assume it's a hash ref
	require 'Imager/Fill.pm';
	unless ($opts{fill} = Imager::Fill->new(%{$opts{fill}})) {
	  $self->{ERRSTR} = $Imager::ERRSTR;
	  return;
	}
      }
      i_arc_aa_cfill($self->{IMG},$opts{'x'},$opts{'y'},$opts{'r'},$opts{'d1'},
		     $opts{'d2'}, $opts{fill}{fill});
    }
    elsif ($opts{filled}) {
      my $color = _color($opts{'color'});
      unless ($color) { 
	$self->{ERRSTR} = $Imager::ERRSTR; 
	return; 
      }
      if ($opts{d1} == 0 && $opts{d2} == 361 && $opts{aa}) {
	i_circle_aa($self->{IMG}, $opts{'x'}, $opts{'y'}, $opts{'r'}, 
		    $color);
      }
      else {
	i_arc_aa($self->{IMG},$opts{'x'},$opts{'y'},$opts{'r'},
		 $opts{'d1'}, $opts{'d2'}, $color); 
      }
    }
    else {
      my $color = _color($opts{'color'});
      if ($opts{d2} - $opts{d1} >= 360) {
	$good = i_circle_out_aa($self->{IMG}, $opts{'x'}, $opts{'y'}, $opts{'r'}, $color);
      }
      else {
	$good = i_arc_out_aa($self->{IMG}, $opts{'x'}, $opts{'y'}, $opts{'r'}, $opts{'d1'}, $opts{'d2'}, $color);
      }
    }
  }
  else {
    if ($opts{fill}) {
      unless (UNIVERSAL::isa($opts{fill}, 'Imager::Fill')) {
	# assume it's a hash ref
	require 'Imager/Fill.pm';
	unless ($opts{fill} = Imager::Fill->new(%{$opts{fill}})) {
	  $self->{ERRSTR} = $Imager::ERRSTR;
	  return;
	}
      }
      i_arc_cfill($self->{IMG},$opts{'x'},$opts{'y'},$opts{'r'},$opts{'d1'},
		  $opts{'d2'}, $opts{fill}{fill});
    }
    else {
      my $color = _color($opts{'color'});
      unless ($color) { 
	$self->{ERRSTR} = $Imager::ERRSTR; 
	return;
      }
      if ($opts{filled}) {
	i_arc($self->{IMG},$opts{'x'},$opts{'y'},$opts{'r'},
	      $opts{'d1'}, $opts{'d2'}, $color); 
      }
      else {
	if ($opts{d1} == 0 && $opts{d2} == 361) {
	  $good = i_circle_out($self->{IMG}, $opts{x}, $opts{y}, $opts{r}, $color);
	}
	else {
	  $good = i_arc_out($self->{IMG}, $opts{x}, $opts{y}, $opts{r}, $opts{d1}, $opts{d2}, $color);
	}
      }
    }
  }
  unless ($good) {
    $self->_set_error($self->_error_as_msg);
    return;
  }

  return $self;
}

# Draws a line from one point to the other
# the endpoint is set if the endp parameter is set which it is by default.
# to turn of the endpoint being set use endp=>0 when calling line.

sub line {
  my $self=shift;
  my $dflcl=i_color_new(0,0,0,0);
  my %opts=(color=>$dflcl,
	    endp => 1,
	    @_);
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  unless (exists $opts{x1} and exists $opts{y1}) { $self->{ERRSTR}='missing begining coord'; return undef; }
  unless (exists $opts{x2} and exists $opts{y2}) { $self->{ERRSTR}='missing ending coord'; return undef; }

  my $color = _color($opts{'color'});
  unless ($color) {
    $self->{ERRSTR} = $Imager::ERRSTR;
    return;
  }

  $opts{antialias} = $opts{aa} if defined $opts{aa};
  if ($opts{antialias}) {
    i_line_aa($self->{IMG},$opts{x1}, $opts{y1}, $opts{x2}, $opts{y2},
              $color, $opts{endp});
  } else {
    i_line($self->{IMG},$opts{x1}, $opts{y1}, $opts{x2}, $opts{y2},
           $color, $opts{endp});
  }
  return $self;
}

# Draws a line between an ordered set of points - It more or less just transforms this
# into a list of lines.

sub polyline {
  my $self=shift;
  my ($pt,$ls,@points);
  my $dflcl=i_color_new(0,0,0,0);
  my %opts=(color=>$dflcl,@_);

  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  if (exists($opts{points})) { @points=@{$opts{points}}; }
  if (!exists($opts{points}) and exists($opts{'x'}) and exists($opts{'y'}) ) {
    @points=map { [ $opts{'x'}->[$_],$opts{'y'}->[$_] ] } (0..(scalar @{$opts{'x'}}-1));
    }

#  print Dumper(\@points);

  my $color = _color($opts{'color'});
  unless ($color) { 
    $self->{ERRSTR} = $Imager::ERRSTR; 
    return; 
  }
  $opts{antialias} = $opts{aa} if defined $opts{aa};
  if ($opts{antialias}) {
    for $pt(@points) {
      if (defined($ls)) { 
        i_line_aa($self->{IMG},$ls->[0],$ls->[1],$pt->[0],$pt->[1],$color, 1);
      }
      $ls=$pt;
    }
  } else {
    for $pt(@points) {
      if (defined($ls)) { 
        i_line($self->{IMG},$ls->[0],$ls->[1],$pt->[0],$pt->[1],$color,1);
      }
      $ls=$pt;
    }
  }
  return $self;
}

sub polygon {
  my $self = shift;
  my ($pt,$ls,@points);
  my $dflcl = i_color_new(0,0,0,0);
  my %opts = (color=>$dflcl, @_);

  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  if (exists($opts{points})) {
    $opts{'x'} = [ map { $_->[0] } @{$opts{points}} ];
    $opts{'y'} = [ map { $_->[1] } @{$opts{points}} ];
  }

  if (!exists $opts{'x'} or !exists $opts{'y'})  {
    $self->{ERRSTR} = 'no points array, or x and y arrays.'; return undef;
  }

  if ($opts{'fill'}) {
    unless (UNIVERSAL::isa($opts{'fill'}, 'Imager::Fill')) {
      # assume it's a hash ref
      require 'Imager/Fill.pm';
      unless ($opts{'fill'} = Imager::Fill->new(%{$opts{'fill'}})) {
        $self->{ERRSTR} = $Imager::ERRSTR;
        return undef;
      }
    }
    i_poly_aa_cfill($self->{IMG}, $opts{'x'}, $opts{'y'}, 
                    $opts{'fill'}{'fill'});
  }
  else {
    my $color = _color($opts{'color'});
    unless ($color) { 
      $self->{ERRSTR} = $Imager::ERRSTR; 
      return; 
    }
    i_poly_aa($self->{IMG}, $opts{'x'}, $opts{'y'}, $color);
  }

  return $self;
}


# this the multipoint bezier curve
# this is here more for testing that actual usage since
# this is not a good algorithm.  Usually the curve would be
# broken into smaller segments and each done individually.

sub polybezier {
  my $self=shift;
  my ($pt,$ls,@points);
  my $dflcl=i_color_new(0,0,0,0);
  my %opts=(color=>$dflcl,@_);

  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  if (exists $opts{points}) {
    $opts{'x'}=map { $_->[0]; } @{$opts{'points'}};
    $opts{'y'}=map { $_->[1]; } @{$opts{'points'}};
  }

  unless ( @{$opts{'x'}} and @{$opts{'x'}} == @{$opts{'y'}} ) {
    $self->{ERRSTR}='Missing or invalid points.';
    return;
  }

  my $color = _color($opts{'color'});
  unless ($color) { 
    $self->{ERRSTR} = $Imager::ERRSTR; 
    return; 
  }
  i_bezier_multi($self->{IMG},$opts{'x'},$opts{'y'},$color);
  return $self;
}

sub flood_fill {
  my $self = shift;
  my %opts = ( color=>Imager::Color->new(255, 255, 255), @_ );
  my $rc;

  unless (exists $opts{'x'} && exists $opts{'y'}) {
    $self->{ERRSTR} = "missing seed x and y parameters";
    return undef;
  }

  if ($opts{border}) {
    my $border = _color($opts{border});
    unless ($border) {
      $self->_set_error($Imager::ERRSTR);
      return;
    }
    if ($opts{fill}) {
      unless (UNIVERSAL::isa($opts{fill}, 'Imager::Fill')) {
	# assume it's a hash ref
	require Imager::Fill;
	unless ($opts{fill} = Imager::Fill->new(%{$opts{fill}})) {
	  $self->{ERRSTR} = $Imager::ERRSTR;
	  return;
	}
      }
      $rc = i_flood_cfill_border($self->{IMG}, $opts{'x'}, $opts{'y'}, 
				 $opts{fill}{fill}, $border);
    }
    else {
      my $color = _color($opts{'color'});
      unless ($color) {
	$self->{ERRSTR} = $Imager::ERRSTR;
	return;
      }
      $rc = i_flood_fill_border($self->{IMG}, $opts{'x'}, $opts{'y'}, 
				$color, $border);
    }
    if ($rc) { 
      return $self; 
    } 
    else { 
      $self->{ERRSTR} = $self->_error_as_msg(); 
      return;
    }
  }
  else {
    if ($opts{fill}) {
      unless (UNIVERSAL::isa($opts{fill}, 'Imager::Fill')) {
	# assume it's a hash ref
	require 'Imager/Fill.pm';
	unless ($opts{fill} = Imager::Fill->new(%{$opts{fill}})) {
	  $self->{ERRSTR} = $Imager::ERRSTR;
	  return;
	}
      }
      $rc = i_flood_cfill($self->{IMG}, $opts{'x'}, $opts{'y'}, $opts{fill}{fill});
    }
    else {
      my $color = _color($opts{'color'});
      unless ($color) {
	$self->{ERRSTR} = $Imager::ERRSTR;
	return;
      }
      $rc = i_flood_fill($self->{IMG}, $opts{'x'}, $opts{'y'}, $color);
    }
    if ($rc) { 
      return $self; 
    } 
    else { 
      $self->{ERRSTR} = $self->_error_as_msg(); 
      return;
    }
  } 
}

sub setpixel {
  my ($self, %opts) = @_;

  $self->_valid_image("setpixel")
    or return;

  my $color = $opts{color};
  unless (defined $color) {
    $color = $self->{fg};
    defined $color or $color = NC(255, 255, 255);
  }

  unless (ref $color && UNIVERSAL::isa($color, "Imager::Color")) {
    unless ($color = _color($color, 'setpixel')) {
      $self->_set_error("setpixel: " . Imager->errstr);
      return;
    }
  }

  unless (exists $opts{'x'} && exists $opts{'y'}) {
    $self->_set_error('setpixel: missing x or y parameter');
    return;
  }

  my $x = $opts{'x'};
  my $y = $opts{'y'};
  if (ref $x || ref $y) {
    $x = ref $x ? $x : [ $x ];
    $y = ref $y ? $y : [ $y ];
    unless (@$x) {
      $self->_set_error("setpixel: x is a reference to an empty array");
      return;
    }
    unless (@$y) {
      $self->_set_error("setpixel: y is a reference to an empty array");
      return;
    }

    # make both the same length, replicating the last element
    if (@$x < @$y) {
      $x = [ @$x, ($x->[-1]) x (@$y - @$x) ];
    }
    elsif (@$y < @$x) {
      $y = [ @$y, ($y->[-1]) x (@$x - @$y) ];
    }

    my $set = 0;
    if ($color->isa('Imager::Color')) {
      for my $i (0..$#$x) {
        i_ppix($self->{IMG}, $x->[$i], $y->[$i], $color)
	  or ++$set;
      }
    }
    else {
      for my $i (0..$#$x) {
        i_ppixf($self->{IMG}, $x->[$i], $y->[$i], $color)
	  or ++$set;
      }
    }

    return $set;
  }
  else {
    if ($color->isa('Imager::Color')) {
      i_ppix($self->{IMG}, $x, $y, $color)
	and return;
    }
    else {
      i_ppixf($self->{IMG}, $x, $y, $color)
	and return;
    }
  }

  return $self;
}

sub getpixel {
  my $self = shift;

  my %opts = ( "type"=>'8bit', @_);

  $self->_valid_image("getpixel")
    or return;

  unless (exists $opts{'x'} && exists $opts{'y'}) {
    $self->_set_error('getpixel: missing x or y parameter');
    return;
  }

  my $x = $opts{'x'};
  my $y = $opts{'y'};
  my $type = $opts{'type'};
  if (ref $x || ref $y) {
    $x = ref $x ? $x : [ $x ];
    $y = ref $y ? $y : [ $y ];
    unless (@$x) {
      $self->_set_error("getpixel: x is a reference to an empty array");
      return;
    }
    unless (@$y) {
      $self->_set_error("getpixel: y is a reference to an empty array");
      return;
    }

    # make both the same length, replicating the last element
    if (@$x < @$y) {
      $x = [ @$x, ($x->[-1]) x (@$y - @$x) ];
    }
    elsif (@$y < @$x) {
      $y = [ @$y, ($y->[-1]) x (@$x - @$y) ];
    }

    my @result;
    if ($type eq '8bit') {
      for my $i (0..$#$x) {
        push(@result, i_get_pixel($self->{IMG}, $x->[$i], $y->[$i]));
      }
    }
    elsif ($type eq 'float' || $type eq 'double') {
      for my $i (0..$#$x) {
        push(@result, i_gpixf($self->{IMG}, $x->[$i], $y->[$i]));
      }
    }
    else {
      $self->_set_error("getpixel: type must be '8bit' or 'float'");
      return;
    }
    return wantarray ? @result : \@result;
  }
  else {
    if ($type eq '8bit') {
      return i_get_pixel($self->{IMG}, $x, $y);
    }
    elsif ($type eq 'float' || $type eq 'double') {
      return i_gpixf($self->{IMG}, $x, $y);
    }
    else {
      $self->_set_error("getpixel: type must be '8bit' or 'float'");
      return;
    }
  }
}

sub getscanline {
  my $self = shift;
  my %opts = ( type => '8bit', x=>0, @_);

  $self->_valid_image or return;

  defined $opts{width} or $opts{width} = $self->getwidth - $opts{x};

  unless (defined $opts{'y'}) {
    $self->_set_error("missing y parameter");
    return;
  }

  if ($opts{type} eq '8bit') {
    return i_glin($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
		  $opts{'y'});
  }
  elsif ($opts{type} eq 'float') {
    return i_glinf($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
		  $opts{'y'});
  }
  elsif ($opts{type} eq 'index') {
    unless (i_img_type($self->{IMG})) {
      $self->_set_error("type => index only valid on paletted images");
      return;
    }
    return i_gpal($self->{IMG}, $opts{x}, $opts{x} + $opts{width},
                  $opts{'y'});
  }
  else {
    $self->_set_error("invalid type parameter - must be '8bit' or 'float'");
    return;
  }
}

sub setscanline {
  my $self = shift;
  my %opts = ( x=>0, @_);

  $self->_valid_image or return;

  unless (defined $opts{'y'}) {
    $self->_set_error("missing y parameter");
    return;
  }

  if (!$opts{type}) {
    if (ref $opts{pixels} && @{$opts{pixels}}) {
      # try to guess the type
      if ($opts{pixels}[0]->isa('Imager::Color')) {
	$opts{type} = '8bit';
      }
      elsif ($opts{pixels}[0]->isa('Imager::Color::Float')) {
	$opts{type} = 'float';
      }
      else {
	$self->_set_error("missing type parameter and could not guess from pixels");
	return;
      }
    }
    else {
      # default
      $opts{type} = '8bit';
    }
  }

  if ($opts{type} eq '8bit') {
    if (ref $opts{pixels}) {
      return i_plin($self->{IMG}, $opts{x}, $opts{'y'}, @{$opts{pixels}});
    }
    else {
      return i_plin($self->{IMG}, $opts{x}, $opts{'y'}, $opts{pixels});
    }
  }
  elsif ($opts{type} eq 'float') {
    if (ref $opts{pixels}) {
      return i_plinf($self->{IMG}, $opts{x}, $opts{'y'}, @{$opts{pixels}});
    }
    else {
      return i_plinf($self->{IMG}, $opts{x}, $opts{'y'}, $opts{pixels});
    }
  }
  elsif ($opts{type} eq 'index') {
    if (ref $opts{pixels}) {
      return i_ppal($self->{IMG}, $opts{x}, $opts{'y'}, @{$opts{pixels}});
    }
    else {
      return i_ppal_p($self->{IMG}, $opts{x}, $opts{'y'}, $opts{pixels});
    }
  }
  else {
    $self->_set_error("invalid type parameter - must be '8bit' or 'float'");
    return;
  }
}

sub getsamples {
  my $self = shift;
  my %opts = ( type => '8bit', x=>0, offset => 0, @_);

  defined $opts{width} or $opts{width} = $self->getwidth - $opts{x};

  unless (defined $opts{'y'}) {
    $self->_set_error("missing y parameter");
    return;
  }
  
  if ($opts{target}) {
    my $target = $opts{target};
    my $offset = $opts{offset};
    if ($opts{type} eq '8bit') {
      my @samples = i_gsamp($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
			    $opts{y}, $opts{channels})
	or return;
      @{$target}[$offset .. $offset + @samples - 1] = @samples;
      return scalar(@samples);
    }
    elsif ($opts{type} eq 'float') {
      my @samples = i_gsampf($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
			     $opts{y}, $opts{channels});
      @{$target}[$offset .. $offset + @samples - 1] = @samples;
      return scalar(@samples);
    }
    elsif ($opts{type} =~ /^(\d+)bit$/) {
      my $bits = $1;

      my @data;
      my $count = i_gsamp_bits($self->{IMG}, $opts{x}, $opts{x}+$opts{width}, 
			       $opts{y}, $bits, $target, 
			       $offset, $opts{channels});
      unless (defined $count) {
	$self->_set_error(Imager->_error_as_msg);
	return;
      }

      return $count;
    }
    else {
      $self->_set_error("invalid type parameter - must be '8bit' or 'float'");
      return;
    }
  }
  else {
    if ($opts{type} eq '8bit') {
      return i_gsamp($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
		     $opts{y}, $opts{channels});
    }
    elsif ($opts{type} eq 'float') {
      return i_gsampf($self->{IMG}, $opts{x}, $opts{x}+$opts{width},
		      $opts{y}, $opts{channels});
    }
    elsif ($opts{type} =~ /^(\d+)bit$/) {
      my $bits = $1;

      my @data;
      i_gsamp_bits($self->{IMG}, $opts{x}, $opts{x}+$opts{width}, 
		   $opts{y}, $bits, \@data, 0, $opts{channels})
	or return;
      return @data;
    }
    else {
      $self->_set_error("invalid type parameter - must be '8bit' or 'float'");
      return;
    }
  }
}

sub setsamples {
  my $self = shift;
  my %opts = ( x => 0, offset => 0, @_ );

  unless ($self->{IMG}) {
    $self->_set_error('setsamples: empty input image');
    return;
  }

  my $data = $opts{data};
  unless(defined $data) {
    $self->_set_error('setsamples: data parameter missing');
    return;
  }

  my $type = $opts{type};
  defined $type or $type = '8bit';

  my $width = defined $opts{width} ? $opts{width}
    : $self->getwidth() - $opts{x};

  my $count;
  if ($type eq '8bit') {
    $count = i_psamp($self->{IMG}, $opts{x}, $opts{y}, $opts{channels},
		     $data, $opts{offset}, $width);
  }
  elsif ($type eq 'float') {
    $count = i_psampf($self->{IMG}, $opts{x}, $opts{y}, $opts{channels},
		      $data, $opts{offset}, $width);
  }
  elsif ($type =~ /^([0-9]+)bit$/) {
    my $bits = $1;

    unless (ref $data) {
      $self->_set_error("setsamples: data must be an array ref for type not 8bit or float");
      return;
    }

    $count = i_psamp_bits($self->{IMG}, $opts{x}, $opts{y}, $bits,
			  $opts{channels}, $data, $opts{offset}, 
			  $width);
  }
  else {
    $self->_set_error('setsamples: type parameter invalid');
    return;
  }

  unless (defined $count) {
    $self->_set_error(Imager->_error_as_msg);
    return;
  }

  return $count;
}

# make an identity matrix of the given size
sub _identity {
  my ($size) = @_;

  my $matrix = [ map { [ (0) x $size ] } 1..$size ];
  for my $c (0 .. ($size-1)) {
    $matrix->[$c][$c] = 1;
  }
  return $matrix;
}

# general function to convert an image
sub convert {
  my ($self, %opts) = @_;
  my $matrix;

  unless (defined wantarray) {
    my @caller = caller;
    warn "convert() called in void context - convert() returns the converted image at $caller[1] line $caller[2]\n";
    return;
  }

  # the user can either specify a matrix or preset
  # the matrix overrides the preset
  if (!exists($opts{matrix})) {
    unless (exists($opts{preset})) {
      $self->{ERRSTR} = "convert() needs a matrix or preset";
      return;
    }
    else {
      if ($opts{preset} eq 'gray' || $opts{preset} eq 'grey') {
	# convert to greyscale, keeping the alpha channel if any
	if ($self->getchannels == 3) {
	  $matrix = [ [ 0.222, 0.707, 0.071 ] ];
	}
	elsif ($self->getchannels == 4) {
	  # preserve the alpha channel
	  $matrix = [ [ 0.222, 0.707, 0.071, 0 ],
		      [ 0,     0,     0,     1 ] ];
	}
	else {
	  # an identity
	  $matrix = _identity($self->getchannels);
	}
      }
      elsif ($opts{preset} eq 'noalpha') {
	# strip the alpha channel
	if ($self->getchannels == 2 or $self->getchannels == 4) {
	  $matrix = _identity($self->getchannels);
	  pop(@$matrix); # lose the alpha entry
	}
	else {
	  $matrix = _identity($self->getchannels);
	}
      }
      elsif ($opts{preset} eq 'red' || $opts{preset} eq 'channel0') {
	# extract channel 0
	$matrix = [ [ 1 ] ];
      }
      elsif ($opts{preset} eq 'green' || $opts{preset} eq 'channel1') {
	$matrix = [ [ 0, 1 ] ];
      }
      elsif ($opts{preset} eq 'blue' || $opts{preset} eq 'channel2') {
	$matrix = [ [ 0, 0, 1 ] ];
      }
      elsif ($opts{preset} eq 'alpha') {
	if ($self->getchannels == 2 or $self->getchannels == 4) {
	  $matrix = [ [ (0) x ($self->getchannels-1), 1 ] ];
	}
	else {
	  # the alpha is just 1 <shrug>
	  $matrix = [ [ (0) x $self->getchannels, 1 ] ];
	}
      }
      elsif ($opts{preset} eq 'rgb') {
	if ($self->getchannels == 1) {
	  $matrix = [ [ 1 ], [ 1 ], [ 1 ] ];
	}
	elsif ($self->getchannels == 2) {
	  # preserve the alpha channel
	  $matrix = [ [ 1, 0 ], [ 1, 0 ], [ 1, 0 ], [ 0, 1 ] ];
	}
	else {
	  $matrix = _identity($self->getchannels);
	}
      }
      elsif ($opts{preset} eq 'addalpha') {
	if ($self->getchannels == 1) {
	  $matrix = _identity(2);
	}
	elsif ($self->getchannels == 3) {
	  $matrix = _identity(4);
	}
	else {
	  $matrix = _identity($self->getchannels);
	}
      }
      else {
	$self->{ERRSTR} = "Unknown convert preset $opts{preset}";
	return undef;
      }
    }
  }
  else {
    $matrix = $opts{matrix};
  }

  my $new = Imager->new;
  $new->{IMG} = i_convert($self->{IMG}, $matrix);
  unless ($new->{IMG}) {
    # most likely a bad matrix
    $self->{ERRSTR} = _error_as_msg();
    return undef;
  }
  return $new;
}

# combine channels from multiple input images, a class method
sub combine {
  my ($class, %opts) = @_;

  my $src = delete $opts{src};
  unless ($src) {
    $class->_set_error("src parameter missing");
    return;
  }
  my @imgs;
  my $index = 0;
  for my $img (@$src) {
    unless (eval { $img->isa("Imager") }) {
      $class->_set_error("src must contain image objects");
      return;
    }
    unless ($img->{IMG}) {
      $class->_set_error("empty input image");
      return;
    }
    push @imgs, $img->{IMG};
  }
  my $result;
  if (my $channels = delete $opts{channels}) {
    $result = i_combine(\@imgs, $channels);
  }
  else {
    $result = i_combine(\@imgs);
  }
  unless ($result) {
    $class->_set_error($class->_error_as_msg);
    return;
  }

  my $img = $class->new;
  $img->{IMG} = $result;

  return $img;
}


# general function to map an image through lookup tables

sub map {
  my ($self, %opts) = @_;
  my @chlist = qw( red green blue alpha );

  if (!exists($opts{'maps'})) {
    # make maps from channel maps
    my $chnum;
    for $chnum (0..$#chlist) {
      if (exists $opts{$chlist[$chnum]}) {
	$opts{'maps'}[$chnum] = $opts{$chlist[$chnum]};
      } elsif (exists $opts{'all'}) {
	$opts{'maps'}[$chnum] = $opts{'all'};
      }
    }
  }
  if ($opts{'maps'} and $self->{IMG}) {
    i_map($self->{IMG}, $opts{'maps'} );
  }
  return $self;
}

sub difference {
  my ($self, %opts) = @_;

  defined $opts{mindist} or $opts{mindist} = 0;

  defined $opts{other}
    or return $self->_set_error("No 'other' parameter supplied");
  defined $opts{other}{IMG}
    or return $self->_set_error("No image data in 'other' image");

  $self->{IMG}
    or return $self->_set_error("No image data");

  my $result = Imager->new;
  $result->{IMG} = i_diff_image($self->{IMG}, $opts{other}{IMG}, 
                                $opts{mindist})
    or return $self->_set_error($self->_error_as_msg());

  return $result;
}

# destructive border - image is shrunk by one pixel all around

sub border {
  my ($self,%opts)=@_;
  my($tx,$ty)=($self->getwidth()-1,$self->getheight()-1);
  $self->polyline('x'=>[0,$tx,$tx,0,0],'y'=>[0,0,$ty,$ty,0],%opts);
}


# Get the width of an image

sub getwidth {
  my $self = shift;

  if (my $raw = $self->{IMG}) {
    return i_img_get_width($raw);
  }
  else {
    $self->{ERRSTR} = 'image is empty'; return undef;
  }
}

# Get the height of an image

sub getheight {
  my $self = shift;

  if (my $raw = $self->{IMG}) {
    return i_img_get_height($raw);
  }
  else {
    $self->{ERRSTR} = 'image is empty'; return undef;
  }
}

# Get number of channels in an image

sub getchannels {
  my $self = shift;
  if (!defined($self->{IMG})) { $self->{ERRSTR} = 'image is empty'; return undef; }
  return i_img_getchannels($self->{IMG});
}

# Get channel mask

sub getmask {
  my $self = shift;
  if (!defined($self->{IMG})) { $self->{ERRSTR} = 'image is empty'; return undef; }
  return i_img_getmask($self->{IMG});
}

# Set channel mask

sub setmask {
  my $self = shift;
  my %opts = @_;
  if (!defined($self->{IMG})) { 
    $self->{ERRSTR} = 'image is empty';
    return undef;
  }
  unless (defined $opts{mask}) {
    $self->_set_error("mask parameter required");
    return;
  }
  i_img_setmask( $self->{IMG} , $opts{mask} );

  1;
}

# Get number of colors in an image

sub getcolorcount {
  my $self=shift;
  my %opts=('maxcolors'=>2**30,@_);
  if (!defined($self->{IMG})) { $self->{ERRSTR}='image is empty'; return undef; }
  my $rc=i_count_colors($self->{IMG},$opts{'maxcolors'});
  return ($rc==-1? undef : $rc);
}

# Returns a reference to a hash. The keys are colour named (packed) and the
# values are the number of pixels in this colour.
sub getcolorusagehash {
  my $self = shift;
  
  my %opts = ( maxcolors => 2**30, @_ );
  my $max_colors = $opts{maxcolors};
  unless (defined $max_colors && $max_colors > 0) {
    $self->_set_error('maxcolors must be a positive integer');
    return;
  }

  unless (defined $self->{IMG}) {
    $self->_set_error('empty input image'); 
    return;
  }

  my $channels= $self->getchannels;
  # We don't want to look at the alpha channel, because some gifs using it
  # doesn't define it for every colour (but only for some)
  $channels -= 1 if $channels == 2 or $channels == 4;
  my %color_use;
  my $height = $self->getheight;
  for my $y (0 .. $height - 1) {
    my $colors = $self->getsamples('y' => $y, channels => [ 0 .. $channels - 1 ]);
    while (length $colors) {
      $color_use{ substr($colors, 0, $channels, '') }++;
    }
    keys %color_use > $max_colors
      and return;
  }
  return \%color_use;
}

# This will return a ordered array of the colour usage. Kind of the sorted
# version of the values of the hash returned by getcolorusagehash.
# You might want to add safety checks and change the names, etc...
sub getcolorusage {
  my $self = shift;

  my %opts = ( maxcolors => 2**30, @_ );
  my $max_colors = $opts{maxcolors};
  unless (defined $max_colors && $max_colors > 0) {
    $self->_set_error('maxcolors must be a positive integer');
    return;
  }

  unless (defined $self->{IMG}) {
    $self->_set_error('empty input image'); 
    return undef;
  }

  return i_get_anonymous_color_histo($self->{IMG}, $max_colors);
}

# draw string to an image

sub string {
  my $self = shift;
  unless ($self->{IMG}) { $self->{ERRSTR}='empty input image'; return undef; }

  my %input=('x'=>0, 'y'=>0, @_);
  defined($input{string}) or $input{string} = $input{text};

  unless(defined $input{string}) {
    $self->{ERRSTR}="missing required parameter 'string'";
    return;
  }

  unless($input{font}) {
    $self->{ERRSTR}="missing required parameter 'font'";
    return;
  }

  unless ($input{font}->draw(image=>$self, %input)) {
    return;
  }

  return $self;
}

sub align_string {
  my $self = shift;

  my $img;
  if (ref $self) {
    unless ($self->{IMG}) { 
      $self->{ERRSTR}='empty input image'; 
      return;
    }
    $img = $self;
  }
  else {
    $img = undef;
  }

  my %input=('x'=>0, 'y'=>0, @_);
  defined $input{string}
    or $input{string} = $input{text};

  unless(exists $input{string}) {
    $self->_set_error("missing required parameter 'string'");
    return;
  }

  unless($input{font}) {
    $self->_set_error("missing required parameter 'font'");
    return;
  }

  my @result;
  unless (@result = $input{font}->align(image=>$img, %input)) {
    return;
  }

  return wantarray ? @result : $result[0];
}

my @file_limit_names = qw/width height bytes/;

sub set_file_limits {
  shift;

  my %opts = @_;
  my %values;
  
  if ($opts{reset}) {
    @values{@file_limit_names} = (0) x @file_limit_names;
  }
  else {
    @values{@file_limit_names} = i_get_image_file_limits();
  }

  for my $key (keys %values) {
    defined $opts{$key} and $values{$key} = $opts{$key};
  }

  i_set_image_file_limits($values{width}, $values{height}, $values{bytes});
}

sub get_file_limits {
  i_get_image_file_limits();
}

# Shortcuts that can be exported

sub newcolor { Imager::Color->new(@_); }
sub newfont  { Imager::Font->new(@_); }
sub NCF {
  require Imager::Color::Float;
  return Imager::Color::Float->new(@_);
}

*NC=*newcolour=*newcolor;
*NF=*newfont;

*open=\&read;
*circle=\&arc;


#### Utility routines

sub errstr { 
  ref $_[0] ? $_[0]->{ERRSTR} : $ERRSTR
}

sub _set_error {
  my ($self, $msg) = @_;

  if (ref $self) {
    $self->{ERRSTR} = $msg;
  }
  else {
    $ERRSTR = $msg;
  }
  return;
}

# Default guess for the type of an image from extension

my @simple_types = qw(png tga gif raw ico cur xpm mng jng ilbm pcx psd eps);

my %ext_types =
  (
   ( map { $_ => $_ } @simple_types ),
   tiff => "tiff",
   tif => "tiff",
   pbm => "pnm",
   pgm => "pnm",
   ppm => "pnm",
   pnm => "pnm", # technically wrong, but historically it works in Imager
   jpeg => "jpeg",
   jpg => "jpeg",
   bmp => "bmp",
   dib => "bmp",
   rgb => "sgi",
   bw => "sgi",
   sgi => "sgi",
   fit => "fits",
   fits => "fits",
   rle => "utah",
  );

sub def_guess_type {
  my $name=lc(shift);

  my ($ext) = $name =~ /\.([^.]+)$/
    or return;

  my $type = $ext_types{$ext}
    or return;

  return $type;
}

sub combines {
  return @combine_types;
}

# get the minimum of a list

sub _min {
  my $mx=shift;
  for(@_) { if ($_<$mx) { $mx=$_; }}
  return $mx;
}

# get the maximum of a list

sub _max {
  my $mx=shift;
  for(@_) { if ($_>$mx) { $mx=$_; }}
  return $mx;
}

# string stuff for iptc headers

sub _clean {
  my($str)=$_[0];
  $str = substr($str,3);
  $str =~ s/[\n\r]//g;
  $str =~ s/\s+/ /g;
  $str =~ s/^\s//;
  $str =~ s/\s$//;
  return $str;
}

# A little hack to parse iptc headers.

sub parseiptc {
  my $self=shift;
  my(@sar,$item,@ar);
  my($caption,$photogr,$headln,$credit);

  my $str=$self->{IPTCRAW};

  defined $str
    or return;

  @ar=split(/8BIM/,$str);

  my $i=0;
  foreach (@ar) {
    if (/^\004\004/) {
      @sar=split(/\034\002/);
      foreach $item (@sar) {
	if ($item =~ m/^x/) {
	  $caption = _clean($item);
	  $i++;
	}
	if ($item =~ m/^P/) {
	  $photogr = _clean($item);
	  $i++;
	}
	if ($item =~ m/^i/) {
	  $headln = _clean($item);
	  $i++;
	}
	if ($item =~ m/^n/) {
	  $credit = _clean($item);
	  $i++;
	}
      }
    }
  }
  return (caption=>$caption,photogr=>$photogr,headln=>$headln,credit=>$credit);
}

sub Inline {
  my ($lang) = @_;

  $lang eq 'C'
    or die "Only C language supported";

  require Imager::ExtUtils;
  return Imager::ExtUtils->inline_config;
}

# threads shouldn't try to close raw Imager objects
sub Imager::ImgRaw::CLONE_SKIP { 1 }

sub preload {
  # this serves two purposes:
  # - a class method to load the file support modules included with Imager
  #   (or were included, once the library dependent modules are split out)
  # - something for Module::ScanDeps to analyze
  # https://rt.cpan.org/Ticket/Display.html?id=6566
  local $@;
  eval { require Imager::File::GIF };
  eval { require Imager::File::JPEG };
  eval { require Imager::File::PNG };
  eval { require Imager::File::SGI };
  eval { require Imager::File::TIFF };
  eval { require Imager::File::ICO };
  eval { require Imager::Font::W32 };
  eval { require Imager::Font::FT2 };
  eval { require Imager::Font::T1 };
}

# backward compatibility for %formats
package Imager::FORMATS;
use strict;
use constant IX_FORMATS => 0;
use constant IX_LIST => 1;
use constant IX_INDEX => 2;
use constant IX_CLASSES => 3;

sub TIEHASH {
  my ($class, $formats, $classes) = @_;

  return bless [ $formats, [ ], 0, $classes ], $class;
}

sub _check {
  my ($self, $key) = @_;

  (my $file = $self->[IX_CLASSES]{$key} . ".pm") =~ s(::)(/)g;
  my $value;
  my $error;
  my $loaded = Imager::_load_file($file, \$error);
  if ($loaded) {
    $value = 1;
  }
  else {
    if ($error =~ /^Can't locate /) {
      $error = "Can't locate $file";
    }
    $reader_load_errors{$key} = $writer_load_errors{$key} = $error;
    $value = undef;
  }
  $self->[IX_FORMATS]{$key} = $value;

  return $value;
}

sub FETCH {
  my ($self, $key) = @_;

  exists $self->[IX_FORMATS]{$key} and return $self->[IX_FORMATS]{$key};

  $self->[IX_CLASSES]{$key} or return undef;

  return $self->_check($key);
}

sub STORE {
  die "%Imager::formats is not user monifiable";
}

sub DELETE {
  die "%Imager::formats is not user monifiable";
}

sub CLEAR {
  die "%Imager::formats is not user monifiable";
}

sub EXISTS {
  my ($self, $key) = @_;

  if (exists $self->[IX_FORMATS]{$key}) {
    my $value = $self->[IX_FORMATS]{$key}
      or return;
    return 1;
  }

  $self->_check($key) or return 1==0;

  return 1==1;
}

sub FIRSTKEY {
  my ($self) = @_;

  unless (@{$self->[IX_LIST]}) {
    # full populate it
    @{$self->[IX_LIST]} = grep $self->[IX_FORMATS]{$_},
      keys %{$self->[IX_FORMATS]};

    for my $key (keys %{$self->[IX_CLASSES]}) {
      $self->[IX_FORMATS]{$key} and next;
      $self->_check($key)
	and push @{$self->[IX_LIST]}, $key;
    }
  }

  @{$self->[IX_LIST]} or return;
  $self->[IX_INDEX] = 1;
  return $self->[IX_LIST][0];
}

sub NEXTKEY {
  my ($self) = @_;

  $self->[IX_INDEX] < @{$self->[IX_LIST]}
    or return;

  return $self->[IX_LIST][$self->[IX_INDEX]++];
}

sub SCALAR {
  my ($self) = @_;

  return scalar @{$self->[IX_LIST]};
}

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Imager - Perl extension for Generating 24 bit Images

=head1 SYNOPSIS

  # Thumbnail example

  #!/usr/bin/perl -w
  use strict;
  use Imager;

  die "Usage: thumbmake.pl filename\n" if !-f $ARGV[0];
  my $file = shift;

  my $format;

  # see Imager::Files for information on the read() method
  my $img = Imager->new(file=>$file)
    or die Imager->errstr();

  $file =~ s/\.[^.]*$//;

  # Create smaller version
  # documented in Imager::Transformations
  my $thumb = $img->scale(scalefactor=>.3);

  # Autostretch individual channels
  $thumb->filter(type=>'autolevels');

  # try to save in one of these formats
  SAVE:

  for $format ( qw( png gif jpeg tiff ppm ) ) {
    # Check if given format is supported
    if ($Imager::formats{$format}) {
      $file.="_low.$format";
      print "Storing image as: $file\n";
      # documented in Imager::Files
      $thumb->write(file=>$file) or
        die $thumb->errstr;
      last SAVE;
    }
  }

=head1 DESCRIPTION

Imager is a module for creating and altering images.  It can read and
write various image formats, draw primitive shapes like lines,and
polygons, blend multiple images together in various ways, scale, crop,
render text and more.

=head2 Overview of documentation

=over

=item *

Imager - This document - Synopsis, Example, Table of Contents and
Overview.

=item *

L<Imager::Tutorial> - a brief introduction to Imager.

=item *

L<Imager::Cookbook> - how to do various things with Imager.

=item *

L<Imager::ImageTypes> - Basics of constructing image objects with
C<new()>: Direct type/virtual images, RGB(A)/paletted images,
8/16/double bits/channel, color maps, channel masks, image tags, color
quantization.  Also discusses basic image information methods.

=item *

L<Imager::Files> - IO interaction, reading/writing images, format
specific tags.

=item *

L<Imager::Draw> - Drawing Primitives, lines, boxes, circles, arcs,
flood fill.

=item *

L<Imager::Color> - Color specification.

=item *

L<Imager::Fill> - Fill pattern specification.

=item *

L<Imager::Font> - General font rendering, bounding boxes and font
metrics.

=item *

L<Imager::Transformations> - Copying, scaling, cropping, flipping,
blending, pasting, convert and map.

=item *

L<Imager::Engines> - Programmable transformations through
C<transform()>, C<transform2()> and C<matrix_transform()>.

=item *

L<Imager::Filters> - Filters, sharpen, blur, noise, convolve etc. and
filter plug-ins.

=item *

L<Imager::Expr> - Expressions for evaluation engine used by
transform2().

=item *

L<Imager::Matrix2d> - Helper class for affine transformations.

=item *

L<Imager::Fountain> - Helper for making gradient profiles.

=item *

L<Imager::API> - using Imager's C API

=item *

L<Imager::APIRef> - API function reference

=item *

L<Imager::Inline> - using Imager's C API from Inline::C

=item *

L<Imager::ExtUtils> - tools to get access to Imager's C API.

=back

=head2 Basic Overview

An Image object is created with C<$img = Imager-E<gt>new()>.
Examples:

  $img=Imager->new();                         # create empty image
  $img->read(file=>'lena.png',type=>'png') or # read image from file
     die $img->errstr();                      # give an explanation
                                              # if something failed

or if you want to create an empty image:

  $img=Imager->new(xsize=>400,ysize=>300,channels=>4);

This example creates a completely black image of width 400 and height
300 and 4 channels.

=head1 ERROR HANDLING

In general a method will return false when it fails, if it does use
the C<errstr()> method to find out why:

=over

=item errstr()

Returns the last error message in that context.

If the last error you received was from calling an object method, such
as read, call errstr() as an object method to find out why:

  my $image = Imager->new;
  $image->read(file => 'somefile.gif')
     or die $image->errstr;

If it was a class method then call errstr() as a class method:

  my @imgs = Imager->read_multi(file => 'somefile.gif')
    or die Imager->errstr;

Note that in some cases object methods are implemented in terms of
class methods so a failing object method may set both.

=back

The C<Imager-E<gt>new> method is described in detail in
L<Imager::ImageTypes>.

=head1 METHOD INDEX

Where to find information on methods for Imager class objects.

addcolors() - L<Imager::ImageTypes/addcolors()> - add colors to a
paletted image

addtag() -  L<Imager::ImageTypes/addtag()> - add image tags

align_string() - L<Imager::Draw/align_string()> - draw text aligned on a
point

arc() - L<Imager::Draw/arc()> - draw a filled arc

bits() - L<Imager::ImageTypes/bits()> - number of bits per sample for the
image

box() - L<Imager::Draw/box()> - draw a filled or outline box.

circle() - L<Imager::Draw/circle()> - draw a filled circle

close_log() - L<Imager::ImageTypes/close_log()> - close the Imager
debugging log.

colorcount() - L<Imager::ImageTypes/colorcount()> - the number of
colors in an image's palette (paletted images only)

combine() - L<Imager::Transformations/combine()> - combine channels
from one or more images.

combines() - L<Imager::Draw/combines()> - return a list of the
different combine type keywords

compose() - L<Imager::Transformations/compose()> - compose one image
over another.

convert() - L<Imager::Transformations/convert()> - transform the color
space

copy() - L<Imager::Transformations/copy()> - make a duplicate of an
image

crop() - L<Imager::Transformations/crop()> - extract part of an image

def_guess_type() - L<Imager::Files/def_guess_type()> - default function
used to guess the output file format based on the output file name

deltag() -  L<Imager::ImageTypes/deltag()> - delete image tags

difference() - L<Imager::Filters/difference()> - produce a difference
images from two input images.

errstr() - L</errstr()> - the error from the last failed operation.

filter() - L<Imager::Filters/filter()> - image filtering

findcolor() - L<Imager::ImageTypes/findcolor()> - search the image
palette, if it has one

flip() - L<Imager::Transformations/flip()> - flip an image, vertically,
horizontally

flood_fill() - L<Imager::Draw/flood_fill()> - fill an enclosed or same
color area

getchannels() - L<Imager::ImageTypes/getchannels()> - the number of
samples per pixel for an image

getcolorcount() - L<Imager::ImageTypes/getcolorcount()> - the number of
different colors used by an image (works for direct color images)

getcolors() - L<Imager::ImageTypes/getcolors()> - get colors from the image
palette, if it has one

getcolorusage() - L<Imager::ImageTypes/getcolorusage()>

getcolorusagehash() - L<Imager::ImageTypes/getcolorusagehash()>

get_file_limits() - L<Imager::Files/get_file_limits()>

getheight() - L<Imager::ImageTypes/getheight()> - height of the image in
pixels

getmask() - L<Imager::ImageTypes/getmask()> - write mask for the image

getpixel() - L<Imager::Draw/getpixel()> - retrieve one or more pixel
colors

getsamples() - L<Imager::Draw/getsamples()> - retrieve samples from a
row or partial row of pixels.

getscanline() - L<Imager::Draw/getscanline()> - retrieve colors for a
row or partial row of pixels.

getwidth() - L<Imager::ImageTypes/getwidth()> - width of the image in
pixels.

img_set() - L<Imager::ImageTypes/img_set()> - re-use an Imager object
for a new image.

init() - L<Imager::ImageTypes/init()>

is_bilevel() - L<Imager::ImageTypes/is_bilevel()> - returns whether
image write functions should write the image in their bilevel (blank
and white, no gray levels) format

is_logging() L<Imager::ImageTypes/is_logging()> - test if the debug
log is active.

line() - L<Imager::Draw/line()> - draw an interval

load_plugin() - L<Imager::Filters/load_plugin()>

log() - L<Imager::ImageTypes/log()> - send a message to the debugging
log.

make_palette() - L<Imager::ImageTypes/make_palette()> - produce a
color palette from one or more input images.

map() - L<Imager::Transformations/map()> - remap color
channel values

masked() -  L<Imager::ImageTypes/masked()> - make a masked image

matrix_transform() - L<Imager::Engines/matrix_transform()>

maxcolors() - L<Imager::ImageTypes/maxcolors()>

NC() - L<Imager::Handy/NC()>

NCF() - L<Imager::Handy/NCF()>

new() - L<Imager::ImageTypes/new()>

newcolor() - L<Imager::Handy/newcolor()>

newcolour() - L<Imager::Handy/newcolour()>

newfont() - L<Imager::Handy/newfont()>

NF() - L<Imager::Handy/NF()>

open() - L<Imager::Files/read()> - an alias for read()

open_log() - L<Imager::ImageTypes/open_log()> - open the debug log.

=for stopwords IPTC

parseiptc() - L<Imager::Files/parseiptc()> - parse IPTC data from a JPEG
image

paste() - L<Imager::Transformations/paste()> - draw an image onto an
image

polygon() - L<Imager::Draw/polygon()>

polyline() - L<Imager::Draw/polyline()>

preload() - L<Imager::Files/preload()>

read() - L<Imager::Files/read()> - read a single image from an image file

read_multi() - L<Imager::Files/read_multi()> - read multiple images from an image
file

read_types() - L<Imager::Files/read_types()> - list image types Imager
can read.

register_filter() - L<Imager::Filters/register_filter()>

register_reader() - L<Imager::Files/register_reader()>

register_writer() - L<Imager::Files/register_writer()>

rotate() - L<Imager::Transformations/rotate()>

rubthrough() - L<Imager::Transformations/rubthrough()> - draw an image
onto an image and use the alpha channel

scale() - L<Imager::Transformations/scale()>

scale_calculate() - L<Imager::Transformations/scale_calculate()>

scaleX() - L<Imager::Transformations/scaleX()>

scaleY() - L<Imager::Transformations/scaleY()>

setcolors() - L<Imager::ImageTypes/setcolors()> - set palette colors
in a paletted image

set_file_limits() - L<Imager::Files/set_file_limits()>

setmask() - L<Imager::ImageTypes/setmask()>

setpixel() - L<Imager::Draw/setpixel()>

setsamples() - L<Imager::Draw/setsamples()>

setscanline() - L<Imager::Draw/setscanline()>

settag() - L<Imager::ImageTypes/settag()>

string() - L<Imager::Draw/string()> - draw text on an image

tags() -  L<Imager::ImageTypes/tags()> - fetch image tags

to_paletted() -  L<Imager::ImageTypes/to_paletted()>

to_rgb16() - L<Imager::ImageTypes/to_rgb16()>

to_rgb8() - L<Imager::ImageTypes/to_rgb8()>

to_rgb_double() - L<Imager::ImageTypes/to_rgb_double()> - convert to
double per sample image.

transform() - L<Imager::Engines/"transform()">

transform2() - L<Imager::Engines/"transform2()">

type() -  L<Imager::ImageTypes/type()> - type of image (direct vs paletted)

unload_plugin() - L<Imager::Filters/unload_plugin()>

virtual() - L<Imager::ImageTypes/virtual()> - whether the image has it's own
data

write() - L<Imager::Files/write()> - write an image to a file

write_multi() - L<Imager::Files/write_multi()> - write multiple image to an image
file.

write_types() - L<Imager::Files/read_types()> - list image types Imager
can write.

=head1 CONCEPT INDEX

animated GIF - L<Imager::Files/"Writing an animated GIF">

aspect ratio - C<i_xres>, C<i_yres>, C<i_aspect_only> in
L<Imager::ImageTypes/"Common Tags">.

blend - alpha blending one image onto another
L<Imager::Transformations/rubthrough()>

blur - L<Imager::Filters/gaussian>, L<Imager::Filters/conv>

boxes, drawing - L<Imager::Draw/box()>

changes between image - L<Imager::Filters/"Image Difference">

channels, combine into one image - L<Imager::Transformations/combine()>

color - L<Imager::Color>

color names - L<Imager::Color>, L<Imager::Color::Table>

combine modes - L<Imager::Draw/"Combine Types">

compare images - L<Imager::Filters/"Image Difference">

contrast - L<Imager::Filters/contrast>, L<Imager::Filters/autolevels>

convolution - L<Imager::Filters/conv>

cropping - L<Imager::Transformations/crop()>

CUR files - L<Imager::Files/"ICO (Microsoft Windows Icon) and CUR (Microsoft Windows Cursor)">

C<diff> images - L<Imager::Filters/"Image Difference">

dpi - C<i_xres>, C<i_yres> in L<Imager::ImageTypes/"Common Tags">,
L<Imager::Cookbook/"Image spatial resolution">

drawing boxes - L<Imager::Draw/box()>

drawing lines - L<Imager::Draw/line()>

drawing text - L<Imager::Draw/string()>, L<Imager::Draw/align_string()>

error message - L</"ERROR HANDLING">

files, font - L<Imager::Font>

files, image - L<Imager::Files>

filling, types of fill - L<Imager::Fill>

filling, boxes - L<Imager::Draw/box()>

filling, flood fill - L<Imager::Draw/flood_fill()>

flood fill - L<Imager::Draw/flood_fill()>

fonts - L<Imager::Font>

fonts, drawing with - L<Imager::Draw/string()>,
L<Imager::Draw/align_string()>, L<Imager::Font::Wrap>

fonts, metrics - L<Imager::Font/bounding_box()>, L<Imager::Font::BBox>

fonts, multiple master - L<Imager::Font/"MULTIPLE MASTER FONTS">

fountain fill - L<Imager::Fill/"Fountain fills">,
L<Imager::Filters/fountain>, L<Imager::Fountain>,
L<Imager::Filters/gradgen>

GIF files - L<Imager::Files/"GIF">

GIF files, animated - L<Imager::Files/"Writing an animated GIF">

gradient fill - L<Imager::Fill/"Fountain fills">,
L<Imager::Filters/fountain>, L<Imager::Fountain>,
L<Imager::Filters/gradgen>

gray scale, convert image to - L<Imager::Transformations/convert()>

gaussian blur - L<Imager::Filters/gaussian>

hatch fills - L<Imager::Fill/"Hatched fills">

ICO files - L<Imager::Files/"ICO (Microsoft Windows Icon) and CUR (Microsoft Windows Cursor)">

invert image - L<Imager::Filters/hardinvert>,
L<Imager::Filters/hardinvertall>

JPEG - L<Imager::Files/"JPEG">

limiting image sizes - L<Imager::Files/"Limiting the sizes of images you read">

lines, drawing - L<Imager::Draw/line()>

matrix - L<Imager::Matrix2d>, 
L<Imager::Engines/"Matrix Transformations">,
L<Imager::Font/transform()>

metadata, image - L<Imager::ImageTypes/"Tags">

mosaic - L<Imager::Filters/mosaic>

noise, filter - L<Imager::Filters/noise>

noise, rendered - L<Imager::Filters/turbnoise>,
L<Imager::Filters/radnoise>

paste - L<Imager::Transformations/paste()>,
L<Imager::Transformations/rubthrough()>

pseudo-color image - L<Imager::ImageTypes/to_paletted()>,
L<Imager::ImageTypes/new()>

=for stopwords posterize

posterize - L<Imager::Filters/postlevels>

PNG files - L<Imager::Files>, L<Imager::Files/"PNG">

PNM - L<Imager::Files/"PNM (Portable aNy Map)">

rectangles, drawing - L<Imager::Draw/box()>

resizing an image - L<Imager::Transformations/scale()>, 
L<Imager::Transformations/crop()>

RGB (SGI) files - L<Imager::Files/"SGI (RGB, BW)">

saving an image - L<Imager::Files>

scaling - L<Imager::Transformations/scale()>

SGI files - L<Imager::Files/"SGI (RGB, BW)">

sharpen - L<Imager::Filters/unsharpmask>, L<Imager::Filters/conv>

size, image - L<Imager::ImageTypes/getwidth()>,
L<Imager::ImageTypes/getheight()>

size, text - L<Imager::Font/bounding_box()>

tags, image metadata - L<Imager::ImageTypes/"Tags">

text, drawing - L<Imager::Draw/string()>, L<Imager::Draw/align_string()>,
L<Imager::Font::Wrap>

text, wrapping text in an area - L<Imager::Font::Wrap>

text, measuring - L<Imager::Font/bounding_box()>, L<Imager::Font::BBox>

tiles, color - L<Imager::Filters/mosaic>

transparent images - L<Imager::ImageTypes>,
L<Imager::Cookbook/"Transparent PNG">

=for stopwords unsharp

unsharp mask - L<Imager::Filters/unsharpmask>

watermark - L<Imager::Filters/watermark>

writing an image to a file - L<Imager::Files>

=head1 THREADS

Imager doesn't support perl threads.

Imager has limited code to prevent double frees if you create images,
colors etc, and then create a thread, but has no code to prevent two
threads entering Imager's error handling code, and none is likely to
be added.

=head1 SUPPORT

The best place to get help with Imager is the mailing list.

To subscribe send a message with C<subscribe> in the body to:

   imager-devel+request@molar.is

or use the form at:

=over

L<http://www.molar.is/en/lists/imager-devel/>

=back

where you can also find the mailing list archive.

You can report bugs by pointing your browser at:

=over

L<https://rt.cpan.org/NoAuth/ReportBug.html?Queue=Imager>

=back

or by sending an email to:

=over

bug-Imager@rt.cpan.org

=back

Please remember to include the versions of Imager, perl, supporting
libraries, and any relevant code.  If you have specific images that
cause the problems, please include those too.

If you don't want to publish your email address on a mailing list you
can use CPAN::Forum:

  http://www.cpanforum.com/dist/Imager

You will need to register to post.

=head1 CONTRIBUTING TO IMAGER

=head2 Feedback

I like feedback.

If you like or dislike Imager, you can add a public review of Imager
at CPAN Ratings:

  http://cpanratings.perl.org/dist/Imager

=for stopwords Bitcard

This requires a Bitcard account (http://www.bitcard.org).

You can also send email to the maintainer below.

If you send me a bug report via email, it will be copied to Request
Tracker.

=head2 Patches

I accept patches, preferably against the master branch in git.  Please
include an explanation of the reason for why the patch is needed or
useful.

Your patch should include regression tests where possible, otherwise
it will be delayed until I get a chance to write them.

To browse Imager's git repository:

  http://git.imager.perl.org/imager.git

or:

  https://github.com/tonycoz/imager

To clone:

  git clone git://git.imager.perl.org/imager.git

or:

  git clone git://github.com/tonycoz/imager.git

=head1 AUTHOR

Tony Cook <tonyc@cpan.org> is the current maintainer for Imager.

Arnar M. Hrafnkelsson is the original author of Imager.

Many others have contributed to Imager, please see the C<README> for a
complete list.

=head1 LICENSE

Imager is licensed under the same terms as perl itself.

=for stopwords
makeblendedfont Fontforge

A test font, generated by the Debian packaged Fontforge,
F<FT2/fontfiles/MMOne.pfb>, contains a Postscript operator definition
copyrighted by Adobe.  See F<adobe.txt> in the source for license
information.

=head1 SEE ALSO

L<perl>(1), L<Imager::ImageTypes>(3), L<Imager::Files>(3),
L<Imager::Draw>(3), L<Imager::Color>(3), L<Imager::Fill>(3),
L<Imager::Font>(3), L<Imager::Transformations>(3),
L<Imager::Engines>(3), L<Imager::Filters>(3), L<Imager::Expr>(3),
L<Imager::Matrix2d>(3), L<Imager::Fountain>(3)

L<http://imager.perl.org/>

L<Affix::Infix2Postfix>(3), L<Parse::RecDescent>(3)

Other perl imaging modules include:

L<GD>(3), L<Image::Magick>(3), L<Graphics::Magick>(3),
L<Prima::Image>, L<IPA>.

If you're trying to use Imager for array processing, you should
probably using L<PDL>.

=cut
