package Imager::Fountain;
use strict;
use Imager::Color::Float;
use vars qw($VERSION);

$VERSION = "1.007";

=head1 NAME

  Imager::Fountain - a class for building fountain fills suitable for use by
 the fountain filter.

=head1 SYNOPSIS

  use Imager::Fountain;
  my $f1 = Imager::Fountain->read(gimp=>$filename);
  $f->write(gimp=>$filename);
  my $f1 = Imager::Fountain->new;
  $f1->add(start=>0, middle=>0.5, end=>1.0,
           c0=>Imager::Color->new(...),
           c1=>Imager::Color->new(...),
           type=>$trans_type, color=>$color_trans_type);

=head1 DESCRIPTION

Provide an interface to build arrays suitable for use by the Imager
fountain filter.  These can be loaded from or saved to a GIMP gradient
file or you can build them from scratch.

=over

=item read(gimp=>$filename)

=item read(gimp=>$filename, name=>\$name)

Loads a gradient from the given GIMP gradient file, and returns a
new Imager::Fountain object.

If the name parameter is supplied as a scalar reference then any name
field from newer GIMP gradient files will be returned in it.

  my $gradient = Imager::Fountain->read(gimp=>'foo.ggr');
  my $name;
  my $gradient2 = Imager::Fountain->read(gimp=>'bar.ggr', name=>\$name);

=cut

sub read {
  my ($class, %opts) = @_;

  if ($opts{gimp}) {
    my $fh;
    $fh = ref($opts{gimp}) ? $opts{gimp} : IO::File->new($opts{gimp});
    unless ($fh) {
      $Imager::ERRSTR = "Cannot open $opts{gimp}: $!";
      return;
    }

    my $trash_name;
    my $name_ref = $opts{name} && ref $opts{name} ? $opts{name} : \$trash_name;

    return $class->_load_gimp_gradient($fh, $opts{gimp}, $name_ref);
  }
  else {
    warn "${class}::read: Nothing to do!";
    return;
  }
}

=item write(gimp=>$filename)

=item write(gimp=>$filename, name=>$name)

Save the gradient to a GIMP gradient file.

The second variant allows the gradient name to be set (for newer
versions of the GIMP).

  $gradient->write(gimp=>'foo.ggr')
    or die Imager->errstr;
  $gradient->write(gimp=>'bar.ggr', name=>'the bar gradient')
    or die Imager->errstr;

=cut

sub write {
  my ($self, %opts) = @_;

  if ($opts{gimp}) {
    my $fh;
    $fh = ref($opts{gimp}) ? $opts{gimp} : IO::File->new("> ".$opts{gimp});
    unless ($fh) {
      $Imager::ERRSTR = "Cannot open $opts{gimp}: $!";
      return;
    }

    return $self->_save_gimp_gradient($fh, $opts{gimp}, $opts{name});
  }
  else {
    warn "Nothing to do\n";
    return;
  }
}

=item new

Create an empty fountain fill description.

=cut

sub new {
  my ($class) = @_;

  return bless [], $class;
}

sub _first {
  for (@_) {
    return $_ if defined;
  }
  return undef;
}

=item add(start=>$start, middle=>$middle, end=>1.0, c0=>$start_color, c1=>$end_color, type=>$trans_type, color=>$color_trans_type)

Adds a new segment to the fountain fill, the possible options are:

=over

=item *

C<start> - the start position in the gradient where this segment takes
effect between 0 and 1.  Default: 0.

=item *

C<middle> - the mid-point of the transition between the 2
colors, between 0 and 1.  Default: average of C<start> and C<end>.

=item *

C<end> - the end of the gradient, from 0 to 1.  Default: 1.

=item *

C<c0> - the color of the fountain fill where the fill parameter is
equal to I<start>.  Default: opaque black.

=item *

C<c1> - the color of the fountain fill where the fill parameter is
equal to I<end>.  Default: opaque black.

=item *

C<type> - the type of segment, controls the way in which the fill parameter
moves from 0 to 1.  Default: linear.

This can take any of the following values:

=over

=item *

C<linear>

=item *

C<curved> - unimplemented so far.

=item *

C<sine>

=item *

C<sphereup>

=item *

C<spheredown>

=back

=item *

C<color> - the way in which the color transitions between C<c0> and C<c1>.
Default: direct.

This can take any of the following values:

=over

=item *

C<direct> - each channel is simple scaled between c0 and c1.

=item *

C<hueup> - the color is converted to a HSV value and the scaling is
done such that the hue increases as the fill parameter increases.

=item *

C<huedown> - the color is converted to a HSV value and the scaling is
done such that the hue decreases as the fill parameter increases.

=back

=back

In most cases you can ignore some of the arguments, eg.

  # assuming $f is a new Imager::Fountain in each case here
  use Imager ':handy';
  # simple transition from red to blue
  $f->add(c0=>NC('#FF0000'), c1=>NC('#0000FF'));
  # simple 2 stages from red to green to blue
  $f->add(end=>0.5, c0=>NC('#FF0000'), c1=>NC('#00FF00'))
  $f->add(start=>0.5, c0=>NC('#00FF00'), c1=>NC('#0000FF'));

=cut

# used to translate segment types and color transition types to numbers
my %type_names =
  (
   linear => 0,
   curved => 1,
   sine => 2,
   sphereup=> 3,
   spheredown => 4,
  );

my %color_names =
  (
   direct => 0,
   hueup => 1,
   huedown => 2
  );

sub add {
  my ($self, %opts) = @_;

  my $start = _first($opts{start}, 0);
  my $end = _first($opts{end}, 1);
  my $middle = _first($opts{middle}, ($start+$end)/2);
  my @row =
    (
     $start, $middle, $end,
     _first($opts{c0}, Imager::Color::Float->new(0,0,0,1)),
     _first($opts{c1}, Imager::Color::Float->new(1,1,1,0)),
     _first($opts{type} && $type_names{$opts{type}}, $opts{type}, 0),
     _first($opts{color} && $color_names{$opts{color}}, $opts{color}, 0)
    );
  push(@$self, \@row);

  $self;
}

=item simple(positions=>[ ... ], colors=>[...])

Creates a simple fountain fill object consisting of linear segments.

The array references passed as positions and colors must have the same
number of elements.  They must have at least 2 elements each.

colors must contain Imager::Color or Imager::Color::Float objects.

eg.

  my $f = Imager::Fountain->simple(positions=>[0, 0.2, 1.0],
                                   colors=>[ NC(255,0,0), NC(0,255,0), 
                                             NC(0,0,255) ]);

=cut

sub simple {
  my ($class, %opts) = @_;

  if ($opts{positions} && $opts{colors}) {
    my $positions = $opts{positions};
    my $colors = $opts{colors};
    unless (@$positions == @$colors) {
      $Imager::ERRSTR = "positions and colors must be the same size";
      return;
    }
    unless (@$positions >= 2) {
      $Imager::ERRSTR = "not enough segments";
      return;
    }
    my $f = $class->new;
    for my $i (0.. $#$colors-1) {
      $f->add(start=>$positions->[$i], end=>$positions->[$i+1],
              c0 => $colors->[$i], c1=>$colors->[$i+1]);
    }
    return $f;
  }
  else {
    warn "Nothing to do";
    return;
  }
}

=back

=head2 Implementation Functions

Documented for internal use.

=over

=item _load_gimp_gradient($class, $fh, $name)

Does the work of loading a GIMP gradient file.

=cut

sub _load_gimp_gradient {
  my ($class, $fh, $filename, $name) = @_;

  my $head = <$fh>;
  chomp $head;
  unless ($head eq 'GIMP Gradient') {
    $Imager::ERRSTR = "$filename is not a GIMP gradient file";
    return;
  }
  my $count = <$fh>;
  chomp $count;
  if ($count =~ /^name:\s?(.*)/i) {
    ref $name and $$name = $1;
    $count = <$fh>; # try again
    chomp $count;
  }
  unless ($count =~ /^\d+$/) {
    $Imager::ERRSTR = "$filename is missing the segment count";
    return;
  }
  my @result;
  for my $i (1..$count) {
    my $row = <$fh>;
    chomp $row;
    my @row = split ' ', $row;
    unless (@row == 13) {
      $Imager::ERRSTR = "Bad segment definition";
      return;
    }
    my ($start, $middle, $end) = splice(@row, 0, 3);
    my $c0 = Imager::Color::Float->new(splice(@row, 0, 4));
    my $c1 = Imager::Color::Float->new(splice(@row, 0, 4));
    my ($type, $color) = @row;
    push(@result, [ $start, $middle, $end, $c0, $c1, $type, $color ]);
  }
  return bless \@result, 
}

=item _save_gimp_gradient($self, $fh, $name)

Does the work of saving to a GIMP gradient file.

=cut

sub _save_gimp_gradient {
  my ($self, $fh, $filename, $name) = @_;

  print $fh "GIMP Gradient\n";
  defined $name or $name = '';
  $name =~ tr/ -~/ /cds;
  if ($name) {
    print $fh "Name: $name\n";
  }
  print $fh scalar(@$self),"\n";
  for my $row (@$self) {
    printf $fh "%.6f %.6f %.6f ",@{$row}[0..2];
    for my $i (0, 1) {
      for ($row->[3+$i]->rgba) {
        printf $fh "%.6f ", $_/255.0;
      }
    }
    print $fh "@{$row}[5,6]";
    unless (print $fh "\n") {
      $Imager::ERRSTR = "write error: $!";
      return;
    }
  }

  return 1;
}

=back

=head1 FILL PARAMETER

The add() documentation mentions a fill parameter in a few places,
this is as good a place as any to discuss it.

The process of deciding the color produced by the gradient works
through the following steps:

=over

=item 1.

calculate the base value, which is typically a distance or an angle of
some sort.  This can be positive or occasionally negative, depending on
the type of fill being performed (linear, radial, etc).

=item 2.

clamp or convert the base value to the range 0 through 1, how this is
done depends on the repeat parameter.  I'm calling this result the
fill parameter.

=item 3.

the appropriate segment is found.  This is currently done with a
linear search, and the first matching segment is used.  If there is no
matching segment the pixel is not touched.

=item 4.

the fill parameter is scaled from 0 to 1 depending on the segment type.

=item 5.

the color produced, depending on the segment color type.

=back

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=head1 SEE ALSO

Imager(3)

=cut
