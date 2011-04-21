/* grTOGL3.c -
 *
 * Copyright 2003 Open Circuit Design, Inc., for MultiGiG Ltd.
 *
 * This file contains additional functions to manipulate an X window system
 * color display.  Included here are device-dependent routines to draw and 
 * erase text and draw a grid.
 *
 */

#include <stdio.h>
#include <string.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "tcltk/tclmagic.h"
#include "utils/magic.h"
#include "utils/geometry.h"
#include "utils/malloc.h"
#include "windows/windows.h"
#include "graphics/graphics.h"
#include "graphics/graphicsInt.h"
#include "textio/textio.h"
#include "utils/signals.h"
#include "utils/utils.h"
#include "utils/hash.h"
#include "graphics/grTOGLInt.h"
#include "graphics/grTkCommon.h"
#include "database/fonts.h"

extern Display *grXdpy;

/* locals */

GLuint  grXBases[4];


/*---------------------------------------------------------
 * grtoglDrawGrid:
 *	grxDrawGrid adds a grid to the grid layer, using the current
 *	write mask and color.
 *
 * Results:
 *	TRUE is returned normally.  However, if the grid gets too small
 *	to be useful, then nothing is drawn and FALSE is returned.
 *
 * Side Effects:    None.
 *---------------------------------------------------------
 */

bool
grtoglDrawGrid (prect, outline, clip)
    Rect *prect;			/* A rectangle that forms the template
			         * for the grid.  Note:  in order to maintain
			         * precision for the grid, the rectangle
			         * coordinates are specified in units of
			         * screen coordinates multiplied by SUBPIXEL.
			         */
    int outline;		/* the outline style */
    Rect *clip;			/* a clipping rectangle */
{
    int xsize, ysize;
    int x, y;
    int xstart, ystart;
    int snum, low, hi, shifted;

    xsize = prect->r_xtop - prect->r_xbot;
    ysize = prect->r_ytop - prect->r_ybot;
    if (!xsize || !ysize || GRID_TOO_SMALL(xsize, ysize))
	return FALSE;
    
    xstart = prect->r_xbot % xsize;
    while (xstart < clip->r_xbot << SUBPIXELBITS) xstart += xsize;
    ystart = prect->r_ybot % ysize;
    while (ystart < clip->r_ybot << SUBPIXELBITS) ystart += ysize;
    
    grtoglSetLineStyle(outline);

    glBegin(GL_LINES);

    snum = 0;
    low = clip->r_ybot;
    hi = clip->r_ytop;
    for (x = xstart; x < (clip->r_xtop+1) << SUBPIXELBITS; x += xsize)
    {
	shifted = x >> SUBPIXELBITS;
	glVertex2i(shifted, low);
	glVertex2i(shifted, hi);
	snum++;
    }

    snum = 0;
    low = clip->r_xbot;
    hi = clip->r_xtop;
    for (y = ystart; y < (clip->r_ytop+1) << SUBPIXELBITS; y += ysize)
    {
	shifted = y >> SUBPIXELBITS;
	glVertex2i(low, shifted);
	glVertex2i(hi, shifted);
	snum++;
    }
    glEnd();
    return TRUE;
}


/*---------------------------------------------------------
 * grtoglLoadFont
 *	This local routine transfers the X font bitmaps
 *	into OpenGL display lists for simple text
 *	rendering.
 *
 * Results:	Success/Failure
 *
 * Side Effects:    None.
 *---------------------------------------------------------
 */

bool
grtoglLoadFont()
{
    Font id;
    unsigned int i;

    for (i = 0; i < 4; i++) {
	id = Tk_FontId(grTkFonts[i]);

	grXBases[i] = glGenLists(256);
	if (grXBases[i] == 0) {
	    TxError("Out of display lists!\n");
	    return FALSE;
	}
	glXUseXFont(id, 0, 256, grXBases[i]);
    }
    return TRUE;
}


/*---------------------------------------------------------
 * grtoglSetCharSize:
 *	This local routine sets the character size in the display,
 *	if necessary.
 *
 * Results:	None.
 *
 * Side Effects:    None.
 *---------------------------------------------------------
 */

void
grtoglSetCharSize (size)
    int size;		/* Width of characters, in pixels (6 or 8). */
{
    toglCurrent.fontSize = size;
    switch (size)
    {
	case GR_TEXT_DEFAULT:
	case GR_TEXT_SMALL:
	    toglCurrent.font = grSmallFont;
	    break;
	case GR_TEXT_MEDIUM:
	    toglCurrent.font = grMediumFont;
	    break;
	case GR_TEXT_LARGE:
	    toglCurrent.font = grLargeFont;
	    break;
	case GR_TEXT_XLARGE:
	    toglCurrent.font = grXLargeFont;
	    break;
	default:
	    TxError("%s%d\n", "grtoglSetCharSize: Unknown character size ",
		size );
	    break;
    }
}


/*
 * ----------------------------------------------------------------------------
 * GrTOGLTextSize --
 *
 *	Determine the size of a text string. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A rectangle is filled in that is the size of the text in pixels.
 *	The origin (0, 0) of this rectangle is located on the baseline
 *	at the far left side of the string.
 * ----------------------------------------------------------------------------
 */

void
GrTOGLTextSize(text, size, r)
    char *text;
    int size;
    Rect *r;
{
    Tk_FontMetrics overall;
    Tk_Font font;
    int width;
    
    switch (size) {
    case GR_TEXT_DEFAULT:
    case GR_TEXT_SMALL:
	font = grSmallFont;
	break;
    case GR_TEXT_MEDIUM:
	font = grMediumFont;
	break;
    case GR_TEXT_LARGE:
	font = grLargeFont;
	break;
    case GR_TEXT_XLARGE:
	font = grXLargeFont;
	break;
    default:
	TxError("%s%d\n", "GrTOGLTextSize: Unknown character size ",
		size );
	break;
    }
    if (font == NULL) return;
    Tk_GetFontMetrics(font, &overall);
    width = Tk_TextWidth(font, text, strlen(text));
    /* Hack alert!  Tk_TextWidth returns values too small! */
    width = width + (width >> 4);
    r->r_ytop = overall.ascent;
    r->r_ybot = -overall.descent;
    r->r_xtop = width;
    r->r_xbot = 0;
}


/*
 * ----------------------------------------------------------------------------
 * GrTOGLReadPixel --
 *
 *	Read one pixel from the screen.
 *
 * Results:
 *	An integer containing the pixel's color.
 *
 * Side effects:
 *	none.
 *
 * ----------------------------------------------------------------------------
 */

int
GrTOGLReadPixel (w, x, y)
    MagWindow *w;
    int x,y;		/* the location of a pixel in screen coords */
{
    return 0;		/* OpenGL has no such function, so return 0 */
}


/*
 * ----------------------------------------------------------------------------
 * GrTOGLBitBlt --
 *
 *	Copy information in bit block transfers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	changes the screen.
 * ----------------------------------------------------------------------------
 */

void
GrTOGLBitBlt(r, p)
    Rect *r;
    Point *p;
{
    glCopyPixels(r->r_xbot, r->r_ybot, r->r_xtop - r->r_xbot + 1,
		r->r_ytop - r->r_ybot + 1, GL_COLOR);
}

#ifdef VECTOR_FONTS

/*
 *----------------------------------------------------------------------
 *
 * Technically, there should be no self-intersecting polygons in outline
 * fonts.  However, decomposition of bezier curves into line segments
 * may occasionally produce one, so it needs to be handled.
 *----------------------------------------------------------------------
 */

void
myCombine(GLdouble coords[3], GLdouble *vertex_data[4],
          GLfloat weight[4], GLdouble **outData, void *dataptr)
{
    /* This needs to be free'd at the end of gluTessEndPolygon()! */
    GLdouble *new = (GLdouble *)mallocMagic(2 * sizeof(GLdouble));
    new[0] = coords[0];
    new[1] = coords[1];
    *outData = new;
    /* Diagnostic */
    TxError("Intersecting polygon in char \"%c\" at %g %g!\n", 
	*((char *)dataptr), coords[0], coords[1]);
}

/*
 *----------------------------------------------------------------------
 * Draw a text character
 * This routine differs from grtoglFillPolygon() in that it uses the
 * glu library to handle non-convex polygons as may appear in font
 * outlines.
 *----------------------------------------------------------------------
 */

void
grtoglDrawCharacter(clist, tc, pixsize)
    FontChar *clist;
    unsigned char tc;
    int pixsize;
{
    Point *tp;
    int np, nptotal;
    int i, j;
    static GLUtesselator *tess = NULL;
    static GLdouble *v = NULL;
    static int maxnp = 0;
    FontChar *ccur;

    if (pixsize < 5) return;	/* Label too small to be useful */

    if (tess == NULL)
    {
	tess = gluNewTess();
	gluTessCallback(tess, GLU_TESS_BEGIN, (_GLUfuncptr)glBegin);
	gluTessCallback(tess, GLU_TESS_VERTEX, (_GLUfuncptr)glVertex3dv);
	gluTessCallback(tess, GLU_TESS_END, (_GLUfuncptr)glEnd);
	gluTessCallback(tess, GLU_TESS_COMBINE_DATA, (_GLUfuncptr)myCombine);
    }
    // Boundary-only does not look particularly good. . .
    gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

    nptotal = 0;
    for (ccur = clist; ccur != NULL; ccur = ccur->fc_next)
	nptotal += ccur->fc_numpoints;

    if (nptotal > maxnp)
    {
	if (v != NULL) freeMagic((char *)v);
	maxnp = nptotal;
	v = (GLdouble *)mallocMagic(nptotal * 3 * sizeof(GLdouble));
    }

    j = 0;
    for (ccur = clist; ccur != NULL; ccur = ccur->fc_next)
    {
	tp = ccur->fc_points;
	np = ccur->fc_numpoints;

	for (i = 0; i < np; i++, j += 3) {
	    v[j] = tp[i].p_x;
	    v[j + 1] = tp[i].p_y;
	    v[j + 2] = 0;
	}
    }

    gluTessBeginPolygon(tess, (GLvoid *)(&tc));
    j = 0;
    for (ccur = clist; ccur != NULL; ccur = ccur->fc_next)
    {
	np = ccur->fc_numpoints;
	gluTessBeginContour(tess);
	for (i = 0; i < np; i++, j += 3) {
	    gluTessVertex(tess, &v[j], &v[j]);
	}
	gluTessEndContour(tess);
    }
    gluTessEndPolygon(tess);
}

/*---------------------------------------------------------
 * grtoglFontText:
 *
 *	This routine draws text from font vectors using the
 *	font vector routines in DBlabel.c.  Text is clipped
 *	to the clipping rectangle.
 *
 *	For speed, we should be transferring the font
 *	vectors into OpenGL display lists!
 *	
 *---------------------------------------------------------
 */

void
grtoglFontText(text, font, size, rotate, pos, clip, obscure)
    char *text;			/* The text to be drawn */
    int   font;			/* Font to use from fontList */
    int   size;			/* Pixel size of the font */
    int	  rotate;		/* Text rotation */
    Point *pos;			/* Text base position */
    Rect  *clip;		/* Clipping area */
    LinkedRect *obscure;	/* List of obscuring areas */ 
{
    char *tptr;
    Point *coffset;		/* vector to next character */
    Rect *cbbox;
    GLfloat fsize, matvals[16];
    FontChar *clist;
    int cheight, baseline;
    float tmp;

    /* Keep it simple for now---ignore clip and obscure */

    glDisable(GL_BLEND);
    glPushMatrix();
    glTranslated(pos->p_x, pos->p_y, 0);
    glRotated(rotate, 0, 0, 1);

    /* Get label size */
    cbbox = &DBFontList[font]->mf_extents;

    fsize = (GLfloat)size / (GLfloat)cbbox->r_ytop;
    glScalef(fsize, fsize, 1.0);

    /* Adjust to baseline */
    baseline = 0;
    for (tptr = text; *tptr != '\0'; tptr++)
    {
	DBFontChar(font, *tptr, NULL, NULL, &cbbox);
	if (cbbox->r_ybot < baseline)
	    baseline = cbbox->r_ybot;
    }
    glTranslated(0, -baseline, 0);

    for (tptr = text; *tptr != '\0'; tptr++)
    {
	DBFontChar(font, *tptr, &clist, &coffset, NULL);
	grtoglDrawCharacter(clist, *tptr, size);
	glTranslated(coffset->p_x, coffset->p_y, 0);
    }
    glPopMatrix();
}

#endif /* VECTOR_FONTS */

/*---------------------------------------------------------
 * grtoglPutText:
 *      (modified on SunPutText)
 *
 *	This routine puts a chunk of text on the screen in the current
 *	color, size, etc.  The caller must ensure that it fits on
 *	the screen -- no clipping is done except to the obscuring rectangle
 *	list and the clip rectangle.
 *
 * Results:	
 *	none.
 *
 * Side Effects:
 *	The text is drawn on the screen.  
 *
 *---------------------------------------------------------
 */

void
grtoglPutText (text, pos, clip, obscure)
    char *text;			/* The text to be drawn. */
    Point *pos;			/* A point located at the leftmost point of
				 * the baseline for this string.
				 */
    Rect *clip;			/* A rectangle to clip against */
    LinkedRect *obscure;	/* A list of obscuring rectangles */

{
    Rect location;
    Rect overlap;
    Rect textrect;
    LinkedRect *ob;
    void grTOGLGeoSub();
    int i;
    float tscale;

    GrTOGLTextSize(text, toglCurrent.fontSize, &textrect);

    location.r_xbot = pos->p_x + textrect.r_xbot;
    location.r_xtop = pos->p_x + textrect.r_xtop;
    location.r_ybot = pos->p_y + textrect.r_ybot;
    location.r_ytop = pos->p_y + textrect.r_ytop;

    /* erase parts of the bitmap that are obscured */
    for (ob = obscure; ob != NULL; ob = ob->r_next)
    {
	if (GEO_TOUCH(&ob->r_r, &location))
	{
	    overlap = location;
	    GeoClip(&overlap, &ob->r_r);
	    grTOGLGeoSub(&location, &overlap);
	}
    }
 
    overlap = location;
    GeoClip(&overlap, clip);

    /* copy the text to the color screen */
    if ((overlap.r_xbot < overlap.r_xtop)&&(overlap.r_ybot <= overlap.r_ytop))
    {
	glScissor(overlap.r_xbot, overlap.r_ybot, overlap.r_xtop - overlap.r_xbot,
		overlap.r_ytop - overlap.r_ybot);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glRasterPos2i(pos->p_x, pos->p_y);
	glListBase(grXBases[(toglCurrent.fontSize == GR_TEXT_DEFAULT) ?
		GR_TEXT_SMALL : toglCurrent.fontSize]);
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, (unsigned char *)text);
	glDisable(GL_SCISSOR_TEST);
    }
}


/* grTOGLGeoSub:
 *	return the tallest sub-rectangle of r not obscured by area
 *	area must be within r.
 */

void
grTOGLGeoSub(r, area)
Rect *r;		/* Rectangle to be subtracted from. */
Rect *area;		/* Area to be subtracted. */

{
    if (r->r_xbot == area->r_xbot) r->r_xbot = area->r_xtop;
    else
    if (r->r_xtop == area->r_xtop) r->r_xtop = area->r_xbot;
    else
    if (r->r_ybot <= area->r_ybot) r->r_ybot = area->r_ytop;
    else
    if (r->r_ytop == area->r_ytop) r->r_ytop = area->r_ybot;
    else
    r->r_xtop = area->r_xbot;
}
