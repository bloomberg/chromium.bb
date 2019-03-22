# Pretty-printers for libc++.

# Copyright (C) 2008-2013 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import re
import gdb
import sys

if sys.version_info[0] > 2:
    # Python 3 stuff
    Iterator = object
    # Python 3 folds these into the normal functions.
    imap = map
    izip = zip
    # Also, int subsumes long
    long = int
else:
    # Python 2 stuff
    class Iterator:
        """Compatibility mixin for iterators

        Instead of writing next() methods for iterators, write
        __next__() methods and use this mixin to make them work in
        Python 2 as well as Python 3.

        Idea stolen from the "six" documentation:
        <http://pythonhosted.org/six/#six.Iterator>
        """

        def next(self):
            return self.__next__()

    # In Python 2, we still need these from itertools
    from itertools import imap, izip

# Try to use the new-style pretty-printing if available.
_use_gdb_pp = True
try:
    import gdb.printing
except ImportError:
    _use_gdb_pp = False

# Try to install type-printers.
_use_type_printing = False
try:
    import gdb.types
    if hasattr(gdb.types, 'TypePrinter'):
        _use_type_printing = True
except ImportError:
    pass


# Starting with the type ORIG, search for the member type NAME.  This
# handles searching upward through superclasses.  This is needed to
# work around http://sourceware.org/bugzilla/show_bug.cgi?id=13615.
def find_type(orig, name):
    typ = orig.strip_typedefs()
    while True:
        search = str(typ) + '::' + name
        try:
            return gdb.lookup_type(search)
        except RuntimeError:
            pass
        # The type was not found, so try the superclass.  We only need
        # to check the first superclass, so we don't bother with
        # anything fancier here.
        field = typ.fields()[0]
        if not field.is_base_class:
            raise ValueError("Cannot find type %s::%s" % (orig, name))
        typ = field.type


def pair_to_tuple(val):
    if val.type.name.startswith("std::__1::__compressed_pair"):
        t1 = val.type.template_argument(0)
        t2 = val.type.template_argument(1)

        base1 = val.type.fields()[0].type
        base2 = val.type.fields()[1].type

        return (
            (val if base1.template_argument(2)
             else val.cast(base1)["__value_"]).cast(t1),
            (val if base2.template_argument(2)
             else val.cast(base2)["__value_"]).cast(t2)
        )

    else:
        return (val['first'], val['second'])

void_type = gdb.lookup_type('void')

def ptr_to_void_ptr(val):
  if gdb.types.get_basic_type(val.type).code == gdb.TYPE_CODE_PTR:
    return val.cast(void_type.pointer())
  else:
    return val
  


class StdStringPrinter:
    "Print a std::basic_string of some kind"

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename

    def to_string(self):
        # Make sure &string works, too.
        type = self.val.type
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target()

        ss = pair_to_tuple(self.val['__r_'])[0]['__s']
        __short_mask = 0x1
        if (ss['__size_'] & __short_mask) == 0:
            len = (ss['__size_'] >> 1)
            ptr = ss['__data_']
        else:
            sl = pair_to_tuple(self.val['__r_'])[0]['__l']
            len = sl['__size_']
            ptr = sl['__data_']

        return ptr.string(length=len)

    def display_hint(self):
        return 'string'


class SharedPointerPrinter:
    "Print a shared_ptr or weak_ptr"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def to_string(self):
        state = 'empty'
        refcounts = self.val['__cntrl_']
        if refcounts != 0:
            usecount = refcounts['__shared_owners_'] + 1
            weakcount = refcounts['__shared_weak_owners_']
            if usecount == 0:
                state = 'expired, weak %d' % weakcount
            else:
                state = 'count %d, weak %d' % (usecount, weakcount)

        return '%s (%s) = %s => %s' % (self.typename, state, self.val['__ptr_'], self.val['__ptr_'].dereference())


class UniquePointerPrinter:
    "Print a unique_ptr"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def to_string(self):
        v = pair_to_tuple(self.val['__ptr_'])[0]
        if v == 0:
            return '%s<%s> = %s <nullptr>' % (str(self.typename), str(v.type.target()), str(v))
        return '%s<%s> = %s => %s' % (str(self.typename), str(v.type.target()), str(v), v.dereference())


class StdPairPrinter:
    "Print a std::pair"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        vals = pair_to_tuple(self.val)
        return [('first', vals[0]),
                ('second', vals[1])]

    def to_string(self):
        return 'pair'

#    def display_hint(self):
#        return 'array'


class StdTuplePrinter:
    "Print a std::tuple"

    class _iterator(Iterator):

        def __init__(self, head):
            self.head = head['base_']
            self.fields = self.head.type.fields()
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= len(self.fields):
                raise StopIteration
            field = self.head.cast(self.fields[self.count].type)['value']
            self.count += 1
            return ('[%d]' % (self.count - 1), field)

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        return self._iterator(self.val)

    def to_string(self):
        if len(self.val.type.fields()) == 0:
            return 'empty %s' % (self.typename)
        return 'tuple'

#    def display_hint(self):
#        return 'array'


class StdListPrinter:
    "Print a std::list"

    class _iterator(Iterator):

        def __init__(self, nodetype, head):
            self.nodetype = nodetype
            self.base = head['__next_']
            self.head = head.address
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.base == self.head:
                raise StopIteration
            elt = self.base.cast(self.nodetype).dereference()
            self.base = elt['__next_']
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, elt['__value_'])

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val

    def children(self):
        nodebase = find_type(self.val.type, '__node_base')
        nodetype = find_type(nodebase, '__node_pointer')
        nodetype = nodetype.strip_typedefs()
        return self._iterator(nodetype, self.val['__end_'])

    def to_string(self):
        if self.val['__end_']['__next_'] == self.val['__end_'].address:
            return 'empty %s' % (self.typename)
        return '%s' % (self.typename)

#    def display_hint(self):
#        return 'array'


class StdListIteratorPrinter:
    "Print std::list::iterator"

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename

    def to_string(self):
        return self.val['__ptr_']['__value_']


class StdForwardListPrinter:
    "Print a std::forward_list"

    class _iterator(Iterator):

        def __init__(self, head):
            self.node = head
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.node == 0:
                raise StopIteration

            result = ('[%d]' % self.count, self.node['__value_'])
            self.count += 1
            self.node = self.node['__next_']
            return result

    def __init__(self, typename, val):
        self.val = val
        self.typename = typename
        self.head = pair_to_tuple(val['__before_begin_'])[0]['__next_']

    def children(self):
        return self._iterator(self.head)

    def to_string(self):
        if self.head == 0:
            return 'empty %s' % (self.typename)
        return '%s' % (self.typename)


class StdVectorPrinter:
    "Print a std::vector"

    class _iterator(Iterator):

        def __init__(self, start, finish_or_size, bits_per_word, bitvec):
            self.bitvec = bitvec
            if bitvec:
                self.item = start
                self.so = 0
                self.size = finish_or_size
                self.bits_per_word = bits_per_word
            else:
                self.item = start
                self.finish = finish_or_size
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            count = self.count
            self.count = self.count + 1
            if self.bitvec:
                if count == self.size:
                    raise StopIteration
                elt = self.item.dereference()
                if elt & (1 << self.so):
                    obit = 1
                else:
                    obit = 0
                self.so = self.so + 1
                if self.so >= self.bits_per_word:
                    self.item = self.item + 1
                    self.so = 0
                return ('[%d]' % count, obit)
            else:
                if self.item == self.finish:
                    raise StopIteration
                elt = self.item.dereference()
                self.item = self.item + 1
                return ('[%d]' % count, elt)

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.is_bool = 0
        for f in val.type.fields():
            if f.name == '__bits_per_word':
                self.is_bool = 1

    def children(self):
        if self.is_bool:
            return self._iterator(self.val['__begin_'],
                                  self.val['__size_'],
                                  self.val['__bits_per_word'],
                                  self.is_bool)
        else:
            return self._iterator(self.val['__begin_'],
                                  self.val['__end_'],
                                  0,
                                  self.is_bool)

    def to_string(self):
        if self.is_bool:
            length = self.val['__size_']
            capacity = pair_to_tuple(self.val['__cap_alloc_'])[0] * self.val['__bits_per_word']
            if length == 0:
                return 'empty %s<bool> (capacity=%d)' % (self.typename, int(capacity))
            else:
                return '%s<bool> (length=%d, capacity=%d)' % (self.typename, int(length), int(capacity))
        else:
            start = ptr_to_void_ptr(self.val['__begin_'])
            finish = ptr_to_void_ptr(self.val['__end_'])
            end = ptr_to_void_ptr(pair_to_tuple(self.val['__end_cap_'])[0])
            length = finish - start
            capacity = end - start
            if length == 0:
                return 'empty %s (capacity=%d)' % (self.typename, int(capacity))
            else:
                return '%s (length=%d, capacity=%d)' % (self.typename, int(length), int(capacity))

    def display_hint(self):
        return 'array'


class StdVectorIteratorPrinter:
    "Print std::vector::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__i'].dereference()


class StdVectorBoolIteratorPrinter:
    "Print std::vector<bool>::iterator"

    def __init__(self, typename, val):
        self.segment = val['__seg_'].dereference()
        self.ctz = val['__ctz_']

    def to_string(self):
        if self.segment & (1 << self.ctz):
            return 1
        else:
            return 0


class StdDequePrinter:
    "Print a std::deque"

    class _iterator(Iterator):

        def __init__(self, size, block_size, start, map_begin, map_end):
            self.block_size = block_size
            self.count = 0
            self.end_p = size + start
            self.end_mp = map_begin + self.end_p / block_size
            self.p = 0
            self.mp = map_begin + start / block_size
            if map_begin == map_end:
                self.p = 0
                self.end_p = 0
            else:
                self.p = self.mp.dereference() + start % block_size
                self.end_p = self.end_mp.dereference() + self.end_p % block_size

        def __iter__(self):
            return self

        def __next__(self):
            old_p = self.p

            self.count += 1
            self.p += 1
            if (self.p - self.mp.dereference()) == self.block_size:
                self.mp += 1
                self.p = self.mp.dereference()

            if (self.mp > self.end_mp) or ((self.p > self.end_p) and (self.mp == self.end_mp)):
                raise StopIteration

            return ('[%d]' % int(self.count - 1), old_p.dereference())

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.size = pair_to_tuple(val['__size_'])[0]

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (size=%d)' % (self.typename, long(self.size))

    def children(self):
        if self.size == 0:
            return []
        block_map = self.val['__map_']
        size_of_value_type = self.val.type.template_argument(0).sizeof
        block_size = self.val['__block_size']
        if block_size.is_optimized_out:
            # Warning, this is pretty flaky
            block_size = 4096 / size_of_value_type if size_of_value_type < 256 else 16
        return self._iterator(self.size, block_size,
                              self.val['__start_'], block_map['__begin_'],
                              block_map['__end_'])

#    def display_hint (self):
#        return 'array'


class StdDequeIteratorPrinter:
    "Print std::deque::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__ptr_'].dereference()


class StdStackOrQueuePrinter:
    "Print a std::stack or std::queue"

    def __init__(self, typename, val):
        self.typename = typename
        self.visualizer = gdb.default_visualizer(val['c'])

    def children(self):
        return self.visualizer.children()

    def to_string(self):
        return '%s = %s' % (self.typename, self.visualizer.to_string())

    def display_hint(self):
        if hasattr(self.visualizer, 'display_hint'):
            return self.visualizer.display_hint()
        return None


class StdBitsetPrinter:
    "Print a std::bitset"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.bit_count = val.type.template_argument(0)

    def to_string(self):
        return '%s (length=%d)' % (self.typename, self.bit_count)

    def children(self):
        words = self.val['__first_']
        words_count = self.val['__n_words']
        bits_per_word = self.val['__bits_per_word']
        word_index = 0
        result = []

        while word_index < words_count:
            bit_index = 0
            word = words[word_index]
            while word != 0:
                if (word & 0x1) != 0:
                    result.append(
                        ('[%d]' % (word_index * bits_per_word + bit_index), 1))
                word >>= 1
                bit_index += 1
            word_index += 1

        return result


class StdSetPrinter:
    "Print a std::set or std::multiset"

    # Turn an RbtreeIterator into a pretty-print iterator.
    class _iterator(Iterator):

        def __init__(self, rbiter):
            self.rbiter = rbiter
            self.count = 0

        def __iter__(self):
            return self

        def __len__(self):
            return len(self.rbiter)

        def __next__(self):
            item = self.rbiter.__next__()
            item = item.dereference()['__value_']
            result = (('[%d]' % self.count), item)
            self.count += 1
            return result

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.rbiter = RbtreeIterator(self.val['__tree_'])

    def to_string(self):
        length = len(self.rbiter)
        if length == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, int(length))

    def children(self):
        return self._iterator(self.rbiter)

#    def display_hint (self):
#        return 'set'


class RbtreeIterator:

    def __init__(self, rbtree):
        self.node = rbtree['__begin_node_']
        self.size = pair_to_tuple(rbtree['__pair3_'])[0]
        self.node_pointer_type = gdb.lookup_type(
            rbtree.type.strip_typedefs().name + '::__node_pointer')
        self.count = 0

    def __iter__(self):
        return self

    def __len__(self):
        return int(self.size)

    def __next__(self):
        if self.count == self.size:
            raise StopIteration

        node = self.node.cast(self.node_pointer_type)
        result = node
        self.count += 1
        if self.count < self.size:
            # Compute the next node.
            if node.dereference()['__right_']:
                node = node.dereference()['__right_']
                while node.dereference()['__left_']:
                    node = node.dereference()['__left_']
            else:
                parent_node = node.dereference()['__parent_']
                while node != parent_node.dereference()['__left_']:
                    node = parent_node
                    parent_node = parent_node.dereference()['__parent_']
                node = parent_node

            self.node = node
        return result


class StdRbtreeIteratorPrinter:
    "Print std::set::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__ptr_']['__value_']


class StdMapPrinter:
    "Print a std::map or std::multimap"

    # Turn an RbtreeIterator into a pretty-print iterator.
    class _iterator(Iterator):

        def __init__(self, rbiter):
            self.rbiter = rbiter
            self.count = 0

        def __iter__(self):
            return self

        def __len__(self):
            return len(self.rbiter)

        def __next__(self):
            item = self.rbiter.__next__()
            item = item.dereference()['__value_']
            result = ('[%d] %s' % (self.count, str(
                item['__cc']['first'])), item['__cc']['second'])
            self.count += 1
            return result

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.rbiter = RbtreeIterator(val['__tree_'])

    def to_string(self):
        length = len(self.rbiter)
        if length == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, int(length))

    def children(self):
        return self._iterator(self.rbiter)

#    def display_hint (self):
#        return 'map'


class StdMapIteratorPrinter:
    "Print std::map::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        vals = pair_to_tuple(self.val['__i_']['__ptr_'])

        return '[%s] %s' % (vals[0], vals[1])


class HashtableIterator:

    def __init__(self, hashtable):
        self.node = pair_to_tuple(hashtable['__p1_'])[0]['__next_']
        self.size = pair_to_tuple(hashtable['__p2_'])[0]

    def __iter__(self):
        return self

    def __len__(self):
        return self.size

    def __next__(self):
        if self.node == 0:
            raise StopIteration
        hash_node_type = gdb.lookup_type(
            self.node.dereference().type.name + '::__node_pointer')
        node = self.node.cast(hash_node_type).dereference()
        self.node = node['__next_']
        value = node['__value_']
        try:
            # unordered_map's value is a union of __nc and __cc.
            value = value['__nc']
        except gdb.error:
            pass
        return value


class StdHashtableIteratorPrinter:
    "Print std::unordered_set::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return self.val['__node_']['__value_']


class StdUnorderedMapIteratorPrinter:
    "Print std::unordered_map::iterator"

    def __init__(self, typename, val):
        self.val = val

    def to_string(self):
        return '[%s] %s' % pair_to_tuple(self.val['__i_']['__node_'])


class UnorderedSetPrinter:
    "Print a std::unordered_set"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.hashtable = val['__table_']
        self.size = pair_to_tuple(self.hashtable['__p2_'])[0]
        self.hashtableiter = HashtableIterator(self.hashtable)

    def hashtable(self):
        return self.hashtable

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, self.size)

    def children(self):
        result = []
        count = 0
        for elt in self.hashtableiter:
            result.append(('[%d]' % count, str(elt)))
            count += 1
        return result


class UnorderedMapPrinter:
    "Print a std::unordered_map"

    def __init__(self, typename, val):
        self.typename = typename
        self.val = val
        self.hashtable = val['__table_']
        self.size = pair_to_tuple(self.hashtable['__p2_'])[0]
        self.hashtableiter = HashtableIterator(self.hashtable)

    def hashtable(self):
        return self.hashtable

    def to_string(self):
        if self.size == 0:
            return 'empty %s' % self.typename
        else:
            return '%s (count=%d)' % (self.typename, self.size)

    def children(self):
        result = []
        count = 0
        for elt in self.hashtableiter:
            result.append(
                ('[%d] %s' % (count, str(elt['first'])), elt['second']))
            count += 1
        return result

#    def display_hint (self):
#        return 'map'

# A "regular expression" printer which conforms to the
# "SubPrettyPrinter" protocol from gdb.printing.


class RxPrinter(object):

    def __init__(self, name, function):
        super(RxPrinter, self).__init__()
        self.name = name
        self.function = function
        self.enabled = True

    def invoke(self, value):
        if not self.enabled:
            return None
        return self.function(self.name, value)

# A pretty-printer that conforms to the "PrettyPrinter" protocol from
# gdb.printing.  It can also be used directly as an old-style printer.


class Printer(object):

    def __init__(self, name):
        super(Printer, self).__init__()
        self.name = name
        self.subprinters = []
        self.lookup = {}
        self.enabled = True
        self.compiled_rx = re.compile('^([a-zA-Z0-9_:]+)<.*>$')

    def add(self, name, function):
        # A small sanity check.
        # FIXME
        if not self.compiled_rx.match(name + '<>'):
            raise ValueError(
                'libstdc++ programming error: "%s" does not match' % name)
        printer = RxPrinter(name, function)
        self.subprinters.append(printer)
        self.lookup[name] = printer

    # Add a name using _GLIBCXX_BEGIN_NAMESPACE_VERSION.
    def add_version(self, base, name, function):
        self.add(base + name, function)
        self.add(base + '__1::' + name, function)

    # Add a name using _GLIBCXX_BEGIN_NAMESPACE_CONTAINER.
    def add_container(self, base, name, function):
        self.add_version(base, name, function)
        self.add_version(base + '__1::', name, function)

    @staticmethod
    def get_basic_type(type):
        # If it points to a reference, get the reference.
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target()

        # Get the unqualified type, stripped of typedefs.
        type = type.unqualified().strip_typedefs()

        return type.tag

    def __call__(self, val):
        typename = self.get_basic_type(val.type)
        if not typename:
            return None

        # All the types we match are template types, so we can use a
        # dictionary.
        match = self.compiled_rx.match(typename)
        if not match:
            return None

        basename = match.group(1)
        if basename in self.lookup:
            return self.lookup[basename].invoke(val)

        # Cannot find a pretty printer.  Return None.
        return None

libcxx_printer = None


class FilteringTypePrinter(object):

    def __init__(self, match, name):
        self.match = match
        self.name = name
        self.enabled = True

    class _recognizer(object):

        def __init__(self, match, name):
            self.match = match
            self.name = name
            self.type_obj = None

        def recognize(self, type_obj):
            if type_obj.tag is None:
                return None

            if self.type_obj is None:
                if not self.match in type_obj.tag:
                    # Filter didn't match.
                    return None
                try:
                    self.type_obj = gdb.lookup_type(self.name).strip_typedefs()
                except:
                    pass
            if self.type_obj == type_obj:
                return self.name
            return None

    def instantiate(self):
        return self._recognizer(self.match, self.name)


def add_one_type_printer(obj, match, name):
    printer = FilteringTypePrinter(match, 'std::' + name)
    gdb.types.register_type_printer(obj, printer)


def register_type_printers(obj):
    global _use_type_printing

    if not _use_type_printing:
        return

    for pfx in ('', 'w'):
        add_one_type_printer(obj, 'basic_string', pfx + 'string')
        add_one_type_printer(obj, 'basic_ios', pfx + 'ios')
        add_one_type_printer(obj, 'basic_streambuf', pfx + 'streambuf')
        add_one_type_printer(obj, 'basic_istream', pfx + 'istream')
        add_one_type_printer(obj, 'basic_ostream', pfx + 'ostream')
        add_one_type_printer(obj, 'basic_iostream', pfx + 'iostream')
        add_one_type_printer(obj, 'basic_stringbuf', pfx + 'stringbuf')
        add_one_type_printer(obj, 'basic_istringstream',
                             pfx + 'istringstream')
        add_one_type_printer(obj, 'basic_ostringstream',
                             pfx + 'ostringstream')
        add_one_type_printer(obj, 'basic_stringstream',
                             pfx + 'stringstream')
        add_one_type_printer(obj, 'basic_filebuf', pfx + 'filebuf')
        add_one_type_printer(obj, 'basic_ifstream', pfx + 'ifstream')
        add_one_type_printer(obj, 'basic_ofstream', pfx + 'ofstream')
        add_one_type_printer(obj, 'basic_fstream', pfx + 'fstream')
        add_one_type_printer(obj, 'basic_regex', pfx + 'regex')
        add_one_type_printer(obj, 'sub_match', pfx + 'csub_match')
        add_one_type_printer(obj, 'sub_match', pfx + 'ssub_match')
        add_one_type_printer(obj, 'match_results', pfx + 'cmatch')
        add_one_type_printer(obj, 'match_results', pfx + 'smatch')
        add_one_type_printer(obj, 'regex_iterator', pfx + 'cregex_iterator')
        add_one_type_printer(obj, 'regex_iterator', pfx + 'sregex_iterator')
        add_one_type_printer(obj, 'regex_token_iterator',
                             pfx + 'cregex_token_iterator')
        add_one_type_printer(obj, 'regex_token_iterator',
                             pfx + 'sregex_token_iterator')

    # Note that we can't have a printer for std::wstreampos, because
    # it shares the same underlying type as std::streampos.
    add_one_type_printer(obj, 'fpos', 'streampos')
    add_one_type_printer(obj, 'basic_string', 'u16string')
    add_one_type_printer(obj, 'basic_string', 'u32string')

    for dur in ('nanoseconds', 'microseconds', 'milliseconds',
                'seconds', 'minutes', 'hours'):
        add_one_type_printer(obj, 'duration', dur)

    add_one_type_printer(obj, 'linear_congruential_engine', 'minstd_rand0')
    add_one_type_printer(obj, 'linear_congruential_engine', 'minstd_rand')
    add_one_type_printer(obj, 'mersenne_twister_engine', 'mt19937')
    add_one_type_printer(obj, 'mersenne_twister_engine', 'mt19937_64')
    add_one_type_printer(obj, 'subtract_with_carry_engine', 'ranlux24_base')
    add_one_type_printer(obj, 'subtract_with_carry_engine', 'ranlux48_base')
    add_one_type_printer(obj, 'discard_block_engine', 'ranlux24')
    add_one_type_printer(obj, 'discard_block_engine', 'ranlux48')
    add_one_type_printer(obj, 'shuffle_order_engine', 'knuth_b')


def register_libcxx_printers(obj):
    "Register libc++ pretty-printers with objfile Obj."

    global _use_gdb_pp
    global libcxx_printer

    if _use_gdb_pp:
        gdb.printing.register_pretty_printer(obj, libcxx_printer)
    else:
        if obj is None:
            obj = gdb
        obj.pretty_printers.append(libcxx_printer)

    register_type_printers(obj)


def build_libcxx_dictionary():
    global libcxx_printer

    libcxx_printer = Printer("libc++-v1")

    # For _GLIBCXX_BEGIN_NAMESPACE_VERSION.
    vers = '(__1::)?'
    # For _GLIBCXX_BEGIN_NAMESPACE_CONTAINER.
    container = '(__cxx2011::' + vers + ')?'

    # libstdc++ objects requiring pretty-printing.
    # In order from:
    # http://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/a01847.html
    libcxx_printer.add_version('std::', 'basic_string', StdStringPrinter)
    libcxx_printer.add_container('std::', 'bitset', StdBitsetPrinter)
    libcxx_printer.add_container('std::', 'deque', StdDequePrinter)
    libcxx_printer.add_container('std::', 'list', StdListPrinter)
    libcxx_printer.add_container('std::', 'map', StdMapPrinter)
    libcxx_printer.add_container('std::', 'multimap', StdMapPrinter)
    libcxx_printer.add_container('std::', 'multiset', StdSetPrinter)
    libcxx_printer.add_version('std::', 'priority_queue',
                               StdStackOrQueuePrinter)
    libcxx_printer.add_version('std::', 'queue', StdStackOrQueuePrinter)
    libcxx_printer.add_version('std::', 'tuple', StdTuplePrinter)
    libcxx_printer.add_version('std::', 'pair', StdPairPrinter)
    libcxx_printer.add_container('std::', 'set', StdSetPrinter)
    libcxx_printer.add_version('std::', 'stack', StdStackOrQueuePrinter)
    libcxx_printer.add_version('std::', 'unique_ptr', UniquePointerPrinter)
    libcxx_printer.add_container('std::', 'vector', StdVectorPrinter)
    # vector<bool>

    # Printer registrations for classes compiled with -D_GLIBCXX_DEBUG.
    libcxx_printer.add('std::__debug::bitset', StdBitsetPrinter)
    libcxx_printer.add('std::__debug::deque', StdDequePrinter)
    libcxx_printer.add('std::__debug::list', StdListPrinter)
    libcxx_printer.add('std::__debug::map', StdMapPrinter)
    libcxx_printer.add('std::__debug::multimap', StdMapPrinter)
    libcxx_printer.add('std::__debug::multiset', StdSetPrinter)
    libcxx_printer.add('std::__debug::priority_queue', StdStackOrQueuePrinter)
    libcxx_printer.add('std::__debug::queue', StdStackOrQueuePrinter)
    libcxx_printer.add('std::__debug::set', StdSetPrinter)
    libcxx_printer.add('std::__debug::stack', StdStackOrQueuePrinter)
    libcxx_printer.add('std::__debug::unique_ptr', UniquePointerPrinter)
    libcxx_printer.add('std::__debug::vector', StdVectorPrinter)

    # For array - the default GDB pretty-printer seems reasonable.
    libcxx_printer.add_version('std::', 'shared_ptr', SharedPointerPrinter)
    libcxx_printer.add_version('std::', 'weak_ptr', SharedPointerPrinter)
    libcxx_printer.add_container('std::', 'unordered_map', UnorderedMapPrinter)
    libcxx_printer.add_container('std::', 'unordered_set', UnorderedSetPrinter)
    libcxx_printer.add_container('std::', 'unordered_multimap',
                                 UnorderedMapPrinter)
    libcxx_printer.add_container('std::', 'unordered_multiset',
                                 UnorderedSetPrinter)
    libcxx_printer.add_container(
        'std::', 'forward_list', StdForwardListPrinter)

    libcxx_printer.add_version('std::', 'shared_ptr', SharedPointerPrinter)
    libcxx_printer.add_version('std::', 'weak_ptr', SharedPointerPrinter)
    libcxx_printer.add_version('std::', 'unordered_map', UnorderedMapPrinter)
    libcxx_printer.add_version('std::', 'unordered_set', UnorderedSetPrinter)
    libcxx_printer.add_version('std::', 'unordered_multimap',
                               UnorderedMapPrinter)
    libcxx_printer.add_version('std::', 'unordered_multiset',
                               UnorderedSetPrinter)

    # These are the C++0x printer registrations for -D_GLIBCXX_DEBUG cases.
    libcxx_printer.add('std::__debug::unordered_map', UnorderedMapPrinter)
    libcxx_printer.add('std::__debug::unordered_set', UnorderedSetPrinter)
    libcxx_printer.add('std::__debug::unordered_multimap', UnorderedMapPrinter)
    libcxx_printer.add('std::__debug::unordered_multiset', UnorderedSetPrinter)
    libcxx_printer.add('std::__debug::forward_list', StdForwardListPrinter)

    libcxx_printer.add_container('std::', '__list_iterator',
                                 StdListIteratorPrinter)
    libcxx_printer.add_container('std::', '__list_const_iterator',
                                 StdListIteratorPrinter)
    libcxx_printer.add_version('std::', '__tree_iterator',
                               StdRbtreeIteratorPrinter)
    libcxx_printer.add_version('std::', '__tree_const_iterator',
                               StdRbtreeIteratorPrinter)
    libcxx_printer.add_version('std::', '__hash_iterator',
                               StdHashtableIteratorPrinter)
    libcxx_printer.add_version('std::', '__hash_const_iterator',
                               StdHashtableIteratorPrinter)
    libcxx_printer.add_version('std::', '__hash_map_iterator',
                               StdUnorderedMapIteratorPrinter)
    libcxx_printer.add_version('std::', '__hash_map_const_iterator',
                               StdUnorderedMapIteratorPrinter)
    libcxx_printer.add_version('std::', '__map_iterator',
                               StdMapIteratorPrinter)
    libcxx_printer.add_version('std::', '__map_const_iterator',
                               StdMapIteratorPrinter)
    libcxx_printer.add_container('std::', '__deque_iterator',
                                 StdDequeIteratorPrinter)
    libcxx_printer.add_version('std::', '__wrap_iter',
                               StdVectorIteratorPrinter)
    libcxx_printer.add_version('std::', '__bit_iterator',
                               StdVectorBoolIteratorPrinter)

    # Debug (compiled with -D_GLIBCXX_DEBUG) printer
    # registrations.  The Rb_tree debug iterator when unwrapped
    # from the encapsulating __gnu_debug::_Safe_iterator does not
    # have the __norm namespace. Just use the existing printer
    # registration for that.
    libcxx_printer.add('std::__norm::__list_iterator',
                       StdListIteratorPrinter)
    libcxx_printer.add('std::__norm::__list_const_iterator',
                       StdListIteratorPrinter)
    libcxx_printer.add('std::__norm::__deque_iterator',
                       StdDequeIteratorPrinter)

build_libcxx_dictionary()
