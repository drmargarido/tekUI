
/*
**	$Id: display_x11_font.c,v 2006/09/10 14:38:04 fschulze Exp $
**	teklib/src/display_x11/display_x11_font.c - x11 font management
**
**  Written by Franciska Schulze <fschulze@schulze-mueller.de>
**	See copyright notice in teklib/COPYRIGHT
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <ctype.h>
#include <limits.h>

#include <tek/debug.h>
#include <tek/teklib.h>
#include <tek/proto/hal.h>

#include "display_x11_mod.h"

/*****************************************************************************/

static TBOOL hostopenfont(X11DISPLAY *mod, struct FontNode *fn,
	struct fnt_attr *fattr);
#if defined(ENABLE_XFT)
static void hostqueryfonts_xft(X11DISPLAY *mod, struct FontQueryHandle *fqh,
	struct fnt_attr *fattr);
static TAPTR initlib(X11DISPLAY *mod, TSTRPTR libname, const TSTRPTR *libsyms,
	TAPTR iface, TINT numsyms);
#endif
static void hostqueryfonts_xlib(X11DISPLAY *mod, struct FontQueryHandle *fqh,
	struct fnt_attr *fattr);

/*****************************************************************************/
#if defined(ENABLE_XFT)
static const
TSTRPTR libxftsyms[LIBXFT_NUMSYMS] =
{
	"XftFontOpen",
	"XftFontClose",
	"XftTextExtentsUtf8",
	"XftDrawStringUtf8",
	"XftDrawRect",
	"XftLockFace",
	"XftUnlockFace",
	"XftColorAllocValue",
	"XftColorFree",
	"XftDrawCreate",
	"XftDrawDestroy",
	"XftDrawSetClip",
};

static const
TSTRPTR libfcsyms[LIBFC_NUMSYMS] =
{
	"FcDefaultSubstitute",
	"FcFontSetDestroy",
	"FcFontSort",
	"FcPatternAddBool",
	"FcPatternAddDouble",
	"FcPatternAddInteger",
	"FcPatternBuild",
	"FcPatternDestroy",
	"FcPatternPrint",
	"FcPatternGetString",
	"FcPatternGetDouble",
	"FcPatternGetInteger",
	"FcPatternGetBool",
	"FcInit",
	"FcFini",
};
#endif

/*****************************************************************************/
/* try to open libfontconfig and libxft and bind all symbols
** if initlibxft() succeeds g->use_xft is set to TTRUE
** undefining ENABLE_XFT can be used to enforce xlib based font rendering
*/

LOCAL TBOOL x11_initlibxft(X11DISPLAY *mod)
{
	#if defined(ENABLE_XFT)

	#if 0
	int major, minor, first;
 	if (XQueryExtension(mod->x11_Display, "Composite", &major, &minor, &first))
 		return TFALSE;
	if (mod->x11_Depth > 24)
		return TFALSE;
 	#endif

	/* init fontconfig (needed for font matching) */
	mod->x11_libfchandle = initlib(mod, "libfontconfig.so", libfcsyms,
		(void *)&mod->x11_fciface, LIBFC_NUMSYMS);

	if (mod->x11_libfchandle)
	{
		/* try to init libXft, if it fails, there will be no font
		antialiasing at all :( */
		mod->x11_libxfthandle = initlib(mod, "libXft.so", libxftsyms,
							(void *)&mod->x11_xftiface, LIBXFT_NUMSYMS);

		if (mod->x11_libxfthandle)
		{
			if ((*mod->x11_fciface.FcInit)())
				mod->x11_use_xft = TTRUE;
			else
				TDBPRINTF(TDB_ERROR,("fontconfig init failed\n"));
		}
	}

	if (!mod->x11_use_xft)
		TDBPRINTF(TDB_WARN,("defaulting to xlib based font rendering\n"));

	return mod->x11_use_xft;

	#else

	return TFALSE;

	#endif
}

LOCAL void x11_exitlibxft(X11DISPLAY *mod)
{
	#if defined(ENABLE_XFT)
	if (mod->x11_libfchandle)
		(*mod->x11_fciface.FcFini)();
	if (mod->x11_libfchandle)
		dlclose(mod->x11_libfchandle);
	if (mod->x11_libxfthandle)
		dlclose(mod->x11_libxfthandle);
	#endif
}

/*****************************************************************************/
/* dlopen the library named libname, bind numsyms symbols from libsyms[]
** to iface and return the libhandle obtained by dlopen or TNULL
*/
#if defined(ENABLE_XFT)
static TAPTR initlib(X11DISPLAY *mod, TSTRPTR libname, const TSTRPTR *libsyms,
	TAPTR iface, TINT numsyms)
{
	TAPTR libhandle = TNULL;

	/* try to open lib */
	libhandle = dlopen(libname, RTLD_NOW);
	if (libhandle)
	{
		TINT i;

		/* clear any old error conditions */
		dlerror();

		for (i = 0; i < numsyms; i++)
		{
			((void **)iface)[i] = dlsym(libhandle, libsyms[i]);
			if (dlerror())	break;
		}

		if (i != numsyms)
		{
			/* missing symbols */
			TDBPRINTF(TDB_ERROR,("%s initialisation failed\n", libname));
			dlclose(libhandle);
			libhandle = TNULL;
		}
	}
	else
	{
		/* lib not found */
		TDBPRINTF(TDB_ERROR,("failed to open %s\n", libname));
	}

	return libhandle;
}
#endif

/*****************************************************************************/
/* FontQueryHandle destructor
** free all memory associated with a fontqueryhandle including
** all fontquerynodes, a fontqueryhandle is obtained by calling
** x11_hostqueryfonts()
*/

static THOOKENTRY TTAG
fqhdestroy(struct THook *hook, TAPTR obj, TTAG msg)
{
	if (msg == TMSG_DESTROY)
	{
		struct FontQueryHandle *fqh = obj;
		X11DISPLAY *mod = fqh->handle.thn_Owner;
		TAPTR TExecBase = TGetExecBase(mod);
		struct TNode *node, *next;

		node = fqh->reslist.tlh_Head;
		for (; (next = node->tln_Succ); node = next)
		{
			struct FontQueryNode *fqn = (struct FontQueryNode *)node;

			/* remove from resultlist */
			TRemove(&fqn->node);

			/* destroy fontname */
			if (fqn->tags[0].tti_Value)
				TFree((TAPTR)fqn->tags[0].tti_Value);

			/* destroy node */
			TFree(fqn);
		}

		/* destroy queryhandle */
		TFree(fqh);
	}

	return 0;
}

/*****************************************************************************/
/* extract a substring from an x11 font
** m should refer to the position of a '-' and fstring should be
** something like "-*-fname-medium-r-*-*-xxx-*-*-*-*-*-iso8859-1"
*/
static TSTRPTR
fnt_getsubstring(X11DISPLAY *mod, TSTRPTR fstring, TINT m)
{
	TINT p = 0;
	TINT mcount = 0;
	TSTRPTR substr = TNULL;
	TAPTR TExecBase = TGetExecBase(mod);
	TSIZE len = strlen(fstring);
	TSIZE i;

	/* match '-' at pos m and m+1 */
	for (i = 0; i < len; i++)
	{
		if (fstring[i] == '-')
		{
			mcount++;
			if (mcount == m)
				p = i;
			if (mcount == m+1)
				break;
		}
	}

	/* extract substring */
	substr = TAlloc0(mod->x11_MemMgr, i-p);
	if (substr)
	{
		TCopyMem(fstring+p+1, substr, i-p-1);
		TDBPRINTF(TDB_TRACE,("extracted = '%s'\n", substr));
	}
	else
		TDBPRINTF(TDB_FAIL,("out of memory :(\n"));

	return substr;
}

/*****************************************************************************/
/* extract properties from an x11 font string and convert
** them to our own taglist based properties system
*/
static void
fnt_getattr(X11DISPLAY *mod, TTAGITEM *tag, TSTRPTR fstring)
{
	TAPTR TExecBase = TGetExecBase(mod);

	/* "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-%s" */
	switch (tag->tti_Tag)
	{
		case TVisual_FontName:
		{
			/* extract name -> 2th and 3th '-' */
			tag->tti_Value = (TTAG) fnt_getsubstring(mod, fstring, 2);
			break;
		}
		case TVisual_FontPxSize:
		{
			/* extract pixel size -> 7th and 8th '-' */
			TSTRPTR pxsize = fnt_getsubstring(mod, fstring, 7);
			if (pxsize)
			{
				tag->tti_Value = (TTAG) atoi(pxsize);
				TFree(pxsize);
			}
			break;
		}
		case TVisual_FontItalic:
		{
			/* extract slant -> 4th and 5th '-' */
			TSTRPTR slant = fnt_getsubstring(mod, fstring, 4);
			if (slant)
			{
				/* italic attribute set? */
				if (strncmp(FNT_SLANT_I, slant, strlen(FNT_SLANT_I)) == 0)
					tag->tti_Value = (TTAG) TTRUE;
				TFree(slant);
			}
			break;
		}
		case TVisual_FontBold:
		{
			/* extract weight -> 3th and 4th '-' */
			TSTRPTR weight = fnt_getsubstring(mod, fstring, 3);
			if (weight)
			{
				/* bold attribute set? */
				if (strncmp(FNT_WGHT_BOLD, weight, strlen(FNT_WGHT_BOLD)) == 0)
					tag->tti_Value = (TTAG) TTRUE;
				TFree(weight);
			}
			break;
		}
		default:
			break;
	}
}

/*****************************************************************************/
/* allocate a fontquerynode and fill in properties derived from
** a FcPattern
*/
#if defined(ENABLE_XFT)
static struct FontQueryNode *
fnt_getfqnode_xft(X11DISPLAY *mod, FcPattern *pattern, TUINT pxsize)
{
	TAPTR TExecBase = TGetExecBase(mod);
	struct FontQueryNode *fqnode = TNULL;

	/* allocate fquery node */
	fqnode = TAlloc0(mod->x11_MemMgr, sizeof(struct FontQueryNode));
	if (fqnode)
	{
		FcChar8 *fname = TNULL;

		/* fquerynode ready - fill in attributes */
 		if ((*mod->x11_fciface.FcPatternGetString)(pattern, FC_FAMILY, 0, &fname)
 				== FcResultMatch)
 		{
 			/* got fontname, now copy it to fqnode */
 			TINT flen = strlen((char *) fname);
			TSTRPTR myfname = TAlloc0(mod->x11_MemMgr, flen + 1);

			if (myfname)
			{
				TCopyMem(fname, myfname, flen);
 				fqnode->tags[0].tti_Tag = TVisual_FontName;
				fqnode->tags[0].tti_Value = (TTAG) myfname;
			}
			else
				TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
		}

		if (fqnode->tags[0].tti_Value)
		{
			double fpxsize;
			FcBool fscale = TFALSE;
			TINT fslant, fweight, i = 1;
			TBOOL fitalic = TFALSE, fbold = TFALSE;

			if ((*mod->x11_fciface.FcPatternGetDouble)
				(pattern, FC_PIXEL_SIZE, 0, &fpxsize) == FcResultMatch)
			{
				fqnode->tags[i].tti_Tag = TVisual_FontPxSize;
				fqnode->tags[i++].tti_Value = (TTAG) fpxsize;
			}
			else
			{
				if (pxsize != FNTQUERY_UNDEFINED)
				{
					/* font pxsize argument got ignored when matching,
					   now it is added back */
					fqnode->tags[i].tti_Tag = TVisual_FontPxSize;
					fqnode->tags[i++].tti_Value = pxsize;
				}
			}

			if ((*mod->x11_fciface.FcPatternGetInteger)
				(pattern, FC_SLANT, 0, &fslant) == FcResultMatch)
			{
				if (fslant == FC_SLANT_ITALIC) fitalic = TTRUE;
				fqnode->tags[i].tti_Tag = TVisual_FontItalic;
				fqnode->tags[i++].tti_Value = (TTAG) fitalic;
			}

			if ((*mod->x11_fciface.FcPatternGetInteger)
				(pattern, FC_WEIGHT, 0, &fweight) == FcResultMatch)
			{
				if (fweight == FC_WEIGHT_BOLD) fbold = TTRUE;
				fqnode->tags[i].tti_Tag = TVisual_FontItalic;
				fqnode->tags[i++].tti_Value = (TTAG) fbold;
			}

			if ((*mod->x11_fciface.FcPatternGetBool)
				(pattern, FC_SCALABLE, 0, &fscale) == FcResultMatch)
			{
				fqnode->tags[i].tti_Tag = TVisual_FontScaleable;
				fqnode->tags[i++].tti_Value = (TTAG) fscale;
			}

			fqnode->tags[i].tti_Tag = TTAG_DONE;

		} /* endif fqnode->tags[0].tti_Value */
		else
		{
			TFree(fqnode);
			fqnode = TNULL;
		}

	} /* endif fqnode */
	else
		TDBPRINTF(TDB_FAIL,("out of memory :(\n"));

	return fqnode;
}
#endif

/*****************************************************************************/
/* allocate a fontquerynode and fill in properties derived from
** a x11 fontname
*/
static struct FontQueryNode *
fnt_getfqnode_xlib(X11DISPLAY *mod, TSTRPTR fontname)
{
	TAPTR TExecBase = TGetExecBase(mod);
	struct FontQueryNode *fqnode = TNULL;

	/* allocate fquery node */
	fqnode = TAlloc0(mod->x11_MemMgr, sizeof(struct FontQueryNode));
	if (fqnode)
	{
		/* fquerynode ready - fill in attributes */
		fqnode->tags[0].tti_Tag = TVisual_FontName;
		fnt_getattr(mod, &fqnode->tags[0], fontname);

		if (fqnode->tags[0].tti_Value)
		{
			fqnode->tags[1].tti_Tag = TVisual_FontPxSize;
			fnt_getattr(mod, &fqnode->tags[1], fontname);

			fqnode->tags[2].tti_Tag = TVisual_FontItalic;
			fnt_getattr(mod, &fqnode->tags[2], fontname);

			fqnode->tags[3].tti_Tag = TVisual_FontBold;
			fnt_getattr(mod, &fqnode->tags[3], fontname);

			/* always false */
			fqnode->tags[4].tti_Tag = TVisual_FontScaleable;
			fqnode->tags[4].tti_Value = (TTAG) TFALSE;

			fqnode->tags[5].tti_Tag = TTAG_DONE;
		}
		else
		{
			TFree(fqnode);
			fqnode = TNULL;
		}
	}
	else
		TDBPRINTF(TDB_FAIL,("out of memory :(\n"));

	return fqnode;
}

/*****************************************************************************/
/* check if a font with similar properties is already contained
** in our resultlist
*/
static TBOOL
fnt_checkfqnode(struct TList *rlist, struct FontQueryNode *fqnode)
{
	TUINT8 flags;
	TBOOL match = TFALSE;
	struct TNode *node, *next;

	TSTRPTR newfname = (TSTRPTR)fqnode->tags[0].tti_Value;
	TINT newpxsize = (TINT)fqnode->tags[1].tti_Value;
	TBOOL newslant = (TBOOL)fqnode->tags[2].tti_Value;
	TBOOL newweight = (TBOOL)fqnode->tags[3].tti_Value;
	TSIZE flen = strlen(newfname);

	for (node = rlist->tlh_Head; (next = node->tln_Succ); node = next)
	{
		struct FontQueryNode *fqn = (struct FontQueryNode *)node;
		flags = 0;

		if (strlen((TSTRPTR) fqn->tags[0].tti_Value) == flen)
		{
			if (strncmp((TSTRPTR)fqn->tags[0].tti_Value, newfname, flen) == 0)
				flags = FNT_MATCH_NAME;
		}

		if ((TINT)fqn->tags[1].tti_Value == newpxsize)
			flags |= FNT_MATCH_SIZE;

		if ((TBOOL)fqn->tags[2].tti_Value == newslant)
			flags |= FNT_MATCH_SLANT;

		if ((TBOOL)fqn->tags[3].tti_Value == newweight)
			flags |= FNT_MATCH_WEIGHT;

		if (flags == FNT_MATCH_ALL)
		{
			/* fqnode is not unique */
			match = TTRUE;
			break;
		}
	}

	return match;
}

/*****************************************************************************/
/* dump properties of a fontquerynode
*/
static void
fnt_dumpnode(struct FontQueryNode *fqn)
{
	TDBPRINTF(10, ("-----------------------------------------------\n"));
	TDBPRINTF(10, ("dumping fontquerynode @ %p\n", fqn));
	TDBPRINTF(10, (" * FontName: %s\n", (TSTRPTR)fqn->tags[0].tti_Value));
	TDBPRINTF(10, (" * PxSize:   %d\n", (TINT)fqn->tags[1].tti_Value));
	TDBPRINTF(10, (" * Italic:   %s\n", (TBOOL)fqn->tags[2].tti_Value ? "on" : "off"));
	TDBPRINTF(10, (" * Bold:     %s\n", (TBOOL)fqn->tags[3].tti_Value ? "on" : "off"));
	TDBPRINTF(10, ("-----------------------------------------------\n"));
}

/*****************************************************************************/
/* dump all fontquerynodes of a (result-)list
*/
static void
fnt_dumplist(struct TList *rlist)
{
	struct TNode *node, *next;
	node = rlist->tlh_Head;
	for (; (next = node->tln_Succ); node = next)
	{
		struct FontQueryNode *fqn = (struct FontQueryNode *)node;
		fnt_dumpnode(fqn);
	}
}

/*****************************************************************************/
/* parses a single fontname or a comma separated list of fontnames
** and returns a list of fontnames, spaces are NOT filtered, so
** "helvetica, fixed" will result in "helvetica" and " fixed"
*/
static void
fnt_getfnnodes(X11DISPLAY *mod, struct TList *fnlist, TSTRPTR fname)
{
	TINT i, p = 0;
	TBOOL lastrun = TFALSE;
	TINT fnlen = strlen(fname);
	TAPTR TExecBase = TGetExecBase(mod);

	for (i = 0; i < fnlen; i++)
	{
		if (i == fnlen-1) lastrun = TTRUE;

		if (fname[i] == ',' || lastrun)
		{
			TINT len = (i > p) ? (lastrun ? (i-p+1) : (i-p)) : fnlen+1;
			TSTRPTR ts = TAlloc0(mod->x11_MemMgr, len+1);

			if (ts)
			{
				struct fnt_node *fnn;

				TCopyMem(fname+p, ts, len);

				fnn = TAlloc0(mod->x11_MemMgr, sizeof(struct fnt_node));
				if (fnn)
				{
					/* add fnnode to fnlist */
					fnn->fname = ts;
					TAddTail(fnlist, &fnn->node);
				}
				else
				{
					TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
					break;
				}
			}
			else
			{
				TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
				break;
			}

			p = i+1;
		}
	}
}

/*****************************************************************************/
/* translate user attribute bold to a corresponding xlib string
*/
static TSTRPTR
fnt_getfbold(TBOOL fbold)
{
	switch (fbold)
	{
	 	case FNTQUERY_UNDEFINED:
	 		return FNT_WILDCARD;
	 	case TFALSE:
			return FNT_WGHT_MEDIUM;
		case TTRUE:
			return	FNT_WGHT_BOLD;
	}

	return TNULL;
}

/*****************************************************************************/
/* translate user attribute italic to a corresponding xlib string
*/
static TSTRPTR
fnt_getfitalic(TBOOL fitalic)
{
	switch (fitalic)
	{
	 	case FNTQUERY_UNDEFINED:
	 		return FNT_WILDCARD;
	 	case TFALSE:
			return FNT_SLANT_R;
		case TTRUE:
			return	FNT_SLANT_I;
	}

	return TNULL;
}

/*****************************************************************************/
/* search pattern for properties specified by flag and compare them with
** fname and fattr, if properties match, the corresponing bit is set in
** the flagfield the function returns
*/
#if defined(ENABLE_XFT)
static TUINT
fnt_matchfont_xft(X11DISPLAY *mod, FcPattern *pattern, TSTRPTR fname,
	struct fnt_attr *fattr, TUINT flag)
{
	TUINT match = 0;
	TAPTR TExecBase = TGetExecBase(mod);

	if (flag & FNT_MATCH_NAME)
	{
		TINT i, len;
		FcChar8 *fcfname = NULL;
		TSTRPTR tempname = TNULL;

		(*mod->x11_fciface.FcPatternGetString)(pattern, FC_FAMILY, 0, &fcfname);

		/* convert fontnames to lower case */
		len = strlen(fname);
		for (i = 0; i < len; i++)
			fname[i] = tolower(fname[i]);

		len = strlen((TSTRPTR) fcfname);
		tempname = TAlloc0(mod->x11_MemMgr, len+1);
		if (!tempname)
		{
			TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
			return -1;
		}

		for (i = 0; i < len; i++)
			tempname[i] = tolower(fcfname[i]);

		/* compare converted fontnames */
		if (strncmp(fname, tempname, len) == 0)
			match = FNT_MATCH_NAME;

		TFree(tempname);
	}

	if (flag & FNT_MATCH_SIZE)
	{
		double fcsize;
		(*mod->x11_fciface.FcPatternGetDouble)(pattern, FC_PIXEL_SIZE, 0, &fcsize);
		if (fattr->fpxsize == fcsize)
			match |= FNT_MATCH_SIZE;
	}

	if (flag & FNT_MATCH_SLANT)
	{
		int fcslant;
		(*mod->x11_fciface.FcPatternGetInteger)(pattern, FC_SLANT, 0, &fcslant);
		if ((fcslant == FC_SLANT_ITALIC && fattr->fitalic == TTRUE) ||
			(fcslant == FC_SLANT_ROMAN  && fattr->fitalic == TFALSE))
			match |= FNT_MATCH_SLANT;
	}

	if (flag & FNT_MATCH_WEIGHT)
	{
		int fcweight;
		(*mod->x11_fciface.FcPatternGetInteger)(pattern, FC_WEIGHT, 0, &fcweight);
		if ((fcweight == FC_WEIGHT_BOLD && fattr->fbold == TTRUE) ||
			(fcweight == FC_WEIGHT_MEDIUM && fattr->fbold == TFALSE))
			match |= FNT_MATCH_WEIGHT;
	}

	if (flag & FNT_MATCH_SCALE)
	{
		FcBool fcscale;
		(*mod->x11_fciface.FcPatternGetBool)(pattern, FC_SCALABLE, 0, &fcscale);
		if ((fcscale == FcTrue && fattr->fscale == TTRUE) ||
			(fcscale == FcFalse && fattr->fscale == TFALSE))
			match |= FNT_MATCH_SCALE;
	}

	return match;
}
#endif

/*****************************************************************************/
/* CALL:
**	x11_hostopenfont(visualbase, tags)
**
** USE:
**  to match and open exactly one font, according to its properties
**
** INPUT:
**	tag name				| description
**	------------------------+---------------------------
**	 TVisual_FontName		| font name
**   TVisual_FontPxSize		| font size in pixel
**	 TVisual_FontItalic		| enable slant italic
**	 TVisual_FontBold		| enable bold weight
**	 TVisual_FontScaleable	| use truetype font
**
**	tag name				| default¹	| wildcard
**	------------------------+-----------+---------------
**	 TVisual_FontName		| "fixed"	| "*"
**   TVisual_FontPxSize		| 14		|  /
**	 TVisual_FontItalic		| false		|  /
**	 TVisual_FontBold		| false		|  /
**	 TVisual_FontScaleable	| false		|  /
**
** ¹ the defaults are used when the tag is missing
**
** RETURN:
** - a pointer to a font ready to be used or TNULL
**
** EXAMPLES:
** - to open the default font of your platform leave all tags empty
** - to open the default font in say 16px, set TVisual_FontPxSize to
**   16 and leave all other tags empty
**
** NOTES:
** - this function won't activate the font, use setfont() to make the
**   font the current active font
** - the function will open the first matching font, all properties
**	 which don't have a default value will be totally random (for
**	 example the x/y-resolution of the font)
** - the encoding of the font is set to "iso8859-1", it can be changed
**   in visual_font.h
*/

LOCAL TAPTR
x11_hostopenfont(X11DISPLAY *mod, TTAGITEM *tags)
{
	struct fnt_attr fattr;
	struct FontNode *fn;
	TAPTR font = TNULL;
	TAPTR TExecBase = TGetExecBase(mod);

	/* fetch user specified attributes */
	fattr.fname = (TSTRPTR) TGetTag(tags, TVisual_FontName, (TTAG) FNT_DEFNAME);
	fattr.fpxsize = (TINT) TGetTag(tags, TVisual_FontPxSize, (TTAG) FNT_DEFPXSIZE);
	fattr.fitalic = (TBOOL) TGetTag(tags, TVisual_FontItalic, (TTAG) TFALSE);
	fattr.fbold = (TBOOL) TGetTag(tags, TVisual_FontBold, (TTAG) TFALSE);
	fattr.fscale = (TBOOL) TGetTag(tags, TVisual_FontScaleable, (TTAG) TFALSE);

	if (fattr.fname)
	{
		fn = TAlloc0(mod->x11_MemMgr, sizeof(struct FontNode));
		if (fn)
		{
			fn->handle.thn_Owner = mod;

			if (hostopenfont(mod, fn, &fattr))
			{
				/* load succeeded, save font attributes */
				fn->pxsize = fattr.fpxsize;
				if (fattr.fitalic)
					fn->attr = FNT_ITALIC;
				if (fattr.fbold)
					fn->attr |= FNT_BOLD;

				/* append to the list of open fonts */
				TDBPRINTF(TDB_INFO,("O '%s' %dpx\n", fattr.fname, fattr.fpxsize));
				TAddTail(&mod->x11_fm.openfonts, &fn->handle.thn_Node);
				font = (TAPTR)fn;
			}
			else
			{
				/* load failed, free fontnode */
				TDBPRINTF(TDB_TRACE,("X unable to load '%s'\n", fattr.fname));
				TFree(fn);
			}
		}
		else
			TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
	}
	else
		TDBPRINTF(TDB_ERROR,("X invalid fontname '%s' specified\n", fattr.fname));

	return font;
}

static TBOOL
hostopenfont(X11DISPLAY *mod, struct FontNode *fn, struct fnt_attr *fattr)
{
	TBOOL succ = TFALSE;
	TAPTR TExecBase = TGetExecBase(mod);

	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
	{
		/* load xft font */
		fn->xftfont = (*mod->x11_xftiface.XftFontOpen)(
			mod->x11_Display,
			mod->x11_Screen,
			XFT_FAMILY, XftTypeString, fattr->fname,
			XFT_PIXEL_SIZE, XftTypeDouble, (double) fattr->fpxsize,
			XFT_SLANT, XftTypeInteger,
				(fattr->fitalic ? XFT_SLANT_ITALIC : XFT_SLANT_ROMAN),
			XFT_WEIGHT, XftTypeInteger,
				(fattr->fbold ? XFT_WEIGHT_BOLD : XFT_WEIGHT_MEDIUM),
			XFT_SCALABLE, XftTypeBool, fattr->fscale,
			XFT_ENCODING, XftTypeString, FNT_DEFREGENC,
			XFT_ANTIALIAS, XftTypeBool, TTRUE, NULL);

		if (fn->xftfont)
			succ = TTRUE;
	}
	else
	#endif
	{
		/* load xlib font */
		TSTRPTR fquery = TAlloc0(mod->x11_MemMgr,
			FNT_LENGTH + strlen(fattr->fname));

		if (fquery)
		{
			sprintf(fquery, "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-%s",
				fattr->fname,
				fattr->fbold ? FNT_WGHT_BOLD : FNT_WGHT_MEDIUM,
				fattr->fitalic ? FNT_SLANT_I : FNT_SLANT_R,
				fattr->fpxsize,
				FNT_DEFREGENC
			);

			TDBPRINTF(TDB_TRACE,("? %s\n", fquery));

			fn->font = XLoadQueryFont(mod->x11_Display, fquery);

			if (fn->font)
				succ = TTRUE;

			TFree(fquery);
		}
		else
			TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
	}

	return succ;
}

/*****************************************************************************/
/* CALL:
**  x11_hostqueryfonts(visualbase, tags)
**
** USE:
**  to match one or more fonts, according to their properties
**
** INPUT:
**	tag name				| description
**	------------------------+---------------------------
**	 TVisual_FontName		| font name
**   TVisual_FontPxSize		| font size in pixel
**	 TVisual_FontItalic		| enable slant italic
**	 TVisual_FontBold		| enable bold weight
**	 TVisual_FontScaleable	| prefer truetype font
**	 TVisual_FontNumResults	| how many fonts to return
**
**	tag name				| default¹
**	------------------------+----------------------------
**	 TVisual_FontName		| FNTQUERY_UNDEFINED
**   TVisual_FontPxSize		| FNTQUERY_UNDEFINED
**	 TVisual_FontItalic		| FNTQUERY_UNDEFINED
**	 TVisual_FontBold		| FNTQUERY_UNDEFINED
**	 TVisual_FontScaleable	| FNTQUERY_UNDEFINED
**	 TVisual_FontNumResults	| INT_MAX
**
** ¹ the defaults are used when the tag is missing
**
** RETURN:
** - a pointer to a FontQueryHandle, which is basically a list of
**   taglists, referring to the fonts matched
** - use x11_hostgetnextfont() to traverse the list
** - use TDestroy to free all memory associated with a FontQueryHandle
**
** EXAMPLES:
** - to match all available fonts, use an empty taglist
** - to match more than one specific font, use a coma separated list
**   for TVisual_FontName, e.g. "helvetica,utopia,fixed", note that
**   spaces are not filtered
**
** NOTES:
** - this function won't open any fonts, to do so use x11_hostopenfont()
** - the fonts matched by the function are filtered, for example fonts
**   with different x/y-resolution won't show up in the results,
**   therefore it's totally random which x/y-resolution the font will
**   exhibit, when you open it
** - there is also a default value for the encoding, it's currently set
**	 to "iso8859-1", it can be changed in visual_font.h
*/

LOCAL TAPTR
x11_hostqueryfonts(X11DISPLAY *mod, TTAGITEM *tags)
{
	TSTRPTR fname = TNULL;
	struct fnt_attr fattr;
	struct TNode *node, *next;
	struct FontQueryHandle *fqh = TNULL;
	TAPTR TExecBase = TGetExecBase(mod);

	/* init fontname list */
	TInitList(&fattr.fnlist);

	/* fetch and parse fname */
	fname = (TSTRPTR) TGetTag(tags, TVisual_FontName, (TTAG) FNT_WILDCARD);
	if (fname)	fnt_getfnnodes(mod, &fattr.fnlist, fname);

	/* fetch user specified attributes */
	fattr.fpxsize = (TINT) TGetTag(tags, TVisual_FontPxSize, (TTAG) FNTQUERY_UNDEFINED);
	fattr.fitalic = (TBOOL) TGetTag(tags, TVisual_FontItalic, (TTAG) FNTQUERY_UNDEFINED);
	fattr.fbold = (TBOOL) TGetTag(tags, TVisual_FontBold, (TTAG) FNTQUERY_UNDEFINED);
	fattr.fscale = (TBOOL) TGetTag(tags, TVisual_FontScaleable, (TTAG) FNTQUERY_UNDEFINED);
	fattr.fnum = (TINT) TGetTag(tags, TVisual_FontNumResults, (TTAG) INT_MAX);

	/* init result list */
	fqh = TAlloc0(mod->x11_MemMgr, sizeof(struct FontQueryHandle));
	if (fqh)
	{
		fqh->handle.thn_Owner = mod;
		/* connect destructor */
		TInitHook(&fqh->handle.thn_Hook, fqhdestroy, fqh);
		TInitList(&fqh->reslist);
		/* init list iterator */
		fqh->nptr = &fqh->reslist.tlh_Head;

		#if defined(ENABLE_XFT)
		if (mod->x11_use_xft)
			hostqueryfonts_xft(mod, fqh, &fattr);
		else
		#endif
			hostqueryfonts_xlib(mod, fqh, &fattr);

		TDB(TDB_INFO,(fnt_dumplist(&fqh->reslist)));
	}
	else
		TDBPRINTF(TDB_FAIL, ("out of memory :(\n"));

	/* free memory of fnt_nodes */
	for (node = fattr.fnlist.tlh_Head; (next = node->tln_Succ); node = next)
	{
		struct fnt_node *fnn = (struct fnt_node *)node;
		TFree(fnn->fname);
		TRemove(&fnn->node);
		TFree(fnn);
	}

	return fqh;
}

#if defined(ENABLE_XFT)
static void
hostqueryfonts_xft(X11DISPLAY *mod, struct FontQueryHandle *fqh, struct fnt_attr *fattr)
{
	FcPattern *pattern = TNULL;
	struct TNode *node, *next;
	TAPTR TExecBase = TGetExecBase(mod);
	TUINT matchflg = 0;
	TINT fcount = 0;

	for (node = fattr->fnlist.tlh_Head; (next = node->tln_Succ); node = next)
	{
		FcResult result;
		FcFontSet *fontset = TNULL;
		struct fnt_node *fnn = (struct fnt_node *)node;

		/* create a pattern to match fontname */
		pattern = (*mod->x11_fciface.FcPatternBuild)(0,
			FC_FAMILY, FcTypeString, fnn->fname,
			XFT_ENCODING, FcTypeString, FNT_DEFREGENC,
			FC_ANTIALIAS, FcTypeBool, FcTrue,
			NULL);

		if (!pattern)
		{
			TDBPRINTF(TDB_ERROR,("X unable to create pattern for '%s'\n", fnn->fname));
			continue;
		}

		/* add attributes to pattern and build matchflag */

		if (strncmp(fnn->fname, FNT_WILDCARD, 1) != 0)
			matchflg = FNT_MATCH_NAME;

		/* font pxsize attribute is ignored when the scaleable
			attribute of the font is set, because some scaleable
			fonts don't seem to support FC_PIXEL_SIZE attribute  */
		if (fattr->fpxsize != FNTQUERY_UNDEFINED && !fattr->fscale)
		{
			(*mod->x11_fciface.FcPatternAddDouble)
				(pattern, FC_PIXEL_SIZE, (double) fattr->fpxsize);
			matchflg |= FNT_MATCH_SIZE;
		}

		if (fattr->fitalic != FNTQUERY_UNDEFINED)
		{
			(*mod->x11_fciface.FcPatternAddInteger) (pattern, FC_SLANT,
				(fattr->fitalic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN));
			matchflg |= FNT_MATCH_SLANT;
		}

		if (fattr->fbold != FNTQUERY_UNDEFINED)
		{
			(*mod->x11_fciface.FcPatternAddInteger)(pattern, FC_WEIGHT,
				(fattr->fbold ? FC_WEIGHT_BOLD : FC_WEIGHT_MEDIUM));
			matchflg |= FNT_MATCH_WEIGHT;
		}

		if (fattr->fscale != FNTQUERY_UNDEFINED)
		{
			(*mod->x11_fciface.FcPatternAddBool)(pattern, FC_SCALABLE,
				fattr->fscale);
			matchflg |= FNT_MATCH_SCALE;
		}

		/* don't call FcConfigSubstitute because matching
			should be as exact as possible */
		TDB(TDB_TRACE, ((*mod->x11_fciface.FcPatternPrint)(pattern)));
		(*mod->x11_fciface.FcDefaultSubstitute)(pattern);
		TDB(TDB_TRACE, ((*mod->x11_fciface.FcPatternPrint)(pattern)));

		fontset = (*mod->x11_fciface.FcFontSort)(NULL, pattern, FcFalse, TNULL, &result);
		if (fontset)
		{
			TINT i;

			if (fontset->nfont == 0)
				TDBPRINTF(TDB_WARN,("X query returned no results\n"));

			for (i = 0; i < fontset->nfont; i++)
			{
				if (fnt_matchfont_xft(mod, fontset->fonts[i], fnn->fname,
					fattr, matchflg) == matchflg)
				{
					struct FontQueryNode *fqnode;

					/* create fqnode and fill in attributes */
					fqnode = fnt_getfqnode_xft(mod, fontset->fonts[i], fattr->fpxsize);
					if(!fqnode)	break;

					/* compare fqnode with nodes in result list */
					if (fnt_checkfqnode(&fqh->reslist, fqnode) == 0)
					{
						if (fcount < fattr->fnum)
						{
							/* fqnode is unique, add to result list */
							TAddTail(&fqh->reslist, &fqnode->node);
							fcount++;
						}
						else
						{
							/* max count of desired results reached */
							TFree((TSTRPTR)fqnode->tags[0].tti_Value);
							TFree(fqnode);
							break;
						}
					}
					else
					{
						/* fqnode is not unique, destroy it */
						TDBPRINTF(TDB_ERROR,("X node is not unique\n"));
						TFree((TSTRPTR)fqnode->tags[0].tti_Value);
						TFree(fqnode);
					}
				}
			}

			(*mod->x11_fciface.FcFontSetDestroy)(fontset);

		} /* endif fontset */
		else
			TDBPRINTF(TDB_ERROR,("X query failed\n"));

		(*mod->x11_fciface.FcPatternDestroy)(pattern);

		if (fcount == fattr->fnum)
			break;

	} /* end of fnlist iteration */
}
#endif

static void
hostqueryfonts_xlib(X11DISPLAY *mod, struct FontQueryHandle *fqh, struct fnt_attr *fattr)
{
	TSTRPTR fquery = TNULL;
	TCHR **fontlist = TNULL;
	TAPTR TExecBase = TGetExecBase(mod);
	struct TNode *node, *next;

	for (node = fattr->fnlist.tlh_Head; (next = node->tln_Succ); node = next)
	{
		TINT i, numfonts = 0;
		struct fnt_node *fnn = (struct fnt_node *)node;

		if (fnn->fname)
		{
			fquery = TAlloc0(mod->x11_MemMgr, FNT_LENGTH + strlen(fnn->fname));
			if (!fquery)
			{
				TDBPRINTF(TDB_FAIL,("out of memory :(\n"));
				break;
			}
		}
		else
		{
			TDBPRINTF(TDB_ERROR, ("X invalid fontname '%s' specified\n", fnn->fname));
			continue;
		}

		/* build fontquery name */
		if (fattr->fpxsize == FNTQUERY_UNDEFINED)
		{
			/* match pixel size using wildcard */
			sprintf(fquery, "-*-%s-%s-%s-*-*-*-*-*-*-*-*-%s",
				fnn->fname,
				fnt_getfbold(fattr->fbold),
				fnt_getfitalic(fattr->fitalic),
				FNT_DEFREGENC
			);
		}
		else
		{
			/* match exact pixel size */
			sprintf(fquery, "-*-%s-%s-%s-*-*-%d-*-*-*-*-*-%s",
				fnn->fname,
				fnt_getfbold(fattr->fbold),
				fnt_getfitalic(fattr->fitalic),
				fattr->fpxsize,
				FNT_DEFREGENC
			);
		}

		TDBPRINTF(TDB_INFO,("? %s\n", fquery));

		fontlist = XListFonts(mod->x11_Display, fquery, fattr->fnum, &numfonts);

		if (numfonts  == 0)
			TDBPRINTF(TDB_WARN,("X query returned no results\n"));

		for (i = 0; i < numfonts; i++)
		{
			struct FontQueryNode *fqnode;
			TDBPRINTF(TDB_INFO,("! %s\n", fontlist[i]));

			/* create fqnode and fill in attributes */
			fqnode = fnt_getfqnode_xlib(mod, fontlist[i]);
			if(!fqnode)	break;

			/* compare fqnode with nodes in result list */
			if (fnt_checkfqnode(&fqh->reslist, fqnode) == 0)
			{
				/* fqnode is unique, add to result list */
				TAddTail(&fqh->reslist, &fqnode->node);
			}
			else
			{
				/* fqnode is not unique, destroy it */
				TDBPRINTF(TDB_ERROR,("X node is not unique\n"));
				TFree((TSTRPTR)fqnode->tags[0].tti_Value);
				TFree(fqnode);
			}

		} /* end of fontlist iteration */

		TFree(fquery);

		if (fontlist)
			XFreeFontNames(fontlist);

	} /* end of fnlist iteration */
}

/*****************************************************************************/
/* CALL:
**  x11_hostsetfont(visual, fontpointer)
**
** USE:
**  makes the font referred to by fontpointer the current active font
**  for the visual
**
** INPUT:
**  a pointer to a font returned by x11_hostopenfont()
**
** NOTES:
** - if a font is active it can't be closed
*/

LOCAL void
x11_hostsetfont(X11DISPLAY *mod, X11WINDOW *v, TAPTR font)
{
	if (font)
	{
		#if defined(ENABLE_XFT)
		if (!mod->x11_use_xft)
		#endif
		{
			XGCValues gcv;
			struct FontNode *fn = (struct FontNode *) font;
			gcv.font = fn->font->fid;
			XChangeGC(mod->x11_Display, v->gc, GCFont, &gcv);
		}
		v->curfont = font;
	}
	else
		TDBPRINTF(TDB_ERROR,("invalid font specified\n"));
}

/*****************************************************************************/
/* CALL:
**  x11_hostgetnextfont(visualbase, fontqueryhandle)
**
** USE:
**  iterates a list of taglists, returning the next taglist
**  pointer or TNULL
**
** INPUT:
**  a fontqueryhandle obtained by calling x11_hostqueryfonts()
**
** RETURN:
**  a pointer to a taglist, representing a font or TNULL
**
** NOTES:
**  - the taglist returned by this function can be directly fed to
**	  x11_hostopenfont()
**  - if the end of the list is reached, TNULL is returned and the
**    iterator is reset to the head of the list
*/

LOCAL TTAGITEM *
x11_hostgetnextfont(X11DISPLAY *mod, TAPTR fqhandle)
{
	struct FontQueryHandle *fqh = fqhandle;
	struct TNode *next = *fqh->nptr;

	if (next->tln_Succ == TNULL)
	{
		fqh->nptr = &fqh->reslist.tlh_Head;
		return TNULL;
	}

	fqh->nptr = (struct TNode **)next;
	return ((struct FontQueryNode *)next)->tags;
}

/*****************************************************************************/
/* CALL:
**  x11_hostclosefont(visualbase, fontpointer)
**
** USE:
**  attempts to free all memory associated with the font referred to
**  by fontpointer
**
** INPUT:
**  a pointer to a font returned by x11_hostopenfont()
**
** NOTES:
**  - the default font is only freed, if there are no more references
**	  to it left
**  - the attempt to free any other font which is currently in use,
**	  will be ignored
*/

LOCAL void
x11_hostclosefont(X11DISPLAY *mod, TAPTR font)
{
	struct FontNode *fn = (struct FontNode *) font;
	TAPTR TExecBase = TGetExecBase(mod);

	if (font == mod->x11_fm.deffont)
	{
		if (mod->x11_fm.defref)
		{
			/* prevent freeing of default font if it's */
			/* still referenced */
			mod->x11_fm.defref--;
			return;
		}
	}
	else
	{
		struct TNode *node, *next;
		node = mod->x11_vlist.tlh_Head;
		for (; (next = node->tln_Succ); node = next)
		{
			/* check if the font is currently used by another visual */
			X11WINDOW *v = (X11WINDOW *) node;

			if (font == v->curfont)
			{
				TDBPRINTF(TDB_ERROR,("attempt to close font which is currently in use\n"));
				return; /* do nothing */
			}
		}
	}

	/* free xfont */
	if (fn->font)
	{
		XFreeFont(mod->x11_Display, fn->font);
		fn->font = TNULL;
	}

	/* free xftfont */
	#if defined(ENABLE_XFT)
	if (fn->xftfont)
	{
		(*mod->x11_xftiface.XftFontClose)(mod->x11_Display, fn->xftfont);
		fn->xftfont = TNULL;
	}
	#endif

	/* remove font from openfonts list */
	TRemove(&fn->handle.thn_Node);

	/* free fontnode itself */
	TFree(fn);
}

/*****************************************************************************/
/* CALL:
**  x11_hosttextsize(visualbase, fontpointer, textstring, len)
**
** USE:
**  to obtain the width of a given string when the font referred to
**  by fontpointer is used to render the text
**
** INPUT:
**  - a pointer to a font returned by x11_hostopenfont()
**  - the textstring to measure
**
** RETURN:
**  - the width of the textstring in pixel
*/

LOCAL TINT
x11_hosttextsize(X11DISPLAY *mod, TAPTR font, TSTRPTR text, TINT len)
{
	TINT width = 0;
	struct FontNode *fn = (struct FontNode *) font;

	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
	{
		XGlyphInfo extents;
		(*mod->x11_xftiface.XftTextExtentsUtf8)(mod->x11_Display, fn->xftfont,
			(FcChar8 *)text, len, &extents);
		width = extents.xOff; /* why not extents.width?!? */
	}
	else
	#endif
	{
		TSTRPTR latin = x11_utf8tolatin(mod, text, len, &len);
		if (latin)
			width = XTextWidth(fn->font, latin, len);
	}
	return width;
}

/*****************************************************************************/
/* CALL:
**	x11_getfattrs(visualbase, fontpointer, taglist);
**
** USE:
**  fills the taglist with the requested properties of the
**  font referred to by fontpointer
**
** INPUT:
**  - a pointer to a font returned by x11_hostopenfont()
**	- the following tags can be used
**
**  tag name				| description
**	------------------------+---------------------------
**  TVisual_FontPxSize		| font size in pixel
**	TVisual_FontItalic		| true if slant == italic
**	TVisual_FontBold		| true if weight == bold
** 	TVisual_FontAscent		| the font ascent in pixel
**	TVisual_FontDescent		| the font descent in pixel
**  TVisual_FontHeight		| height in pixel
**	TVisual_FontUlPosition	| position of an underline
**	TVisual_FontUlThickness	| thickness of the underline
**
** RETURN:
**  - the number of processed properties
**
** NOTES:
**  - not all fonts provide information about their underline
**	  position or their underline thickness, therefore
**    TVisual_FontUlPosition defaults to fontdescent / 2 and
**	  TVisual_FontUlThickness defaults to 1
*/

LOCAL THOOKENTRY TTAG
x11_hostgetfattrfunc(struct THook *hook, TAPTR obj, TTAG msg)
{
	struct attrdata *data = hook->thk_Data;
	X11DISPLAY *mod = data->mod;
	TTAGITEM *item = obj;
	struct FontNode *fn = (struct FontNode *) data->font;

	switch (item->tti_Tag)
	{
		default:
			return TTRUE;

		case TVisual_FontPxSize:
			*((TTAG *) item->tti_Value) = fn->pxsize;
			break;

		case TVisual_FontItalic:
			*((TTAG *) item->tti_Value) = (fn->attr & FNT_ITALIC) ?
				TTRUE : TFALSE;
			break;

		case TVisual_FontBold:
			*((TTAG *) item->tti_Value) = (fn->attr & FNT_BOLD) ?
				TTRUE : TFALSE;
			break;

		case TVisual_FontAscent:
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
				*((TTAG *) item->tti_Value) = fn->xftfont->ascent;
			else
			#endif
				*((TTAG *) item->tti_Value) = fn->font->ascent;
			break;

		case TVisual_FontDescent:
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
				*((TTAG *) item->tti_Value) = fn->xftfont->descent;
			else
			#endif
				*((TTAG *) item->tti_Value) = fn->font->descent;
			break;

		case TVisual_FontHeight:
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
				*((TTAG *) item->tti_Value) = fn->xftfont->ascent + fn->xftfont->descent;
			else
			#endif
				*((TTAG *) item->tti_Value) = fn->font->ascent + fn->font->descent;
			break;

		case TVisual_FontUlPosition:
		{
			unsigned long ulp;
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
			{
				FT_Face face = (*mod->x11_xftiface.XftLockFace)(fn->xftfont);
				ulp = fn->xftfont->descent / 2;
				if (face)
				{
					if (face->units_per_EM != 0)
					{
						ulp = -face->underline_position *
							fn->xftfont->height / face->units_per_EM;
					}
					(*mod->x11_xftiface.XftUnlockFace)(fn->xftfont);
				}
			}
			else
			#endif
			{
				ulp = fn->font->descent / 2;
				XGetFontProperty(fn->font, XA_UNDERLINE_POSITION, &ulp);
			}
			*((TTAG *) item->tti_Value) = (TTAG) ulp;
			break;
		}
		case TVisual_FontUlThickness:
		{
			unsigned long ult = 1;
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
			{
				FT_Face face = (*mod->x11_xftiface.XftLockFace)(fn->xftfont);
				if (face)
				{
					if (face->units_per_EM != 0)
					{
						ult = face->underline_thickness *
							fn->xftfont->height / face->units_per_EM;
					}
					(*mod->x11_xftiface.XftUnlockFace)(fn->xftfont);
				}
			}
			else
			#endif
			{
				XGetFontProperty(fn->font, XA_UNDERLINE_THICKNESS, &ult);
			}
			*((TTAG *) item->tti_Value) = (TTAG) TMAX(1, ult);
			break;
		}

		/* ... */
	}
	data->num++;
	return TTRUE;
}
