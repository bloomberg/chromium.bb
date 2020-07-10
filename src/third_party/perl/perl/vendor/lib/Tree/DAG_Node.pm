package Tree::DAG_Node;

use strict;
use warnings;
use warnings qw(FATAL utf8); # Fatalize encoding glitches.

our $Debug   = 0;
our $VERSION = '1.31';

use File::Slurp::Tiny 'read_lines';

# -----------------------------------------------

sub add_daughter { # alias
  my($it,@them) = @_;  $it->add_daughters(@them);
}

# -----------------------------------------------

sub add_daughters { # write-only method
  my($mother, @daughters) = @_;
  return unless @daughters; # no-op
  return
    $mother->_add_daughters_wrapper(
      sub { push @{$_[0]}, $_[1]; },
      @daughters
    );
}

# -----------------------------------------------

sub add_daughter_left { # alias
  my($it,@them) = @_;  $it->add_daughters_left(@them);
}

# -----------------------------------------------

sub add_daughters_left { # write-only method
  my($mother, @daughters) = @_;
  return unless @daughters;
  return
    $mother->_add_daughters_wrapper(
      sub { unshift @{$_[0]}, $_[1]; },
      @daughters
    );
}

# -----------------------------------------------

sub _add_daughters_wrapper {
  my($mother, $callback, @daughters) = @_;
  return unless @daughters;

  my %ancestors;
  @ancestors{ $mother->ancestors } = undef;
  # This could be made more efficient by not bothering to compile
  # the ancestor list for $mother if all the nodes to add are
  # daughterless.
  # But then you have to CHECK if they're daughterless.
  # If $mother is [big number] generations down, then it's worth checking.

  foreach my $daughter (@daughters) { # which may be ()
    die "daughter must be a node object!" unless UNIVERSAL::can($daughter, 'is_node');

    printf "Mother  : %s (%s)\n", $mother, ref $mother if $Debug;
    printf "Daughter: %s (%s)\n", $daughter, ref $daughter if $Debug;
    printf "Adding %s to %s\n",
      ($daughter->name() || $daughter),
      ($mother->name()   || $mother)     if $Debug > 1;

    die 'Mother (' . $mother -> name . ") can't be its own daughter\n" if $mother eq $daughter;

    die "$daughter (" . ($daughter->name || 'no_name') .
      ") is an ancestor of $mother (" . ($mother->name || 'no_name') .
      "), so can't became its daughter\n" if exists $ancestors{$daughter};

    my $old_mother = $daughter->{'mother'};

    next if defined($old_mother) && ref($old_mother) && $old_mother eq $mother;
      # noop if $daughter is already $mother's daughter

    $old_mother->remove_daughters($daughter)
      if defined($old_mother) && ref($old_mother);

    &{$callback}($mother->{'daughters'}, $daughter);
  }
  $mother->_update_daughter_links; # need only do this at the end

  return;
}

# -----------------------------------------------

sub add_left_sister { # alias
  my($it,@them) = @_;  $it->add_left_sisters(@them);
}

# -----------------------------------------------

sub add_left_sisters { # write-only method
  my($this, @new) = @_;
  return() unless @new;

  @new = $this->replace_with(@new, $this);
  shift @new; pop @new; # kill the copies of $this
  return @new;
}

# -----------------------------------------------

sub add_right_sister { # alias
  my($it,@them) = @_;  $it->add_right_sisters(@them);
}

# -----------------------------------------------

sub add_right_sisters { # write-only method
  my($this, @new) = @_;
  return() unless @new;
  @new = $this->replace_with($this, @new);
  shift @new; shift @new; # kill the copies of $this
  return @new;
}

# -----------------------------------------------

sub address {
  my($it, $address) = @_[0,1];
  if(defined($address) && length($address)) { # given the address, return the node.
    # invalid addresses return undef
    my $root = $it->root;
    my @parts = map {$_ + 0}
                    $address =~ m/(\d+)/g; # generous!
    die "Address \"$address\" is an ill-formed address" unless @parts;
    die "Address \"$address\" must start with '0'" unless shift(@parts) == 0;

    my $current_node = $root;
    while(@parts) { # no-op for root
      my $ord = shift @parts;
      my @daughters = @{$current_node->{'daughters'}};

      if($#daughters < $ord) { # illegal address
        print "* $address has an out-of-range index ($ord)!" if $Debug;
        return undef;
      }
      $current_node = $daughters[$ord];
      unless(ref($current_node)) {
        print "* $address points to or thru a non-node!" if $Debug;
        return undef;
      }
    }
    return $current_node;

  } else { # given the node, return the address
    my @parts = ();
    my $current_node = $it;
    my $mother;

    while(defined( $mother = $current_node->{'mother'} ) && ref($mother)) {
      unshift @parts, $current_node->my_daughter_index;
      $current_node = $mother;
    }
    return join(':', 0, @parts);
  }
}

# -----------------------------------------------

sub ancestors {
  my $this = shift;
  my $mama = $this->{'mother'}; # initial condition
  return () unless ref($mama); # I must be root!

  # Could be defined recursively, as:
  # if(ref($mama = $this->{'mother'})){
  #   return($mama, $mama->ancestors);
  # } else {
  #   return ();
  # }
  # But I didn't think of that until I coded the stuff below, which is
  # faster.

  my @ancestors = ( $mama ); # start off with my mama
  while(defined( $mama = $mama->{'mother'} ) && ref($mama)) {
    # Walk up the tree
    push(@ancestors, $mama);
    # This turns into an infinite loop if someone gets stupid
    #  and makes this tree cyclic!  Don't do it!
  }
  return @ancestors;
}

# -----------------------------------------------

sub attribute { # alias
  my($it,@them) = @_;  $it->attributes(@them);
}

# -----------------------------------------------

sub attributes { # read/write attribute-method
  # expects a ref, presumably a hashref
  my $this = shift;
  if(@_) {
    die "my parameter must be a reference" unless ref($_[0]);
    $this->{'attributes'} = $_[0];
  }
  return $this->{'attributes'};
}

# -----------------------------------------------

sub clear_daughters { # write-only method
  my($mother) = $_[0];
  my @daughters = @{$mother->{'daughters'}};

  @{$mother->{'daughters'}} = ();
  foreach my $one (@daughters) {
    next unless UNIVERSAL::can($one, 'is_node'); # sanity check
    $one->{'mother'} = undef;
  }
  # Another, simpler, way to do it:
  #  $mother->remove_daughters($mother->daughters);

  return @daughters; # NEW
}

# -----------------------------------------------

sub common { # Return the lowest node common to all these nodes...
  # Called as $it->common($other) or $it->common(@others)
  my @ones = @_; # all nodes I was given
  my($first, @others) = @_;

  return $first unless @others; # degenerate case

  my %ones;
  @ones{ @ones } = undef;

  foreach my $node (@others) {
    die "TILT: node \"$node\" is not a node"
      unless UNIVERSAL::can($node, 'is_node');
    my %first_lineage;
    @first_lineage{$first, $first->ancestors} = undef;
    my $higher = undef; # the common of $first and $node
    my @my_lineage = $node->ancestors;

   Find_Common:
    while(@my_lineage) {
      if(exists $first_lineage{$my_lineage[0]}) {
        $higher = $my_lineage[0];
        last Find_Common;
      }
      shift @my_lineage;
    }
    return undef unless $higher;
    $first = $higher;
  }
  return $first;
}

# -----------------------------------------------

sub common_ancestor {
  my @ones = @_; # all nodes I was given
  my($first, @others) = @_;

  return $first->{'mother'} unless @others;
    # which may be undef if $first is the root!

  my %ones;
  @ones{ @ones } = undef; # my arguments

  my $common = $first->common(@others);
  if(exists($ones{$common})) { # if the common is one of my nodes...
    return $common->{'mother'};
    # and this might be undef, if $common is root!
  } else {
    return $common;
    # which might be null if that's all common came up with
  }
}

# -----------------------------------------------

sub copy
{
	my($from, $o) = @_[0,1];
	$o = {} unless ref $o;

	# Straight dup, and bless into same class.

	my $to = bless { %$from }, ref($from);

	# Null out linkages.

	$to -> _init_mother;
	$to -> _init_daughters;

	# Dup the 'attributes' attribute.

	if ($$o{'no_attribute_copy'})
	{
		$$to{attributes} = {};
	}
	else
	{
		my $attrib_copy = ref($to->{'attributes'});

		if ($attrib_copy)
		{
			if ($attrib_copy eq 'HASH')
			{
				# Dup the hashref.

				$$to{'attributes'} = { %{$$to{'attributes'}} };
			}
			elsif ($attrib_copy = UNIVERSAL::can($to->{'attributes'}, 'copy') )
			{
				# $attrib_copy now points to the copier method.

				$$to{'attributes'} = &{$attrib_copy}($from);

			} # Otherwise I don't know how to copy it; leave as is.
		}
	}

	$$o{'from_to'}{$from} = $to; # SECRET VOODOO

	# ...autovivifies an anon hashref for 'from_to' if need be
	# This is here in case I later want/need a table corresponding
	# old nodes to new.

	return $to;
}

# -----------------------------------------------

sub copy_at_and_under {
  my($from, $o) = @_[0,1];
  $o = {} unless ref $o;
  my @daughters = map($_->copy_at_and_under($o), @{$from->{'daughters'}});
  my $to = $from->copy($o);
  $to->set_daughters(@daughters) if @daughters;
  return $to;
}

# -----------------------------------------------

sub copy_tree {
  my($this, $o) = @_[0,1];
  my $root = $this->root;
  $o = {} unless ref $o;

  my $new_root = $root->copy_at_and_under($o);

  return $new_root;
}

# -----------------------------------------------

sub daughters { # read-only attrib-method: returns a list.
  my $this = shift;

  if(@_) { # undoc'd and disfavored to use as a write-method
    die "Don't set daughters with daughters anymore\n";
    warn "my parameter must be a listref" unless ref($_[0]);
    $this->{'daughters'} = $_[0];
    $this->_update_daughter_links;
  }
  #return $this->{'daughters'};
  return @{$this->{'daughters'} || []};
}

# ------------------------------------------------

sub decode_lol
{
	my($self, $result) = @_;
	my(@worklist)      = $result;

	my($obj);
	my($ref_type);
	my(@stack);

	do
	{
		$obj      = shift @worklist;
		$ref_type = ref $obj;

		if ($ref_type eq 'ARRAY')
		{
			unshift @worklist, @$obj;
		}
		elsif ($ref_type eq 'HASH')
		{
			push @stack, {%$obj};
		}
		elsif ($ref_type)
		{
			die "Unsupported object type $ref_type\n";
		}
		else
		{
			push @stack, $obj;
		}

	} while (@worklist);

	return [@stack];

} # End of decode_lol.

# -----------------------------------------------

sub delete_tree {
  my $it = $_[0];
  $it->root->walk_down({ # has to be callbackback, not callback
    'callbackback' => sub {
       %{$_[0]} = ();
       bless($_[0], 'DEADNODE'); # cause become dead!  cause become dead!
       return 1;
     }
  });
  return;
  # Why DEADNODE?  Because of the nice error message:
  #  "Can't locate object method "leaves_under" via package "DEADNODE"."
  # Moreover, DEADNODE doesn't provide is_node, so fails my can() tests.
}

sub DEADNODE::delete_tree { return; }
  # in case you kill it AGAIN!!!!!  AND AGAIN AND AGAIN!!!!!! OO-HAHAHAHA!

# -----------------------------------------------

sub depth_under {
  my $node = shift;
  my $max_depth = 0;
  $node->walk_down({
    '_depth' => 0,
    'callback' => sub {
      my $depth = $_[1]->{'_depth'};
      $max_depth = $depth if $depth > $max_depth;
      return 1;
    },
  });
  return $max_depth;
}

# -----------------------------------------------

sub descendants {
  # read-only method:  return a list of my descendants
  my $node = shift;
  my @list = $node->self_and_descendants;
  shift @list; # lose myself.
  return @list;
}

# -----------------------------------------------

sub draw_ascii_tree {
  # Make a "box" for this node and its possible daughters, recursively.

  # The guts of this routine are horrific AND recursive!

  # Feel free to send me better code.  I worked on this until it
  #  gave me a headache and it worked passably, and then I stopped.

  my $it = $_[0];
  my $o = ref($_[1]) ? $_[1] : {};
  my(@box, @daughter_boxes, $width, @daughters);
  @daughters = @{$it->{'daughters'}};

  $o->{'no_name'}   = 0 unless exists $o->{'no_name'};
  $o->{'h_spacing'} = 1 unless exists $o->{'h_spacing'};
  $o->{'h_compact'} = 1 unless exists $o->{'h_compact'};
  $o->{'v_compact'} = 1 unless exists $o->{'v_compact'};

  my $printable_name;
  if($o->{'no_name'}) {
    $printable_name = '*';
  } else {
    $printable_name = defined $it->name ? $it->name : $it;
    $printable_name =~ tr<\cm\cj\t >< >s;
    $printable_name = "<$printable_name>";
  }

  if(!scalar(@daughters)) { # I am a leaf!
    # Now add the top parts, and return.
    @box = ("|", $printable_name);
  } else {
    @daughter_boxes = map { &draw_ascii_tree($_, $o) } @daughters;

    my $max_height = 0;
    foreach my $box (@daughter_boxes) {
      my $h = @$box;
      $max_height = $h if $h > $max_height;
    }

    @box = ('') x $max_height; # establish the list

    foreach my $one (@daughter_boxes) {
      my $length = length($one->[0]);
      my $height = @$one;

      #now make all the same height.
      my $deficit = $max_height - $height;
      if($deficit > 0) {
        push @$one, ( scalar( ' ' x $length ) ) x $deficit;
        $height = scalar(@$one);
      }


      # Now tack 'em onto @box
      ##########################################################
      # This used to be a sub of its own.  Ho-hum.

      my($b1, $b2) = (\@box, $one);
      my($h1, $h2) = (scalar(@$b1), scalar(@$b2));

      my(@diffs, $to_chop);
      if($o->{'h_compact'}) { # Try for h-scrunching.
        my @diffs;
        my $min_diff = length($b1->[0]); # just for starters
        foreach my $line (0 .. ($h1 - 1)) {
          my $size_l = 0; # length of terminal whitespace
          my $size_r = 0; # length of initial whitespace
          $size_l = length($1) if $b1->[$line] =~ /( +)$/s;
          $size_r = length($1) if $b2->[$line] =~ /^( +)/s;
          my $sum = $size_l + $size_r;

          $min_diff = $sum if $sum < $min_diff;
          push @diffs, [$sum, $size_l, $size_r];
        }
        $to_chop = $min_diff - $o->{'h_spacing'};
        $to_chop = 0 if $to_chop < 0;
      }

      if(not(  $o->{'h_compact'} and $to_chop  )) {
        # No H-scrunching needed/possible
        foreach my $line (0 .. ($h1 - 1)) {
          $b1->[ $line ] .= $b2->[ $line ] . (' ' x $o->{'h_spacing'});
        }
      } else {
        # H-scrunching is called for.
        foreach my $line (0 .. ($h1 - 1)) {
          my $r = $b2->[$line]; # will be the new line
          my $remaining = $to_chop;
          if($remaining) {
            my($l_chop, $r_chop) = @{$diffs[$line]}[1,2];

            if($l_chop) {
              if($l_chop > $remaining) {
                $l_chop = $remaining;
                $remaining = 0;
              } elsif($l_chop == $remaining) {
                $remaining = 0;
              } else { # remaining > l_chop
                $remaining -= $l_chop;
              }
            }
            if($r_chop) {
              if($r_chop > $remaining) {
                $r_chop = $remaining;
                $remaining = 0;
              } elsif($r_chop == $remaining) {
                $remaining = 0;
              } else { # remaining > r_chop
                $remaining -= $r_chop; # should never happen!
              }
            }

            substr($b1->[$line], -$l_chop) = '' if $l_chop;
            substr($r, 0, $r_chop) = '' if $r_chop;
          } # else no-op
          $b1->[ $line ] .= $r . (' ' x $o->{'h_spacing'});
        }
         # End of H-scrunching ickyness
      }
       # End of ye big tack-on

    }
     # End of the foreach daughter_box loop

    # remove any fencepost h_spacing
    if($o->{'h_spacing'}) {
      foreach my $line (@box) {
        substr($line, -$o->{'h_spacing'}) = '' if length($line);
      }
    }

    # end of catenation
    die "SPORK ERROR 958203: Freak!!!!!" unless @box;

    # Now tweak the pipes
    my $new_pipes = $box[0];
    my $pipe_count = $new_pipes =~ tr<|><+>;
    if($pipe_count < 2) {
      $new_pipes = "|";
    } else {
      my($init_space, $end_space);

      # Thanks to Gilles Lamiral for pointing out the need to set to '',
      #  to avoid -w warnings about undeffiness.

      if( $new_pipes =~ s<^( +)><>s ) {
        $init_space = $1;
      } else {
        $init_space = '';
      }

      if( $new_pipes =~ s<( +)$><>s ) {
        $end_space  = $1
      } else {
        $end_space = '';
      }

      $new_pipes =~ tr< ><->;
      substr($new_pipes,0,1) = "/";
      substr($new_pipes,-1,1) = "\\";

      $new_pipes = $init_space . $new_pipes . $end_space;
      # substr($new_pipes, int((length($new_pipes)), 1)) / 2) = "^"; # feh
    }

    # Now tack on the formatting for this node.
    if($o->{'v_compact'} == 2) {
      if(@daughters == 1) {
        unshift @box, "|", $printable_name;
      } else {
        unshift @box, "|", $printable_name, $new_pipes;
      }
    } elsif ($o->{'v_compact'} == 1 and @daughters == 1) {
      unshift @box, "|", $printable_name;
    } else { # general case
      unshift @box, "|", $printable_name, $new_pipes;
    }
  }

  # Flush the edges:
  my $max_width = 0;
  foreach my $line (@box) {
    my $w = length($line);
    $max_width = $w if $w > $max_width;
  }
  foreach my $one (@box) {
    my $space_to_add = $max_width - length($one);
    next unless $space_to_add;
    my $add_left = int($space_to_add / 2);
    my $add_right = $space_to_add - $add_left;
    $one = (' ' x $add_left) . $one . (' ' x $add_right);
  }

  return \@box; # must not return a null list!
}

# -----------------------------------------------

sub dump_names {
  my($it, $o) = @_[0,1];
  $o = {} unless ref $o;
  my @out = ();
  $o->{'_depth'} ||= 0;
  $o->{'indent'} ||= '  ';
  $o->{'tick'} ||= '';

  $o->{'callback'} = sub {
      my($this, $o) = @_[0,1];
      push(@out,
        join('',
             $o->{'indent'} x $o->{'_depth'},
             $o->{'tick'},
             defined $this->name ? $this->name : $this,
             "\n"
        )
      );
      return 1;
    }
  ;
  $it->walk_down($o);
  return @out;
}

# -----------------------------------------------

sub format_node
{
	my($self, $options, $node) = @_;
	my($s) = $node -> name;
	$s     .= '. Attributes: ' . $self -> hashref2string($node -> attributes) if (! $$options{no_attributes});

	return $s;

} # End of format_node.

# -----------------------------------------------

sub generation {
  my($node, $limit) = @_[0,1];
  return $node
    if $node eq $limit || not(
			      defined($node->{'mother'}) &&
			      ref($node->{'mother'})
			     ); # bailout

  return map(@{$_->{'daughters'}}, $node->{'mother'}->generation($limit));
    # recurse!
    # Yup, my generation is just all the daughters of my mom's generation.
}

# -----------------------------------------------

sub generation_under {
  my($node, @rest) = @_;
  return $node->generation(@rest);
}

# -----------------------------------------------

sub hashref2string
{
	my($self, $hashref) = @_;
	$hashref ||= {};

	return '{' . join(', ', map{qq|$_ => "$$hashref{$_}"|} sort keys %$hashref) . '}';

} # End of hashref2string.

# -----------------------------------------------

sub _init { # method
  my $this = shift;
  my $o = ref($_[0]) eq 'HASH' ? $_[0] : {};

  # Sane initialization.
  $this->_init_mother($o);
  $this->_init_daughters($o);
  $this->_init_name($o);
  $this->_init_attributes($o);

  return;
}

# -----------------------------------------------

sub _init_attributes { # to be called by an _init
  my($this, $o) = @_[0,1];

  $this->{'attributes'} = {};

  # Undocumented and disfavored.  Consider this just an example.
  $this->attributes( $o->{'attributes'} ) if exists $o->{'attributes'};
}

# -----------------------------------------------

sub _init_daughters { # to be called by an _init
  my($this, $o) = @_[0,1];

  $this->{'daughters'} = [];

  # Undocumented and disfavored.  Consider this just an example.
  $this->set_daughters( @{$o->{'daughters'}} )
    if ref($o->{'daughters'}) && (@{$o->{'daughters'}});
  # DO NOT use this option (as implemented) with new_daughter or
  #  new_daughter_left!!!!!
  # BAD THINGS MAY HAPPEN!!!
}

# -----------------------------------------------

sub _init_mother { # to be called by an _init
  my($this, $o) = @_[0,1];

  $this->{'mother'} = undef;

  # Undocumented and disfavored.  Consider this just an example.
  ( $o->{'mother'} )->add_daughter($this)
    if defined($o->{'mother'}) && ref($o->{'mother'});
  # DO NOT use this option (as implemented) with new_daughter or
  #  new_daughter_left!!!!!
  # BAD THINGS MAY HAPPEN!!!
}

# -----------------------------------------------

sub _init_name { # to be called by an _init
  my($this, $o) = @_[0,1];

  $this->{'name'} = undef;

  # Undocumented and disfavored.  Consider this just an example.
  $this->name( $o->{'name'} ) if exists $o->{'name'};
}

# -----------------------------------------------

sub is_daughter_of {
  my($it,$mama) = @_[0,1];
  return $it->{'mother'} eq $mama;
}

# -----------------------------------------------

sub is_node { return 1; } # always true.
# NEVER override this with anything that returns false in the belief
#  that this'd signal "not a node class".  The existence of this method
#  is what I test for, with the various "can()" uses in this class.

# -----------------------------------------------

sub is_root
{
	my($self) = @_;

	return defined $self -> mother ? 0 : 1;

} # End of is_root.

# -----------------------------------------------

sub leaves_under {
  # read-only method:  return a list of all leaves under myself.
  # Returns myself in the degenerate case of being a leaf myself.
  my $node = shift;
  my @List = ();
  $node->walk_down({ 'callback' =>
    sub {
      my $node = $_[0];
      my @daughters = @{$node->{'daughters'}};
      push(@List, $node) unless @daughters;
      return 1;
    }
  });
  die "Spork Error 861: \@List has no contents!?!?" unless @List;
    # impossible
  return @List;
}

# -----------------------------------------------

sub left_sister {
  my $it = $_[0];
  my $mother = $it->{'mother'};
  return undef unless $mother;
  my @sisters = @{$mother->{'daughters'}};

  return undef if @sisters  == 1; # I'm an only daughter

  my $left = undef;
  foreach my $one (@sisters) {
    return $left if $one eq $it;
    $left = $one;
  }
  die "SPORK ERROR 9757: I'm not in my mother's daughter list!?!?";
}

# -----------------------------------------------

sub left_sisters {
  my $it = $_[0];
  my $mother = $it->{'mother'};
  return() unless $mother;
  my @sisters = @{$mother->{'daughters'}};
  return() if @sisters  == 1; # I'm an only daughter

  my @out = ();
  foreach my $one (@sisters) {
    return @out if $one eq $it;
    push @out, $one;
  }
  die "SPORK ERROR 9767: I'm not in my mother's daughter list!?!?";
}

# -----------------------------------------------

sub lol_to_tree {
  my($class, $lol, $seen_r) = @_[0,1,2];
  $seen_r = {} unless ref($seen_r) eq 'HASH';
  return if ref($lol) && $seen_r->{$lol}++; # catch circularity

  $class = ref($class) || $class;
  my $node = $class->new();

  unless(ref($lol) eq 'ARRAY') {  # It's a terminal node.
    $node->name($lol) if defined $lol;
    return $node;
  }
  return $node unless @$lol;  # It's a terminal node, oddly represented

  #  It's a non-terminal node.

  my @options = @$lol;
  unless(ref($options[-1]) eq 'ARRAY') {
    # This is what separates this method from simple_lol_to_tree
    $node->name(pop(@options));
  }

  foreach my $d (@options) {  # Scan daughters (whether scalars or listrefs)
    $node->add_daughter( $class->lol_to_tree($d, $seen_r) );  # recurse!
  }

  return $node;
}

# -----------------------------------------------

sub mother { # read-only attrib-method: returns an object (the mother node)
  my $this = shift;
  die "I'm a read-only method!" if @_;
  return $this->{'mother'};
}

# -----------------------------------------------

sub my_daughter_index {
  # returns what number is my index in my mother's daughter list
  # special case: 0 for root.
  my $node = $_[0];
  my $ord = -1;
  my $mother = $node->{'mother'};

  return 0 unless $mother;
  my @sisters = @{$mother->{'daughters'}};

  die "SPORK ERROR 6512:  My mother has no kids!!!" unless @sisters;

 Find_Self:
  for(my $i = 0; $i < @sisters; $i++) {
    if($sisters[$i] eq $node) {
      $ord = $i;
      last Find_Self;
    }
  }
  die "SPORK ERROR 2837: I'm not a daughter of my mother?!?!" if $ord == -1;
  return $ord;
}

# -----------------------------------------------

sub name { # read/write attribute-method.  returns/expects a scalar
  my $this = shift;
  $this->{'name'} = $_[0] if @_;
  return $this->{'name'};
}

# -----------------------------------------------

sub new { # constructor
  my $class = shift;
  $class = ref($class) if ref($class); # tchristic style.  why not?

  my $o = ref($_[0]) eq 'HASH' ? $_[0] : {}; # o for options hashref
  my $it = bless( {}, $class );
  print "Constructing $it in class $class\n" if $Debug;
  $it->_init( $o );
  return $it;
}

# -----------------------------------------------

sub new_daughter {
  my($mother, @options) = @_;
  my $daughter = $mother->new(@options);

  push @{$mother->{'daughters'}}, $daughter;
  $daughter->{'mother'} = $mother;

  return $daughter;
}

# -----------------------------------------------

sub new_daughter_left {
  my($mother, @options) = @_;
  my $daughter = $mother->new(@options);

  unshift @{$mother->{'daughters'}}, $daughter;
  $daughter->{'mother'} = $mother;

  return $daughter;
}

# -----------------------------------------------

sub node2string
{
	my($self, $options, $node, $vert_dashes) = @_;
	my($depth)         = scalar($node -> ancestors) || 0;
	my($sibling_count) = defined $node -> mother ? scalar $node -> self_and_sisters : 1;
	my($offset)        = ' ' x 5;
	my(@indent)        = map{$$vert_dashes[$_] || $offset} 0 .. $depth - 1;
	@$vert_dashes      =
	(
		@indent,
		($sibling_count == 1 ? $offset : '    |'),
	);

	if ($sibling_count == ($node -> my_daughter_index + 1) )
	{
		$$vert_dashes[$depth] = $offset;
	}

	return join('' => @indent[1 .. $#indent]) . ($depth ? '    |--- ' : '') . $self -> format_node($options, $node);

} # End of node2string.

# -----------------------------------------------

sub quote_name
{
	my($self, $name) = @_;

	return "'$name'";

} # End of quote_name.

# -----------------------------------------------

sub random_network { # constructor or method.
  my $class = $_[0];
  my $o = ref($_[1]) ? $_[1] : {};
  my $am_cons = 0;
  my $root;

  if(ref($class)){ # I'm a method.
    $root = $_[0]; # build under the given node, from same class.
    $class = ref $class;
    $am_cons = 0;
  } else { # I'm a constructor
    $root = $class->new; # build under a new node, with class named.
    $root->name("Root");
    $am_cons = 1;
  }

  my $min_depth = $o->{'min_depth'} || 2;
  my $max_depth = $o->{'max_depth'} || ($min_depth + 3);
  my $max_children = $o->{'max_children'} || 4;
  my $max_node_count = $o->{'max_node_count'} || 25;

  die "max_children has to be positive" if int($max_children) < 1;

  my @mothers = ( $root );
  my @children = ( );
  my $node_count = 1; # the root

 Gen:
  foreach my $depth (1 .. $max_depth) {
    last if $node_count > $max_node_count;
   Mother:
    foreach my $mother (@mothers) {
      last Gen if $node_count > $max_node_count;
      my $children_number;
      if($depth <= $min_depth) {
        until( $children_number = int(rand(1 + $max_children)) ) {}
      } else {
        $children_number = int(rand($max_children));
      }
     Beget:
      foreach (1 .. $children_number) {
        last Gen if $node_count > $max_node_count;
        my $node = $mother->new_daughter;
        $node->name("Node$node_count");
        ++$node_count;
        push(@children, $node);
      }
    }
    @mothers = @children;
    @children = ();
    last unless @mothers;
  }

  return $root;
}

# -----------------------------------------------

sub read_attributes
{
	my($self, $s) = @_;

	my($attributes);
	my($name);

	if ($s =~ /^(.+)\. Attributes: (\{.*\})$/)
	{
		($name, $attributes) = ($1, $self -> string2hashref($2) );
	}
	else
	{
		($name, $attributes) = ($s, {});
	}

	return Tree::DAG_Node -> new({name => $name, attributes => $attributes});

} # End of read_attributes.

# -----------------------------------------------

sub read_tree
{
	my($self, $file_name) = @_;
	my($count)       = 0;
	my($last_indent) = 0;
	my($test_string) = '--- ';
	my($test_length) = length $test_string;

	my($indent);
	my($node);
	my($offset);
	my($root);
	my(@stack);
	my($tos);

	for my $line (read_lines($file_name, binmode => ':encoding(utf-8)', chomp => 1) )
	{
		$count++;

		if ($count == 1)
		{
			$root = $node = $self -> read_attributes($line);
		}
		else
		{
			$indent = index($line, $test_string);

			if ($indent > $last_indent)
			{
				$tos = $node;

				push @stack, $node, $indent;
			}
			elsif ($indent < $last_indent)
			{
				$offset = $last_indent;

				while ($offset > $indent)
				{
					$offset = pop @stack;
					$tos    = pop @stack;
				}

				push @stack, $tos, $offset;
			}

			# Warning: The next line must set $node.
			# Don't put the RHS into the call to add_daughter()!

			$node        = $self -> read_attributes(substr($line, $indent + $test_length) );
			$last_indent = $indent;

			$tos -> add_daughter($node);
		}
	}

	return $root;

} # End of read_tree.

# -----------------------------------------------

sub remove_daughters { # write-only method
  my($mother, @daughters) = @_;
  die "mother must be an object!" unless ref $mother;
  return unless @daughters;

  my %to_delete;
  @daughters = grep {ref($_)
		       and defined($_->{'mother'})
		       and $mother eq $_->{'mother'}
                    } @daughters;
  return unless @daughters;
  @to_delete{ @daughters } = undef;

  # This could be done better and more efficiently, I guess.
  foreach my $daughter (@daughters) {
    $daughter->{'mother'} = undef;
  }
  my $them = $mother->{'daughters'};
  @$them = grep { !exists($to_delete{$_}) } @$them;

  # $mother->_update_daughter_links; # unnecessary
  return;
}

# -----------------------------------------------

sub remove_daughter { # alias
  my($it,@them) = @_;  $it->remove_daughters(@them);
}

# -----------------------------------------------

sub replace_with { # write-only method
  my($this, @replacements) = @_;

  if(not( defined($this->{'mother'}) && ref($this->{'mother'}) )) { # if root
    foreach my $replacement (@replacements) {
      $replacement->{'mother'}->remove_daughters($replacement)
        if $replacement->{'mother'};
    }
      # make 'em roots
  } else { # I have a mother
    my $mother = $this->{'mother'};

    #@replacements = grep(($_ eq $this  ||  $_->{'mother'} ne $mother),
    #                     @replacements);
    @replacements = grep { $_ eq $this
                           || not(defined($_->{'mother'}) &&
                                  ref($_->{'mother'}) &&
                                  $_->{'mother'} eq $mother
                                 )
                         }
                         @replacements;
    # Eliminate sisters (but not self)
    # i.e., I want myself or things NOT with the same mother as myself.

    $mother->set_daughters(   # old switcheroo
                           map($_ eq $this ? (@replacements) : $_ ,
                               @{$mother->{'daughters'}}
                              )
                          );
    # and set_daughters does all the checking and possible
    # unlinking
  }
  return($this, @replacements);
}

# -----------------------------------------------

sub replace_with_daughters { # write-only method
  my($this) = $_[0]; # takes no params other than the self
  my $mother = $this->{'mother'};
  return($this, $this->clear_daughters)
    unless defined($mother) && ref($mother);

  my @daughters = $this->clear_daughters;
  my $sib_r = $mother->{'daughters'};
  @$sib_r = map($_ eq $this ? (@daughters) : $_,
                @$sib_r   # old switcheroo
            );
  foreach my $daughter (@daughters) {
    $daughter->{'mother'} = $mother;
  }
  return($this, @daughters);
}

# -----------------------------------------------

sub right_sister {
  my $it = $_[0];
  my $mother = $it->{'mother'};
  return undef unless $mother;
  my @sisters = @{$mother->{'daughters'}};
  return undef if @sisters  == 1; # I'm an only daughter

  my $seen = 0;
  foreach my $one (@sisters) {
    return $one if $seen;
    $seen = 1 if $one eq $it;
  }
  die "SPORK ERROR 9777: I'm not in my mother's daughter list!?!?"
    unless $seen;
  return undef;
}

# -----------------------------------------------

sub right_sisters {
  my $it = $_[0];
  my $mother = $it->{'mother'};
  return() unless $mother;
  my @sisters = @{$mother->{'daughters'}};
  return() if @sisters  == 1; # I'm an only daughter

  my @out;
  my $seen = 0;
  foreach my $one (@sisters) {
    push @out, $one if $seen;
    $seen = 1 if $one eq $it;
  }
  die "SPORK ERROR 9787: I'm not in my mother's daughter list!?!?"
    unless $seen;
  return @out;
}

# -----------------------------------------------

sub root {
  my $it = $_[0];
  my @ancestors = ($it, $it->ancestors);
  return $ancestors[-1];
}

# -----------------------------------------------

sub self_and_descendants {
  # read-only method:  return a list of myself and any/all descendants
  my $node = shift;
  my @List = ();
  $node->walk_down({ 'callback' => sub { push @List, $_[0]; return 1;}});
  die "Spork Error 919: \@List has no contents!?!?" unless @List;
    # impossible
  return @List;
}

# -----------------------------------------------

sub self_and_sisters {
  my $node = $_[0];
  my $mother = $node->{'mother'};
  return $node unless defined($mother) && ref($mother);  # special case
  return @{$node->{'mother'}->{'daughters'}};
}

# -----------------------------------------------

sub set_daughters { # write-only method
  my($mother, @them) = @_;
  $mother->clear_daughters;
  $mother->add_daughters(@them) if @them;
  # yup, it's that simple
}

# -----------------------------------------------

sub simple_lol_to_tree {
  my($class, $lol, $seen_r) = @_[0,1,2];
  $class = ref($class) || $class;
  $seen_r = {} unless ref($seen_r) eq 'HASH';
  return if ref($lol) && $seen_r->{$lol}++; # catch circularity

  my $node = $class->new();

  unless(ref($lol) eq 'ARRAY') {  # It's a terminal node.
    $node->name($lol) if defined $lol;
    return $node;
  }

  #  It's a non-terminal node.
  foreach my $d (@$lol) { # scan daughters (whether scalars or listrefs)
    $node->add_daughter( $class->simple_lol_to_tree($d, $seen_r) );  # recurse!
  }

  return $node;
}

# -----------------------------------------------

sub sisters {
  my $node = $_[0];
  my $mother = $node->{'mother'};
  return() unless $mother;  # special case
  return grep($_ ne $node,
              @{$node->{'mother'}->{'daughters'}}
             );
}

# -----------------------------------------------

sub string2hashref
{
	my($self, $s) = @_;
	$s            ||= '';
	my($result)   = {};

	my($k);
	my($v);

	if ($s)
	{
		# Expect:
		# 1: The presence of the comma in "(',')" complicates things, so we can't use split(/\s*,\s*/, $s).
		#	{x => "(',')"}
		# 2: The presence of "=>" complicates things, so we can't use split(/\s*=>\s*/).
		#	{x => "=>"}
		# 3: So, assume ', ' is the outer separator, and then ' => ' is the inner separator.

		# Firstly, clean up the input, just to be safe.
		# None of these will match output from hashref2string($h).

		$s            =~ s/^\s*\{*//;
		$s            =~ s/\s*\}\s*$/\}/;
		my($finished) = 0;

		# The first '\' is for UltraEdit's syntax hiliting.

		my($reg_exp)  =
		qr/
			([\"'])([^"']*?)\1\s*=>\s*(["'])([^"']*?)\3,?\s*
			|
			(["'])([^"']*?)\5\s*=>\s*(.*?),?\s*
			|
			(.*?)\s*=>\s*(["'])([^"']*?)\9,?\s*
			|
			(.*?)\s*=>\s*(.*?),?\s*
		/sx;

		my(@got);

		while (! $finished)
		{
			if ($s =~ /$reg_exp/gc)
			{
				push @got, defined($2) ? ($2, $4) : defined($6) ? ($6, $7) : defined($8) ? ($8, $10) : ($11, $12);
			}
			else
			{
				$finished = 1;
			}
		}

		$result = {@got};
	}

	return $result;

} # End of string2hashref.

# -----------------------------------------------

sub tree_to_lol {
  # I haven't /rigorously/ tested this.
  my($it, $o) = @_[0,1]; # $o is currently unused anyway
  $o = {} unless ref $o;

  my $out = [];
  my @lol_stack = ($out);
  $o->{'callback'} = sub {
      my($this, $o) = @_[0,1];
      my $new = [];
      push @{$lol_stack[-1]}, $new;
      push(@lol_stack, $new);
      return 1;
    }
  ;
  $o->{'callbackback'} = sub {
      my($this, $o) = @_[0,1];
      my $name = defined $this->name ? $it -> quote_name($this->name) : 'undef';
      push @{$lol_stack[-1]}, $name;
      pop @lol_stack;
      return 1;
    }
  ;
  $it->walk_down($o);
  die "totally bizarre error 12416" unless ref($out->[0]);
  $out = $out->[0]; # the real root
  return $out;
}

# -----------------------------------------------

sub tree_to_lol_notation {
  my($it, $o) = @_[0,1];
  $o = {} unless ref $o;
  my @out = ();
  $o->{'_depth'} ||= 0;
  $o->{'multiline'} = 0 unless exists($o->{'multiline'});

  my $line_end;
  if($o->{'multiline'}) {
    $o->{'indent'} ||= '  ';
    $line_end = "\n";
  } else {
    $o->{'indent'} ||= '';
    $line_end = '';
  }

  $o->{'callback'} = sub {
      my($this, $o) = @_[0,1];
      push(@out,
             $o->{'indent'} x $o->{'_depth'},
             "[$line_end",
      );
      return 1;
    }
  ;
  $o->{'callbackback'} = sub {
      my($this, $o) = @_[0,1];
      my $name = defined $this->name ? $it -> quote_name($this->name) : 'undef';
      push(@out,
             $o->{'indent'} x ($o->{'_depth'} + 1),
             "$name$line_end",
             $o->{'indent'} x $o->{'_depth'},
             "],$line_end",
      );
      return 1;
    }
  ;
  $it->walk_down($o);
  return join('', @out);
}

# -----------------------------------------------

sub tree_to_simple_lol {
  # I haven't /rigorously/ tested this.
  my $root = $_[0];

  return $root->name unless scalar($root->daughters);
   # special case we have to nip in the bud

  my($it, $o) = @_[0,1]; # $o is currently unused anyway
  $o = {} unless ref $o;

  my $out = [];
  my @lol_stack = ($out);
  $o->{'callback'} = sub {
      my($this, $o) = @_[0,1];
      my $new;
      my $name = defined $this->name ? $it -> quote_name($this->name) : 'undef';
      $new = scalar($this->daughters) ? [] : $name;
        # Terminal nodes are scalars, the rest are listrefs we'll fill in
        # as we recurse the tree below here.
      push @{$lol_stack[-1]}, $new;
      push(@lol_stack, $new);
      return 1;
    }
  ;
  $o->{'callbackback'} = sub { pop @lol_stack; return 1; };
  $it->walk_down($o);
  die "totally bizarre error 12416" unless ref($out->[0]);
  $out = $out->[0]; # the real root
  return $out;
}

# -----------------------------------------------

sub tree_to_simple_lol_notation {
  my($it, $o) = @_[0,1];
  $o = {} unless ref $o;
  my @out = ();
  $o->{'_depth'} ||= 0;
  $o->{'multiline'} = 0 unless exists($o->{'multiline'});

  my $line_end;
  if($o->{'multiline'}) {
    $o->{'indent'} ||= '  ';
    $line_end = "\n";
  } else {
    $o->{'indent'} ||= '';
    $line_end = '';
  }

  $o->{'callback'} = sub {
      my($this, $o) = @_[0,1];
      if(scalar($this->daughters)) {   # Nonterminal
        push(@out,
               $o->{'indent'} x $o->{'_depth'},
               "[$line_end",
        );
      } else {   # Terminal
        my $name = defined $this->name ? $it -> quote_name($this->name) : 'undef';
        push @out,
          $o->{'indent'} x $o->{'_depth'},
          "$name,$line_end";
      }
      return 1;
    }
  ;
  $o->{'callbackback'} = sub {
      my($this, $o) = @_[0,1];
      push(@out,
             $o->{'indent'} x $o->{'_depth'},
             "], $line_end",
      ) if scalar($this->daughters);
      return 1;
    }
  ;

  $it->walk_down($o);
  return join('', @out);
}

# -----------------------------------------------

sub tree2string
{
	my($self, $options, $tree) = @_;
	$options                   ||= {};
	$$options{no_attributes}   ||= 0;
	$tree                      ||= $self;

	my(@out);
	my(@vert_dashes);

	$tree -> walk_down
	({
		callback =>
		sub
		{
			my($node) = @_;

			push @out, $self -> node2string($options, $node, \@vert_dashes);

			return 1,
		},
		_depth => 0,
	});

	return [@out];

} # End of tree2string.

# -----------------------------------------------

sub unlink_from_mother {
  my $node = $_[0];
  my $mother = $node->{'mother'};
  $mother->remove_daughters($node) if defined($mother) && ref($mother);
  return $mother;
}

# -----------------------------------------------

sub _update_daughter_links {
  # Eliminate any duplicates in my daughters list, and update
  #  all my daughters' links to myself.
  my $this = shift;

  my $them = $this->{'daughters'};

  # Eliminate duplicate daughters.
  my %seen = ();
  @$them = grep { ref($_) && not($seen{$_}++) } @$them;
   # not that there should ever be duplicate daughters anyhoo.

  foreach my $one (@$them) { # linkage bookkeeping
    die "daughter <$one> isn't an object!" unless ref $one;
    $one->{'mother'} = $this;
  }
  return;
}

# -----------------------------------------------

sub walk_down {
  my($this, $o) = @_[0,1];

  # All the can()s are in case an object changes class while I'm
  # looking at it.

  die "I need options!" unless ref($o);
  die "I need a callback or a callbackback" unless
    ( ref($o->{'callback'}) || ref($o->{'callbackback'}) );

  my $callback = ref($o->{'callback'}) ? $o->{'callback'} : undef;
  my $callbackback = ref($o->{'callbackback'}) ? $o->{'callbackback'} : undef;
  my $callback_status = 1;

  print "Callback: $callback   Callbackback: $callbackback\n" if $Debug;

  printf "* Entering %s\n", ($this->name || $this) if $Debug;
  $callback_status = &{ $callback }( $this, $o ) if $callback;

  if($callback_status) {
    # Keep recursing unless callback returned false... and if there's
    # anything to recurse into, of course.
    my @daughters = UNIVERSAL::can($this, 'is_node') ? @{$this->{'daughters'}} : ();
    if(@daughters) {
      $o->{'_depth'} += 1;
      #print "Depth " , $o->{'_depth'}, "\n";
      foreach my $one (@daughters) {
        $one->walk_down($o) if UNIVERSAL::can($one, 'is_node');
        # and if it can do "is_node", it should provide a walk_down!
      }
      $o->{'_depth'} -= 1;
    }
  } else {
    printf "* Recursing below %s pruned\n", ($this->name || $this) if $Debug;
  }

  # Note that $callback_status doesn't block callbackback from being called
  if($callbackback){
    if(UNIVERSAL::can($this, 'is_node')) { # if it's still a node!
      print "* Calling callbackback\n" if $Debug;
      scalar( &{ $callbackback }( $this, $o ) );
      # scalar to give it the same context as callback
    } else {
      print "* Can't call callbackback -- $this isn't a node anymore\n"
        if $Debug;
    }
  }
  if($Debug) {
    if(UNIVERSAL::can($this, 'is_node')) { # if it's still a node!
      printf "* Leaving %s\n", ($this->name || $this)
    } else {
      print "* Leaving [no longer a node]\n";
    }
  }
  return;
}

# -----------------------------------------------

1;

=pod

=encoding utf-8

=head1 NAME

Tree::DAG_Node - An N-ary tree

=head1 SYNOPSIS

=head2 Using as a base class

	package Game::Tree::Node;

	use parent 'Tree::DAG_Node';

	# Now add your own methods overriding/extending the methods in C<Tree::DAG_Node>...

=head2 Using as a class on its own

	use Tree::DAG_Node;

	my($root) = Tree::DAG_Node -> new({name => 'root', attributes => {uid => 0} });

	$root -> add_daughter(Tree::DAG_Node -> new({name => 'one', attributes => {uid => 1} }) );
	$root -> add_daughter(Tree::DAG_Node -> new({name => 'two', attributes => {} }) );
	$root -> add_daughter(Tree::DAG_Node -> new({name => 'three'}) ); # Attrs default to {}.

Or:

	my($count) = 0;
	my($tree)  = Tree::DAG_Node -> new({name => 'Root', attributes => {'uid' => $count} });

Or:

	my $root = Tree::DAG_Node -> new();

	$root -> name("I'm the tops");
	$root -> attributes({uid => 0});

	my $new_daughter = $root -> new_daughter;

	$new_daughter -> name('Another node');
	$new_daughter -> attributes({uid => 1});
	...

Lastly, for fancy wrappers - called _add_daughter() - around C<new()>, see these modules:
L<Marpa::Demo::StringParser> and L<GraphViz2::Marpa>. Both of these modules use L<Moo>.

See scripts/*.pl for other samples.

=head2 Using with utf-8 data

read_tree($file_name) works with utf-8 data. See t/read.tree.t and t/tree.utf8.attributes.txt.
Such a file can be created by redirecting the output of tree2string() to a file of type utf-8.

See the docs for L<Encode> for the difference between utf8 and utf-8. In brief, use utf-8.

See also scripts/write_tree.pl and scripts/read.tree.pl and scripts/read.tree.log.

=head1 DESCRIPTION

This class encapsulates/makes/manipulates objects that represent nodes
in a tree structure. The tree structure is not an object itself, but
is emergent from the linkages you create between nodes.  This class
provides the methods for making linkages that can be used to build up
a tree, while preventing you from ever making any kinds of linkages
which are not allowed in a tree (such as having a node be its own
mother or ancestor, or having a node have two mothers).

This is what I mean by a "tree structure", a bit redundantly stated:

=over 4

=item o A tree is a special case of an acyclic directed graph

=item o A tree is a network of nodes where there's exactly one root node

Also, the only primary relationship between nodes is the mother-daughter relationship.

=item o No node can be its own mother, or its mother's mother, etc

=item o Each node in the tree has exactly one parent

Except for the root of course, which is parentless.

=item o Each node can have any number (0 .. N) daughter nodes

A given node's daughter nodes constitute an I<ordered> list.

However, you are free to consider this ordering irrelevant.
Some applications do need daughters to be ordered, so I chose to
consider this the general case.

=item o A node can appear in only one tree, and only once in that tree

Notably (notable because it doesn't follow from the two above points),
a node cannot appear twice in its mother's daughter list.

=item o There's an idea of up versus down

Up means towards to the root, and down means away from the root (and towards the leaves).

=item o There's an idea of left versus right

Left is toward the start (index 0) of a given node's daughter list, and right is toward the end of a
given node's daughter list.

=back

Trees as described above have various applications, among them:
representing syntactic constituency, in formal linguistics;
representing contingencies in a game tree; representing abstract
syntax in the parsing of any computer language -- whether in
expression trees for programming languages, or constituency in the
parse of a markup language document.  (Some of these might not use the
fact that daughters are ordered.)

(Note: B-Trees are a very special case of the above kinds of trees,
and are best treated with their own class.  Check CPAN for modules
encapsulating B-Trees; or if you actually want a database, and for
some reason ended up looking here, go look at L<AnyDBM_File>.)

Many base classes are not usable except as such -- but C<Tree::DAG_Node>
can be used as a normal class.  You can go ahead and say:

	use Tree::DAG_Node;
	my $root = Tree::DAG_Node->new();
	$root->name("I'm the tops");
	$new_daughter = Tree::DAG_Node->new();
	$new_daughter->name("More");
	$root->add_daughter($new_daughter);

and so on, constructing and linking objects from C<Tree::DAG_Node> and
making useful tree structures out of them.

=head1 A NOTE TO THE READER

This class is big and provides lots of methods.  If your problem is
simple (say, just representing a simple parse tree), this class might
seem like using an atomic sledgehammer to swat a fly.  But the
complexity of this module's bells and whistles shouldn't detract from
the efficiency of using this class for a simple purpose.  In fact, I'd
be very surprised if any one user ever had use for more that even a
third of the methods in this class.  And remember: an atomic
sledgehammer B<will> kill that fly.

=head1 OBJECT CONTENTS

Implementationally, each node in a tree is an object, in the sense of
being an arbitrarily complex data structure that belongs to a class
(presumably C<Tree::DAG_Node>, or ones derived from it) that provides
methods.

The attributes of a node-object are:

=over

=item o mother -- this node's mother.  undef if this is a root

=item o daughters -- the (possibly empty) list of daughters of this node

=item o name -- the name for this node

Need not be unique, or even printable.  This is printed in some of the
various dumper methods, but it's up to you if you don't put anything
meaningful or printable here.

=item o attributes -- whatever the user wants to use it for

Presumably a hashref to whatever other attributes the user wants to
store without risk of colliding with the object's real attributes.
(Example usage: attributes to an SGML tag -- you definitely wouldn't
want the existence of a "mother=foo" pair in such a tag to collide with
a node object's 'mother' attribute.)

Aside from (by default) initializing it to {}, and having the access
method called "attributes" (described a ways below), I don't do
anything with the "attributes" in this module.  I basically intended
this so that users who don't want/need to bother deriving a class
from C<Tree::DAG_Node>, could still attach whatever data they wanted in a
node.

=back

"mother" and "daughters" are attributes that relate to linkage -- they
are never written to directly, but are changed as appropriate by the
"linkage methods", discussed below.

The other two (and whatever others you may add in derived classes) are
simply accessed thru the same-named methods, discussed further below.

=head2 About The Documented Interface

Stick to the documented interface (and comments in the source --
especially ones saying "undocumented!" and/or "disfavored!" -- do not
count as documentation!), and don't rely on any behavior that's not in
the documented interface.

Specifically, unless the documentation for a particular method says
"this method returns thus-and-such a value", then you should not rely on
it returning anything meaningful.

A I<passing> acquaintance with at least the broader details of the source
code for this class is assumed for anyone using this class as a base
class -- especially if you're overriding existing methods, and
B<definitely> if you're overriding linkage methods.

=head1 MAIN CONSTRUCTOR, AND INITIALIZER

=over

=item the constructor CLASS->new() or CLASS->new($options)

This creates a new node object, calls $object->_init($options)
to provide it sane defaults (like: undef name, undef mother, no
daughters, 'attributes' setting of a new empty hashref), and returns
the object created.  (If you just said "CLASS->new()" or "CLASS->new",
then it pretends you called "CLASS->new({})".)

See also the comments under L</new($hashref)> for options supported in the call to new().

If you use C<Tree::DAG_Node> as a superclass, and you add
attributes that need to be initialized, what you need to do is provide
an _init method that calls $this->SUPER::_init($options) to use its
superclass's _init method, and then initializes the new attributes:

  sub _init {
    my($this, $options) = @_[0,1];
    $this->SUPER::_init($options); # call my superclass's _init to
      # init all the attributes I'm inheriting

    # Now init /my/ new attributes:
    $this->{'amigos'} = []; # for example
  }

=item the constructor $obj->new() or $obj->new($options)

Just another way to get at the L</new($hashref)> method. This B<does not copy>
$obj, but merely constructs a new object of the same class as it.
Saves you the bother of going $class = ref $obj; $obj2 = $class->new;

=item the method $node->_init($options)

Initialize the object's attribute values.  See the discussion above.
Presumably this should be called only by the guts of the L</new($hashref)>
constructor -- never by the end user.

Currently there are no documented options for putting in the
$options hashref, but (in case you want to disregard the above rant)
the option exists for you to use $options for something useful
in a derived class.

Please see the source for more information.

=item see also (below) the constructors "new_daughter" and "new_daughter_left"

=back

=head1 METHODS

=head2 add_daughter(LIST)

An exact synonym for L</add_daughters(LIST)>.

=head2 add_daughters(LIST)

This method adds the node objects in LIST to the (right) end of
$mother's I<daughter> list.  Making a node N1 the daughter of another
node N2 also means that N1's I<mother> attribute is "automatically" set
to N2; it also means that N1 stops being anything else's daughter as
it becomes N2's daughter.

If you try to make a node its own mother, a fatal error results.  If
you try to take one of a node N1's ancestors and make it also a
daughter of N1, a fatal error results.  A fatal error results if
anything in LIST isn't a node object.

If you try to make N1 a daughter of N2, but it's B<already> a daughter
of N2, then this is a no-operation -- it won't move such nodes to the
end of the list or anything; it just skips doing anything with them.

=head2 add_daughter_left(LIST)

An exact synonym for L</add_daughters_left(LIST)>.

=head2 add_daughters_left(LIST)

This method is just like L</add_daughters(LIST)>, except that it adds the
node objects in LIST to the (left) beginning of $mother's daughter
list, instead of the (right) end of it.

=head2 add_left_sister(LIST)

An exact synonym for L</add_left_sisters(LIST)>.

=head2 add_left_sisters(LIST)

This adds the elements in LIST (in that order) as immediate left sisters of
$node.  In other words, given that B's mother's daughter-list is (A,B,C,D),
calling B->add_left_sisters(X,Y) makes B's mother's daughter-list
(A,X,Y,B,C,D).

If LIST is empty, this is a no-op, and returns empty-list.

This is basically implemented as a call to $node->replace_with(LIST,
$node), and so all replace_with's limitations and caveats apply.

The return value of $node->add_left_sisters(LIST) is the elements of
LIST that got added, as returned by replace_with -- minus the copies
of $node you'd get from a straight call to $node->replace_with(LIST,
$node).

=head2 add_right_sister(LIST)

An exact synonym for L</add_right_sisters(LIST)>.

=head2 add_right_sisters(LIST)

Just like add_left_sisters (which see), except that the elements
in LIST (in that order) as immediate B<right> sisters of $node;

In other words, given that B's mother's daughter-list is (A,B,C,D),
calling B->add_right_sisters(X,Y) makes B's mother's daughter-list
(A,B,X,Y,C,D).

=head2 address()

=head2 address(ADDRESS)

With the first syntax, returns the address of $node within its tree,
based on its position within the tree.  An address is formed by noting
the path between the root and $node, and concatenating the
daughter-indices of the nodes this passes thru (starting with 0 for
the root, and ending with $node).

For example, if to get from node ROOT to node $node, you pass thru
ROOT, A, B, and $node, then the address is determined as:

=over 4

=item o ROOT's my_daughter_index is 0

=item o A's my_daughter_index is, suppose, 2

A is index 2 in ROOT's daughter list.

=item o B's my_daughter_index is, suppose, 0

B is index 0 in A's daughter list.

=item o $node's my_daughter_index is, suppose, 4

$node is index 4 in B's daughter list.

=back

The address of the above-described $node is, therefore, "0:2:0:4".

(As a somewhat special case, the address of the root is always "0";
and since addresses start from the root, all addresses start with a
"0".)

The second syntax, where you provide an address, starts from the root
of the tree $anynode belongs to, and returns the node corresponding to
that address.  Returns undef if no node corresponds to that address.
Note that this routine may be somewhat liberal in its interpretation
of what can constitute an address; i.e., it accepts "0.2.0.4", besides
"0:2:0:4".

Also note that the address of a node in a tree is meaningful only in
that tree as currently structured.

(Consider how ($address1 cmp $address2) may be magically meaningful
to you, if you meant to figure out what nodes are to the right of what
other nodes.)

=head2 ancestors()

Returns the list of this node's ancestors, starting with its mother,
then grandmother, and ending at the root.  It does this by simply
following the 'mother' attributes up as far as it can.  So if $item IS
the root, this returns an empty list.

Consider that scalar($node->ancestors) returns the ply of this node
within the tree -- 2 for a granddaughter of the root, etc., and 0 for
root itself.

=head2 attribute()

=head2 attribute(SCALAR)

Exact synonyms for L</attributes()> and L</attributes(SCALAR)>.

=head2 attributes()

=head2 attributes(SCALAR)

In the first form, returns the value of the node object's "attributes"
attribute.  In the second form, sets it to the value of SCALAR.  I
intend this to be used to store a reference to a (presumably
anonymous) hash the user can use to store whatever attributes he
doesn't want to have to store as object attributes.  In this case, you
needn't ever set the value of this.  (_init has already initialized it
to {}.)  Instead you can just do...

  $node->attributes->{'foo'} = 'bar';

...to write foo => bar.

=head2 clear_daughters()

This unlinks all $mother's daughters.
Returns the list of what used to be $mother's daughters.

Not to be confused with L</remove_daughters(LIST)>.

=head2 common(LIST)

Returns the lowest node in the tree that is ancestor-or-self to the
nodes $node and LIST.

If the nodes are far enough apart in the tree, the answer is just the
root.

If the nodes aren't all in the same tree, the answer is undef.

As a degenerate case, if LIST is empty, returns $node.

=head2 common_ancestor(LIST)

Returns the lowest node that is ancestor to all the nodes given (in
nodes $node and LIST).  In other words, it answers the question: "What
node in the tree, as low as possible, is ancestor to the nodes given
($node and LIST)?"

If the nodes are far enough apart, the answer is just the root --
except if any of the nodes are the root itself, in which case the
answer is undef (since the root has no ancestor).

If the nodes aren't all in the same tree, the answer is undef.

As a degenerate case, if LIST is empty, returns $node's mother;
that'll be undef if $node is root.

=head2 copy($option)

Returns a copy of the calling node (the invocant). E.g.: my($copy) = $node -> copy;

$option is a hashref of options, with these (key => value) pairs:

=over 4

=item o no_attribute_copy => $Boolean

If set to 1, do not copy the node's attributes.

If not specified, defaults to 0, which copies attributes.

=back

=head2 copy_at_and_under()

=head2 copy_at_and_under($options)

This returns a copy of the subtree consisting of $node and everything
under it.

If you pass no options, copy_at_and_under pretends you've passed {}.

This works by recursively building up the new tree from the leaves,
duplicating nodes using $orig_node->copy($options_ref) and then
linking them up into a new tree of the same shape.

Options you specify are passed down to calls to $node->copy.

=head2 copy_tree()

=head2 copy_tree($options)

This returns the root of a copy of the tree that $node is a member of.
If you pass no options, copy_tree pretends you've passed {}.

This method is currently implemented as just a call to
$this->root->copy_at_and_under($options), but magic may be
added in the future.

Options you specify are passed down to calls to $node->copy.

=head2 daughters()

This returns the (possibly empty) list of daughters for $node.

=head2 decode_lol($lol)

Returns an arrayref having decoded the deeply nested structure $lol.

$lol will be the output of either tree_to_lol() or tree_to_simple_lol().

See scripts/read.tree.pl, and it's output file scripts/read.tree.log.

=head2 delete_tree()

Destroys the entire tree that $node is a member of (starting at the
root), by nulling out each node-object's attributes (including, most
importantly, its linkage attributes -- hopefully this is more than
sufficient to eliminate all circularity in the data structure), and
then moving it into the class DEADNODE.

Use this when you're finished with the tree in question, and want to
free up its memory.  (If you don't do this, it'll get freed up anyway
when your program ends.)

If you try calling any methods on any of the node objects in the tree
you've destroyed, you'll get an error like:

  Can't locate object method "leaves_under"
    via package "DEADNODE".

So if you see that, that's what you've done wrong.  (Actually, the
class DEADNODE does provide one method: a no-op method "delete_tree".
So if you want to delete a tree, but think you may have deleted it
already, it's safe to call $node->delete_tree on it (again).)

The L</delete_tree()> method is needed because Perl's garbage collector
would never (as currently implemented) see that it was time to
de-allocate the memory the tree uses -- until either you call
$node->delete_tree, or until the program stops (at "global
destruction" time, when B<everything> is unallocated).

Incidentally, there are better ways to do garbage-collecting on a
tree, ways which don't require the user to explicitly call a method
like L</delete_tree()> -- they involve dummy classes, as explained at
L<http://mox.perl.com/misc/circle-destroy.pod>

However, introducing a dummy class concept into C<Tree::DAG_Node> would
be rather a distraction.  If you want to do this with your derived
classes, via a DESTROY in a dummy class (or in a tree-metainformation
class, maybe), then feel free to.

The only case where I can imagine L</delete_tree()> failing to totally
void the tree, is if you use the hashref in the "attributes" attribute
to store (presumably among other things) references to other nodes'
"attributes" hashrefs -- which 1) is maybe a bit odd, and 2) is your
problem, because it's your hash structure that's circular, not the
tree's.  Anyway, consider:

      # null out all my "attributes" hashes
      $anywhere->root->walk_down({
        'callback' => sub {
          $hr = $_[0]->attributes; %$hr = (); return 1;
        }
      });
      # And then:
      $anywhere->delete_tree;

(I suppose L</delete_tree()> is a "destructor", or as close as you can
meaningfully come for a circularity-rich data structure in Perl.)

See also L</WHEN AND HOW TO DESTROY THE TREE>.

=head2 depth_under()

Returns an integer representing the number of branches between this
$node and the most distant leaf under it.  (In other words, this
returns the ply of subtree starting of $node.  Consider
scalar($it->ancestors) if you want the ply of a node within the whole
tree.)

=head2 descendants()

Returns a list consisting of all the descendants of $node.  Returns
empty-list if $node is a terminal_node.

(Note that it's spelled "descendants", not "descendents".)

=head2 draw_ascii_tree([$options])

Here, the [] refer to an optional parameter.

Returns an arrayref of lines suitable for printing.

Draws a nice ASCII-art representation of the tree structure.

The tree looks like:

	             |
	          <Root>
	   /-------+-----+---+---\
	   |       |     |   |   |
	  <I>     <H>   <D> <E> <B>
	 /---\   /---\   |   |   |
	 |   |   |   |  <F> <F> <C>
	<J> <J> <J> <J>  |   |
	 |   |   |   |  <G> <G>
	<K> <L> <K> <L>
	     |       |
	    <M>     <M>
	     |       |
	    <N>     <N>
	     |       |
	    <O>     <O>

See scripts/cut.and.paste.subtrees.pl.

Example usage:

  print map("$_\n", @{$tree->draw_ascii_tree});

I<draw_ascii_tree()> takes parameters you set in the $options hashref:

=over 4

=item o h_compact

Takes 0 or 1.  Sets the extent to which
I<draw_ascii_tree()> tries to save horizontal space.

If I think of a better scrunching algorithm, there'll be a "2" setting
for this.

Default: 1.

=item o h_spacing

Takes a number 0 or greater.  Sets the number of spaces
inserted horizontally between nodes (and groups of nodes) in a tree.

Default: 1.

=item o no_name

If true, I<draw_ascii_tree()> doesn't print the name of
the node; it simply prints a "*".

Default: 0 (i.e., print the node name.)

=item o v_compact

Takes a number 0, 1, or 2.  Sets the degree to which
I<draw_ascii_tree()> tries to save vertical space.  Defaults to 1.

=back

The code occasionally returns trees that are a bit cock-eyed in parts; if
anyone can suggest a better drawing algorithm, I'd be appreciative.

See also L</tree2string($options, [$some_tree])>.

=head2 dump_names($options)

Returns an array.

Dumps, as an indented list, the names of the nodes starting at $node,
and continuing under it.  Options are:

=over 4

=item o _depth -- A nonnegative number

Indicating the depth to consider $node as being at (and so the generation under that is that plus
one, etc.).  You may choose to use set _depth => scalar($node->ancestors).

Default: 0.

=item o tick -- a string to preface each entry with

This string goes between the indenting-spacing and the node's name.  You
may prefer "*" or "-> " or something.

Default: ''.

=item o indent -- the string used to indent with

Another sane value might be '. ' (period, space).  Setting it to empty-string suppresses indenting.

Default: ' ' x 2.

=back

The output is not printed, but is returned as a list, where each
item is a line, with a "\n" at the end.

=head2 format_node($options, $node)

Returns a string consisting of the node's name and, optionally, it's attributes.

Possible keys in the $options hashref:

=over 4

=item o no_attributes => $Boolean

If 1, the node's attributes are not included in the string returned.

Default: 0 (include attributes).

=back

Calls L</hashref2string($hashref)>.

Called by L</node2string($options, $node, $vert_dashes)>.

You would not normally call this method.

If you don't wish to supply options, use format_node({}, $node).

=head2 generation()

Returns a list of all nodes (going left-to-right) that are in $node's
generation -- i.e., that are the some number of nodes down from
the root.  $root->generation() is just $root.

Of course, $node is always in its own generation.

=head2 generation_under($node)

Like L</generation()>, but returns only the nodes in $node's generation
that are also descendants of $node -- in other words,

    @us = $node->generation_under( $node->mother->mother );

is all $node's first cousins (to borrow yet more kinship terminology) --
assuming $node does indeed have a grandmother.  Actually "cousins" isn't
quite an apt word, because C<@us> ends up including $node's siblings and
$node.

Actually, L</generation_under($node)> is just an alias to L</generation()>, but I
figure that this:

   @us = $node->generation_under($way_upline);

is a bit more readable than this:

   @us = $node->generation($way_upline);

But it's up to you.

$node->generation_under($node) returns just $node.

If you call $node->generation_under($node) but NODE2 is not $node or an
ancestor of $node, it behaves as if you called just $node->generation().

=head2 hashref2string($hashref)

Returns the given hashref as a string.

Called by L</format_node($options, $node)>.

=head2 is_daughter_of($node2)

Returns true iff $node is a daughter of $node2.
Currently implemented as just a test of ($it->mother eq $node2).

=head2 is_node()

This always returns true.  More pertinently, $object->can('is_node')
is true (regardless of what L</is_node()> would do if called) for objects
belonging to this class or for any class derived from it.

=head2 is_root()

Returns 1 if the caller is the root, and 0 if it is not.

=head2 leaves_under()

Returns a list (going left-to-right) of all the leaf nodes under
$node.  ("Leaf nodes" are also called "terminal nodes" -- i.e., nodes
that have no daughters.)  Returns $node in the degenerate case of
$node being a leaf itself.

=head2 left_sister()

Returns the node that's the immediate left sister of $node.  If $node
is the leftmost (or only) daughter of its mother (or has no mother),
then this returns undef.

See also L</add_left_sisters(LIST)> and L</add_right_sisters(LIST)>.

=head2 left_sisters()

Returns a list of nodes that're sisters to the left of $node.  If
$node is the leftmost (or only) daughter of its mother (or has no
mother), then this returns an empty list.

See also L</add_left_sisters(LIST)> and L</add_right_sisters(LIST)>.

=head2 lol_to_tree($lol)

This must be called as a class method.

Converts something like bracket-notation for "Chomsky trees" (or
rather, the closest you can come with Perl
list-of-lists(-of-lists(-of-lists))) into a tree structure.  Returns
the root of the tree converted.

The conversion rules are that:  1) if the last (possibly the only) item
in a given list is a scalar, then that is used as the "name" attribute
for the node based on this list.  2) All other items in the list
represent daughter nodes of the current node -- recursively so, if
they are list references; otherwise, (non-terminal) scalars are
considered to denote nodes with that name.  So ['Foo', 'Bar', 'N'] is
an alternate way to represent [['Foo'], ['Bar'], 'N'].

An example will illustrate:

  use Tree::DAG_Node;
  $lol =
    [
      [
        [ [ 'Det:The' ],
          [ [ 'dog' ], 'N'], 'NP'],
        [ '/with rabies\\', 'PP'],
        'NP'
      ],
      [ 'died', 'VP'],
      'S'
    ];
   $tree = Tree::DAG_Node->lol_to_tree($lol);
   $diagram = $tree->draw_ascii_tree;
   print map "$_\n", @$diagram;

...returns this tree:

                   |
                  <S>
                   |
                /------------------\
                |                  |
              <NP>                <VP>
                |                  |
        /---------------\        <died>
        |               |
      <NP>            <PP>
        |               |
     /-------\   </with rabies\>
     |       |
 <Det:The>  <N>
             |
           <dog>

By the way (and this rather follows from the above rules), when
denoting a LoL tree consisting of just one node, this:

  $tree = Tree::DAG_Node->lol_to_tree( 'Lonely' );

is okay, although it'd probably occur to you to denote it only as:

  $tree = Tree::DAG_Node->lol_to_tree( ['Lonely'] );

which is of course fine, too.

=head2 mother()

This returns what node is $node's mother.  This is undef if $node has
no mother -- i.e., if it is a root.

See also L</is_root()> and L</root()>.

=head2 my_daughter_index()

Returns what index this daughter is, in its mother's C<daughter> list.
In other words, if $node is ($node->mother->daughters)[3], then
$node->my_daughter_index returns 3.

As a special case, returns 0 if $node has no mother.

=head2 name()

=head2 name(SCALAR)

In the first form, returns the value of the node object's "name"
attribute.  In the second form, sets it to the value of SCALAR.

=head2 new($hashref)

These options are supported in $hashref:

=over 4

=item o attributes => A hashref of attributes

=item o daughters => An arrayref of nodes

=item o mother => A node

=item o name => A string

=back

See also L</MAIN CONSTRUCTOR, AND INITIALIZER> for a long discussion on object creation.

=head2 new_daughter()

=head2 new_daughter($options)

This B<constructs> a B<new> node (of the same class as $mother), and
adds it to the (right) end of the daughter list of $mother. This is
essentially the same as going

      $daughter = $mother->new;
      $mother->add_daughter($daughter);

but is rather more efficient because (since $daughter is guaranteed new
and isn't linked to/from anything), it doesn't have to check that
$daughter isn't an ancestor of $mother, isn't already daughter to a
mother it needs to be unlinked from, isn't already in $mother's
daughter list, etc.

As you'd expect for a constructor, it returns the node-object created.

Note that if you radically change 'mother'/'daughters' bookkeeping,
you may have to change this routine, since it's one of the places
that directly writes to 'daughters' and 'mother'.

=head2 new_daughter_left()

=head2 new_daughter_left($options)

This is just like $mother->new_daughter, but adds the new daughter
to the left (start) of $mother's daughter list.

Note that if you radically change 'mother'/'daughters' bookkeeping,
you may have to change this routine, since it's one of the places
that directly writes to 'daughters' and 'mother'.

=head2 node2string($options, $node, $vert_dashes)

Returns a string of the node's name and attributes, with a leading indent, suitable for printing.

Possible keys in the $options hashref:

=over 4

=item o no_attributes => $Boolean

If 1, the node's attributes are not included in the string returned.

Default: 0 (include attributes).

=back

Ignore the parameter $vert_dashes. The code uses it as temporary storage.

Calls L</format_node($options, $node)>.

Called by L</tree2string($options, [$some_tree])>.

=head2 quote_name($name)

Returns the string "'$name'", which is used in various methods for outputting node names.

=head2 random_network($options)

This method can be called as a class method or as an object method.

In the first case, constructs a randomly arranged network under a new
node, and returns the root node of that tree.  In the latter case,
constructs the network under $node.

Currently, this is implemented a bit half-heartedly, and
half-wittedly.  I basically needed to make up random-looking networks
to stress-test the various tree-dumper methods, and so wrote this.  If
you actually want to rely on this for any application more
serious than that, I suggest examining the source code and seeing if
this does really what you need (say, in reliability of randomness);
and feel totally free to suggest changes to me (especially in the form
of "I rewrote L</random_network($options)>, here's the code...")

It takes four options:

=over 4

=item o max_node_count -- maximum number of nodes this tree will be allowed to have (counting the
root)

Default: 25.

=item o min_depth -- minimum depth for the tree

Leaves can be generated only after this depth is reached, so the tree will be at
least this deep -- unless max_node_count is hit first.

Default: 2.

=item o max_depth -- maximum depth for the tree

The tree will not be deeper than this.

Default: 3 plus min_depth.

=item o max_children -- maximum number of children any mother in the tree can have.

Default: 4.

=back

=head2 read_attributes($s)

Parses the string $s and extracts the name and attributes, assuming the format is as generated by
L</tree2string($options, [$some_tree])>.

This bascially means the attribute string was generated by L</hashref2string($hashref)>.

Attributes may be absent, in which case they default to {}.

Returns a new node with this name and these attributes.

This method is for use by L</read_tree($file_name)>.

See t/tree.without.attributes.txt and t/tree.with.attributes.txt for sample data.

=head2 read_tree($file_name)

Returns the root of the tree read from $file_name.

The file must have been written by re-directing the output of
L</tree2string($options, [$some_tree])> to a file, since it makes assumptions about the format
of the stringified attributes.

read_tree() works with utf-8 data. See t/read.tree.t and t/tree.utf8.attributes.txt.

Note: To call this method you need a caller. It'll be a tree of 1 node. The reason is that inside
this method it calls various other methods, and for these calls it needs $self. That way, those
methods can be called from anywhere, and not just from within read_tree().

For reading and writing trees to databases, see L<Tree::DAG_Node::Persist>.

Calls L</string2hashref($s)>.

=head2 remove_daughter(LIST)

An exact synonym for L</remove_daughters(LIST)>.

=head2 remove_daughters(LIST)

This removes the nodes listed in LIST from $mother's daughter list.
This is a no-operation if LIST is empty.  If there are things in LIST
that aren't a current daughter of $mother, they are ignored.

Not to be confused with L</clear_daughters()>.

=head2 replace_with(LIST)

This replaces $node in its mother's daughter list, by unlinking $node
and replacing it with the items in LIST.  This returns a list consisting
of $node followed by LIST, i.e., the nodes that replaced it.

LIST can include $node itself (presumably at most once).  LIST can
also be the empty list.  However, if any items in LIST are sisters to
$node, they are ignored, and are not in the copy of LIST passed as the
return value.

As you might expect for any linking operation, the items in LIST
cannot be $node's mother, or any ancestor to it; and items in LIST are,
of course, unlinked from their mothers (if they have any) as they're
linked to $node's mother.

(In the special (and bizarre) case where $node is root, this simply calls
$this->unlink_from_mother on all the items in LIST, making them roots of
their own trees.)

Note that the daughter-list of $node is not necessarily affected; nor
are the daughter-lists of the items in LIST.  I mention this in case you
think replace_with switches one node for another, with respect to its
mother list B<and> its daughter list, leaving the rest of the tree
unchanged. If that's what you want, replacing $Old with $New, then you
want:

  $New->set_daughters($Old->clear_daughters);
  $Old->replace_with($New);

(I can't say $node's and LIST-items' daughter lists are B<never>
affected my replace_with -- they can be affected in this case:

  $N1 = ($node->daughters)[0]; # first daughter of $node
  $N2 = ($N1->daughters)[0];   # first daughter of $N1;
  $N3 = Tree::DAG_Node->random_network; # or whatever
  $node->replace_with($N1, $N2, $N3);

As a side affect of attaching $N1 and $N2 to $node's mother, they're
unlinked from their parents ($node, and $N1, respectively).
But N3's daughter list is unaffected.

In other words, this method does what it has to, as you'd expect it
to.

=head2 replace_with_daughters()

This replaces $node in its mother's daughter list, by unlinking $node
and replacing it with its daughters.  In other words, $node becomes
motherless and daughterless as its daughters move up and take its place.
This returns a list consisting of $node followed by the nodes that were
its daughters.

In the special (and bizarre) case where $node is root, this simply
unlinks its daughters from it, making them roots of their own trees.

Effectively the same as $node->replace_with($node->daughters), but more
efficient, since less checking has to be done.  (And I also think
$node->replace_with_daughters is a more common operation in
tree-wrangling than $node->replace_with(LIST), so deserves a named
method of its own, but that's just me.)

Note that if you radically change 'mother'/'daughters' bookkeeping,
you may have to change this routine, since it's one of the places
that directly writes to 'daughters' and 'mother'.

=head2 right_sister()

Returns the node that's the immediate right sister of $node.  If $node
is the rightmost (or only) daughter of its mother (or has no mother),
then this returns undef.

See also L</add_left_sisters(LIST)> and L</add_right_sisters(LIST)>.

=head2 right_sisters()

Returns a list of nodes that're sisters to the right of $node. If
$node is the rightmost (or only) daughter of its mother (or has no
mother), then this returns an empty list.

See also L</add_left_sisters(LIST)> and L</add_right_sisters(LIST)>.

=head2 root()

Returns the root of whatever tree $node is a member of.  If $node is
the root, then the result is $node itself.

Not to be confused with L</is_root()>.

=head2 self_and_descendants()

Returns a list consisting of itself (as element 0) and all the
descendants of $node.  Returns just itself if $node is a
terminal_node.

(Note that it's spelled "descendants", not "descendents".)

=head2 self_and_sisters()

Returns a list of all nodes (going left-to-right) that have the same
mother as $node -- including $node itself. This is just like
$node->mother->daughters, except that that fails where $node is root,
whereas $root->self_and_siblings, as a special case, returns $root.

(Contrary to how you may interpret how this method is named, "self" is
not (necessarily) the first element of what's returned.)

=head2 set_daughters(LIST)

This unlinks all $mother's daughters, and replaces them with the
daughters in LIST.

Currently implemented as just $mother->clear_daughters followed by
$mother->add_daughters(LIST).

=head2 simple_lol_to_tree($simple_lol)

This must be called as a class method.

This is like lol_to_tree, except that rule 1 doesn't apply -- i.e.,
all scalars (or really, anything not a listref) in the LoL-structure
end up as named terminal nodes, and only terminal nodes get names
(and, of course, that name comes from that scalar value).  This method
is useful for making things like expression trees, or at least
starting them off.  Consider that this:

    $tree = Tree::DAG_Node->simple_lol_to_tree(
      [ 'foo', ['bar', ['baz'], 'quux'], 'zaz', 'pati' ]
    );

converts from something like a Lispish or Iconish tree, if you pretend
the brackets are parentheses.

Note that there is a (possibly surprising) degenerate case of what I'm
calling a "simple-LoL", and it's like this:

  $tree = Tree::DAG_Node->simple_lol_to_tree('Lonely');

This is the (only) way you can specify a tree consisting of only a
single node, which here gets the name 'Lonely'.

=head2 sisters()

Returns a list of all nodes (going left-to-right) that have the same
mother as $node -- B<not including> $node itself.  If $node is root,
this returns empty-list.

=head2 string2hashref($s)

Returns the hashref built from the string.

The string is expected to be something like
'{AutoCommit => '1', PrintError => "0", ReportError => 1}'.

The empty string is returned as {}.

Called by L</read_tree($file_name)>.

=head2 tree_to_lol()

Returns that tree (starting at $node) represented as a LoL, like what
$lol, above, holds.  (This is as opposed to L</tree_to_lol_notation($options)>,
which returns the viewable code like what gets evaluated and stored in
$lol, above.)

Undefined node names are returned as the string 'undef'.

See also L</decode_lol($lol)>.

Lord only knows what you use this for -- maybe for feeding to
Data::Dumper, in case L</tree_to_lol_notation($options)> doesn't do just what you
want?

=head2 tree_to_lol_notation($options)

Dumps a tree (starting at $node) as the sort of LoL-like bracket
notation you see in the above example code.  Returns just one big
block of text.  The only option is "multiline" -- if true, it dumps
the text as the sort of indented structure as seen above; if false
(and it defaults to false), dumps it all on one line (with no
indenting, of course).

For example, starting with the tree from the above example,
this:

  print $tree->tree_to_lol_notation, "\n";

prints the following (which I've broken over two lines for sake of
printability of documentation):

  [[[['Det:The'], [['dog'], 'N'], 'NP'], [["/with rabies\x5c"],
  'PP'], 'NP'], [['died'], 'VP'], 'S'],

Doing this:

  print $tree->tree_to_lol_notation({ multiline => 1 });

prints the same content, just spread over many lines, and prettily
indented.

Undefined node names are returned as the string 'undef'.

=head2 tree_to_simple_lol()

Returns that tree (starting at $node) represented as a simple-LoL --
i.e., one where non-terminal nodes are represented as listrefs, and
terminal nodes are gotten from the contents of those nodes' "name'
attributes.

Note that in the case of $node being terminal, what you get back is
the same as $node->name.

Compare to tree_to_simple_lol_notation.

Undefined node names are returned as the string 'undef'.

See also L</decode_lol($lol)>.

=head2 tree_to_simple_lol_notation($options)

A simple-LoL version of tree_to_lol_notation (which see); takes the
same options.

Undefined node names are returned as the string 'undef'.

=head2 tree2string($options, [$some_tree])

Here, the [] represent an optional parameter.

Returns an arrayref of lines, suitable for printing.

Draws a nice ASCII-art representation of the tree structure.

The tree looks like:

	Root. Attributes: {}
	    |--- Â. Attributes: {# => "ÂÂ"}
	    |    |--- â. Attributes: {# => "ââ"}
	    |    |    |--- É. Attributes: {# => "ÉÉ"}
	    |    |--- ä. Attributes: {# => "ää"}
	    |    |--- é. Attributes: {# => "éé"}
	    |         |--- Ñ. Attributes: {# => "ÑÑ"}
	    |              |--- ñ. Attributes: {# => "ññ"}
	    |                   |--- Ô. Attributes: {# => "ÔÔ"}
	    |                        |--- ô. Attributes: {# => "ôô"}
	    |                        |--- ô. Attributes: {# => "ôô"}
	    |--- ß. Attributes: {# => "ßß"}
	         |--- ®. Attributes: {# => "®®"}
	         |    |--- ©. Attributes: {# => "©©"}
	         |--- £. Attributes: {# => "££"}
	         |--- €. Attributes: {# => "€€"}
	         |--- √. Attributes: {# => "√√"}
	         |--- ×xX. Attributes: {# => "×xX×xX"}
	              |--- í. Attributes: {# => "íí"}
	              |--- ú. Attributes: {# => "úú"}
	              |--- «. Attributes: {# => "««"}
	              |--- ». Attributes: {# => "»»"}

Or, without attributes:

	Root
	    |--- Â
	    |    |--- â
	    |    |    |--- É
	    |    |--- ä
	    |    |--- é
	    |         |--- Ñ
	    |              |--- ñ
	    |                   |--- Ô
	    |                        |--- ô
	    |                        |--- ô
	    |--- ß
	         |--- ®
	         |    |--- ©
	         |--- £
	         |--- €
	         |--- √
	         |--- ×xX
	              |--- í
	              |--- ú
	              |--- «
	              |--- »

See scripts/cut.and.paste.subtrees.pl.

Example usage:

  print map("$_\n", @{$tree->tree2string});

Can be called with $some_tree set to any $node, and will print the tree assuming $node is the root.

If you don't wish to supply options, use tree2string({}, $node).

Possible keys in the $options hashref (which defaults to {}):

=over 4

=item o no_attributes => $Boolean

If 1, the node's attributes are not included in the string returned.

Default: 0 (include attributes).

=back

Calls L</node2string($options, $node, $vert_dashes)>.

See also L</draw_ascii_tree([$options])>.

=head2 unlink_from_mother()

This removes node from the daughter list of its mother.  If it has no
mother, this is a no-operation.

Returns the mother unlinked from (if any).

=head2 walk_down($options)

Performs a depth-first traversal of the structure at and under $node.
What it does at each node depends on the value of the options hashref,
which you must provide.  There are three options, "callback" and
"callbackback" (at least one of which must be defined, as a sub
reference), and "_depth".

This is what I<walk_down()> does, in pseudocode form:

=over 4

=item o Starting point

Start at the $node given.

=item o Callback

If there's a I<callback>, call it with $node as the first argument,
and the options hashref as the second argument (which contains the
potentially useful I<_depth>, remember).  This function must return
true or false -- if false, it will block the next step:

=item o Daughters

If $node has any daughter nodes, increment I<_depth>, and call
$daughter->walk_down($options) for each daughter (in order, of
course), where options_hashref is the same hashref it was called with.
When this returns, decrements I<_depth>.

=item Callbackback

If there's a I<callbackback>, call just it as with I<callback> (but
tossing out the return value).  Note that I<callback> returning false
blocks traversal below $node, but doesn't block calling callbackback
for $node.  (Incidentally, in the unlikely case that $node has stopped
being a node object, I<callbackback> won't get called.)

=item o Return

=back

$node->walk_down($options) is the way to recursively do things to a tree (if you
start at the root) or part of a tree; if what you're doing is best done
via pre-pre order traversal, use I<callback>; if what you're doing is
best done with post-order traversal, use I<callbackback>.
I<walk_down()> is even the basis for plenty of the methods in this
class.  See the source code for examples both simple and horrific.

Note that if you don't specify I<_depth>, it effectively defaults to
0.  You should set it to scalar($node->ancestors) if you want
I<_depth> to reflect the true depth-in-the-tree for the nodes called,
instead of just the depth below $node.  (If $node is the root, there's
no difference, of course.)

And B<by the way>, it's a bad idea to modify the tree from the callback.
Unpredictable things may happen.  I instead suggest having your callback
add to a stack of things that need changing, and then, once I<walk_down()>
is all finished, changing those nodes from that stack.

Note that the existence of I<walk_down()> doesn't mean you can't write
you own special-use traversers.

=head1 WHEN AND HOW TO DESTROY THE TREE

It should be clear to you that if you've built a big parse tree or
something, and then you're finished with it, you should call
$some_node->delete_tree on it if you want the memory back.

But consider this case:  you've got this tree:

      A
    / | \
   B  C  D
   |     | \
   E     X  Y

Let's say you decide you don't want D or any of its descendants in the
tree, so you call D->unlink_from_mother.  This does NOT automagically
destroy the tree D-X-Y.  Instead it merely splits the tree into two:

     A                        D
    / \                      / \
   B   C                    X   Y
   |
   E

To destroy D and its little tree, you have to explicitly call
delete_tree on it.

Note, however, that if you call C->unlink_from_mother, and if you don't
have a link to C anywhere, then it B<does> magically go away.  This is
because nothing links to C -- whereas with the D-X-Y tree, D links to
X and Y, and X and Y each link back to D. Note that calling
C->delete_tree is harmless -- after all, a tree of only one node is
still a tree.

So, this is a surefire way of getting rid of all $node's children and
freeing up the memory associated with them and their descendants:

  foreach my $it ($node->clear_daughters) { $it->delete_tree }

Just be sure not to do this:

  foreach my $it ($node->daughters) { $it->delete_tree }
  $node->clear_daughters;

That's bad; the first call to $_->delete_tree will climb to the root
of $node's tree, and nuke the whole tree, not just the bits under $node.
You might as well have just called $node->delete_tree.
(Moreavor, once $node is dead, you can't call clear_daughters on it,
so you'll get an error there.)

=head1 BUG REPORTS

If you find a bug in this library, report it to me as soon as possible,
at the address listed in the MAINTAINER section, below.  Please try to
be as specific as possible about how you got the bug to occur.

=head1 HELP!

If you develop a given routine for dealing with trees in some way, and
use it a lot, then if you think it'd be of use to anyone else, do email
me about it; it might be helpful to others to include that routine, or
something based on it, in a later version of this module.

It's occurred to me that you might like to (and might yourself develop
routines to) draw trees in something other than ASCII art.  If you do so
-- say, for PostScript output, or for output interpretable by some
external plotting program --  I'd be most interested in the results.

=head1 RAMBLINGS

This module uses "strict", but I never wrote it with -w warnings in
mind -- so if you use -w, do not be surprised if you see complaints
from the guts of DAG_Node.  As long as there is no way to turn off -w
for a given module (instead of having to do it in every single
subroutine with a "local $^W"), I'm not going to change this. However,
I do, at points, get bursts of ambition, and I try to fix code in
DAG_Node that generates warnings, I<as I come across them> -- which is
only occasionally.  Feel free to email me any patches for any such
fixes you come up with, tho.

Currently I don't assume (or enforce) anything about the class
membership of nodes being manipulated, other than by testing whether
each one provides a method L</is_node()>, a la:

  die "Not a node!!!" unless UNIVERSAL::can($node, "is_node");

So, as far as I'm concerned, a given tree's nodes are free to belong to
different classes, just so long as they provide/inherit L</is_node()>, the
few methods that this class relies on to navigate the tree, and have the
same internal object structure, or a superset of it. Presumably this
would be the case for any object belonging to a class derived from
C<Tree::DAG_Node>, or belonging to C<Tree::DAG_Node> itself.

When routines in this class access a node's "mother" attribute, or its
"daughters" attribute, they (generally) do so directly (via
$node->{'mother'}, etc.), for sake of efficiency.  But classes derived
from this class should probably do this instead thru a method (via
$node->mother, etc.), for sake of portability, abstraction, and general
goodness.

However, no routines in this class (aside from, necessarily, I<_init()>,
I<_init_name()>, and L</name()>) access the "name" attribute directly;
routines (like the various tree draw/dump methods) get the "name" value
thru a call to $obj->name().  So if you want the object's name to not be
a real attribute, but instead have it derived dynamically from some feature
of the object (say, based on some of its other attributes, or based on
its address), you can to override the L</name()> method, without causing
problems.  (Be sure to consider the case of $obj->name as a write
method, as it's used in I</lol_to_tree($lol)> and L</random_network($options)>.)

=head1 FAQ

=head2 Which is the best tree processing module?

C<Tree::DAG_Node>, as it happens. More details: L</SEE ALSO>.

=head2 How to process every node in tree?

See L</walk_down($options)>. $options normally looks like this, assuming we wish to pass in
an arrayref as a stack:

	my(@stack);

	$tree -> walk_down
	({
		callback =>
		sub
		{
			my(@node, $options) = @_;

			# Process $node, using $options...

			push @{$$options{stack} }, $node -> name;

			return 1; # Keep walking.
		},
		_depth => 0,
		stack  => \@stack,
	});

	# Process @stack...

=head2 How do I switch from Tree to Tree::DAG_Node?

=over 4

=item o The node's name

In C<Tree> you use $node -> value and in C<Tree::DAG_Node> it's $node -> name.

=item o The node's attributes

In C<Tree> you use $node -> meta and in C<Tree::DAG_Node> it's $node -> attributes.

=back

=head2 Are there techniques for processing lists of nodes?

=over 4

=item o Copy the daughter list, and change it

	@them    = $mother->daughters;
	@removed = splice(@them, 0, 2, @new_nodes);

	$mother->set_daughters(@them);

=item o Select a sub-set of nodes

	$mother->set_daughters
	(
		grep($_->name =~ /wanted/, $mother->daughters)
	);

=back

=head2 Why did you break up the sections of methods in the POD?

Because I want to list the methods in alphabetical order.

=head2 Why did you move the POD to the end?

Because the apostrophes in the text confused the syntax hightlighter in my editor UltraEdit.

=head1 SEE ALSO

=over 4

=item o L<HTML::Element>, L<HTML::Tree> and L<HTML::TreeBuilder>

Sean is also the author of these modules.

=item o L<Tree>

Lightweight.

=item o L<Tree::Binary>

Lightweight.

=item o L<Tree::DAG_Node::Persist>

Lightweight.

=item o L<Tree::Persist>

Lightweight.

=item o L<Forest>

Uses L<Moose>.

=back

C<Tree::DAG_Node> itself is also lightweight.

=head1 REFERENCES

Wirth, Niklaus.  1976.  I<Algorithms + Data Structures = Programs>
Prentice-Hall, Englewood Cliffs, NJ.

Knuth, Donald Ervin.  1997.  I<Art of Computer Programming, Volume 1,
Third Edition: Fundamental Algorithms>.  Addison-Wesley,  Reading, MA.

Wirth's classic, currently and lamentably out of print, has a good
section on trees.  I find it clearer than Knuth's (if not quite as
encyclopedic), probably because Wirth's example code is in a
block-structured high-level language (basically Pascal), instead
of in assembler (MIX).

Until some kind publisher brings out a new printing of Wirth's book,
try poking around used bookstores (or C<www.abebooks.com>) for a copy.
I think it was also republished in the 1980s under the title
I<Algorithms and Data Structures>, and in a German edition called
I<Algorithmen und Datenstrukturen>.  (That is, I'm sure books by Knuth
were published under those titles, but I'm I<assuming> that they're just
later printings/editions of I<Algorithms + Data Structures =
Programs>.)

=head1 MACHINE-READABLE CHANGE LOG

The file Changes was converted into Changelog.ini by L<Module::Metadata::Changes>.

=head1 REPOSITORY

L<https://github.com/ronsavage/Tree-DAG_Node>

=head1 SUPPORT

Email the author, or log a bug on RT:

L<https://rt.cpan.org/Public/Dist/Display.html?Name=Tree-DAG_Node>.

=head1 ACKNOWLEDGEMENTS

The code to print the tree, in tree2string(), was adapted from
L<Forest::Tree::Writer::ASCIIWithBranches> by the dread Stevan Little.

=head1 MAINTAINER

David Hand, C<< <cogent@cpan.org> >> up to V 1.06.

Ron Savage C<< <rsavage@cpan.org> >> from V 1.07.

In this POD, usage of 'I' refers to Sean, up until V 1.07.

=head1 AUTHOR

Sean M. Burke, C<< <sburke@cpan.org> >>

=head1 COPYRIGHT, LICENSE, AND DISCLAIMER

Copyright 1998-2001, 2004, 2007 by Sean M. Burke and David Hand.

This Program of ours is 'OSI Certified Open Source Software';
you can redistribute it and/or modify it under the terms of
The Perl License, a copy of which is available at:
http://dev.perl.org/licenses/

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=cut
