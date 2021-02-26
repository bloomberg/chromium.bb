// facevisitor-patents.h
//
//   Quickly check the deprecated (but still existing) functions for managing
//   former patented TrueType hinting.
//
//   Drivers:
//     - TrueType
//
// Copyright 2018-2019 by
// Armin Hasitzka.
//
// This file is part of the FreeType project, and may only be used,
// modified, and distributed under the terms of the FreeType project
// license, LICENSE.TXT.  By continuing to use, modify, or distribute
// this file you indicate that you have read the license and
// understand and accept it fully.


#ifndef VISITORS_FACE_VISITOR_TRUETYPE_PATENTS_H_
#define VISITORS_FACE_VISITOR_TRUETYPE_PATENTS_H_


#include "utils/utils.h"
#include "visitors/facevisitor.h"


namespace freetype {


  class FaceVisitorTrueTypePatents
    : public FaceVisitor
  {
  public:


    void
    run( Unique_FT_Face  face )
    override;
  };
}


#endif // VISITORS_FACE_VISITOR_TRUETYPE_PATENTS_H_
