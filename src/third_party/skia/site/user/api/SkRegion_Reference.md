SkRegion Reference
===
<a href='#Region'>Region</a> is a compressed one bit mask. <a href='#Region'>Region</a> describes an aliased clipping area
on integer boundaries. <a href='#Region'>Region</a> can also describe an array of integer rectangles.

<a href='SkCanvas_Reference#Canvas'>Canvas</a> uses <a href='#Region'>Region</a> to reduce the current clip. <a href='#Region'>Region</a> may be drawn to <a href='SkCanvas_Reference#Canvas'>Canvas</a>;
<a href='SkPaint_Reference#Paint'>Paint</a> determines if <a href='#Region'>Region</a> is filled or stroked, its <a href='SkColor_Reference#Color'>Color</a>, and so on.

<a href='#Region'>Region</a> may be constructed from <a href='SkIRect_Reference#IRect'>IRect</a> array or <a href='SkPath_Reference#Path'>Path</a>. Diagonal lines and curves
in <a href='SkPath_Reference#Path'>Path</a> become integer rectangle edges. <a href='#Region'>Regions</a> operators compute union,
intersection, difference, and so on. <a href='SkCanvas_Reference#Canvas'>Canvas</a> allows only intersection and
difference; successive clips can only reduce available <a href='SkCanvas_Reference#Canvas'>Canvas</a> area.

<a name='SkRegion'></a>

<pre style="padding: 1em 1em 1em 1em;width: 62.5em; background-color: #f0f0f0">
class <a href='#SkRegion'>SkRegion</a> {
public:
    <a href='#SkRegion_empty_constructor'>SkRegion()</a>;
    <a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a>;
    explicit <a href='#SkRegion_copy_const_SkIRect'>SkRegion(const SkIRect& rect)</a>;
    <a href='#SkRegion_destructor'>~SkRegion()</a>;
    <a href='#SkRegion'>SkRegion</a>& <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>;
    bool <a href='#SkRegion_equal1_operator'>operator==(const SkRegion& other)_const</a>;
    bool <a href='#SkRegion_notequal1_operator'>operator!=(const SkRegion& other)_const</a>;
    bool <a href='#SkRegion_set'>set</a>(const <a href='#SkRegion'>SkRegion</a>& src);
    void <a href='#SkRegion_swap'>swap</a>(<a href='#SkRegion'>SkRegion</a>& other);
    bool <a href='#SkRegion_isEmpty'>isEmpty</a>() const;
    bool <a href='#SkRegion_isRect'>isRect</a>() const;
    bool <a href='#SkRegion_isComplex'>isComplex</a>() const;
    const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_getBounds'>getBounds</a>() const;
    int <a href='#SkRegion_computeRegionComplexity'>computeRegionComplexity</a>() const;
    bool <a href='#SkRegion_getBoundaryPath'>getBoundaryPath</a>(<a href='SkPath_Reference#SkPath'>SkPath</a>* path) const;
    bool <a href='#SkRegion_setEmpty'>setEmpty</a>();
    bool <a href='#SkRegion_setRect'>setRect</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect);
    bool <a href='#SkRegion_setRect_2'>setRect</a>(int32_t left, int32_t top, int32_t right, int32_t bottom);
    bool <a href='#SkRegion_setRects'>setRects</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a> rects[], int count);
    bool <a href='#SkRegion_setRegion'>setRegion</a>(const <a href='#SkRegion'>SkRegion</a>& region);
    bool <a href='#SkRegion_setPath'>setPath</a>(const <a href='SkPath_Reference#SkPath'>SkPath</a>& path, const <a href='#SkRegion'>SkRegion</a>& clip);
    bool <a href='#SkRegion_intersects'>intersects</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect) const;
    bool <a href='#SkRegion_intersects_2'>intersects</a>(const <a href='#SkRegion'>SkRegion</a>& other) const;
    bool <a href='#SkRegion_contains'>contains</a>(int32_t x, int32_t y) const;
    bool <a href='#SkRegion_contains_2'>contains</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& other) const;
    bool <a href='#SkRegion_contains_3'>contains</a>(const <a href='#SkRegion'>SkRegion</a>& other) const;
    bool <a href='#SkRegion_quickContains'>quickContains</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& r) const;
    bool <a href='#SkRegion_quickContains_2'>quickContains</a>(int32_t left, int32_t top, int32_t right,
                       int32_t bottom) const;
    bool <a href='#SkRegion_quickReject'>quickReject</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect) const;
    bool <a href='#SkRegion_quickReject_2'>quickReject</a>(const <a href='#SkRegion'>SkRegion</a>& rgn) const;
    void <a href='#SkRegion_translate'>translate</a>(int dx, int dy);
    void <a href='#SkRegion_translate_2'>translate</a>(int dx, int dy, <a href='#SkRegion'>SkRegion</a>* dst) const;

    enum <a href='undocumented#Op'>Op</a> {
        <a href='#SkRegion_kDifference_Op'>kDifference_Op</a>,
        <a href='#SkRegion_kIntersect_Op'>kIntersect_Op</a>,
        <a href='#SkRegion_kUnion_Op'>kUnion_Op</a>,
        <a href='#SkRegion_kXOR_Op'>kXOR_Op</a>,
        <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference_Op</a>,
        <a href='#SkRegion_kReplace_Op'>kReplace_Op</a>,
        <a href='#SkRegion_kLastOp'>kLastOp</a> = <a href='#SkRegion_kReplace_Op'>kReplace_Op</a>,
    };

    static const int <a href='#SkRegion_kOpCnt'>kOpCnt</a> = <a href='#SkRegion_kLastOp'>kLastOp</a> + 1;
    bool <a href='#SkRegion_op'>op</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, <a href='undocumented#Op'>Op</a> op);
    bool <a href='#SkRegion_op_2'>op</a>(int left, int top, int right, int bottom, <a href='undocumented#Op'>Op</a> op);
    bool <a href='#SkRegion_op_3'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgn, <a href='undocumented#Op'>Op</a> op);
    bool <a href='#SkRegion_op_4'>op</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, const <a href='#SkRegion'>SkRegion</a>& rgn, <a href='undocumented#Op'>Op</a> op);
    bool <a href='#SkRegion_op_5'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgn, const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, <a href='undocumented#Op'>Op</a> op);
    bool <a href='#SkRegion_op_6'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgna, const <a href='#SkRegion'>SkRegion</a>& rgnb, <a href='undocumented#Op'>Op</a> op);
    char* <a href='#SkRegion_toString'>toString</a>();

    class <a href='#SkRegion_Iterator'>Iterator</a> {
    public:
        <a href='#SkRegion_Iterator_Iterator'>Iterator()</a>;
        <a href='#SkRegion_Iterator_copy_const_SkRegion'>Iterator(const SkRegion& region)</a>;
        bool <a href='#SkRegion_Iterator_rewind'>rewind</a>();
        void <a href='#SkRegion_Iterator_reset'>reset</a>(const <a href='#SkRegion'>SkRegion</a>& region);
        bool <a href='#SkRegion_Iterator_done'>done</a>() const;
        void <a href='#SkRegion_Iterator_next'>next</a>();
        const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Iterator_rect'>rect</a>() const;
        const <a href='#SkRegion'>SkRegion</a>* <a href='#SkRegion_Iterator_rgn'>rgn</a>() const;
    };

    class <a href='#SkRegion_Cliperator'>Cliperator</a> {
    public:
        <a href='#SkRegion_Cliperator_const_SkRegion_const_SkIRect'>Cliperator(const SkRegion& region, const SkIRect& clip)</a>;
        bool <a href='#SkRegion_Cliperator_done'>done</a>();
        void <a href='#SkRegion_Cliperator_next'>next</a>();
        const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Cliperator_rect'>rect</a>() const;
    };

    class <a href='#SkRegion_Spanerator'>Spanerator</a> {
    public:
        <a href='#SkRegion_Spanerator_const_SkRegion_int_int_int'>Spanerator(const SkRegion& region, int y, int left, int right)</a>;
        bool <a href='#SkRegion_Spanerator_next'>next</a>(int* left, int* right);
    };

    size_t <a href='#SkRegion_writeToMemory'>writeToMemory</a>(void* buffer) const;
    size_t <a href='#SkRegion_readFromMemory'>readFromMemory</a>(const void* buffer, size_t length);
};
</pre>

<a href='#SkRegion'>SkRegion</a> describes the set of pixels used to clip <a href='SkCanvas_Reference#Canvas'>Canvas</a>. <a href='#SkRegion'>SkRegion</a> is compact,
efficiently storing a single integer rectangle, or a run length encoded array
of rectangles. <a href='#SkRegion'>SkRegion</a> may reduce the current <a href='SkCanvas_Reference#Clip'>Canvas Clip</a>, or may be drawn as
one or more integer rectangles. <a href='#SkRegion'>SkRegion</a> iterator returns the scan lines or
rectangles contained by it, optionally intersecting a bounding rectangle.

<a name='SkRegion_Iterator'></a>

<pre style="padding: 1em 1em 1em 1em;width: 62.5em; background-color: #f0f0f0">
    class <a href='#SkRegion_Iterator_Iterator'>Iterator</a> {
    public:
        <a href='#SkRegion_Iterator_Iterator'>Iterator()</a>;
        <a href='#SkRegion_Iterator_copy_const_SkRegion'>Iterator(const SkRegion& region)</a>;
        bool <a href='#SkRegion_Iterator_rewind'>rewind</a>();
        void <a href='#SkRegion_Iterator_reset'>reset</a>(const <a href='#SkRegion'>SkRegion</a>& region);
        bool <a href='#SkRegion_Iterator_done'>done</a>() const;
        void <a href='#SkRegion_Iterator_next'>next</a>();
        const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Iterator_rect'>rect</a>();
        const <a href='#SkRegion'>SkRegion</a>* <a href='#SkRegion_Iterator_rgn'>rgn</a>();
    };
</pre>

Returns sequence of rectangles, sorted along y-axis, then x-axis, that make
up <a href='#Region'>Region</a>.

<a name='SkRegion_Iterator_Iterator'></a>
## Iterator

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion_Iterator_Iterator'>Iterator</a>()
</pre>

Initializes <a href='#SkRegion_Iterator_Iterator'>Iterator</a> with an empty <a href='#Region'>Region</a>. <a href='#SkRegion_Iterator_done'>done</a>() on <a href='#SkRegion_Iterator_Iterator'>Iterator</a> returns true.
Call <a href='#SkRegion_Iterator_reset'>reset</a>() to initialized <a href='#SkRegion_Iterator_Iterator'>Iterator</a> at a later time.

### Return Value

empty <a href='#Region'>Region</a> ierator

### Example

<div><fiddle-embed name="a2db43ee3cbf6893e9b23927fb44298a">

#### Example Output

~~~~
rect={1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_reset'>reset</a> <a href='#SkRegion'>SkRegion</a>

---

<a name='SkRegion_Iterator_copy_const_SkRegion'></a>
## Iterator

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion_Iterator_Iterator'>Iterator</a>(const <a href='#SkRegion'>SkRegion</a>& region)
</pre>

Sets <a href='#SkRegion_Iterator_Iterator'>Iterator</a> to return elements of <a href='SkIRect_Reference#IRect'>IRect</a> array in <a href='#SkRegion_Iterator_copy_const_SkRegion_region'>region</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_Iterator_copy_const_SkRegion_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to iterate</td>
  </tr>
</table>

### Return Value

<a href='#Region'>Region</a> iterator

### Example

<div><fiddle-embed name="e317ceca48a6a7504219af58f35d2c95">

#### Example Output

~~~~
rect={1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_reset'>reset</a> <a href='#SkRegion'>SkRegion</a> <a href='#SkRegion_Cliperator'>Cliperator</a> <a href='#SkRegion_Spanerator'>Spanerator</a>

---

<a name='SkRegion_Iterator_rewind'></a>
## rewind

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_Iterator_rewind'>rewind</a>()
</pre>

<a href='SkPoint_Reference#Point'>Points</a> <a href='#SkRegion_Iterator_Iterator'>Iterator</a> to start of <a href='#Region'>Region</a>.
Returns true if <a href='#Region'>Region</a> was set; otherwise, returns false.

### Return Value

true if <a href='#Region'>Region</a> was set

### Example

<div><fiddle-embed name="32d51e959d6cc720a74ec4822511e2cd">

#### Example Output

~~~~
#Volatile
empty iter rewind success=false
empty iter rect={0,0,0,0}
empty region rewind success=true
empty region rect={0,0,0,0}
after set rect rect={1,2,3,4}
after rewind rewind success=true
after rewind rect={1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_reset'>reset</a>

---

<a name='SkRegion_Iterator_reset'></a>
## reset

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void <a href='#SkRegion_Iterator_reset'>reset</a>(const <a href='#SkRegion'>SkRegion</a>& region)
</pre>

Resets iterator, using the new <a href='#Region'>Region</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_Iterator_reset_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to iterate</td>
  </tr>
</table>

### Example

<div><fiddle-embed name="d153f87bd518a4ab947b7e407ea1db79">

#### Example Output

~~~~
empty region: done=true
after set rect: done=true
after reset: done=false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_rewind'>rewind</a>

---

<a name='SkRegion_Iterator_done'></a>
## done

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_Iterator_done'>done</a>() const
</pre>

Returns true if <a href='#SkRegion_Iterator_Iterator'>Iterator</a> is pointing to final <a href='SkIRect_Reference#IRect'>IRect</a> in <a href='#Region'>Region</a>.

### Return Value

true if data parsing is complete

### Example

<div><fiddle-embed name="814efa7d7f4ae52dfc861a937c1b5c25">

#### Example Output

~~~~
done=true
done=false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_next'>next</a> <a href='#SkRegion_Iterator_rect'>rect</a>

---

<a name='SkRegion_Iterator_next'></a>
## next

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void <a href='#SkRegion_Iterator_next'>next</a>()
</pre>

Advances <a href='#SkRegion_Iterator_Iterator'>Iterator</a> to next <a href='SkIRect_Reference#IRect'>IRect</a> in <a href='#Region'>Region</a> if it is not done.

### Example

<div><fiddle-embed name="771236c2eadfc2fcd02a3e61a0875d39">

#### Example Output

~~~~
rect={1,2,3,4}
rect={5,6,7,8}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_done'>done</a> <a href='#SkRegion_Iterator_rect'>rect</a>

---

<a name='SkRegion_Iterator_rect'></a>
## rect

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Iterator_rect'>rect</a>() const
</pre>

Returns <a href='SkIRect_Reference#IRect'>IRect</a> element in <a href='#Region'>Region</a>. Does not return predictable results if <a href='#Region'>Region</a>
is empty.

### Return Value

part of <a href='#Region'>Region</a> as <a href='SkIRect_Reference#IRect'>IRect</a>

### Example

<div><fiddle-embed name="0e7c58ab5d3bcfb36b1f8464cf6c7d89">

#### Example Output

~~~~
#Volatile
rect={0,0,0,0}
rect={1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_next'>next</a> <a href='#SkRegion_Iterator_done'>done</a>

---

<a name='SkRegion_Iterator_rgn'></a>
## rgn

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
const <a href='#SkRegion'>SkRegion</a>* <a href='#SkRegion_Iterator_rgn'>rgn</a>() const
</pre>

Returns <a href='#Region'>Region</a> if set; otherwise, returns nullptr.

### Return Value

iterated <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="bbc3c454a21186e2a16e843a5b061c44"></fiddle-embed></div>

### See Also

<a href='#SkRegion_Iterator_Iterator'>Iterator</a> <a href='#SkRegion_Iterator_reset'>reset</a>

---

<a name='SkRegion_Cliperator'></a>

<pre style="padding: 1em 1em 1em 1em;width: 62.5em; background-color: #f0f0f0">
    class SK_API <a href='#SkRegion_Cliperator'>Cliperator</a> {
    public:
        <a href='#SkRegion_Cliperator_const_SkRegion_const_SkIRect'>Cliperator(const SkRegion& region, const SkIRect& clip)</a>;
        bool <a href='#SkRegion_Cliperator_done'>done</a>();
        void <a href='#SkRegion_Cliperator_next'>next</a>();
        const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Cliperator_rect'>rect</a>() const;
    };
</pre>

Returns the sequence of rectangles, sorted along y-axis, then x-axis, that make
up <a href='#Region'>Region</a> intersected with the specified clip rectangle.

<a name='SkRegion_Cliperator_const_SkRegion_const_SkIRect'></a>
## Cliperator

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion_Cliperator'>Cliperator</a>(const <a href='#SkRegion'>SkRegion</a>& region, const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& clip)
</pre>

Sets <a href='#SkRegion_Cliperator'>Cliperator</a> to return elements of <a href='SkIRect_Reference#IRect'>IRect</a> array in <a href='#Region'>Region</a> within <a href='#SkRegion_Cliperator_const_SkRegion_const_SkIRect_clip'>clip</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_Cliperator_const_SkRegion_const_SkIRect_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to iterate</td>
  </tr>
  <tr>    <td><a name='SkRegion_Cliperator_const_SkRegion_const_SkIRect_clip'><code><strong>clip</strong></code></a></td>
    <td>bounds of iteration</td>
  </tr>
</table>

### Return Value

<a href='#Region'>Region</a> iterator

### Example

<div><fiddle-embed name="3831fb6006a7e0ad5d140c266c22be78">

#### Example Output

~~~~
rect={1,2,2,3}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion'>SkRegion</a> <a href='#SkRegion_Iterator'>Iterator</a> <a href='#SkRegion_Spanerator'>Spanerator</a>

---

<a name='SkRegion_Cliperator_done'></a>
## done

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_Cliperator_done'>done</a>()
</pre>

Returns true if <a href='#SkRegion_Cliperator'>Cliperator</a> is pointing to final <a href='SkIRect_Reference#IRect'>IRect</a> in <a href='#Region'>Region</a>.

### Return Value

true if data parsing is complete

### Example

<div><fiddle-embed name="6cca7b96836266800d852664a1366453">

#### Example Output

~~~~
empty region done=true
after add rect done=false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Cliperator_next'>next</a> <a href='#SkRegion_Cliperator_rect'>rect</a>

---

<a name='SkRegion_Cliperator_next'></a>
## next

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void  <a href='#SkRegion_Cliperator_next'>next</a>()
</pre>

Advances iterator to next <a href='SkIRect_Reference#IRect'>IRect</a> in <a href='#Region'>Region</a> contained by clip.

### Example

<div><fiddle-embed name="3bbcc7eec19c808a8167bbcc987199f8">

#### Example Output

~~~~
rect={1,3,3,4}
rect={5,6,7,7}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Cliperator_done'>done</a>

---

<a name='SkRegion_Cliperator_rect'></a>
## rect

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_Cliperator_rect'>rect</a>() const
</pre>

Returns <a href='SkIRect_Reference#IRect'>IRect</a> element in <a href='#Region'>Region</a>, intersected with clip passed to <a href='#SkRegion_Cliperator'>Cliperator</a>
constructor. Does not return predictable results if <a href='#Region'>Region</a>
is empty.

### Return Value

part of <a href='#Region'>Region</a> inside clip as <a href='SkIRect_Reference#IRect'>IRect</a>

### Example

<div><fiddle-embed name="05791751f00b4c2426093fa143b43bc7">

#### Example Output

~~~~
#Volatile
empty region rect={1094713344,1065353216,0,-1}
after set rect rect={1,2,3,3}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_Cliperator_next'>next</a> <a href='#SkRegion_Cliperator_done'>done</a>

---

<a name='SkRegion_Spanerator'></a>

<pre style="padding: 1em 1em 1em 1em;width: 62.5em; background-color: #f0f0f0">
    class <a href='#SkRegion_Spanerator'>Spanerator</a> {
    public:
        <a href='#SkRegion_Spanerator_const_SkRegion_int_int_int'>Spanerator(const SkRegion& region, int y, int left, int right)</a>;
        bool <a href='#SkRegion_Spanerator_next'>next</a>(int* left, int* right);
    };
</pre>

Returns the line segment ends within <a href='#Region'>Region</a> that intersect a horizontal line.

<a name='SkRegion_Spanerator_const_SkRegion_int_int_int'></a>
## Spanerator

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion_Spanerator'>Spanerator</a>(const <a href='#SkRegion'>SkRegion</a>& region, int y, int left, int right)
</pre>

Sets <a href='#SkRegion_Spanerator'>Spanerator</a> to return line segments in <a href='#Region'>Region</a> on scan line.

### Parameters

<table>  <tr>    <td><a name='SkRegion_Spanerator_const_SkRegion_int_int_int_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to iterate</td>
  </tr>
  <tr>    <td><a name='SkRegion_Spanerator_const_SkRegion_int_int_int_y'><code><strong>y</strong></code></a></td>
    <td>horizontal line to intersect</td>
  </tr>
  <tr>    <td><a name='SkRegion_Spanerator_const_SkRegion_int_int_int_left'><code><strong>left</strong></code></a></td>
    <td>bounds of iteration</td>
  </tr>
  <tr>    <td><a name='SkRegion_Spanerator_const_SkRegion_int_int_int_right'><code><strong>right</strong></code></a></td>
    <td>bounds of iteration</td>
  </tr>
</table>

### Return Value

<a href='#Region'>Region</a> iterator

### Example

<div><fiddle-embed name="3073b3f8ea7252871b6156ff674dc385"></fiddle-embed></div>

### See Also

<a href='#SkRegion'>SkRegion</a> <a href='#SkRegion_Iterator'>Iterator</a> <a href='#SkRegion_Cliperator'>Cliperator</a>

---

<a name='SkRegion_Spanerator_next'></a>
## next

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_Spanerator_next'>next</a>(int* left, int* right)
</pre>

Advances iterator to next span intersecting <a href='#Region'>Region</a> within line segment provided
in constructor. Returns true if interval was found.

### Parameters

<table>  <tr>    <td><a name='SkRegion_Spanerator_next_left'><code><strong>left</strong></code></a></td>
    <td>pointer to span start; may be nullptr</td>
  </tr>
  <tr>    <td><a name='SkRegion_Spanerator_next_right'><code><strong>right</strong></code></a></td>
    <td>pointer to span end; may be nullptr</td>
  </tr>
</table>

### Return Value

true if interval was found

### Example

<div><fiddle-embed name="03d02180fee5f64ec4a3347e118fb2ec">

#### Example Output

~~~~
empty region: result=false
after set rect: result=true left=2 right=3
~~~~

</fiddle-embed></div>

### See Also

done

---

<a name='SkRegion_empty_constructor'></a>
## SkRegion

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion'>SkRegion</a>()
</pre>

Constructs an empty <a href='#Region'>Region</a>. <a href='#Region'>Region</a> is set to empty bounds
at (0, 0) with zero width and height.

### Return Value

empty <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="4549dcda3e0f9a41b3daee0ed37deca8">

#### Example Output

~~~~
region bounds: {0, 0, 0, 0}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_setEmpty'>setEmpty</a>

---

<a name='SkRegion_copy_const_SkRegion'></a>
## SkRegion

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion'>SkRegion</a>(const <a href='#SkRegion'>SkRegion</a>& region)
</pre>

Constructs a copy of an existing <a href='#SkRegion_copy_const_SkRegion_region'>region</a>.
Copy constructor makes two regions identical by value. Internally, <a href='#SkRegion_copy_const_SkRegion_region'>region</a> and
the returned result share pointer values. The underlying <a href='SkRect_Reference#Rect'>Rect</a> array is
copied when modified.

Creating a <a href='#Region'>Region</a> copy is very efficient and never allocates memory.
<a href='#Region'>Regions</a> are always copied by value from the interface; the underlying shared
pointers are not exposed.

### Parameters

<table>  <tr>    <td><a name='SkRegion_copy_const_SkRegion_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to copy by value</td>
  </tr>
</table>

### Return Value

copy of <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="3daa83fca809b9ec6560d2ef9e2da5e6">

#### Example Output

~~~~
region bounds: {1,2,3,4}
region2 bounds: {1,2,3,4}
after region set empty:
region bounds: {0,0,0,0}
region2 bounds: {1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_setRegion'>setRegion</a> <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>

---

<a name='SkRegion_copy_const_SkIRect'></a>
## SkRegion

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
explicit <a href='#SkRegion'>SkRegion</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect)
</pre>

Constructs a rectangular <a href='#Region'>Region</a> matching the bounds of <a href='#SkRegion_copy_const_SkIRect_rect'>rect</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_copy_const_SkIRect_rect'><code><strong>rect</strong></code></a></td>
    <td>bounds of constructed <a href='#Region'>Region</a></td>
  </tr>
</table>

### Return Value

rectangular <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="5253910233f7961c30b4c18ab911e917"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRect'>setRect</a><sup><a href='#SkRegion_setRect_2'>[2]</a></sup> <a href='#SkRegion_setRegion'>setRegion</a>

---

<a name='SkRegion_destructor'></a>
## ~SkRegion

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion_destructor'>~SkRegion</a>()
</pre>

Releases ownership of any shared data and deletes data if <a href='#Region'>Region</a> is sole owner.

### Example

<div><fiddle-embed name="985ff654a6b67288d322c748132a088e"><div>delete calls <a href='#Region'>Region</a> <a href='undocumented#Destructor'>Destructor</a>, but copy of original in region2 is unaffected.
</div>

#### Example Output

~~~~
region2 bounds: {1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_empty_constructor'>SkRegion()</a> <a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a> <a href='#SkRegion_copy_const_SkIRect'>SkRegion(const SkIRect& rect)</a> <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>

---

<a name='SkRegion_copy_operator'></a>
## operator=

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
<a href='#SkRegion'>SkRegion</a>& <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>
</pre>

Constructs a copy of an existing <a href='#SkRegion_copy_operator_region'>region</a>.
Makes two regions identical by value. Internally, <a href='#SkRegion_copy_operator_region'>region</a> and
the returned result share pointer values. The underlying <a href='SkRect_Reference#Rect'>Rect</a> array is
copied when modified.

Creating a <a href='#Region'>Region</a> copy is very efficient and never allocates memory.
<a href='#Region'>Regions</a> are always copied by value from the interface; the underlying shared
pointers are not exposed.

### Parameters

<table>  <tr>    <td><a name='SkRegion_copy_operator_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to copy by value</td>
  </tr>
</table>

### Return Value

<a href='#Region'>Region</a> to copy by value

### Example

<div><fiddle-embed name="e8513f6394c24efaa301d41921c5241a">

#### Example Output

~~~~
region1 bounds: {1,2,3,4}
region2 bounds: {1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_set'>set</a> <a href='#SkRegion_swap'>swap</a> <a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a>

---

<a name='SkRegion_equal1_operator'></a>
## operator==

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool operator==(const SkRegion& other) const
</pre>

Compares <a href='#Region'>Region</a> and <a href='#SkRegion_equal1_operator_other'>other</a>; returns true if they enclose exactly
the same area.

### Parameters

<table>  <tr>    <td><a name='SkRegion_equal1_operator_other'><code><strong>other</strong></code></a></td>
    <td><a href='#Region'>Region</a> to compare</td>
  </tr>
</table>

### Return Value

true if <a href='#Region'>Region</a> pair are equivalent

### Example

<div><fiddle-embed name="d7f4fdc8bc63ca8410ed166ecef0aef3">

#### Example Output

~~~~
empty one == two
set rect one != two
set empty one == two
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_notequal1_operator'>operator!=(const SkRegion& other) const</a> <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>

---

<a name='SkRegion_notequal1_operator'></a>
## operator!=

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool operator!=(const SkRegion& other) const
</pre>

Compares <a href='#Region'>Region</a> and <a href='#SkRegion_notequal1_operator_other'>other</a>; returns true if they do not enclose the same area.

### Parameters

<table>  <tr>    <td><a name='SkRegion_notequal1_operator_other'><code><strong>other</strong></code></a></td>
    <td><a href='#Region'>Region</a> to compare</td>
  </tr>
</table>

### Return Value

true if <a href='#Region'>Region</a> pair are not equivalent

### Example

<div><fiddle-embed name="3357caa9d8d810f200cbccb668182496">

#### Example Output

~~~~
empty one == two
set rect one != two
union rect one == two
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_equal1_operator'>operator==(const SkRegion& other) const</a> <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>

---

<a name='SkRegion_set'></a>
## set

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_set'>set</a>(const <a href='#SkRegion'>SkRegion</a>& src)
</pre>

Sets <a href='#Region'>Region</a> to <a href='#SkRegion_set_src'>src</a>, and returns true if <a href='#SkRegion_set_src'>src</a> bounds is not empty.
This makes <a href='#Region'>Region</a> and <a href='#SkRegion_set_src'>src</a> identical by value. Internally,
<a href='#Region'>Region</a> and <a href='#SkRegion_set_src'>src</a> share pointer values. The underlying <a href='SkRect_Reference#Rect'>Rect</a> array is
copied when modified.

Creating a <a href='#Region'>Region</a> copy is very efficient and never allocates memory.
<a href='#Region'>Regions</a> are always copied by value from the interface; the underlying shared
pointers are not exposed.

### Parameters

<table>  <tr>    <td><a name='SkRegion_set_src'><code><strong>src</strong></code></a></td>
    <td><a href='#Region'>Region</a> to copy</td>
  </tr>
</table>

### Return Value

copy of <a href='#SkRegion_set_src'>src</a>

### Example

<div><fiddle-embed name="b3538117c7ae2cb7de3b42ca45fe1b13">

#### Example Output

~~~~
region1 bounds: {1,2,3,4}
region2 bounds: {1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a> <a href='#SkRegion_swap'>swap</a> <a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a>

---

<a name='SkRegion_swap'></a>
## swap

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void <a href='#SkRegion_swap'>swap</a>(<a href='#SkRegion'>SkRegion</a>& other)
</pre>

Exchanges <a href='SkIRect_Reference#IRect'>IRect</a> array of <a href='#Region'>Region</a> and <a href='#SkRegion_swap_other'>other</a>. <a href='#SkRegion_swap'>swap</a>() internally exchanges pointers,
so it is lightweight and does not allocate memory.

<a href='#SkRegion_swap'>swap</a>() usage has largely been replaced by <a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a>.
<a href='SkPath_Reference#Path'>Paths</a> do not copy their content on assignment until they are written to,
making assignment as efficient as <a href='#SkRegion_swap'>swap</a>().

### Parameters

<table>  <tr>    <td><a name='SkRegion_swap_other'><code><strong>other</strong></code></a></td>
    <td><a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a> set</td>
  </tr>
</table>

### Example

<div><fiddle-embed name="ae67b7b4c198b46c58e48f5af061c8f1">

#### Example Output

~~~~
region1 bounds: {0,0,0,0}
region2 bounds: {1,2,3,4}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_copy_operator'>operator=(const SkRegion& region)</a> <a href='#SkRegion_set'>set</a> <a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a>

---

<a name='SkRegion_isEmpty'></a>
## isEmpty

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_isEmpty'>isEmpty</a>() const
</pre>

Returns true if <a href='#Region'>Region</a> is empty.
Empty <a href='#Region'>Region</a> has bounds width or height less than or equal to zero.
<a href='#SkRegion_empty_constructor'>SkRegion()</a> constructs empty <a href='#Region'>Region</a>; <a href='#SkRegion_setEmpty'>setEmpty</a>
and <a href='#SkRegion_setRect'>setRect</a> with dimensionless data make <a href='#Region'>Region</a> empty.

### Return Value

true if bounds has no width or height

### Example

<div><fiddle-embed name="10ef0de39e8553dd97cf8668ce185070">

#### Example Output

~~~~
initial: region is empty
set rect: region is not empty
set empty: region is empty
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_isRect'>isRect</a> <a href='#SkRegion_isComplex'>isComplex</a> <a href='#SkRegion_equal1_operator'>operator==(const SkRegion& other) const</a>

---

<a name='SkRegion_isRect'></a>
## isRect

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_isRect'>isRect</a>() const
</pre>

Returns true if <a href='#Region'>Region</a> is one <a href='SkIRect_Reference#IRect'>IRect</a> with positive dimensions.

### Return Value

true if <a href='#Region'>Region</a> contains one <a href='SkIRect_Reference#IRect'>IRect</a>

### Example

<div><fiddle-embed name="b6adbdddf7fe45a1098121c4e5fd57ea">

#### Example Output

~~~~
initial: region is not rect
set rect: region is rect
set empty: region is not rect
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_isEmpty'>isEmpty</a> <a href='#SkRegion_isComplex'>isComplex</a>

---

<a name='SkRegion_isComplex'></a>
## isComplex

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_isComplex'>isComplex</a>() const
</pre>

Returns true if <a href='#Region'>Region</a> is described by more than one rectangle.

### Return Value

true if <a href='#Region'>Region</a> contains more than one <a href='SkIRect_Reference#IRect'>IRect</a>

### Example

<div><fiddle-embed name="1fbd76d75ca2d280e81856311de4e54e">

#### Example Output

~~~~
initial: region is not complex
set rect: region is not complex
op rect: region is complex
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_isEmpty'>isEmpty</a> <a href='#SkRegion_isRect'>isRect</a>

---

<a name='SkRegion_getBounds'></a>
## getBounds

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& <a href='#SkRegion_getBounds'>getBounds</a>() const
</pre>

Returns minimum and maximum axes values of <a href='SkIRect_Reference#IRect'>IRect</a> array.
Returns (0, 0, 0, 0) if <a href='#Region'>Region</a> is empty.

### Return Value

combined bounds of all <a href='SkIRect_Reference#IRect'>IRect</a> elements

### Example

<div><fiddle-embed name="651632582d385d2531e7aa551c31e331">

#### Example Output

~~~~
bounds: {1,2,4,5}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_isEmpty'>isEmpty</a> <a href='#SkRegion_isRect'>isRect</a>

---

<a name='SkRegion_computeRegionComplexity'></a>
## computeRegionComplexity

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
int <a href='#SkRegion_computeRegionComplexity'>computeRegionComplexity</a>() const
</pre>

Returns a value that increases with the number of
elements in <a href='#Region'>Region</a>. Returns zero if <a href='#Region'>Region</a> is empty.
Returns one if <a href='#Region'>Region</a> equals <a href='SkIRect_Reference#IRect'>IRect</a>; otherwise, returns
value greater than one indicating that <a href='#Region'>Region</a> is complex.

Call to compare <a href='#Region'>Regions</a> for relative complexity.

### Return Value

relative complexity

### Example

<div><fiddle-embed name="c4984fefdcecdd1090be160f80939d87">

#### Example Output

~~~~
initial: region complexity 0
set rect: region complexity 1
op rect: region complexity 3
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_isRect'>isRect</a> <a href='#SkRegion_isComplex'>isComplex</a>

---

<a name='SkRegion_getBoundaryPath'></a>
## getBoundaryPath

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_getBoundaryPath'>getBoundaryPath</a>(<a href='SkPath_Reference#SkPath'>SkPath</a>* path) const
</pre>

Appends outline of <a href='#Region'>Region</a> to <a href='#SkRegion_getBoundaryPath_path'>path</a>.
Returns true if <a href='#Region'>Region</a> is not empty; otherwise, returns false, and leaves <a href='#SkRegion_getBoundaryPath_path'>path</a>
unmodified.

### Parameters

<table>  <tr>    <td><a name='SkRegion_getBoundaryPath_path'><code><strong>path</strong></code></a></td>
    <td><a href='SkPath_Reference#Path'>Path</a> to append to</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_getBoundaryPath_path'>path</a> changed

### Example

<div><fiddle-embed name="6631d36406efa3b3e27960c876421a7f"></fiddle-embed></div>

### See Also

<a href='#SkRegion_isEmpty'>isEmpty</a> <a href='#SkRegion_isComplex'>isComplex</a>

---

<a name='SkRegion_setEmpty'></a>
## setEmpty

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setEmpty'>setEmpty</a>()
</pre>

Constructs an empty <a href='#Region'>Region</a>. <a href='#Region'>Region</a> is set to empty bounds
at (0, 0) with zero width and height. Always returns false.

### Return Value

false

### Example

<div><fiddle-embed name="1314f7250963775c5ee89cc5981eee24">

#### Example Output

~~~~
region bounds: {1,2,3,4}
after region set empty:
region bounds: {0,0,0,0}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_empty_constructor'>SkRegion()</a>

---

<a name='SkRegion_setRect'></a>
## setRect

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setRect'>setRect</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect)
</pre>

Constructs a rectangular <a href='#Region'>Region</a> matching the bounds of <a href='#SkRegion_setRect_rect'>rect</a>.
If <a href='#SkRegion_setRect_rect'>rect</a> is empty, constructs empty and returns false.

### Parameters

<table>  <tr>    <td><a name='SkRegion_setRect_rect'><code><strong>rect</strong></code></a></td>
    <td>bounds of constructed <a href='#Region'>Region</a></td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_setRect_rect'>rect</a> is not empty

### Example

<div><fiddle-embed name="e12575ffcd262f2364e0e6bece98a825">

#### Example Output

~~~~
region is not empty
region is empty
setEmpty: false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_copy_const_SkIRect'>SkRegion(const SkIRect& rect)</a>

---

<a name='SkRegion_setRect_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setRect'>setRect</a>(int32_t left, int32_t top, int32_t right, int32_t bottom)
</pre>

Constructs <a href='#Region'>Region</a> with bounds (<a href='#SkRegion_setRect_2_left'>left</a>, <a href='#SkRegion_setRect_2_top'>top</a>, <a href='#SkRegion_setRect_2_right'>right</a>, <a href='#SkRegion_setRect_2_bottom'>bottom</a>).
Returns true if <a href='#SkRegion_setRect_2_left'>left</a> is less than <a href='#SkRegion_setRect_2_right'>right</a> and <a href='#SkRegion_setRect_2_top'>top</a> is less than <a href='#SkRegion_setRect_2_bottom'>bottom</a>; otherwise,
constructs empty <a href='#Region'>Region</a> and returns false.

### Parameters

<table>  <tr>    <td><a name='SkRegion_setRect_2_left'><code><strong>left</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_setRect_2_top'><code><strong>top</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_setRect_2_right'><code><strong>right</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_setRect_2_bottom'><code><strong>bottom</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
</table>

### Return Value

rectangular <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="5b31a1b077818a8150ad50f3b19e7bfe">

#### Example Output

~~~~
set to: 1,2,3,4: success:true {1,2,3,4}
set to: 3,2,1,4: success:false {0,0,0,0}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_copy_const_SkIRect'>SkRegion(const SkIRect& rect)</a>

---

<a name='SkRegion_setRects'></a>
## setRects

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setRects'>setRects</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a> rects[], int count)
</pre>

Constructs <a href='#Region'>Region</a> as the union of <a href='SkIRect_Reference#IRect'>IRect</a> in <a href='#SkRegion_setRects_rects'>rects</a> array. If <a href='#SkRegion_setRects_count'>count</a> is
zero, constructs empty <a href='#Region'>Region</a>. Returns false if constructed <a href='#Region'>Region</a> is empty.

May be faster than repeated calls to <a href='#SkRegion_op'>op</a>().

### Parameters

<table>  <tr>    <td><a name='SkRegion_setRects_rects'><code><strong>rects</strong></code></a></td>
    <td>array of <a href='SkIRect_Reference#IRect'>IRects</a></td>
  </tr>
  <tr>    <td><a name='SkRegion_setRects_count'><code><strong>count</strong></code></a></td>
    <td>array size</td>
  </tr>
</table>

### Return Value

true if constructed <a href='#Region'>Region</a> is not empty

### Example

<div><fiddle-embed name="fc793a14ed76c096a68a755c963c1ee0"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRect'>setRect</a><sup><a href='#SkRegion_setRect_2'>[2]</a></sup> <a href='#SkRegion_op'>op</a><sup><a href='#SkRegion_op_2'>[2]</a></sup><sup><a href='#SkRegion_op_3'>[3]</a></sup><sup><a href='#SkRegion_op_4'>[4]</a></sup><sup><a href='#SkRegion_op_5'>[5]</a></sup><sup><a href='#SkRegion_op_6'>[6]</a></sup>

---

<a name='SkRegion_setRegion'></a>
## setRegion

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setRegion'>setRegion</a>(const <a href='#SkRegion'>SkRegion</a>& region)
</pre>

Constructs a copy of an existing <a href='#SkRegion_setRegion_region'>region</a>.
Makes two regions identical by value. Internally, <a href='#SkRegion_setRegion_region'>region</a> and
the returned result share pointer values. The underlying <a href='SkRect_Reference#Rect'>Rect</a> array is
copied when modified.

Creating a <a href='#Region'>Region</a> copy is very efficient and never allocates memory.
<a href='#Region'>Regions</a> are always copied by value from the interface; the underlying shared
pointers are not exposed.

### Parameters

<table>  <tr>    <td><a name='SkRegion_setRegion_region'><code><strong>region</strong></code></a></td>
    <td><a href='#Region'>Region</a> to copy by value</td>
  </tr>
</table>

### Return Value

<a href='#Region'>Region</a> to copy by value

### Example

<div><fiddle-embed name="5d75d22bd155576838155762ab040751">

#### Example Output

~~~~
region bounds: {1,2,3,4}
region2 bounds: {1,2,3,4}
after region set empty:
region bounds: {1,2,3,4}
region2 bounds: {0,0,0,0}
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_copy_const_SkRegion'>SkRegion(const SkRegion& region)</a>

---

<a name='SkRegion_setPath'></a>
## setPath

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_setPath'>setPath</a>(const <a href='SkPath_Reference#SkPath'>SkPath</a>& path, const <a href='#SkRegion'>SkRegion</a>& clip)
</pre>

Constructs <a href='#Region'>Region</a> to match outline of <a href='#SkRegion_setPath_path'>path</a> within <a href='#SkRegion_setPath_clip'>clip</a>.
Returns false if constructed <a href='#Region'>Region</a> is empty.

Constructed <a href='#Region'>Region</a> draws the same pixels as <a href='#SkRegion_setPath_path'>path</a> through <a href='#SkRegion_setPath_clip'>clip</a> when
<a href='SkPaint_Reference#Anti_Alias'>Anti Aliasing</a> is disabled.

### Parameters

<table>  <tr>    <td><a name='SkRegion_setPath_path'><code><strong>path</strong></code></a></td>
    <td><a href='SkPath_Reference#Path'>Path</a> providing outline</td>
  </tr>
  <tr>    <td><a name='SkRegion_setPath_clip'><code><strong>clip</strong></code></a></td>
    <td><a href='#Region'>Region</a> containing <a href='#SkRegion_setPath_path'>path</a></td>
  </tr>
</table>

### Return Value

true if constructed <a href='#Region'>Region</a> is not empty

### Example

<div><fiddle-embed name="45b9ea2247b9ca7f10aa22ea29a426f4"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='#SkRegion_op'>op</a><sup><a href='#SkRegion_op_2'>[2]</a></sup><sup><a href='#SkRegion_op_3'>[3]</a></sup><sup><a href='#SkRegion_op_4'>[4]</a></sup><sup><a href='#SkRegion_op_5'>[5]</a></sup><sup><a href='#SkRegion_op_6'>[6]</a></sup>

---

<a name='SkRegion_intersects'></a>
## intersects

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_intersects'>intersects</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect) const
</pre>

Returns true if <a href='#Region'>Region</a> intersects <a href='#SkRegion_intersects_rect'>rect</a>.
Returns false if either <a href='#SkRegion_intersects_rect'>rect</a> or <a href='#Region'>Region</a> is empty, or do not intersect.

### Parameters

<table>  <tr>    <td><a name='SkRegion_intersects_rect'><code><strong>rect</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> to intersect</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_intersects_rect'>rect</a> and <a href='#Region'>Region</a> have area in common

### Example

<div><fiddle-embed name="42bde0ef8c2ee372751428cd6e21c1ca"></fiddle-embed></div>

### See Also

<a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='SkRect_Reference#SkRect_intersects'>SkRect::intersects</a><sup><a href='SkRect_Reference#SkRect_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_intersects_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_intersects'>intersects</a>(const <a href='#SkRegion'>SkRegion</a>& other) const
</pre>

Returns true if <a href='#Region'>Region</a> intersects <a href='#SkRegion_intersects_2_other'>other</a>.
Returns false if either <a href='#SkRegion_intersects_2_other'>other</a> or <a href='#Region'>Region</a> is empty, or do not intersect.

### Parameters

<table>  <tr>    <td><a name='SkRegion_intersects_2_other'><code><strong>other</strong></code></a></td>
    <td><a href='#Region'>Region</a> to intersect</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_intersects_2_other'>other</a> and <a href='#Region'>Region</a> have area in common

### Example

<div><fiddle-embed name="4263d79ac0e7df02e90948fdde9fa965"></fiddle-embed></div>

### See Also

<a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='SkRect_Reference#SkRect_intersects'>SkRect::intersects</a><sup><a href='SkRect_Reference#SkRect_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_contains'></a>
## contains

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_contains'>contains</a>(int32_t x, int32_t y) const
</pre>

Returns true if <a href='SkIPoint_Reference#IPoint'>IPoint</a> (<a href='#SkRegion_contains_x'>x</a>, <a href='#SkRegion_contains_y'>y</a>) is inside <a href='#Region'>Region</a>.
Returns false if <a href='#Region'>Region</a> is empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_contains_x'><code><strong>x</strong></code></a></td>
    <td>test <a href='SkIPoint_Reference#IPoint'>IPoint</a> <a href='#SkRegion_contains_x'>x</a>-coordinate</td>
  </tr>
  <tr>    <td><a name='SkRegion_contains_y'><code><strong>y</strong></code></a></td>
    <td>test <a href='SkIPoint_Reference#IPoint'>IPoint</a> <a href='#SkRegion_contains_y'>y</a>-coordinate</td>
  </tr>
</table>

### Return Value

true if (<a href='#SkRegion_contains_x'>x</a>, <a href='#SkRegion_contains_y'>y</a>) is inside <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="e3899c2715c332bfc7648d5f2b9eefc6"></fiddle-embed></div>

### See Also

<a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup> <a href='SkRect_Reference#SkRect_contains'>SkRect::contains</a><sup><a href='SkRect_Reference#SkRect_contains_2'>[2]</a></sup><sup><a href='SkRect_Reference#SkRect_contains_3'>[3]</a></sup>

---

<a name='SkRegion_contains_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_contains'>contains</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& other) const
</pre>

Returns true if <a href='#SkRegion_contains_2_other'>other</a> is completely inside <a href='#Region'>Region</a>.
Returns false if <a href='#Region'>Region</a> or <a href='#SkRegion_contains_2_other'>other</a> is empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_contains_2_other'><code><strong>other</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> to contain</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_contains_2_other'>other</a> is inside <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="100b4cbd5dd7406804e40035833a433c"></fiddle-embed></div>

### See Also

<a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup> <a href='SkRect_Reference#SkRect_contains'>SkRect::contains</a><sup><a href='SkRect_Reference#SkRect_contains_2'>[2]</a></sup><sup><a href='SkRect_Reference#SkRect_contains_3'>[3]</a></sup>

---

<a name='SkRegion_contains_3'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_contains'>contains</a>(const <a href='#SkRegion'>SkRegion</a>& other) const
</pre>

Returns true if <a href='#SkRegion_contains_3_other'>other</a> is completely inside <a href='#Region'>Region</a>.
Returns false if <a href='#Region'>Region</a> or <a href='#SkRegion_contains_3_other'>other</a> is empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_contains_3_other'><code><strong>other</strong></code></a></td>
    <td><a href='#Region'>Region</a> to contain</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_contains_3_other'>other</a> is inside <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="46de22da2f3e08a8d7f064634fc1c7b5"></fiddle-embed></div>

### See Also

<a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup> <a href='SkRect_Reference#SkRect_contains'>SkRect::contains</a><sup><a href='SkRect_Reference#SkRect_contains_2'>[2]</a></sup><sup><a href='SkRect_Reference#SkRect_contains_3'>[3]</a></sup>

---

<a name='SkRegion_quickContains'></a>
## quickContains

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_quickContains'>quickContains</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& r) const
</pre>

Returns true if <a href='#Region'>Region</a> is a single rectangle and contains <a href='#SkRegion_quickContains_r'>r</a>.
May return false even though <a href='#Region'>Region</a> contains <a href='#SkRegion_quickContains_r'>r</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_quickContains_r'><code><strong>r</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> to contain</td>
  </tr>
</table>

### Return Value

true quickly if <a href='#SkRegion_quickContains_r'>r</a> points are equal or inside

### Example

<div><fiddle-embed name="d8e5eac373e2e7cfc1b8cd0229647ba6">

#### Example Output

~~~~
quickContains 1: true
quickContains 2: true
quickContains 3: false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='#SkRegion_quickReject'>quickReject</a><sup><a href='#SkRegion_quickReject_2'>[2]</a></sup> <a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_quickContains_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_quickContains'>quickContains</a>(int32_t left, int32_t top, int32_t right, int32_t bottom) const
</pre>

Returns true if <a href='#Region'>Region</a> is a single rectangle and contains <a href='SkIRect_Reference#IRect'>IRect</a>
(<a href='#SkRegion_quickContains_2_left'>left</a>, <a href='#SkRegion_quickContains_2_top'>top</a>, <a href='#SkRegion_quickContains_2_right'>right</a>, <a href='#SkRegion_quickContains_2_bottom'>bottom</a>).
Returns false if <a href='#Region'>Region</a> is empty or <a href='SkIRect_Reference#IRect'>IRect</a> (<a href='#SkRegion_quickContains_2_left'>left</a>, <a href='#SkRegion_quickContains_2_top'>top</a>, <a href='#SkRegion_quickContains_2_right'>right</a>, <a href='#SkRegion_quickContains_2_bottom'>bottom</a>) is empty.
May return false even though <a href='#Region'>Region</a> contains (<a href='#SkRegion_quickContains_2_left'>left</a>, <a href='#SkRegion_quickContains_2_top'>top</a>, <a href='#SkRegion_quickContains_2_right'>right</a>, <a href='#SkRegion_quickContains_2_bottom'>bottom</a>).

### Parameters

<table>  <tr>    <td><a name='SkRegion_quickContains_2_left'><code><strong>left</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_quickContains_2_top'><code><strong>top</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_quickContains_2_right'><code><strong>right</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_quickContains_2_bottom'><code><strong>bottom</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
</table>

### Return Value

true quickly if <a href='SkIRect_Reference#IRect'>IRect</a> are equal or inside

### Example

<div><fiddle-embed name="eb6d290887e1a3a0b051b4d7b012f5e1">

#### Example Output

~~~~
quickContains 1: true
quickContains 2: true
quickContains 3: false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='#SkRegion_quickReject'>quickReject</a><sup><a href='#SkRegion_quickReject_2'>[2]</a></sup> <a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_quickReject'></a>
## quickReject

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_quickReject'>quickReject</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect) const
</pre>

Returns true if <a href='#Region'>Region</a> does not intersect <a href='#SkRegion_quickReject_rect'>rect</a>.
Returns true if <a href='#SkRegion_quickReject_rect'>rect</a> is empty or <a href='#Region'>Region</a> is empty.
May return false even though <a href='#Region'>Region</a> does not intersect <a href='#SkRegion_quickReject_rect'>rect</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_quickReject_rect'><code><strong>rect</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> to intersect</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_quickReject_rect'>rect</a> does not intersect

### Example

<div><fiddle-embed name="71ac24b7d91ac5ca7c14b43930d5f85d">

#### Example Output

~~~~
quickReject 1: true
quickReject 2: true
quickReject 3: false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_quickContains'>quickContains</a><sup><a href='#SkRegion_quickContains_2'>[2]</a></sup> <a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_quickReject_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_quickReject'>quickReject</a>(const <a href='#SkRegion'>SkRegion</a>& rgn) const
</pre>

Returns true if <a href='#Region'>Region</a> does not intersect <a href='#SkRegion_quickReject_2_rgn'>rgn</a>.
Returns true if <a href='#SkRegion_quickReject_2_rgn'>rgn</a> is empty or <a href='#Region'>Region</a> is empty.
May return false even though <a href='#Region'>Region</a> does not intersect <a href='#SkRegion_quickReject_2_rgn'>rgn</a>.

### Parameters

<table>  <tr>    <td><a name='SkRegion_quickReject_2_rgn'><code><strong>rgn</strong></code></a></td>
    <td><a href='#Region'>Region</a> to intersect</td>
  </tr>
</table>

### Return Value

true if <a href='#SkRegion_quickReject_2_rgn'>rgn</a> does not intersect

### Example

<div><fiddle-embed name="def7dba38947c33b203e4f9db6c88be3">

#### Example Output

~~~~
quickReject 1: true
quickReject 2: true
quickReject 3: false
~~~~

</fiddle-embed></div>

### See Also

<a href='#SkRegion_quickContains'>quickContains</a><sup><a href='#SkRegion_quickContains_2'>[2]</a></sup> <a href='#SkRegion_contains'>contains</a><sup><a href='#SkRegion_contains_2'>[2]</a></sup><sup><a href='#SkRegion_contains_3'>[3]</a></sup> <a href='#SkRegion_intersects'>intersects</a><sup><a href='#SkRegion_intersects_2'>[2]</a></sup>

---

<a name='SkRegion_translate'></a>
## translate

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void <a href='#SkRegion_translate'>translate</a>(int dx, int dy)
</pre>

Offsets <a href='#Region'>Region</a> by <a href='SkIPoint_Reference#IVector'>IVector</a> (<a href='#SkRegion_translate_dx'>dx</a>, <a href='#SkRegion_translate_dy'>dy</a>). Has no effect if <a href='#Region'>Region</a> is empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_translate_dx'><code><strong>dx</strong></code></a></td>
    <td>x-axis offset</td>
  </tr>
  <tr>    <td><a name='SkRegion_translate_dy'><code><strong>dy</strong></code></a></td>
    <td>y-axis offset</td>
  </tr>
</table>

### Example

<div><fiddle-embed name="4e5b9e53aa1b200fed3ee6596ca01f0e"></fiddle-embed></div>

### See Also

<a href='SkCanvas_Reference#SkCanvas_translate'>SkCanvas::translate</a> <a href='SkIRect_Reference#SkIRect_offset'>SkIRect::offset</a><sup><a href='SkIRect_Reference#SkIRect_offset_2'>[2]</a></sup> <a href='SkPath_Reference#SkPath_offset'>SkPath::offset</a><sup><a href='SkPath_Reference#SkPath_offset_2'>[2]</a></sup>

---

<a name='SkRegion_translate_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
void <a href='#SkRegion_translate'>translate</a>(int dx, int dy, <a href='#SkRegion'>SkRegion</a>* dst) const
</pre>

Offsets <a href='#Region'>Region</a> by <a href='SkIPoint_Reference#IVector'>IVector</a> (<a href='#SkRegion_translate_2_dx'>dx</a>, <a href='#SkRegion_translate_2_dy'>dy</a>), writing result to <a href='#SkRegion_translate_2_dst'>dst</a>. <a href='#Region'>Region</a> may be passed
as <a href='#SkRegion_translate_2_dst'>dst</a> parameter, translating <a href='#Region'>Region</a> in place. Has no effect if <a href='#SkRegion_translate_2_dst'>dst</a> is nullptr.
If <a href='#Region'>Region</a> is empty, sets <a href='#SkRegion_translate_2_dst'>dst</a> to empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_translate_2_dx'><code><strong>dx</strong></code></a></td>
    <td>x-axis offset</td>
  </tr>
  <tr>    <td><a name='SkRegion_translate_2_dy'><code><strong>dy</strong></code></a></td>
    <td>y-axis offset</td>
  </tr>
  <tr>    <td><a name='SkRegion_translate_2_dst'><code><strong>dst</strong></code></a></td>
    <td>translated result</td>
  </tr>
</table>

### Example

<div><fiddle-embed name="024200960eb52fee1f471514607e6001"></fiddle-embed></div>

### See Also

<a href='SkCanvas_Reference#SkCanvas_translate'>SkCanvas::translate</a> <a href='SkIRect_Reference#SkIRect_offset'>SkIRect::offset</a><sup><a href='SkIRect_Reference#SkIRect_offset_2'>[2]</a></sup> <a href='SkPath_Reference#SkPath_offset'>SkPath::offset</a><sup><a href='SkPath_Reference#SkPath_offset_2'>[2]</a></sup>

---

## <a name='SkRegion_Op'>Enum SkRegion::Op</a>

<pre style="padding: 1em 1em 1em 1em;width: 62.5em; background-color: #f0f0f0">
    enum <a href='undocumented#Op'>Op</a> {
        <a href='#SkRegion_kDifference_Op'>kDifference_Op</a>,
        <a href='#SkRegion_kIntersect_Op'>kIntersect_Op</a>,
        <a href='#SkRegion_kUnion_Op'>kUnion_Op</a>,
        <a href='#SkRegion_kXOR_Op'>kXOR_Op</a>,
        <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference_Op</a>,
        <a href='#SkRegion_kReplace_Op'>kReplace_Op</a>,
        <a href='#SkRegion_kLastOp'>kLastOp</a> = <a href='#SkRegion_kReplace_Op'>kReplace_Op</a>,
    };
</pre>

The logical operations that can be performed when combining two <a href='#Region'>Regions</a>.

### Constants

<table style='border-collapse: collapse; width: 62.5em'>
  <tr><th style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>Const</th>
<th style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>Value</th>
<th style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>Description</th></tr>
  <tr style='background-color: #f0f0f0; '>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kDifference_Op'><code>SkRegion::kDifference_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>0</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Subtracts operand <a href='#Region'>Region</a> from target <a href='#Region'>Region</a>.
</td>
  </tr>
  <tr>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kIntersect_Op'><code>SkRegion::kIntersect_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>1</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Intersects operand <a href='#Region'>Region</a> and target <a href='#Region'>Region</a>.
</td>
  </tr>
  <tr style='background-color: #f0f0f0; '>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kUnion_Op'><code>SkRegion::kUnion_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>2</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Unions operand <a href='#Region'>Region</a> and target <a href='#Region'>Region</a>.
</td>
  </tr>
  <tr>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kXOR_Op'><code>SkRegion::kXOR_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>3</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Replaces target <a href='#Region'>Region</a> with area exclusive to both <a href='#Region'>Regions</a>.
</td>
  </tr>
  <tr style='background-color: #f0f0f0; '>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kReverseDifference_Op'><code>SkRegion::kReverseDifference_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>4</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Subtracts target <a href='#Region'>Region</a> from operand <a href='#Region'>Region</a>.
</td>
  </tr>
  <tr>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kReplace_Op'><code>SkRegion::kReplace_Op</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>5</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
Replaces target <a href='#Region'>Region</a> with operand <a href='#Region'>Region</a>.
</td>
  </tr>
  <tr style='background-color: #f0f0f0; '>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kLastOp'><code>SkRegion::kLastOp</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>5</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
last operator</td>
  </tr>
</table>

### Example

<div><fiddle-embed name="fb82d2d3ec51912215a0eb9b45dcbc3c"></fiddle-embed></div>

### See Also

<a href='undocumented#SkPathOp'>SkPathOp</a>

### Constants

<table style='border-collapse: collapse; width: 62.5em'>
  <tr><th style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>Const</th>
<th style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>Value</th>
<th style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>Description</th></tr>
  <tr style='background-color: #f0f0f0; '>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '><a name='SkRegion_kOpCnt'><code>SkRegion::kOpCnt</code></a></td>
    <td style='text-align: center; border: 2px solid #dddddd; padding: 8px; '>6</td>
    <td style='text-align: left; border: 2px solid #dddddd; padding: 8px; '>
May be used to verify that <a href='undocumented#Op'>Op</a> is a legal value.</td>
  </tr>
</table>

<a name='SkRegion_op'></a>
## op

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#Region'>Region</a> op <a href='#SkRegion_op_rect'>rect</a>.
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_rect'><code><strong>rect</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="1790b2e054c536a54601138365700ac3"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_op_2'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(int left, int top, int right, int bottom, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#Region'>Region</a> op <a href='SkIRect_Reference#IRect'>IRect</a> (<a href='#SkRegion_op_2_left'>left</a>, <a href='#SkRegion_op_2_top'>top</a>, <a href='#SkRegion_op_2_right'>right</a>, <a href='#SkRegion_op_2_bottom'>bottom</a>).
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_2_left'><code><strong>left</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_2_top'><code><strong>top</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_2_right'><code><strong>right</strong></code></a></td>
    <td>edge of bounds on x-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_2_bottom'><code><strong>bottom</strong></code></a></td>
    <td>edge of bounds on y-axis</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_2_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="2e3497890d523235f96680716c321098"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_op_3'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgn, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#Region'>Region</a> op <a href='#SkRegion_op_3_rgn'>rgn</a>.
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_3_rgn'><code><strong>rgn</strong></code></a></td>
    <td><a href='#Region'>Region</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_3_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="65f4eccea3514ed7e37b5067e15efddb"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_op_4'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, const <a href='#SkRegion'>SkRegion</a>& rgn, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#SkRegion_op_4_rect'>rect</a> op <a href='#SkRegion_op_4_rgn'>rgn</a>.
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_4_rect'><code><strong>rect</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_4_rgn'><code><strong>rgn</strong></code></a></td>
    <td><a href='#Region'>Region</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_4_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="3f964be1e1fd2fbb977b655d3a928f0a"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_op_5'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgn, const <a href='SkIRect_Reference#SkIRect'>SkIRect</a>& rect, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#SkRegion_op_5_rgn'>rgn</a> op <a href='#SkRegion_op_5_rect'>rect</a>.
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_5_rgn'><code><strong>rgn</strong></code></a></td>
    <td><a href='#Region'>Region</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_5_rect'><code><strong>rect</strong></code></a></td>
    <td><a href='SkIRect_Reference#IRect'>IRect</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_5_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="e623208dd44f0b24499ac5f1593d1b39"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_op_6'></a>

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
bool <a href='#SkRegion_op'>op</a>(const <a href='#SkRegion'>SkRegion</a>& rgna, const <a href='#SkRegion'>SkRegion</a>& rgnb, <a href='undocumented#Op'>Op</a> op)
</pre>

Replaces <a href='#Region'>Region</a> with the result of <a href='#SkRegion_op_6_rgna'>rgna</a> op <a href='#SkRegion_op_6_rgnb'>rgnb</a>.
Returns true if replaced <a href='#Region'>Region</a> is not empty.

### Parameters

<table>  <tr>    <td><a name='SkRegion_op_6_rgna'><code><strong>rgna</strong></code></a></td>
    <td><a href='#Region'>Region</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_6_rgnb'><code><strong>rgnb</strong></code></a></td>
    <td><a href='#Region'>Region</a> operand</td>
  </tr>
  <tr>    <td><a name='SkRegion_op_6_op'><code><strong>op</strong></code></a></td>
    <td>operator, one of: <a href='#SkRegion_kDifference_Op'>kDifference Op</a>, <a href='#SkRegion_kIntersect_Op'>kIntersect Op</a>, <a href='#SkRegion_kUnion_Op'>kUnion Op</a>, <a href='#SkRegion_kXOR_Op'>kXOR Op</a>, <a href='#SkRegion_kReverseDifference_Op'>kReverseDifference Op</a>,
<a href='#SkRegion_kReplace_Op'>kReplace Op</a> </td>
  </tr>
</table>

### Return Value

false if result is empty

### Example

<div><fiddle-embed name="13de1a6fcb2302a2a30278cb88d3e17d"></fiddle-embed></div>

### See Also

<a href='#SkRegion_setRects'>setRects</a> <a href='undocumented#Op'>Op</a>

---

<a name='SkRegion_toString'></a>
## toString

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
char* <a href='#SkRegion_toString'>toString</a>()
</pre>

Private: Android framework only.

### Return Value

string representation of <a href='#Region'>Region</a>

---

<a name='SkRegion_writeToMemory'></a>
## writeToMemory

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
size_t <a href='#SkRegion_writeToMemory'>writeToMemory</a>(void* buffer) const
</pre>

Writes <a href='#Region'>Region</a> to <a href='#SkRegion_writeToMemory_buffer'>buffer</a>, and returns number of bytes written.
If <a href='#SkRegion_writeToMemory_buffer'>buffer</a> is nullptr, returns number number of bytes that would be written.

### Parameters

<table>  <tr>    <td><a name='SkRegion_writeToMemory_buffer'><code><strong>buffer</strong></code></a></td>
    <td>storage for binary data</td>
  </tr>
</table>

### Return Value

size of <a href='#Region'>Region</a>

### Example

<div><fiddle-embed name="1419d2a8c22c355ab46240865d056ee5"></fiddle-embed></div>

### See Also

<a href='#SkRegion_readFromMemory'>readFromMemory</a>

---

<a name='SkRegion_readFromMemory'></a>
## readFromMemory

<pre style="padding: 1em 1em 1em 1em; width: 62.5em;background-color: #f0f0f0">
size_t <a href='#SkRegion_readFromMemory'>readFromMemory</a>(const void* buffer, size_t length)
</pre>

Constructs <a href='#Region'>Region</a> from <a href='#SkRegion_readFromMemory_buffer'>buffer</a> of size <a href='#SkRegion_readFromMemory_length'>length</a>. Returns bytes read.
Returned value will be multiple of four or zero if <a href='#SkRegion_readFromMemory_length'>length</a> was too small.

### Parameters

<table>  <tr>    <td><a name='SkRegion_readFromMemory_buffer'><code><strong>buffer</strong></code></a></td>
    <td>storage for binary data</td>
  </tr>
  <tr>    <td><a name='SkRegion_readFromMemory_length'><code><strong>length</strong></code></a></td>
    <td>size of <a href='#SkRegion_readFromMemory_buffer'>buffer</a></td>
  </tr>
</table>

### Return Value

bytes read

### Example

<div><fiddle-embed name="1ede346c430ef23df0eaaf0773dd6a15"></fiddle-embed></div>

### See Also

<a href='#SkRegion_writeToMemory'>writeToMemory</a>

---

