/* Pango
 * pangocairo-render.c: Rendering routines to Cairo surfaces
 *
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "pangocairo-private.h"

typedef struct _PangoCairoRendererClass PangoCairoRendererClass;

#define PANGO_CAIRO_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PANGO_TYPE_CAIRO_RENDERER, PangoCairoRendererClass))
#define PANGO_IS_CAIRO_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PANGO_TYPE_CAIRO_RENDERER))
#define PANGO_CAIRO_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PANGO_TYPE_CAIRO_RENDERER, PangoCairoRendererClass))

struct _PangoCairoRenderer
{
  PangoRenderer parent_instance;

  cairo_t *cr;
};

struct _PangoCairoRendererClass
{
  PangoRendererClass parent_class;
};

G_DEFINE_TYPE (PangoCairoRenderer, pango_cairo_renderer, PANGO_TYPE_RENDERER)

static void
set_color (PangoCairoRenderer *crenderer,
	   PangoRenderPart     part)
{
  PangoColor *color = pango_renderer_get_color (PANGO_RENDERER (crenderer), part);

  if (color)
    cairo_set_rgb_color (crenderer->cr,
			 color->red / 65535.,
			 color->green / 65535.,
			 color->blue / 65535.);
}
     
static void
pango_cairo_renderer_draw_glyphs (PangoRenderer     *renderer,
				  PangoFont         *font,
				  PangoGlyphString  *glyphs,
				  int                x,
				  int                y)
{
  PangoCairoRenderer *crenderer = PANGO_CAIRO_RENDERER (renderer);

  /* cairo_glyph_t is 24 bytes */
#define MAX_STACK 40
  
  int i;
  int x_position = 0;
  cairo_glyph_t *cairo_glyphs;
  cairo_glyph_t stack_glyphs[MAX_STACK];

  cairo_save (crenderer->cr);

  set_color (crenderer, PANGO_RENDER_PART_FOREGROUND);

  if (glyphs->num_glyphs > MAX_STACK)
    cairo_glyphs = g_new (cairo_glyph_t, glyphs->num_glyphs);
  else
    cairo_glyphs = stack_glyphs;

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyphInfo *gi = &glyphs->glyphs[i];

      cairo_glyphs[i].index = gi->glyph;
      cairo_glyphs[i].x = (double)(x + x_position + gi->geometry.x_offset) / PANGO_SCALE;
      cairo_glyphs[i].y = (double)(y + gi->geometry.y_offset) / PANGO_SCALE;

      x_position += gi->geometry.width;
    }

  cairo_set_font (crenderer->cr,
		  _pango_cairo_font_get_cairo_font (PANGO_CAIRO_FONT (font)));
  cairo_show_glyphs (crenderer->cr, cairo_glyphs, glyphs->num_glyphs);
  
  if (glyphs->num_glyphs > MAX_STACK)
    g_free (cairo_glyphs);

  cairo_restore (crenderer->cr);
  
#undef MAX_STACK
}

static void
pango_cairo_renderer_draw_rectangle (PangoRenderer     *renderer,
				     PangoRenderPart    part,
				     int                x,
				     int                y,
				     int                width,
				     int                height)
{
  PangoCairoRenderer *crenderer = PANGO_CAIRO_RENDERER (renderer);

  cairo_save (crenderer->cr);

  set_color (crenderer, part);

  cairo_rectangle (crenderer->cr,
		   (double)x / PANGO_SCALE, (double)y / PANGO_SCALE, 
		   (double)width / PANGO_SCALE, (double)height / PANGO_SCALE);
  cairo_fill (crenderer->cr);
  
  cairo_restore (crenderer->cr);
}

/* Draws an error underline that looks like one of:
 *              H       E                H
 *     /\      /\      /\        /\      /\               -
 *   A/  \    /  \    /  \     A/  \    /  \              |
 *    \   \  /    \  /   /D     \   \  /    \             |
 *     \   \/  C   \/   /        \   \/   C  \            | height = HEIGHT_SQUARES * square
 *      \      /\  F   /          \  F   /\   \           | 
 *       \    /  \    /            \    /  \   \G         |
 *        \  /    \  /              \  /    \  /          |
 *         \/      \/                \/      \/           -
 *         B                         B       
 * |----|
 *   unit_width = (HEIGHT_SQUARES - 1) * square
 *
 * The x, y, width, height passed in give the desired bounding box;
 * x/width are adjusted to make the underline a integer number of units
 * wide.
 */
#define HEIGHT_SQUARES 2.5

static void
pango_cairo_renderer_draw_error_underline (PangoRenderer *renderer,
					   int            x,
					   int            y,
					   int            width,
					   int            height)
{
  PangoCairoRenderer *crenderer = PANGO_CAIRO_RENDERER (renderer);
  cairo_t *cr = crenderer->cr;

  double square = height / HEIGHT_SQUARES;
  double unit_width = (HEIGHT_SQUARES - 1) * square;
  int width_units = (width + unit_width / 2) / unit_width;
  double y_top, y_bottom;
  int i;
  
  cairo_save (cr);

  set_color (crenderer, PANGO_RENDER_PART_UNDERLINE);

  x += (width - width_units * unit_width);
  width = width_units * unit_width;
  
  cairo_new_path (cr);
  
  y_top = y + height;
  y_bottom = y;
  
  /* Bottom of squiggle */
  cairo_move_to (cr, x - square / 2, y_top - square / 2); /* A */
  for (i = 0; i < width_units; i += 2)
    {
      double x_middle = x + (i + 1) * unit_width;
      double x_right = x + (i + 2) * unit_width;
      
      cairo_line_to (cr, x_middle, y_bottom); /* B */
      
      if (i + 1 == width_units)
	/* Nothing */;
      else if (i + 2 == width_units)
	cairo_line_to (cr, x_right + square / 2, y_top - square / 2); /* D */
      else
	cairo_line_to (cr, x_right, y_top - square); /* C */
    }
  
  /* Top of squiggle */
  for (i -= 2; i >= 0; i -= 2)
    {
      double x_left = x + i * unit_width;
      double x_middle = x + (i + 1) * unit_width;
      double x_right = x + (i + 2) * unit_width;
      
      if (i + 1 == width_units)
	cairo_line_to (cr, x_middle + square / 2, y_bottom + square / 2); /* G */
      else
	{
	  if (i + 2 == width_units)
	    cairo_line_to (cr, x_right, y_top); /* E */
	  cairo_line_to (cr, x_middle, y_bottom + square); /* F */
	}
    
      cairo_line_to (cr, x_left, y_top);   /* H */
    }
  
  cairo_close_path (cr);
  cairo_fill (cr);

  cairo_restore (cr);
}

static void
pango_cairo_renderer_init (PangoCairoRenderer *renderer)
{
}

static void
pango_cairo_renderer_class_init (PangoCairoRendererClass *klass)
{
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  renderer_class->draw_glyphs = pango_cairo_renderer_draw_glyphs;
  renderer_class->draw_rectangle = pango_cairo_renderer_draw_rectangle;
  renderer_class->draw_error_underline = pango_cairo_renderer_draw_error_underline;
}

static void
current_point_to_origin (cairo_t *cr)
{
  double x, y;

  cairo_current_point (cr, &x, &y);
  cairo_translate (cr, x, y);
}

/**
 * pango_cairo_show_glyphs:
 * @cr: a Cairo context
 * @font: a #PangoFont
 * @glyphs: a #PangoGlyphString
 * 
 * Draws the glyphs in @glyphs in the specified cairo context.
 * The origin of the glyphs (the left edge of the baseline) will
 * be drawn at the current point of the cairo context.
 *
 * Since: 1.10
 **/
void
pango_cairo_show_glyphs (cairo_t          *cr,
			 PangoFont        *font,
			 PangoGlyphString *glyphs)
{
  PangoFontMap *fontmap;
  PangoCairoRenderer *crenderer;
  PangoRenderer *renderer;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_CAIRO_FONT (font));
  g_return_if_fail (glyphs != NULL);

  fontmap = PANGO_FC_FONT (font)->fontmap;
  renderer = _pango_cairo_font_map_get_renderer (PANGO_CAIRO_FONT_MAP (fontmap));
  crenderer = PANGO_CAIRO_RENDERER (renderer);

  cairo_save (cr);
  current_point_to_origin (cr);

  crenderer->cr = cr;
  pango_renderer_draw_glyphs (renderer, font, glyphs, 0, 0);
  crenderer->cr = NULL;
  
  cairo_restore (cr);
}

/**
 * pango_cairo_show_layout_line:
 * @cr: a Cairo context
 * @line: a #PangoLayoutLine
 * 
 * Draws a #PangoLayoutLine in the specified cairo context.
 * The origin of the glyphs (the left edge of the line) will
 * be drawn at the current point of the cairo context.
 *
 * Since: 1.10
 **/
void
pango_cairo_show_layout_line (cairo_t          *cr,
			      PangoLayoutLine  *line)
{
  PangoContext *context;
  PangoFontMap *fontmap;
  PangoRenderer *renderer;
  PangoCairoRenderer *crenderer;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (line != NULL);

  context = pango_layout_get_context (line->layout);
  fontmap = pango_context_get_font_map (context);
  renderer = _pango_cairo_font_map_get_renderer (PANGO_CAIRO_FONT_MAP (fontmap));
  crenderer = PANGO_CAIRO_RENDERER (renderer);

  cairo_save (cr);
  current_point_to_origin (cr);

  crenderer->cr = cr;
  pango_renderer_draw_layout_line (renderer, line, 0, 0);
  crenderer->cr = NULL;
  
  cairo_restore (cr);
}

/**
 * pango_cairo_show_layout:
 * @cr: a Cairo context
 * @layout: a Pango layout
 * 
 * Draws a #PangoLayoutLine in the specified cairo context.
 * The top-left corner of the #PangoLayout will be drawn
 * at the current point of the cairo context.
 *
 * Since: 1.10
 **/
void
pango_cairo_show_layout (cairo_t     *cr,
			 PangoLayout *layout)
{
  PangoContext *context;
  PangoFontMap *fontmap;
  PangoRenderer *renderer;
  PangoCairoRenderer *crenderer;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  context = pango_layout_get_context (layout);
  fontmap = pango_context_get_font_map (context);
  renderer = _pango_cairo_font_map_get_renderer (PANGO_CAIRO_FONT_MAP (fontmap));
  crenderer = PANGO_CAIRO_RENDERER (renderer);
  
  cairo_save (cr);
  current_point_to_origin (cr);

  crenderer->cr = cr;
  pango_renderer_draw_layout (renderer, layout, 0, 0);
  crenderer->cr = NULL;
  
  cairo_restore (cr);
}
