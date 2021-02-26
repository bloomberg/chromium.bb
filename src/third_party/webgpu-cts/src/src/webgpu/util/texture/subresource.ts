export interface BeginCountRange {
  begin: number;
  count: number;
}

export interface BeginEndRange {
  begin: number;
  end: number;
}

function endOfRange(r: BeginEndRange | BeginCountRange): number {
  return 'count' in r ? r.begin + r.count : r.end;
}

function* rangeAsIterator(r: BeginEndRange | BeginCountRange): Generator<number> {
  for (let i = r.begin; i < endOfRange(r); ++i) {
    yield i;
  }
}

export class SubresourceRange {
  readonly mipRange: BeginEndRange;
  readonly sliceRange: BeginEndRange;

  constructor(subresources: {
    mipRange: BeginEndRange | BeginCountRange;
    sliceRange: BeginEndRange | BeginCountRange;
  }) {
    this.mipRange = {
      begin: subresources.mipRange.begin,
      end: endOfRange(subresources.mipRange),
    };
    this.sliceRange = {
      begin: subresources.sliceRange.begin,
      end: endOfRange(subresources.sliceRange),
    };
  }

  *each(): Generator<{ level: number; slice: number }> {
    for (let level = this.mipRange.begin; level < this.mipRange.end; ++level) {
      for (let slice = this.sliceRange.begin; slice < this.sliceRange.end; ++slice) {
        yield { level, slice };
      }
    }
  }

  *mipLevels(): Generator<{ level: number; slices: Generator<number> }> {
    for (let level = this.mipRange.begin; level < this.mipRange.end; ++level) {
      yield {
        level,
        slices: rangeAsIterator(this.sliceRange),
      };
    }
  }
}
