These are the formats for the various O3D objects that can be stored in binary.

NOTE:
All int16, uint16, int32, uint32 and floats are stored in LITTLE ENDIAN format.

------------------------ Buffer ------------------------------------------------
A Buffer (VertexBuffer, SourceBuffer, IndexBuffer) are all stored as follows.

// Header
uint8[4] id            : A 4 byte ID with the ascii characters "BUFF"
int32    version       : A version number.  Must be 1
int32    numFields     : The number of fields in the buffer.

// For each field
  uint8    fieldType     : Type of field.  1 = FloatField, 2 = UInt32Field, 3 = UByteNField.
  uint8    numComponents : The number of components for that field.
// End for

// stored once.
uint32   numElements : The number of elements in the buffer.

// For each field. Depending on the type of field
  // If FloatField
    float[numElements * numComponents]

  // If UInt32Field
    uint32[numElements * numComponents]

  // If UByteNField
    uint8[numElements * numComponents]
// End for


------------------------ Curve -------------------------------------------------
A Curve is stored as follows.

// Header
uint8[4] id            : A 4 byte ID with the ascii characters "CURV"
int32    version       : A version number.  Must be 1

// For each key in the Curve
  uint8  keyType    : The type of key. 1 = StepCurveKey, 2 = LinearCurveKey, 3 = BezierCurveKey
  float  input      : The input of the key
  float  output     : The output of the key
  // If it is a BezierCurveKey
    float inTangentX   : the x component of the in tangent.
    float inTangentY   : the y component of the in tangent.
    float outTangentX  : the x component of the out tangent.
    float outTangentY  : the y component of the out tangent.
// End for


------------------------ Skin --------------------------------------------------
A Skin is stored as follows.

// Header
uint8[4] id            : A 4 byte ID with the ascii characters "SKIN"
int32    version       : A version number.  Must be 1

// For each vertex in the skin
  int32 numInfluences  : The number of influences on this vertex.
  // For each influence
    int32 matrixIndex    : the Index of a matrix that affects this vertex.
    float weight         : The weight for this matrix.
  // End for each influence
// End for each vertex

