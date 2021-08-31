// type1-render.cpp
//
//   Implementation of Type1RenderFuzzTarget.
//
// Copyright 2018-2019 by
// Armin Hasitzka, David Turner, Robert Wilhelm, and Werner Lemberg.
//
// This file is part of the FreeType project, and may only be used,
// modified, and distributed under the terms of the FreeType project
// license, LICENSE.TXT.  By continuing to use, modify, or distribute
// this file you indicate that you have read the license and
// understand and accept it fully.


#include "targets/font-drivers/type1-render.h"

#include <utility> // std::move

#include "iterators/faceloaditerator.h"
#include "iterators/faceprepiterator-bitmaps.h"
#include "iterators/faceprepiterator-multiplemasters.h"
#include "iterators/faceprepiterator-outlines.h"
#include "iterators/glyphloaditerator-naive.h"
#include "visitors/facevisitor-autohinter.h"
#include "visitors/facevisitor-loadglyphs-bitmaps.h"
#include "visitors/facevisitor-loadglyphs-outlines.h"
#include "visitors/facevisitor-renderglyphs.h"
#include "visitors/facevisitor-subglyphs.h"


  freetype::Type1RenderFuzzTarget::
  Type1RenderFuzzTarget()
  {
    auto  fli = freetype::make_unique<FaceLoadIterator>();

    auto  fpi_bitmaps  = freetype::make_unique<FacePrepIteratorBitmaps>();
    auto  fpi_outlines = freetype::make_unique<FacePrepIteratorOutlines>();
    auto  fpi_mm =
      freetype::make_unique<FacePrepIteratorMultipleMasters>();


    // -----------------------------------------------------------------------
    // Face preparation iterators:

    (void) fpi_bitmaps
      ->add_visitor( freetype::make_unique<FaceVisitorLoadGlyphsBitmaps>() );

    (void) fpi_outlines
      ->add_visitor( freetype::make_unique<FaceVisitorAutohinter>() );
    (void) fpi_outlines
      ->add_visitor( freetype::make_unique<FaceVisitorLoadGlyphsOutlines>() );
    (void) fpi_outlines
      ->add_visitor( freetype::make_unique<FaceVisitorRenderGlyphs>() );
    (void) fpi_outlines
      ->add_visitor( freetype::make_unique<FaceVisitorSubGlyphs>() );

    (void) fpi_mm
      ->add_visitor( freetype::make_unique<FaceVisitorAutohinter>() );
    (void) fpi_mm
      ->add_visitor( freetype::make_unique<FaceVisitorLoadGlyphsOutlines>() );
    (void) fpi_mm
      ->add_visitor( freetype::make_unique<FaceVisitorRenderGlyphs>() );
    (void) fpi_mm
      ->add_visitor( freetype::make_unique<FaceVisitorSubGlyphs>() );

    // -----------------------------------------------------------------------
    // Face load iterators:

    (void) fli->set_supported_font_format( FaceLoader::FontFormat::TYPE_1 );

    (void) fli->add_iterator( std::move( fpi_bitmaps  ) );
    (void) fli->add_iterator( std::move( fpi_outlines ) );
    (void) fli->add_iterator( std::move( fpi_mm       ) );
    
    // -----------------------------------------------------------------------
    // Fuzz target:

    (void) set_property( "type1", "hinting-engine", &HINTING_ADOBE );

    (void) set_iterator( std::move( fli ) );

#ifdef HAVE_ARCHIVE
    (void) set_data_is_tar_archive( false );
#endif
  }
