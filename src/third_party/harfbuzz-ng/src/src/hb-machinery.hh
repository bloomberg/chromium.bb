/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2012,2018  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_MACHINERY_HH
#define HB_MACHINERY_HH

#include "hb.hh"
#include "hb-blob.hh"

#include "hb-iter.hh"
#include "hb-vector.hh"


/*
 * Casts
 */

/* Cast to struct T, reference to reference */
template<typename Type, typename TObject>
static inline const Type& CastR(const TObject &X)
{ return reinterpret_cast<const Type&> (X); }
template<typename Type, typename TObject>
static inline Type& CastR(TObject &X)
{ return reinterpret_cast<Type&> (X); }

/* Cast to struct T, pointer to pointer */
template<typename Type, typename TObject>
static inline const Type* CastP(const TObject *X)
{ return reinterpret_cast<const Type*> (X); }
template<typename Type, typename TObject>
static inline Type* CastP(TObject *X)
{ return reinterpret_cast<Type*> (X); }

/* StructAtOffset<T>(P,Ofs) returns the struct T& that is placed at memory
 * location pointed to by P plus Ofs bytes. */
template<typename Type>
static inline const Type& StructAtOffset(const void *P, unsigned int offset)
{ return * reinterpret_cast<const Type*> ((const char *) P + offset); }
template<typename Type>
static inline Type& StructAtOffset(void *P, unsigned int offset)
{ return * reinterpret_cast<Type*> ((char *) P + offset); }

/* StructAfter<T>(X) returns the struct T& that is placed after X.
 * Works with X of variable size also.  X must implement get_size() */
template<typename Type, typename TObject>
static inline const Type& StructAfter(const TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }
template<typename Type, typename TObject>
static inline Type& StructAfter(TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }


/*
 * Size checking
 */

/* Check _assertion in a method environment */
#define _DEFINE_INSTANCE_ASSERTION1(_line, _assertion) \
  inline void _instance_assertion_on_line_##_line (void) const \
  { static_assert ((_assertion), ""); }
# define _DEFINE_INSTANCE_ASSERTION0(_line, _assertion) _DEFINE_INSTANCE_ASSERTION1 (_line, _assertion)
# define DEFINE_INSTANCE_ASSERTION(_assertion) _DEFINE_INSTANCE_ASSERTION0 (__LINE__, _assertion)

/* Check that _code compiles in a method environment */
#define _DEFINE_COMPILES_ASSERTION1(_line, _code) \
  inline void _compiles_assertion_on_line_##_line (void) const \
  { _code; }
# define _DEFINE_COMPILES_ASSERTION0(_line, _code) _DEFINE_COMPILES_ASSERTION1 (_line, _code)
# define DEFINE_COMPILES_ASSERTION(_code) _DEFINE_COMPILES_ASSERTION0 (__LINE__, _code)


#define DEFINE_SIZE_STATIC(size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) == (size)) \
  inline unsigned int get_size (void) const { return (size); } \
  enum { null_size = (size) }; \
  enum { min_size = (size) }; \
  enum { static_size = (size) }

#define DEFINE_SIZE_UNION(size, _member) \
  DEFINE_COMPILES_ASSERTION ((void) this->u._member.static_size) \
  DEFINE_INSTANCE_ASSERTION (sizeof(this->u._member) == (size)) \
  enum { null_size = (size) }; \
  enum { min_size = (size) }

#define DEFINE_SIZE_MIN(size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) >= (size)) \
  enum { null_size = (size) }; \
  enum { min_size = (size) }

#define DEFINE_SIZE_UNBOUNDED(size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) >= (size)) \
  enum { min_size = (size) }

#define DEFINE_SIZE_ARRAY(size, array) \
  DEFINE_COMPILES_ASSERTION ((void) (array)[0].static_size) \
  DEFINE_INSTANCE_ASSERTION (sizeof (*this) == (size) + VAR * sizeof ((array)[0])) \
  enum { null_size = (size) }; \
  enum { min_size = (size) }

#define DEFINE_SIZE_ARRAY_SIZED(size, array) \
  inline unsigned int get_size (void) const { return (size - (array).min_size + (array).get_size ()); } \
  DEFINE_SIZE_ARRAY(size, array)


/*
 * Dispatch
 */

template <typename Context, typename Return, unsigned int MaxDebugDepth>
struct hb_dispatch_context_t
{
  enum { max_debug_depth = MaxDebugDepth };
  typedef Return return_t;
  template <typename T, typename F>
  inline bool may_dispatch (const T *obj HB_UNUSED, const F *format HB_UNUSED) { return true; }
  static return_t no_dispatch_return_value (void) { return Context::default_return_value (); }
};


/*
 * Sanitize
 *
 *
 * === Introduction ===
 *
 * The sanitize machinery is at the core of our zero-cost font loading.  We
 * mmap() font file into memory and create a blob out of it.  Font subtables
 * are returned as a readonly sub-blob of the main font blob.  These table
 * blobs are then sanitized before use, to ensure invalid memory access does
 * not happen.  The toplevel sanitize API use is like, eg. to load the 'head'
 * table:
 *
 *   hb_blob_t *head_blob = hb_sanitize_context_t ().reference_table<OT::head> (face);
 *
 * The blob then can be converted to a head table struct with:
 *
 *   const head *head_table = head_blob->as<head> ();
 *
 * What the reference_table does is, to call hb_face_reference_table() to load
 * the table blob, sanitize it and return either the sanitized blob, or empty
 * blob if sanitization failed.  The blob->as() function returns the null
 * object of its template type argument if the blob is empty.  Otherwise, it
 * just casts the blob contents to the desired type.
 *
 * Sanitizing a blob of data with a type T works as follows (with minor
 * simplification):
 *
 *   - Cast blob content to T*, call sanitize() method of it,
 *   - If sanitize succeeded, return blob.
 *   - Otherwise, if blob is not writable, try making it writable,
 *     or copy if cannot be made writable in-place,
 *   - Call sanitize() again.  Return blob if sanitize succeeded.
 *   - Return empty blob otherwise.
 *
 *
 * === The sanitize() contract ===
 *
 * The sanitize() method of each object type shall return true if it's safe to
 * call other methods of the object, and false otherwise.
 *
 * Note that what sanitize() checks for might align with what the specification
 * describes as valid table data, but does not have to be.  In particular, we
 * do NOT want to be pedantic and concern ourselves with validity checks that
 * are irrelevant to our use of the table.  On the contrary, we want to be
 * lenient with error handling and accept invalid data to the extent that it
 * does not impose extra burden on us.
 *
 * Based on the sanitize contract, one can see that what we check for depends
 * on how we use the data in other table methods.  Ie. if other table methods
 * assume that offsets do NOT point out of the table data block, then that's
 * something sanitize() must check for (GSUB/GPOS/GDEF/etc work this way).  On
 * the other hand, if other methods do such checks themselves, then sanitize()
 * does not have to bother with them (glyf/local work this way).  The choice
 * depends on the table structure and sanitize() performance.  For example, to
 * check glyf/loca offsets in sanitize() would cost O(num-glyphs).  We try hard
 * to avoid such costs during font loading.  By postponing such checks to the
 * actual glyph loading, we reduce the sanitize cost to O(1) and total runtime
 * cost to O(used-glyphs).  As such, this is preferred.
 *
 * The same argument can be made re GSUB/GPOS/GDEF, but there, the table
 * structure is so complicated that by checking all offsets at sanitize() time,
 * we make the code much simpler in other methods, as offsets and referenced
 * objects do not need to be validated at each use site.
 */

/* This limits sanitizing time on really broken fonts. */
#ifndef HB_SANITIZE_MAX_EDITS
#define HB_SANITIZE_MAX_EDITS 32
#endif
#ifndef HB_SANITIZE_MAX_OPS_FACTOR
#define HB_SANITIZE_MAX_OPS_FACTOR 8
#endif
#ifndef HB_SANITIZE_MAX_OPS_MIN
#define HB_SANITIZE_MAX_OPS_MIN 16384
#endif
#ifndef HB_SANITIZE_MAX_OPS_MAX
#define HB_SANITIZE_MAX_OPS_MAX 0x3FFFFFFF
#endif

struct hb_sanitize_context_t :
       hb_dispatch_context_t<hb_sanitize_context_t, bool, HB_DEBUG_SANITIZE>
{
  inline hb_sanitize_context_t (void) :
	debug_depth (0),
	start (nullptr), end (nullptr),
	max_ops (0),
	writable (false), edit_count (0),
	blob (nullptr),
	num_glyphs (65536),
	num_glyphs_set (false) {}

  inline const char *get_name (void) { return "SANITIZE"; }
  template <typename T, typename F>
  inline bool may_dispatch (const T *obj HB_UNUSED, const F *format)
  { return format->sanitize (this); }
  template <typename T>
  inline return_t dispatch (const T &obj) { return obj.sanitize (this); }
  static return_t default_return_value (void) { return true; }
  static return_t no_dispatch_return_value (void) { return false; }
  bool stop_sublookup_iteration (const return_t r) const { return !r; }

  inline void init (hb_blob_t *b)
  {
    this->blob = hb_blob_reference (b);
    this->writable = false;
  }

  inline void set_num_glyphs (unsigned int num_glyphs_)
  {
    num_glyphs = num_glyphs_;
    num_glyphs_set = true;
  }
  inline unsigned int get_num_glyphs (void) { return num_glyphs; }

  inline void set_max_ops (int max_ops_) { max_ops = max_ops_; }

  template <typename T>
  inline void set_object (const T *obj)
  {
    reset_object ();

    if (!obj) return;

    const char *obj_start = (const char *) obj;
    const char *obj_end = (const char *) obj + obj->get_size ();
    assert (obj_start <= obj_end); /* Must not overflow. */

    if (unlikely (obj_end < this->start || this->end < obj_start))
      this->start = this->end = nullptr;
    else
    {
      this->start = MAX (this->start, obj_start);
      this->end   = MIN (this->end  , obj_end  );
    }
  }

  inline void reset_object (void)
  {
    this->start = this->blob->data;
    this->end = this->start + this->blob->length;
    assert (this->start <= this->end); /* Must not overflow. */
  }

  inline void start_processing (void)
  {
    reset_object ();
    this->max_ops = MAX ((unsigned int) (this->end - this->start) * HB_SANITIZE_MAX_OPS_FACTOR,
			 (unsigned) HB_SANITIZE_MAX_OPS_MIN);
    this->edit_count = 0;
    this->debug_depth = 0;

    DEBUG_MSG_LEVEL (SANITIZE, start, 0, +1,
		     "start [%p..%p] (%lu bytes)",
		     this->start, this->end,
		     (unsigned long) (this->end - this->start));
  }

  inline void end_processing (void)
  {
    DEBUG_MSG_LEVEL (SANITIZE, this->start, 0, -1,
		     "end [%p..%p] %u edit requests",
		     this->start, this->end, this->edit_count);

    hb_blob_destroy (this->blob);
    this->blob = nullptr;
    this->start = this->end = nullptr;
  }

  inline bool check_range (const void *base,
			   unsigned int len) const
  {
    const char *p = (const char *) base;
    bool ok = this->start <= p &&
	      p <= this->end &&
	      (unsigned int) (this->end - p) >= len &&
	      this->max_ops-- > 0;

    DEBUG_MSG_LEVEL (SANITIZE, p, this->debug_depth+1, 0,
       "check_range [%p..%p] (%d bytes) in [%p..%p] -> %s",
       p, p + len, len,
       this->start, this->end,
       ok ? "OK" : "OUT-OF-RANGE");

    return likely (ok);
  }

  template <typename T>
  inline bool check_range (const T *base,
			   unsigned int a,
			   unsigned int b) const
  {
    return !hb_unsigned_mul_overflows (a, b) &&
	   this->check_range (base, a * b);
  }

  template <typename T>
  inline bool check_range (const T *base,
			   unsigned int a,
			   unsigned int b,
			   unsigned int c) const
  {
    return !hb_unsigned_mul_overflows (a, b) &&
	   this->check_range (base, a * b, c);
  }

  template <typename T>
  inline bool check_array (const T *base,
			   unsigned int len) const
  {
    return this->check_range (base, len, T::static_size);
  }

  template <typename T>
  inline bool check_array (const T *base,
			   unsigned int a,
			   unsigned int b) const
  {
    return this->check_range (base, a, b, T::static_size);
  }

  template <typename Type>
  inline bool check_struct (const Type *obj) const
  {
    return likely (this->check_range (obj, obj->min_size));
  }

  inline bool may_edit (const void *base, unsigned int len)
  {
    if (this->edit_count >= HB_SANITIZE_MAX_EDITS)
      return false;

    const char *p = (const char *) base;
    this->edit_count++;

    DEBUG_MSG_LEVEL (SANITIZE, p, this->debug_depth+1, 0,
       "may_edit(%u) [%p..%p] (%d bytes) in [%p..%p] -> %s",
       this->edit_count,
       p, p + len, len,
       this->start, this->end,
       this->writable ? "GRANTED" : "DENIED");

    return this->writable;
  }

  template <typename Type, typename ValueType>
  inline bool try_set (const Type *obj, const ValueType &v) {
    if (this->may_edit (obj, obj->static_size)) {
      const_cast<Type *> (obj)->set (v);
      return true;
    }
    return false;
  }

  template <typename Type>
  inline hb_blob_t *sanitize_blob (hb_blob_t *blob)
  {
    bool sane;

    init (blob);

  retry:
    DEBUG_MSG_FUNC (SANITIZE, start, "start");

    start_processing ();

    if (unlikely (!start))
    {
      end_processing ();
      return blob;
    }

    Type *t = CastP<Type> (const_cast<char *> (start));

    sane = t->sanitize (this);
    if (sane)
    {
      if (edit_count)
      {
	DEBUG_MSG_FUNC (SANITIZE, start, "passed first round with %d edits; going for second round", edit_count);

        /* sanitize again to ensure no toe-stepping */
        edit_count = 0;
	sane = t->sanitize (this);
	if (edit_count) {
	  DEBUG_MSG_FUNC (SANITIZE, start, "requested %d edits in second round; FAILLING", edit_count);
	  sane = false;
	}
      }
    }
    else
    {
      if (edit_count && !writable) {
        start = hb_blob_get_data_writable (blob, nullptr);
	end = start + blob->length;

	if (start)
	{
	  writable = true;
	  /* ok, we made it writable by relocating.  try again */
	  DEBUG_MSG_FUNC (SANITIZE, start, "retry");
	  goto retry;
	}
      }
    }

    end_processing ();

    DEBUG_MSG_FUNC (SANITIZE, start, sane ? "PASSED" : "FAILED");
    if (sane)
    {
      hb_blob_make_immutable (blob);
      return blob;
    }
    else
    {
      hb_blob_destroy (blob);
      return hb_blob_get_empty ();
    }
  }

  template <typename Type>
  inline hb_blob_t *reference_table (const hb_face_t *face, hb_tag_t tableTag = Type::tableTag)
  {
    if (!num_glyphs_set)
      set_num_glyphs (hb_face_get_glyph_count (face));
    return sanitize_blob<Type> (hb_face_reference_table (face, tableTag));
  }

  mutable unsigned int debug_depth;
  const char *start, *end;
  mutable int max_ops;
  private:
  bool writable;
  unsigned int edit_count;
  hb_blob_t *blob;
  unsigned int num_glyphs;
  bool  num_glyphs_set;
};

struct hb_sanitize_with_object_t
{
  template <typename T>
  inline hb_sanitize_with_object_t (hb_sanitize_context_t *c,
				    const T& obj) : c (c)
  {
    c->set_object (obj);
  }
  inline ~hb_sanitize_with_object_t (void)
  {
    c->reset_object ();
  }

  private:
  hb_sanitize_context_t *c;
};


/*
 * Serialize
 */

struct hb_serialize_context_t
{
  inline hb_serialize_context_t (void *start_, unsigned int size)
  {
    this->start = (char *) start_;
    this->end = this->start + size;
    reset ();
  }

  inline void reset (void)
  {
    this->ran_out_of_room = false;
    this->head = this->start;
    this->debug_depth = 0;
  }

  inline bool err (bool e) { return this->ran_out_of_room = this->ran_out_of_room || e; }

  /* To be called around main operation. */
  template <typename Type>
  inline Type *start_serialize (void)
  {
    DEBUG_MSG_LEVEL (SERIALIZE, this->start, 0, +1,
		     "start [%p..%p] (%lu bytes)",
		     this->start, this->end,
		     (unsigned long) (this->end - this->start));

    return start_embed<Type> ();
  }
  inline void end_serialize (void)
  {
    DEBUG_MSG_LEVEL (SERIALIZE, this->start, 0, -1,
		     "end [%p..%p] serialized %d bytes; %s",
		     this->start, this->end,
		     (int) (this->head - this->start),
		     this->ran_out_of_room ? "RAN OUT OF ROOM" : "did not ran out of room");
  }

  inline unsigned int length (void) const { return this->head - this->start; }

  inline void align (unsigned int alignment)
  {
    unsigned int l = length () % alignment;
    if (l)
      allocate_size<void> (alignment - l);
  }

  template <typename Type>
  inline Type *start_embed (void) const
  {
    Type *ret = reinterpret_cast<Type *> (this->head);
    return ret;
  }

  template <typename Type>
  inline Type *allocate_size (unsigned int size)
  {
    if (unlikely (this->ran_out_of_room || this->end - this->head < ptrdiff_t (size))) {
      this->ran_out_of_room = true;
      return nullptr;
    }
    memset (this->head, 0, size);
    char *ret = this->head;
    this->head += size;
    return reinterpret_cast<Type *> (ret);
  }

  template <typename Type>
  inline Type *allocate_min (void)
  {
    return this->allocate_size<Type> (Type::min_size);
  }

  template <typename Type>
  inline Type *embed (const Type &obj)
  {
    unsigned int size = obj.get_size ();
    Type *ret = this->allocate_size<Type> (size);
    if (unlikely (!ret)) return nullptr;
    memcpy (ret, &obj, size);
    return ret;
  }

  template <typename Type>
  inline Type *extend_min (Type &obj)
  {
    unsigned int size = obj.min_size;
    assert (this->start <= (char *) &obj && (char *) &obj <= this->head && (char *) &obj + size >= this->head);
    if (unlikely (!this->allocate_size<Type> (((char *) &obj) + size - this->head))) return nullptr;
    return reinterpret_cast<Type *> (&obj);
  }

  template <typename Type>
  inline Type *extend (Type &obj)
  {
    unsigned int size = obj.get_size ();
    assert (this->start < (char *) &obj && (char *) &obj <= this->head && (char *) &obj + size >= this->head);
    if (unlikely (!this->allocate_size<Type> (((char *) &obj) + size - this->head))) return nullptr;
    return reinterpret_cast<Type *> (&obj);
  }

  /* Output routines. */
  template <typename Type>
  inline Type *copy (void) const
  {
    assert (!this->ran_out_of_room);
    unsigned int len = this->head - this->start;
    void *p = malloc (len);
    if (p)
      memcpy (p, this->start, len);
    return reinterpret_cast<Type *> (p);
  }
  inline hb_bytes_t copy_bytes (void) const
  {
    assert (!this->ran_out_of_room);
    unsigned int len = this->head - this->start;
    void *p = malloc (len);
    if (p)
      memcpy (p, this->start, len);
    else
      return hb_bytes_t ();
    return hb_bytes_t (p, len);
  }
  inline hb_blob_t *copy_blob (void) const
  {
    assert (!this->ran_out_of_room);
    return hb_blob_create (this->start,
			   this->head - this->start,
			   HB_MEMORY_MODE_DUPLICATE,
			   nullptr, nullptr);
  }

  public:
  unsigned int debug_depth;
  char *start, *end, *head;
  bool ran_out_of_room;
};


/*
 * Supplier
 */

template <typename Type>
struct Supplier
{
  inline Supplier (const Type *array, unsigned int len_, unsigned int stride_=sizeof (Type))
  {
    head = array;
    len = len_;
    stride = stride_;
  }
  inline Supplier (const hb_vector_t<Type> *v)
  {
    head = *v;
    len = v->len;
    stride = sizeof (Type);
  }

  inline const Type operator [] (unsigned int i) const
  {
    if (unlikely (i >= len)) return Type ();
    return * (const Type *) (const void *) ((const char *) head + stride * i);
  }

  inline Supplier<Type> & operator += (unsigned int count)
  {
    if (unlikely (count > len))
      count = len;
    len -= count;
    head = (const Type *) (const void *) ((const char *) head + stride * count);
    return *this;
  }

  private:
  inline Supplier (const Supplier<Type> &); /* Disallow copy */
  inline Supplier<Type>& operator= (const Supplier<Type> &); /* Disallow copy */

  unsigned int len;
  unsigned int stride;
  const Type *head;
};


/*
 * Big-endian integers.
 */

template <typename Type, int Bytes> struct BEInt;

template <typename Type>
struct BEInt<Type, 1>
{
  public:
  typedef Type type;
  inline void set (Type V)
  {
    v = V;
  }
  inline operator Type (void) const
  {
    return v;
  }
  private: uint8_t v;
};
template <typename Type>
struct BEInt<Type, 2>
{
  public:
  typedef Type type;
  inline void set (Type V)
  {
    v[0] = (V >>  8) & 0xFF;
    v[1] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
#if defined(__GNUC__) || defined(__clang__)
    /* Spoon-feed the compiler a big-endian integer with alignment 1.
     * https://github.com/harfbuzz/harfbuzz/pull/1398 */
    struct __attribute__((packed)) packed_uint16_t { uint16_t v; };
    return __builtin_bswap16 (((packed_uint16_t *) this)->v);
#endif
    return (v[0] <<  8)
         + (v[1]      );
  }
  private: uint8_t v[2];
};
template <typename Type>
struct BEInt<Type, 3>
{
  public:
  typedef Type type;
  inline void set (Type V)
  {
    v[0] = (V >> 16) & 0xFF;
    v[1] = (V >>  8) & 0xFF;
    v[2] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[0] << 16)
         + (v[1] <<  8)
         + (v[2]      );
  }
  private: uint8_t v[3];
};
template <typename Type>
struct BEInt<Type, 4>
{
  public:
  typedef Type type;
  inline void set (Type V)
  {
    v[0] = (V >> 24) & 0xFF;
    v[1] = (V >> 16) & 0xFF;
    v[2] = (V >>  8) & 0xFF;
    v[3] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[0] << 24)
         + (v[1] << 16)
         + (v[2] <<  8)
         + (v[3]      );
  }
  private: uint8_t v[4];
};


/*
 * Lazy loaders.
 */

template <typename Data, unsigned int WheresData>
struct hb_data_wrapper_t
{
  static_assert (WheresData > 0, "");

  inline Data * get_data (void) const
  {
    return *(((Data **) (void *) this) - WheresData);
  }

  inline bool is_inert (void) const { return !get_data (); }

  template <typename Stored, typename Subclass>
  inline Stored * call_create (void) const { return Subclass::create (get_data ()); }
};
template <>
struct hb_data_wrapper_t<void, 0>
{
  inline bool is_inert (void) const { return false; }

  template <typename Stored, typename Funcs>
  inline Stored * call_create (void) const { return Funcs::create (); }
};

template <typename T1, typename T2> struct hb_non_void_t { typedef T1 value; };
template <typename T2> struct hb_non_void_t<void, T2> { typedef T2 value; };

template <typename Returned,
	  typename Subclass = void,
	  typename Data = void,
	  unsigned int WheresData = 0,
	  typename Stored = Returned>
struct hb_lazy_loader_t : hb_data_wrapper_t<Data, WheresData>
{
  typedef typename hb_non_void_t<Subclass,
				 hb_lazy_loader_t<Returned,Subclass,Data,WheresData,Stored>
				>::value Funcs;

  inline void init0 (void) {} /* Init, when memory is already set to 0. No-op for us. */
  inline void init (void) { instance.set_relaxed (nullptr); }
  inline void fini (void)
  {
    do_destroy (instance.get ());
  }
  inline void free_instance (void)
  {
  retry:
    Stored *p = instance.get ();
    if (unlikely (p && !cmpexch (p, nullptr)))
      goto retry;
    do_destroy (p);
  }

  static inline void do_destroy (Stored *p)
  {
    if (p && p != const_cast<Stored *> (Funcs::get_null ()))
      Funcs::destroy (p);
  }

  inline const Returned * operator -> (void) const { return get (); }
  inline const Returned & operator * (void) const { return *get (); }
  explicit_operator inline operator bool (void) const
  { return get_stored () != Funcs::get_null (); }
  template <typename C> inline operator const C * (void) const { return get (); }

  inline Stored * get_stored (void) const
  {
  retry:
    Stored *p = this->instance.get ();
    if (unlikely (!p))
    {
      if (unlikely (this->is_inert ()))
	return const_cast<Stored *> (Funcs::get_null ());

      p = this->template call_create<Stored, Funcs> ();
      if (unlikely (!p))
	p = const_cast<Stored *> (Funcs::get_null ());

      if (unlikely (!cmpexch (nullptr, p)))
      {
        do_destroy (p);
	goto retry;
      }
    }
    return p;
  }
  inline Stored * get_stored_relaxed (void) const
  {
    return this->instance.get_relaxed ();
  }

  inline bool cmpexch (Stored *current, Stored *value) const
  {
    /* This *must* be called when there are no other threads accessing. */
    return this->instance.cmpexch (current, value);
  }

  inline const Returned * get (void) const { return Funcs::convert (get_stored ()); }
  inline const Returned * get_relaxed (void) const { return Funcs::convert (get_stored_relaxed ()); }
  inline Returned * get_unconst (void) const { return const_cast<Returned *> (Funcs::convert (get_stored ())); }

  /* To be possibly overloaded by subclasses. */
  static inline Returned* convert (Stored *p) { return p; }

  /* By default null/init/fini the object. */
  static inline const Stored* get_null (void) { return &Null(Stored); }
  static inline Stored *create (Data *data)
  {
    Stored *p = (Stored *) calloc (1, sizeof (Stored));
    if (likely (p))
      p->init (data);
    return p;
  }
  static inline Stored *create (void)
  {
    Stored *p = (Stored *) calloc (1, sizeof (Stored));
    if (likely (p))
      p->init ();
    return p;
  }
  static inline void destroy (Stored *p)
  {
    p->fini ();
    free (p);
  }

//  private:
  /* Must only have one pointer. */
  hb_atomic_ptr_t<Stored *> instance;
};

/* Specializations. */

template <typename T, unsigned int WheresFace>
struct hb_face_lazy_loader_t : hb_lazy_loader_t<T,
						hb_face_lazy_loader_t<T, WheresFace>,
						hb_face_t, WheresFace> {};

template <typename T, unsigned int WheresFace>
struct hb_table_lazy_loader_t : hb_lazy_loader_t<T,
						 hb_table_lazy_loader_t<T, WheresFace>,
						 hb_face_t, WheresFace,
						 hb_blob_t>
{
  static inline hb_blob_t *create (hb_face_t *face)
  {
    return hb_sanitize_context_t ().reference_table<T> (face);
  }
  static inline void destroy (hb_blob_t *p)
  {
    hb_blob_destroy (p);
  }
  static inline const hb_blob_t *get_null (void)
  {
      return hb_blob_get_empty ();
  }
  static inline const T* convert (const hb_blob_t *blob)
  {
    return blob->as<T> ();
  }

  inline hb_blob_t* get_blob (void) const
  {
    return this->get_stored ();
  }
};

template <typename Subclass>
struct hb_font_funcs_lazy_loader_t : hb_lazy_loader_t<hb_font_funcs_t, Subclass>
{
  static inline void destroy (hb_font_funcs_t *p)
  {
    hb_font_funcs_destroy (p);
  }
  static inline const hb_font_funcs_t *get_null (void)
  {
      return hb_font_funcs_get_empty ();
  }
};
template <typename Subclass>
struct hb_unicode_funcs_lazy_loader_t : hb_lazy_loader_t<hb_unicode_funcs_t, Subclass>
{
  static inline void destroy (hb_unicode_funcs_t *p)
  {
    hb_unicode_funcs_destroy (p);
  }
  static inline const hb_unicode_funcs_t *get_null (void)
  {
      return hb_unicode_funcs_get_empty ();
  }
};


#endif /* HB_MACHINERY_HH */
