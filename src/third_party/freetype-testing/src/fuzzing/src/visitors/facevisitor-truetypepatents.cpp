// facevisitor-patents.cpp
//
//   Implementation of FaceVisitorTrueTypePatents.
//
// Copyright 2018-2019 by
// Armin Hasitzka.
//
// This file is part of the FreeType project, and may only be used,
// modified, and distributed under the terms of the FreeType project
// license, LICENSE.TXT.  By continuing to use, modify, or distribute
// this file you indicate that you have read the license and
// understand and accept it fully.


#include "visitors/facevisitor-truetypepatents.h"

#include <cassert>

#include "utils/logging.h"


  void
  freetype::FaceVisitorTrueTypePatents::
  run( Unique_FT_Face  face )
  {
    FT_Bool  ret;


    assert( face != nullptr );

    ret = FT_Face_CheckTrueTypePatents( face.get() );
    LOG_IF( ERROR, ret == 1 ) << "FT_Face_CheckTrueTypePatents failed";

    // It's not really in the scope of this fuzzer, but we can try to set the
    // value to `2' as well.  Generally, this should be done by a unit test
    // suite however.
    for ( FT_Bool  b = 0; b < 3; b++ )
    {
      ret = FT_Face_SetUnpatentedHinting( face.get(), b );
      LOG_IF( ERROR, ret == 1 ) << "FT_Face_SetUnpatentedHinting failed";
    }
  }
