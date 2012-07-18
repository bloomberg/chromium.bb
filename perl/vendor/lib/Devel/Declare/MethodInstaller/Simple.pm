package Devel::Declare::MethodInstaller::Simple;

use base 'Devel::Declare::Context::Simple';

use Devel::Declare ();
use Sub::Name;
use strict;
use warnings;

our $VERSION = '0.006011';

sub install_methodhandler {
  my $class = shift;
  my %args  = @_;
  {
    no strict 'refs';
    *{$args{into}.'::'.$args{name}}   = sub (&) {};
  }

  my $warnings = warnings::enabled("redefine");
  my $ctx = $class->new(%args);
  Devel::Declare->setup_for(
    $args{into},
    { $args{name} => { const => sub { $ctx->parser(@_, $warnings) } } }
  );
}

sub code_for {
  my ($self, $name) = @_;

  if (defined $name) {
    my $pkg = $self->get_curstash_name;
    $name = join( '::', $pkg, $name )
      unless( $name =~ /::/ );
    return sub (&) {
      my $code = shift;
      # So caller() gets the subroutine name
      no strict 'refs';
      my $installer = $self->warning_on_redefine
          ? sub { *{$name} = subname $name => $code; }
          : sub { no warnings 'redefine';
                  *{$name} = subname $name => $code; };
      $installer->();
      return;
    };
  } else {
    return sub (&) { shift };
  }
}

sub install {
  my ($self, $name ) = @_;

  $self->shadow( $self->code_for($name) );
}

sub parser {
  my $self = shift;
  $self->init(@_);

  $self->skip_declarator;
  my $name   = $self->strip_name;
  my $proto  = $self->strip_proto;
  my $attrs  = $self->strip_attrs;
  my @decl   = $self->parse_proto($proto);
  my $inject = $self->inject_parsed_proto(@decl);
  if (defined $name) {
    $inject = $self->scope_injector_call() . $inject;
  }
  $self->inject_if_block($inject, $attrs ? "sub ${attrs} " : '');

  $self->install( $name );

  return;
}

sub parse_proto { '' }

sub inject_parsed_proto {
  return $_[1];
}

1;

