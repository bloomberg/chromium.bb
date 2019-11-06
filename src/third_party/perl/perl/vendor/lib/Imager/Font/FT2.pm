package Imager::Font::FT2;
use strict;
use Imager;
use vars qw($VERSION @ISA);
@ISA = qw(Imager::Font);

BEGIN {
  $VERSION = "0.85";

  require XSLoader;
  XSLoader::load('Imager::Font::FT2', $VERSION);
}

*_first = \&Imager::Font::_first;

sub new {
  my $class = shift;
  my %hsh=(color=>Imager::Color->new(255,0,0,255),
	   size=>15,
	   @_);

  unless ($hsh{file}) {
    $Imager::ERRSTR = "No font file specified";
    return;
  }
  unless (-e $hsh{file}) {
    $Imager::ERRSTR = "Font file $hsh{file} not found";
    return;
  }
  unless ($Imager::formats{ft2}) {
    $Imager::ERRSTR = "Freetype2 not supported in this build";
    return;
  }
  my $id = i_ft2_new($hsh{file}, $hsh{'index'} || 0);
  unless ($id) { # the low-level code may miss some error handling
    $Imager::ERRSTR = Imager::_error_as_msg();
    return;
  }
  return bless {
		id       => $id,
		aa       => $hsh{aa} || 0,
		file     => $hsh{file},
		type     => 't1',
		size     => $hsh{size},
		color    => $hsh{color},
                utf8     => $hsh{utf8},
                vlayout  => $hsh{vlayout},
	       }, $class;
}

sub _draw {
  my $self = shift;
  my %input = @_;
  if (exists $input{channel}) {
    i_ft2_cp($self->{id}, $input{image}{IMG}, $input{'x'}, $input{'y'},
             $input{channel}, $input{size}, $input{sizew} || 0,
             $input{string}, , $input{align}, $input{aa}, $input{vlayout},
             $input{utf8});
  } else {
    i_ft2_text($self->{id}, $input{image}{IMG},
               $input{'x'}, $input{'y'},
               $input{color}, $input{size}, $input{sizew} || 0,
               $input{string}, $input{align}, $input{aa}, $input{vlayout},
               $input{utf8});
  }
}

sub _bounding_box {
  my $self = shift;
  my %input = @_;

  return i_ft2_bbox($self->{id}, $input{size}, $input{sizew}, $input{string}, 
		    $input{utf8});
}

sub dpi {
  my $self = shift;
  my @old = i_ft2_getdpi($self->{id});
  if (@_) {
    my %hsh = @_;
    my $result;
    unless ($hsh{xdpi} && $hsh{ydpi}) {
      if ($hsh{dpi}) {
        $hsh{xdpi} = $hsh{ydpi} = $hsh{dpi};
      }
      else {
        $Imager::ERRSTR = "dpi method requires xdpi and ydpi or just dpi";
        return;
      }
      i_ft2_setdpi($self->{id}, $hsh{xdpi}, $hsh{ydpi}) or return;
    }
  }
  
  return @old;
}

sub hinting {
  my ($self, %opts) = @_;

  i_ft2_sethinting($self->{id}, $opts{hinting} || 0);
}

sub _transform {
  my $self = shift;

  my %hsh = @_;
  my $matrix = $hsh{matrix} or return undef;

  return i_ft2_settransform($self->{id}, $matrix)
}

sub utf8 {
  return 1;
}

# check if the font has the characters in the given string
sub has_chars {
  my ($self, %hsh) = @_;

  unless (defined $hsh{string} && length $hsh{string}) {
    $Imager::ERRSTR = "No string supplied to \$font->has_chars()";
    return;
  }
  return i_ft2_has_chars($self->{id}, $hsh{string}, 
			 _first($hsh{'utf8'}, $self->{utf8}, 0));
}

sub face_name {
  my ($self) = @_;

  i_ft2_face_name($self->{id});
}

sub can_glyph_names {
  i_ft2_can_do_glyph_names();
}

sub glyph_names {
  my ($self, %input) = @_;

  my $string = $input{string};
  defined $string
    or return Imager->_set_error("no string parameter passed to glyph_names");
  my $utf8 = _first($input{utf8}, 0);
  my $reliable_only = _first($input{reliable_only}, 1);

  my @names = i_ft2_glyph_name($self->{id}, $string, $utf8, $reliable_only);
  @names or return Imager->_set_error(Imager->_error_as_msg);

  return @names if wantarray;
  return pop @names;
}

sub is_mm {
  my ($self) = @_;

  i_ft2_is_multiple_master($self->{id});
}

sub mm_axes {
  my ($self) = @_;

  my ($num_axis, $num_design, @axes) =
    i_ft2_get_multiple_masters($self->{id})
      or return Imager->_set_error(Imager->_error_as_msg);

  return @axes;
}

sub set_mm_coords {
  my ($self, %opts) = @_;

  $opts{coords}
    or return Imager->_set_error("Missing coords parameter");
  ref($opts{coords}) && $opts{coords} =~ /ARRAY\(0x[\da-f]+\)$/
    or return Imager->_set_error("coords parameter must be an ARRAY ref");

  i_ft2_set_mm_coords($self->{id}, @{$opts{coords}})
    or return Imager->_set_error(Imager->_error_as_msg);

  return 1;
}
1;

__END__

=head1 NAME

Imager::Font::FT2 - font support using FreeType 2

=head1 SYNOPSIS

  use Imager;

  my $img = Imager->new;
  my $font = Imager::Font->new(file => "foo.ttf", type => "ft2");

  $img->string(... font => $font);

=head1 DESCRIPTION

This provides font support on FreeType 2.

=head1 CAVEATS

Unfortunately, older versions of Imager would install
C<Imager::Font::FreeType2> even if FreeType 2 wasn't available, and if
no font was created would succeed in loading the module.  This means
that an existing C<FreeType2.pm> could cause a probe success for
supported font files, so I've renamed it.

=head1 AUTHOR

Tony Cook <tonyc@cpan.org>

=head1 SEE ALSO

Imager, Imager::Font.

=cut
