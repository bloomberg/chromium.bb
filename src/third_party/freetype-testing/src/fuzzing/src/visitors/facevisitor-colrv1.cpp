// facevisitor-colrv1.cpp
//
//   Finds and traverses COLRv1 glyphs using FreeType's
//   COLRv1 API.
//
// Copyright 2021 by
// Dominik Röttsches.
//
// This file is part of the FreeType project, and may only be used,
// modified, and distributed under the terms of the FreeType project
// license, LICENSE.TXT.  By continuing to use, modify, or distribute
// this file you indicate that you have read the license and
// understand and accept it fully.


#include "visitors/facevisitor-colrv1.h"

#include <cassert>

#include "utils/logging.h"

#include <ft2build.h>
#include FT_FREETYPE_H


namespace {


  constexpr unsigned long  MAX_TRAVERSE_GLYPHS = 5;


  bool colrv1_start_glyph( const FT_Face&           ft_face,
                           uint16_t                 glyph_id,
                           FT_Color_Root_Transform  root_transform );


  void iterate_color_stops ( FT_Face                face,
                             FT_ColorStopIterator*  color_stop_iterator )
  {
    const FT_UInt  num_color_stops = color_stop_iterator->num_color_stops;
    FT_ColorStop   color_stop;
    long           color_stop_index = 0;


    while ( FT_Get_Colorline_Stops( face,
                                    &color_stop,
                                    color_stop_iterator ) )
    {
      LOG( INFO ) << "Color stop " << color_stop_index
                  << "/" << num_color_stops - 1
                  << " stop offset: " << color_stop.stop_offset
                  << " palette index: " << color_stop.color.palette_index
                  << " alpha: " << color_stop.color.alpha;
    }
  }


  void colrv1_draw_paint( FT_Face        face,
                          FT_COLR_Paint  colrv1_paint )
  {
    switch ( colrv1_paint.format )
    {

    case FT_COLR_PAINTFORMAT_GLYPH:
    {
      LOG( INFO ) << "PaintGlyph";
      break;
    }

    case FT_COLR_PAINTFORMAT_SOLID:
    {
      LOG( INFO ) << "PaintSolid,"
                  << " palette_index: "
                  << colrv1_paint.u.solid.color.palette_index
                  << " alpha: " << colrv1_paint.u.solid.color.alpha;
      break;
    }

    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    {
      LOG( INFO ) << "PaintLinearGradient,"
                  << " p0.x " << colrv1_paint.u.linear_gradient.p0.x
                  << " p0.y " << colrv1_paint.u.linear_gradient.p0.y
                  << " p1.x " << colrv1_paint.u.linear_gradient.p1.x
                  << " p1.y " << colrv1_paint.u.linear_gradient.p1.y
                  << " p2.x " << colrv1_paint.u.linear_gradient.p2.x
                  << " p2.y " << colrv1_paint.u.linear_gradient.p2.y;

      iterate_color_stops(
        face,
        &colrv1_paint.u.linear_gradient.colorline.color_stop_iterator );

      break;
    }

    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    {
      LOG( INFO ) << "PaintRadialGradient,"
                  << " c0.x " << colrv1_paint.u.radial_gradient.c0.x
                  << " c0.y " << colrv1_paint.u.radial_gradient.c0.y
                  << " c1.x " << colrv1_paint.u.radial_gradient.c1.x
                  << " c1.y " << colrv1_paint.u.radial_gradient.c1.y
                  << " r0 " << colrv1_paint.u.radial_gradient.r0
                  << " r1 " << colrv1_paint.u.radial_gradient.r1;

      iterate_color_stops(
        face,
        &colrv1_paint.u.radial_gradient.colorline.color_stop_iterator );

      break;
    }

    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
    {
      LOG( INFO ) << "PaintSweepGradient,"
                  << " center.x " << colrv1_paint.u.sweep_gradient.center.x
                  << " center.y " << colrv1_paint.u.sweep_gradient.center.y
                  << " start_angle "
                  << colrv1_paint.u.sweep_gradient.start_angle
                  << " end_angle " << colrv1_paint.u.sweep_gradient.end_angle;

      iterate_color_stops(
        face,
        &colrv1_paint.u.sweep_gradient.colorline.color_stop_iterator );

      break;
    }

    case FT_COLR_PAINTFORMAT_TRANSFORMED:
    {
      LOG( INFO ) << "PaintTransformed,"
                  << " xx " << colrv1_paint.u.transformed.affine.xx
                  << " xy " << colrv1_paint.u.transformed.affine.xy
                  << " yx " << colrv1_paint.u.transformed.affine.yx
                  << " yy " << colrv1_paint.u.transformed.affine.yy
                  << " dx " << colrv1_paint.u.transformed.affine.dx
                  << " dy " << colrv1_paint.u.transformed.affine.dy;
      break;
    }

    case FT_COLR_PAINTFORMAT_TRANSLATE:
    {
      LOG( INFO ) << "PaintTranslate,"
                  << " dx " << colrv1_paint.u.translate.dx
                  << " dy " << colrv1_paint.u.translate.dy;
      break;
    }

    case FT_COLR_PAINTFORMAT_ROTATE:
    {
      LOG( INFO ) << "PaintRotate,"
                  << " center.x " << colrv1_paint.u.rotate.center_x
                  << " center.y " << colrv1_paint.u.rotate.center_y
                  << " angle " << colrv1_paint.u.rotate.angle;
      break;
    }

    case FT_COLR_PAINTFORMAT_SKEW:
    {
      LOG( INFO ) << "PaintSkew,"
                  << " center.x " << colrv1_paint.u.skew.center_x
                  << " center.y " << colrv1_paint.u.skew.center_y
                  << " x_skew_angle " << colrv1_paint.u.skew.x_skew_angle
                  << " y_skew_angle " << colrv1_paint.u.skew.y_skew_angle;
      break;
    }

    case FT_COLR_PAINTFORMAT_COLR_GLYPH:
    case FT_COLR_PAINTFORMAT_COLR_LAYERS:
    case FT_COLR_PAINTFORMAT_COMPOSITE:
    case FT_COLR_PAINT_FORMAT_MAX:
    case FT_COLR_PAINTFORMAT_UNSUPPORTED:
    {
        // No action for these paint format types in drawing.
        break;
    }
    }
  }


  bool colrv1_traverse_paint( FT_Face         face,
                              FT_OpaquePaint  opaque_paint )
  {
    FT_COLR_Paint  paint;

    bool  traverse_result = true;


    if ( !FT_Get_Paint( face, opaque_paint, &paint ) )
    {
      LOG( ERROR ) << "FT_Get_Paint failed.";
      return false;
    }

    // Keep track of failures to retrieve the FT_COLR_Paint from FreeType in the
    // recursion, cancel recursion when a paint retrieval fails.
    switch ( paint.format )
    {

    case FT_COLR_PAINTFORMAT_COLR_LAYERS:
    {
      FT_LayerIterator&  layer_iterator = paint.u.colr_layers.layer_iterator;
      FT_OpaquePaint     opaque_paint_fetch;

      LOG( INFO ) << "PaintColrLayers,"
                  << " num_layers " << layer_iterator.num_layers;

      opaque_paint_fetch.p = nullptr;
      while ( FT_Get_Paint_Layers( face,
                                   &layer_iterator,
                                   &opaque_paint_fetch ) )
        colrv1_traverse_paint( face, opaque_paint_fetch );

      break;
    }

    case FT_COLR_PAINTFORMAT_GLYPH:
      colrv1_draw_paint( face, paint );
      traverse_result = colrv1_traverse_paint( face, paint.u.glyph.paint );
      break;

    case FT_COLR_PAINTFORMAT_COLR_GLYPH:

      LOG( INFO ) << "PaintColrGlyph,"
                  << " glyphId " << paint.u.colr_glyph.glyphID;

      traverse_result = colrv1_start_glyph( face,
                                            paint.u.colr_glyph.glyphID,
                                            FT_COLOR_NO_ROOT_TRANSFORM );
      break;

    case FT_COLR_PAINTFORMAT_TRANSLATE:
      colrv1_draw_paint( face, paint );
      traverse_result = colrv1_traverse_paint( face,
                                               paint.u.translate.paint );
      break;

    case FT_COLR_PAINTFORMAT_TRANSFORMED:
      colrv1_draw_paint( face, paint );
      traverse_result = colrv1_traverse_paint( face,
                                               paint.u.transformed.paint );
      break;

    case FT_COLR_PAINTFORMAT_ROTATE:
      colrv1_draw_paint( face, paint );
      traverse_result = colrv1_traverse_paint( face, paint.u.rotate.paint );
      break;

    case FT_COLR_PAINTFORMAT_SKEW:
      colrv1_draw_paint( face, paint );
      traverse_result = colrv1_traverse_paint( face, paint.u.skew.paint );
      break;

    case FT_COLR_PAINTFORMAT_COMPOSITE:
    {
      LOG( INFO ) << "PaintComposite,"
                  << " composite_mode " << paint.u.composite.composite_mode;

      traverse_result = colrv1_traverse_paint( face,
                                               paint.u.composite.backdrop_paint );
      traverse_result =
        traverse_result && colrv1_traverse_paint( face,
                                                  paint.u.composite.source_paint );
        break;
    }

    case FT_COLR_PAINTFORMAT_SOLID:
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
    {
      colrv1_draw_paint( face, paint );
      break;
    }

    case FT_COLR_PAINT_FORMAT_MAX:
    case FT_COLR_PAINTFORMAT_UNSUPPORTED:
      LOG ( INFO ) << "Invalid paint format.";
      break;
    }

    return traverse_result;
  }


  bool colrv1_start_glyph( const FT_Face&           ft_face,
                           uint16_t                 glyph_id,
                           FT_Color_Root_Transform  root_transform )
  {
    FT_OpaquePaint  opaque_paint;
    bool            has_colrv1_layers = false;


    opaque_paint.p = nullptr;

    if ( FT_Get_Color_Glyph_Paint( ft_face,
                                   glyph_id,
                                   root_transform,
                                   &opaque_paint ) )
    {
      has_colrv1_layers = true;
      colrv1_traverse_paint( ft_face, opaque_paint );
    }

    return has_colrv1_layers;
  }
}


  void
  freetype::FaceVisitorColrV1::
  run( Unique_FT_Face  face )
  {
    FT_OpaquePaint  opaque_paint;
    unsigned long   num_glyphs;


    assert( face != nullptr );

    num_glyphs = face->num_glyphs;

    LOG( INFO ) << "Starting COLR v1 traversal.";

    opaque_paint.p = nullptr;

    for ( uint16_t  glyph_id = 0;
          glyph_id < num_glyphs;
          glyph_id++ )
    {
      if ( num_traversed_glyphs >= MAX_TRAVERSE_GLYPHS )
      {
        LOG( INFO ) << "Finished with this font after "
                    << MAX_TRAVERSE_GLYPHS
                    << " traversed glyphs.";
        return;
      }

      if ( !colrv1_start_glyph( face.get(),
                                glyph_id,
                                FT_COLOR_INCLUDE_ROOT_TRANSFORM ) )
      {
        LOG( INFO ) << "No COLRv1 glyph for glyph id " << glyph_id << ".";
        continue;
      }

      num_traversed_glyphs++;
    }
  }
