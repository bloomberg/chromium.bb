#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COMPRESSION_DECOMPRESSION_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COMPRESSION_DECOMPRESSION_STREAM_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DecompressionStream final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DecompressionStream* Create(ScriptState*,
                                     const AtomicString&,
                                     ExceptionState&);
  DecompressionStream(ScriptState*, const AtomicString&, ExceptionState&);

  ReadableStream* readable() const;
  WritableStream* writable() const;

  void Trace(Visitor* visitor) override;

 private:
  const Member<TransformStream> transform_;

  DISALLOW_COPY_AND_ASSIGN(DecompressionStream);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COMPRESSION_DECOMPRESSION_STREAM_H_
