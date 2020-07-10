package Imager::Font::T1;
use strict;
use Imager::Color;
use vars qw(@ISA $VERSION);
@ISA = qw(Imager::Font);
use Scalar::Util ();

BEGIN {
  $VERSION = "1.026";

  require XSLoader;
  XSLoader::load('Imager::Font::T1', $VERSION);
}


*_first = \&Imager::Font::_first;

my $t1aa = 2;

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
  unless ($Imager::formats{t1}) {
    $Imager::ERRSTR = "Type 1 fonts not supported in this build";
    return;
  }
  # we want to avoid T1Lib's file search mechanism
  unless ($hsh{file} =~ m!^/!
	  || $hsh{file} =~ m!^\.\/?/!
	  || $^O =~ /^(MSWin32|cygwin)$/ && $hsh{file} =~ /^[a-z]:/i) {
    $hsh{file} = './' . $hsh{file};
  }

  if($hsh{afm}) {
	  unless (-e $hsh{afm}) {
	    $Imager::ERRSTR = "Afm file $hsh{afm} not found";
	    return;
	  }
	  unless ($hsh{afm} =~ m!^/!
		  || $hsh{afm} =~ m!^\./!
		  || $^O =~ /^(MSWin32|cygwin)$/ && $hsh{file} =~ /^[a-z]:/i) {
	    $hsh{file} = './' . $hsh{file};
	  }
  } else {
	  $hsh{afm} = 0;
  }

  my $font = Imager::Font::T1xs->new($hsh{file},$hsh{afm});
  unless ($font) { # the low-level code may miss some error handling
    Imager->_set_error(Imager->_error_as_msg);
    return;
  }
  return bless {
		t1font    => $font,
		aa    => $hsh{aa} || 0,
		file  => $hsh{file},
		type  => 't1',
		size  => $hsh{size},
		color => $hsh{color},
		t1aa  => $t1aa,
	       }, $class;
}

sub _draw {
  my $self = shift;

  $self->_valid
    or return;

  my %input = @_;
  my $flags = '';
  $flags .= 'u' if $input{underline};
  $flags .= 's' if $input{strikethrough};
  $flags .= 'o' if $input{overline};
  my $aa = $input{aa} ? $self->{t1aa} : 0;
  if (exists $input{channel}) {
    $self->{t1font}->cp($input{image}{IMG}, $input{'x'}, $input{'y'},
		    $input{channel}, $input{size},
		    $input{string}, $input{align},
                    $input{utf8}, $flags, $aa)
      or return;
  } else {
    $self->{t1font}->text($input{image}{IMG}, $input{'x'}, $input{'y'}, 
		      $input{color}, $input{size}, 
		      $input{string}, $input{align}, $input{utf8}, $flags, $aa)
      or return;
  }

  return $self;
}

sub _bounding_box {
  my $self = shift;

  $self->_valid
    or return;

  my %input = @_;
  my $flags = '';
  $flags .= 'u' if $input{underline};
  $flags .= 's' if $input{strikethrough};
  $flags .= 'o' if $input{overline};
  my @bbox =  $self->{t1font}->bbox($input{size}, $input{string},
				    $input{utf8}, $flags);
  unless (@bbox) {
    Imager->_set_error(Imager->_error_as_msg);
    return;
  }

  return @bbox;
}

# check if the font has the characters in the given string
sub has_chars {
  my ($self, %hsh) = @_;

  $self->_valid
    or return;

  unless (defined $hsh{string}) {
    $Imager::ERRSTR = "No string supplied to \$font->has_chars()";
    return;
  }
  if (wantarray) {
    my @result = $self->{t1font}
      ->has_chars($hsh{string}, _first($hsh{'utf8'}, $self->{utf8}, 0));
    unless (@result) {
      Imager->_set_error(Imager->_error_as_msg);
      return;
    }

    return @result;
  }
  else {
    my $result = $self->{t1font}
      ->has_chars($hsh{string}, _first($hsh{'utf8'}, $self->{utf8}, 0));
    unless (defined $result) {
      Imager->_set_error(Imager->_error_as_msg);
      return;
    }
    return $result;
  }
}

sub utf8 {
  1;
}

sub can_glyph_names {
  1;
}

sub face_name {
  my ($self) = @_;

  $self->_valid
    or return;

  return $self->{t1font}->face_name();
}

sub glyph_names {
  my ($self, %input) = @_;

  $self->_valid
    or return;

  my $string = $input{string};
  defined $string
    or return Imager->_set_error("no string parameter passed to glyph_names");
  my $utf8 = _first($input{utf8} || 0);

  my @result = $self->{t1font}->glyph_names($string, $utf8);
  unless (@result) {
    Imager->_set_error(Imager->_error_as_msg);
    return;
  }

  return @result;
}

sub set_aa_level {
  my ($self, $new_t1aa) = @_;

  if (!defined $new_t1aa ||
      ($new_t1aa != 1 && $new_t1aa != 2)) {
    Imager->_set_error("set_aa_level: parameter must be 1 or 2");
    return;
  }

  if (ref $self) {
    $self->_valid
      or return;

    $self->{t1aa} = $new_t1aa;
  }
  else {
    $t1aa = $new_t1aa;
  }

  return 1;
}

sub _valid {
  my $self = shift;

  unless ($self->{t1font} && Scalar::Util::blessed($self->{t1font})) {
    Imager->_set_error("font object was created in another thread");
    return;
  }

  return 1;
}

1;

__END__

=head1 NAME

  Imager::Font::Type1 - low-level functions for Type1 fonts

=head1 DESCRIPTION

Imager::Font creates a Imager::Font::Type1 object when asked to create
a font object based on a C<.pfb> file.

See Imager::Font to see how to use this type.

This class provides low-level functions that require the caller to
perform data validation

By default Imager no longer creates the F<t1lib.log> log file.  You
can re-enable that by calling Imager::init() with the C<t1log> option:

  Imager::init(t1log=>1);

This must be called before creating any fonts.

Currently specific to Imager::Font::Type1, you can use the following
flags when drawing text or calculating a bounding box:

=for stopwords overline strikethrough

=over

=item *

C<underline> - Draw the text with an underline.

=item *

C<overline> - Draw the text with an overline.

=item *

C<strikethrough> - Draw the text with a strikethrough.

=back

Obviously, if you're calculating the bounding box the size of the line
is included in the box, and the line isn't drawn :)

=head2 Anti-aliasing

T1Lib supports multiple levels of anti-aliasing, by default, if you
request anti-aliased output, Imager::Font::T1 will use the maximum
level.

You can override this with the set_t1_aa() method:

=over

=item set_aa_level()

Usage:

  $font->set_aa_level(1);
  Imager::Font::T1->set_aa_level(2);

Sets the T1Lib anti-aliasing level either for the specified font, or
for new font objects.

The only parameter must be 1 or 2.

Returns true on success.

=back

=head1 AUTHOR

Addi, Tony

=cut
