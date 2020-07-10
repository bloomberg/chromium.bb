use strict; use warnings;
package IO::All::Base;

use Fcntl;

sub import {
    my $class = shift;
    my $flag = $_[0] || '';
    my $package = caller;
    no strict 'refs';
    if ($flag eq '-base') {
        push @{$package . "::ISA"}, $class;
        *{$package . "::$_"} = \&$_
          for qw'field const option chain proxy proxy_open';
    }
    elsif ($flag eq -mixin) {
        mixin_import(scalar(caller(0)), $class, @_);
    }
    else {
        my @flags = @_;
        for my $export (@{$class . '::EXPORT'}) {
            *{$package . "::$export"} = $export eq 'io'
            ? $class->_generate_constructor(@flags)
            : \&{$class . "::$export"};
        }
    }
}

sub _generate_constructor {
    my $class = shift;
    my (@flags, %flags, $key);
    for (@_) {
        if (s/^-//) {
            push @flags, $_;
            $flags{$_} = 1;
            $key = $_;
        }
        else {
            $flags{$key} = $_ if $key;
        }
    }
    my $constructor;
    $constructor = sub {
        my $self = $class->new(@_);
        for (@flags) {
            $self->$_($flags{$_});
        }
        $self->_constructor($constructor);
        return $self;
    }
}

sub _init {
    my $self = shift;
    $self->io_handle(undef);
    $self->is_open(0);
    return $self;
}

#===============================================================================
# Closure generating functions
#===============================================================================
sub option {
    my $package = caller;
    my ($field, $default) = @_;
    $default ||= 0;
    field("_$field", $default);
    no strict 'refs';
    *{"${package}::$field"} =
      sub {
          my $self = shift;
          *$self->{"_$field"} = @_ ? shift(@_) : 1;
          return $self;
      };
}

sub chain {
    my $package = caller;
    my ($field, $default) = @_;
    no strict 'refs';
    *{"${package}::$field"} =
      sub {
          my $self = shift;
          if (@_) {
              *$self->{$field} = shift;
              return $self;
          }
          return $default unless exists *$self->{$field};
          return *$self->{$field};
      };
}

sub field {
    my $package = caller;
    my ($field, $default) = @_;
    no strict 'refs';
    return if defined &{"${package}::$field"};
    *{"${package}::$field"} =
      sub {
          my $self = shift;
          unless (exists *$self->{$field}) {
              *$self->{$field} =
                ref($default) eq 'ARRAY' ? [] :
                ref($default) eq 'HASH' ? {} :
                $default;
          }
          return *$self->{$field} unless @_;
          *$self->{$field} = shift;
      };
}

sub const {
    my $package = caller;
    my ($field, $default) = @_;
    no strict 'refs';
    return if defined &{"${package}::$field"};
    *{"${package}::$field"} = sub { $default };
}

sub proxy {
    my $package = caller;
    my ($proxy) = @_;
    no strict 'refs';
    return if defined &{"${package}::$proxy"};
    *{"${package}::$proxy"} =
      sub {
          my $self = shift;
          my @return = $self->io_handle->$proxy(@_);
          $self->_error_check;
          wantarray ? @return : $return[0];
      };
}

sub proxy_open {
    my $package = caller;
    my ($proxy, @args) = @_;
    no strict 'refs';
    return if defined &{"${package}::$proxy"};
    my $method = sub {
        my $self = shift;
        $self->_assert_open(@args);
        my @return = $self->io_handle->$proxy(@_);
        $self->_error_check;
        wantarray ? @return : $return[0];
    };
    *{"$package\::$proxy"} =
    (@args and $args[0] eq '>') ?
    sub {
        my $self = shift;
        $self->$method(@_);
        return $self;
    }
    : $method;
}

sub mixin_import {
    my $target_class = shift;
    $target_class = caller(0)
      if $target_class eq 'mixin';
    my $mixin_class = shift
      or die "Nothing to mixin";
    eval "require $mixin_class";
    my $pseudo_class = CORE::join '-', $target_class, $mixin_class;
    my %methods = mixin_methods($mixin_class);
    no strict 'refs';
    no warnings;
    @{"$pseudo_class\::ISA"} = @{"$target_class\::ISA"};
    @{"$target_class\::ISA"} = ($pseudo_class);
    for (keys %methods) {
        *{"$pseudo_class\::$_"} = $methods{$_};
    }
}

sub mixin_methods {
    my $mixin_class = shift;
    no strict 'refs';
    my %methods = all_methods($mixin_class);
    map {
        $methods{$_}
          ? ($_, \ &{"$methods{$_}\::$_"})
          : ($_, \ &{"$mixin_class\::$_"})
    } (keys %methods);
}

sub all_methods {
    no strict 'refs';
    my $class = shift;
    my %methods = map {
        ($_, $class)
    } grep {
        defined &{"$class\::$_"} and not /^_/
    } keys %{"$class\::"};
    return (%methods);
}

1;
