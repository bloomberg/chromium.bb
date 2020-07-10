use strict;

package Path::Class::Entity;
{
  $Path::Class::Entity::VERSION = '0.37';
}

use File::Spec 3.26;
use File::stat ();
use Cwd;
use Carp();

use overload
  (
   q[""] => 'stringify',
   'bool' => 'boolify',
   fallback => 1,
  );

sub new {
  my $from = shift;
  my ($class, $fs_class) = (ref($from)
			    ? (ref $from, $from->{file_spec_class})
			    : ($from, $Path::Class::Foreign));
  return bless {file_spec_class => $fs_class}, $class;
}

sub is_dir { 0 }

sub _spec_class {
  my ($class, $type) = @_;

  die "Invalid system type '$type'" unless ($type) = $type =~ /^(\w+)$/;  # Untaint
  my $spec = "File::Spec::$type";
  ## no critic
  eval "require $spec; 1" or die $@;
  return $spec;
}

sub new_foreign {
  my ($class, $type) = (shift, shift);
  local $Path::Class::Foreign = $class->_spec_class($type);
  return $class->new(@_);
}

sub _spec { (ref($_[0]) && $_[0]->{file_spec_class}) || 'File::Spec' }

sub boolify { 1 }
  
sub is_absolute { 
  # 5.6.0 has a bug with regexes and stringification that's ticked by
  # file_name_is_absolute().  Help it along with an explicit stringify().
  $_[0]->_spec->file_name_is_absolute($_[0]->stringify) 
}

sub is_relative { ! $_[0]->is_absolute }

sub cleanup {
  my $self = shift;
  my $cleaned = $self->new( $self->_spec->canonpath("$self") );
  %$self = %$cleaned;
  return $self;
}

sub resolve {
  my $self = shift;
  Carp::croak($! . " $self") unless -e $self;  # No such file or directory
  my $cleaned = $self->new( scalar Cwd::realpath($self->stringify) );

  # realpath() always returns absolute path, kind of annoying
  $cleaned = $cleaned->relative if $self->is_relative;

  %$self = %$cleaned;
  return $self;
}

sub absolute {
  my $self = shift;
  return $self if $self->is_absolute;
  return $self->new($self->_spec->rel2abs($self->stringify, @_));
}

sub relative {
  my $self = shift;
  return $self->new($self->_spec->abs2rel($self->stringify, @_));
}

sub stat  { File::stat::stat("$_[0]") }
sub lstat { File::stat::lstat("$_[0]") }

sub PRUNE { return \&PRUNE; }

1;
__END__

=head1 NAME

Path::Class::Entity - Base class for files and directories

=head1 VERSION

version 0.37

=head1 DESCRIPTION

This class is the base class for C<Path::Class::File> and
C<Path::Class::Dir>, it is not used directly by callers.

=head1 AUTHOR

Ken Williams, kwilliams@cpan.org

=head1 SEE ALSO

Path::Class

=cut
