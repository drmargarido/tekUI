
#include "visual_lua.h"
#if defined(ENABLE_PNG)
#include <png.h>
#endif

/*****************************************************************************/
/*
**	check userdata with classname from registry
*/

static TAPTR checkinstptr(lua_State *L, int n, const char *classname)
{
	TAPTR p = luaL_checkudata(L, n, classname);
	if (p == TNULL) luaL_argerror(L, n, "Closed handle");
	return p;
}

#define getpenptr(L, n) luaL_checkudata(L, n, TEK_LIB_VISUALPEN_CLASSNAME)
#define getpixmapptr(L, n) \
luaL_checkudata(L, n, TEK_LIB_VISUALPIXMAP_CLASSNAME)
#define checkfontptr(L, n) checkinstptr(L, n, TEK_LIB_VISUALFONT_CLASSNAME)

/*****************************************************************************/

static int my_typerror (lua_State *L, int narg, const char *tname) {
  const char *msg = lua_pushfstring(L, "%s expected, got %s",
                                    tname, luaL_typename(L, narg));
  return luaL_argerror(L, narg, msg);
}

/*****************************************************************************/
/*
**	check userdata with metatable from upvalue
*/

static void *checkudataupval(lua_State *L, int n, int upidx, const char *tname)
{
	void *p = lua_touserdata(L, n);
	if (p)
	{
		if (lua_getmetatable(L, n))
		{
			if (lua_rawequal(L, -1, lua_upvalueindex(upidx)))
			{
				lua_pop(L, 1);
				return p;
			}
		}
		my_typerror(L, n, tname);
	}
	return NULL;
}

static void *checkinstupval(lua_State *L, int n, int upidx, const char *tname)
{
	TAPTR p = checkudataupval(L, n, upidx, tname);
	if (p == TNULL)
		my_typerror(L, n, tname);
	return p;
}

#define checkvisptr(L, n) checkinstupval(L, n, 1, "visual")
#define checkpenptr(L, n) checkinstupval(L, n, 2, "pen")
#define checkpixmapptr(L, n) checkinstupval(L, n, 3, "pixmap")

/*****************************************************************************/

#define PAINT_UNDEFINED	0
#define PAINT_PEN		1
#define PAINT_PIXMAP	2
#define PAINT_GRADIENT	3

static int checkpaint(lua_State *L, int uidx, void **udata, TBOOL alsopm)
{
	*udata = lua_touserdata(L, uidx);
	if (*udata)
	{
		if (lua_getmetatable(L, uidx))
		{
			int res = PAINT_UNDEFINED;
			if (lua_rawequal(L, -1, lua_upvalueindex(2)))
				res = PAINT_PEN;
			if (res == 0 && alsopm && lua_rawequal(L, -1, lua_upvalueindex(4)))
				res = PAINT_GRADIENT;
			if (res == 0 && alsopm && lua_rawequal(L, -1, lua_upvalueindex(3)))
				res = PAINT_PIXMAP;
			lua_pop(L, 1);
			if (res != PAINT_UNDEFINED)
				return res;
		}
		/* userdata, but not of requested type */
		my_typerror(L, uidx, "paint");
	}
	/* other type */
	return 0;
}

static int lookuppaint(lua_State *L, int refpen, int uidx,
	void **udata, TBOOL alsopm)
{
	int res = checkpaint(L, uidx, udata, alsopm);
	if (res != PAINT_UNDEFINED)
		return res;
	if (!lua_isnoneornil(L, uidx))
	{
		if (refpen >= 0)
		{
			lua_rawgeti(L, lua_upvalueindex(1), refpen); /* VIS */
			lua_pushvalue(L, uidx);
			lua_gettable(L, -2);
			res = checkpaint(L, -1, udata, alsopm);
			lua_pop(L, 2);
		}
	}
	return res;
}

static void *lookuppen(lua_State *L, int refpen, int uidx)
{
	void *udata;
	lookuppaint(L, refpen, uidx, &udata, TFALSE);
	return udata;
}

static void *checklookuppen(lua_State *L, int refpen, int uidx)
{
	void *udata = lookuppen(L, refpen, uidx);
	if (udata == NULL)
		my_typerror(L, uidx, "pen");
	return udata;
}

static int getbgpaint(lua_State *L, TEKVisual *vis, int uidx,
	void **udata)
{
	int res = lookuppaint(L, vis->vis_refPens, uidx, udata, TTRUE);
	if (res != PAINT_UNDEFINED)
		return res;
	if (vis->vis_BGPen)
	{
		*udata = vis->vis_BGPen;
		return vis->vis_BGPenType;
	}
	return PAINT_UNDEFINED;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_wait(lua_State *L)
{
	TEKVisual *vis;
	struct TExecBase *TExecBase;
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	TExecBase = vis->vis_ExecBase;
	TWait(TGetPortSignal(vis->vis_IMsgPort));
	lua_pop(L, 1);
	return 0;
}

/*****************************************************************************/
/*
**	Sleep specified number of microseconds
*/

LOCAL LUACFUNC TINT
tek_lib_visual_sleep(lua_State *L)
{
	struct TExecBase *TExecBase;
	TEKVisual *vis;
	TTIME dt;
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	TExecBase = vis->vis_ExecBase;
	dt.tdt_Int64 = luaL_checknumber(L, 1) * 1000;
	TWaitTime(&dt, 0);
	lua_pop(L, 1);
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_gettime(lua_State *L)
{
	struct TExecBase *TExecBase;
	TEKVisual *vis;
	TTIME dt;
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	TExecBase = vis->vis_ExecBase;
	TGetSystemTime(&dt);
	lua_pushinteger(L, dt.tdt_Int64 / 1000000);
	lua_pushinteger(L, dt.tdt_Int64 % 1000000);
	return 2;
}

/*****************************************************************************/
/*
**	openfont(name, pxsize, attr)
**	attr: "b" bold, "i" italic, "s" scalable
*/

static TBOOL iTStrChr(TSTRPTR s, TINT c)
{
	TINT d;
	while ((d = *s))
	{
		if (d == c)
			return TTRUE;
		s++;
	}
	return TFALSE;
}


LOCAL LUACFUNC TINT
tek_lib_visual_openfont(lua_State *L)
{
	TTAGITEM ftags[8], *tp = ftags;
	TEKVisual *vis;
	TEKFont *font;
	TSTRPTR name = (TSTRPTR) luaL_optstring(L, 1, "");
	TINT size = luaL_optinteger(L, 2, -1);
	TSTRPTR attr = (TSTRPTR) luaL_optstring(L, 3, "");

	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	lua_pop(L, 1);

	tp->tti_Tag = TVisual_FontBold;
	tp++->tti_Value = (TTAG) !!iTStrChr(attr, 'b');
	tp->tti_Tag = TVisual_FontItalic;
	tp++->tti_Value = (TTAG) !!iTStrChr(attr, 'i');
	tp->tti_Tag = TVisual_FontScaleable;
	tp++->tti_Value = (TTAG) !!iTStrChr(attr, 's');
	
	if (name && name[0] != 0)
	{
		tp->tti_Tag = TVisual_FontName;
		tp++->tti_Value = (TTAG) name;
	}

	if (size > 0)
	{
		tp->tti_Tag = TVisual_FontPxSize;
		tp++->tti_Value = (TTAG) size;
	}

	tp->tti_Tag = TVisual_Display;
	tp++->tti_Value = (TTAG) vis->vis_Display;

	tp->tti_Tag = TTAG_DONE;

	/* reserve userdata for a collectable object: */
	font = lua_newuserdata(L, sizeof(TEKFont));
	/* s: fontdata */
	font->font_Font = TVisualOpenFont(vis->vis_Base, ftags);
	if (font->font_Font)
	{
		font->font_VisBase = vis->vis_Base;

		ftags[0].tti_Tag = TVisual_FontHeight;
		ftags[0].tti_Value = (TTAG) &font->font_Height;
		ftags[1].tti_Tag = TVisual_FontUlPosition;
		ftags[1].tti_Value = (TTAG) &font->font_UlPosition;
		ftags[2].tti_Tag = TVisual_FontUlThickness;
		ftags[2].tti_Value = (TTAG) &font->font_UlThickness;
		ftags[3].tti_Tag = TTAG_DONE;

		if (TVisualGetFontAttrs(vis->vis_Base, font->font_Font, ftags) == 3)
		{
			TDBPRINTF(TDB_TRACE,("Height: %d - Pos: %d - Thick: %d\n",
				font->font_Height, font->font_UlPosition,
				font->font_UlThickness));

			/* attach class metatable to userdata object: */
			luaL_newmetatable(L, TEK_LIB_VISUALFONT_CLASSNAME);
			/* s: fontdata, meta */
			lua_setmetatable(L, -2);
			/* s: fontdata */
			lua_pushinteger(L, font->font_Height);
			/* s: fontdata, height */
			return 2;
		}

		TDestroy(font->font_Font);
	}

	lua_pop(L, 1);
	lua_pushnil(L);
	return 1;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_closefont(lua_State *L)
{
	TEKFont *font = checkfontptr(L, 1);
	if (font->font_Font)
	{
		TVisualCloseFont(font->font_VisBase, font->font_Font);
		font->font_Font = TNULL;
	}
	return 0;
}

/*****************************************************************************/
/*
**	return width, height of the specified font and text
*/

LOCAL LUACFUNC TINT
tek_lib_visual_textsize_font(lua_State *L)
{
	TEKFont *font = checkfontptr(L, 1);
	size_t len;
	TSTRPTR s = (TSTRPTR) luaL_checklstring(L, 2, &len);
	lua_pushinteger(L,
		TVisualTextSize(font->font_VisBase, font->font_Font, s, (TINT) len));
	lua_pushinteger(L, font->font_Height);
	return 2;
}

/*****************************************************************************/
/*
**	set font attributes in passed (or newly created) table
*/

LOCAL LUACFUNC TINT
tek_lib_visual_getfontattrs(lua_State *L)
{
	TEKFont *font = checkfontptr(L, 1);
	if (lua_type(L, 2) == LUA_TTABLE)
		lua_pushvalue(L, 2);
	else
		lua_newtable(L);
	lua_pushinteger(L, font->font_Height);
	lua_setfield(L, -2, "Height");
	lua_pushinteger(L, font->font_Height - font->font_UlPosition);
	lua_setfield(L, -2, "UlPosition");
	lua_pushinteger(L, font->font_UlThickness);
	lua_setfield(L, -2, "UlThickness");
	return 1;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_setinput(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TUINT mask = (TUINT) lua_tointeger(L, 2);
	TVisualSetInput(vis->vis_Visual, 0, mask);
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_clearinput(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TUINT mask = (TUINT) lua_tointeger(L, 2);
	TVisualSetInput(vis->vis_Visual, mask, 0);
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_flush(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	if (vis->vis_FlushReq && (vis->vis_Dirty || lua_toboolean(L, 2)))
	{
		struct TExecBase *TExecBase = vis->vis_ExecBase;
		struct TVRequest *req = vis->vis_FlushReq;
		req->tvr_Req.io_ReplyPort = TGetSyncPort(TNULL);
		TDoIO(&req->tvr_Req);
		vis->vis_Dirty = TFALSE;
	}
	lua_pop(L, 1);
	return 0;
}

/*****************************************************************************/

typedef struct
{
	struct TExecBase *execbase;
	TIMSG *imsg;
	int numfields;
} tek_msg;

LOCAL LUACFUNC TINT tek_msg_reply(lua_State *L)
{
	tek_msg *msg = luaL_checkudata(L, 1, "tek_msg*");
	if (msg->imsg)
	{
		struct TExecBase *TExecBase = msg->execbase;
		TIMSG *imsg = msg->imsg;
		if (imsg->timsg_Type == TITYPE_REQSELECTION)
		{
			if (lua_type(L, 2) == LUA_TTABLE)
			{
				size_t len;
				TUINT8 *s, *selection;
				lua_getfield(L, 2, "UTF8Selection");
				s = (TUINT8 *) lua_tolstring(L, -1, &len);
				selection = TAlloc(TNULL, len);
				if (selection)
				{
					TCopyMem(s, selection, len);
					imsg->timsg_Tags[0].tti_Tag = TIMsgReply_UTF8Selection;
					imsg->timsg_Tags[0].tti_Value = (TTAG) selection;
					imsg->timsg_Tags[1].tti_Tag = TIMsgReply_UTF8SelectionLen;
					imsg->timsg_Tags[1].tti_Value = len;
					imsg->timsg_Tags[2].tti_Tag = TTAG_DONE;
					imsg->timsg_ReplyData = (TTAG) imsg->timsg_Tags;
				}
			}
		}
		TReplyMsg(msg->imsg);
		msg->imsg = NULL;
	}
	return 0;
}

LOCAL LUACFUNC TINT tek_msg_len(lua_State *L)
{
	tek_msg *msg = luaL_checkudata(L, 1, "tek_msg*");
	fprintf(stderr, "tek_msg_len\n");
	lua_pushinteger(L, msg->numfields);
	return 1;
}

LOCAL LUACFUNC TINT tek_msg_index(lua_State *L)
{
	tek_msg *msg = luaL_checkudata(L, 1, "tek_msg*");
	TIMSG *imsg = msg->imsg;
	
	if (lua_type(L, 2) == LUA_TSTRING)
	{
		const char *s = lua_tostring(L, 2);
		if (s && s[0] == 'r') /* "reply" */
			lua_pushcfunction(L, tek_msg_reply);
		else
			lua_pushnil(L);
		return 1;
	}
	
	if (!imsg)
	{
		TDBPRINTF(TDB_WARN,("Message invalid - already replied\n"));
		lua_pushnil(L);
		return 1;
	}
	
	switch (lua_tointeger(L, 2))
	{
		default:
			lua_pushnil(L);
			break;
		case -1:
			if (imsg->timsg_UserData > 0)
			{
				TEKVisual *refvis;
				lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
				/* s: visbase */
				lua_getmetatable(L, -1);
				/* s: visbase, reftab */
				/* If we have a userdata, we regard it as an index into the
				visual metatable, referencing a visual: */
				lua_rawgeti(L, -1, (int) imsg->timsg_UserData);
				/* s: visbase, reftab, visual */
				refvis = lua_touserdata(L, -1);
				/* from there, we retrieve the visual's userdata, which
				is stored as a reference in the same table: */
				lua_rawgeti(L, -2, refvis->vis_refUserData);
				/* s: visbase, reftab, visual, userdata */
				lua_remove(L, -2);
				lua_remove(L, -2);
				lua_remove(L, -2);
			}
			else
			{
				/* otherwise, we retrieve a "raw" user data package: */
				lua_pushlstring(L, (void *) (imsg + 1), imsg->timsg_ExtraSize);
			}
			break;
		case 0:
			lua_pushinteger(L, imsg->timsg_TimeStamp.tdt_Int64 % 1000000);
			break;
		case 1:
			lua_pushinteger(L, imsg->timsg_TimeStamp.tdt_Int64 / 1000000);
			break;
		case 2:
			lua_pushinteger(L, imsg->timsg_Type);
			break;
		case 3:
			lua_pushinteger(L, imsg->timsg_Code);
			break;
		case 4:
			lua_pushinteger(L, imsg->timsg_MouseX);
			break;
		case 5:
			lua_pushinteger(L, imsg->timsg_MouseY);
			break;
		case 6:
			lua_pushinteger(L, imsg->timsg_Qualifier);
			break;
		case 7:
			if (imsg->timsg_Type != TITYPE_REFRESH)
			{
				/* UTF-8 representation of keycode: */
				lua_pushstring(L, (const char *) imsg->timsg_KeyCode);
				break;
			}
			lua_pushinteger(L, imsg->timsg_X);
			break;
		case 8:
			lua_pushinteger(L, imsg->timsg_Y);
			break;
		case 9:
			lua_pushinteger(L, imsg->timsg_X + imsg->timsg_Width - 1);
			break;
		case 10:
			lua_pushinteger(L, imsg->timsg_Y + imsg->timsg_Height - 1);
			break;
	}
	return 1;
}

LOCAL LUACFUNC TINT
tek_lib_visual_getmsg(lua_State *L)
{
	struct TExecBase *TExecBase;
	TEKVisual *vis;
	TIMSG *imsg;
	tek_msg *tmsg;
	
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	/* s: visbase */
	vis = lua_touserdata(L, -1);
	TExecBase = vis->vis_ExecBase;
	imsg = (TIMSG *) TGetMsg(vis->vis_IMsgPort);
	if (imsg == TNULL)
	{
		lua_pop(L, 1);
		return 0;
	}
	tmsg = lua_newuserdata(L, sizeof(tek_msg));
	tmsg->execbase = TExecBase;
	lua_getfield(L, LUA_REGISTRYINDEX, "tek_msg*");
	lua_setmetatable(L, -2);
	/* s: tekmsg */
	tmsg->imsg = imsg;
	tmsg->numfields = 8;
	if (imsg->timsg_Type == TITYPE_REFRESH)
		tmsg->numfields += 4;
	else if (imsg->timsg_Type == TITYPE_KEYUP ||
		imsg->timsg_Type == TITYPE_KEYDOWN)
		tmsg->numfields++;
	return 1;
}

/*****************************************************************************/

#if defined(ENABLE_PNG)

struct pngmemread
{
	png_bytep src;
	size_t len;
	size_t left;
	TUINT width;
	TUINT height;
	TUINT *buf;
	TUINT flags;
};

static void pngreadmem(png_structp png, png_bytep buf, png_size_t nbytes)
{
	struct pngmemread *rd = png_get_io_ptr(png);
	nbytes = TMIN(rd->left, nbytes);
	memcpy(buf, rd->src + rd->len - rd->left, nbytes);
	rd->left -= nbytes;
}

static TBOOL tek_lib_visual_load_png(struct TExecBase *TExecBase, struct pngmemread *rd)
{
	unsigned int x, y;
	png_structp png;
	png_infop pnginfo = NULL;
	png_bytep *row_pointers = NULL;
	
	if (png_sig_cmp(rd->src, 0, 8))
		return TFALSE;
	
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL)
		return TFALSE;
	
	if (setjmp(png_jmpbuf(png)))
	{
		if (rd->height && row_pointers)
		{
			for (y = 0; y < rd->height; y++)
			{
				if (row_pointers[y])
					free(row_pointers[y]);
			}
			free(row_pointers);
		}
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return TFALSE;
	}
	
	pnginfo = png_create_info_struct(png);
	if (!png) png_error(png, "no memory");
	
	png_set_read_fn(png, rd, pngreadmem);
	
	png_read_info(png, pnginfo);
	
	rd->width = png_get_image_width(png, pnginfo);
	rd->height = png_get_image_height(png, pnginfo);
	png_set_interlace_handling(png);
	int numchan = png_get_channels(png, pnginfo);
	int coltype = png_get_color_type(png, pnginfo);
	png_set_packing(png);
	png_read_update_info(png, pnginfo);
	
	png_color *palette = NULL;
	int numpal = 0;
	if (coltype == PNG_COLOR_TYPE_PALETTE)
	{
		if (png_get_PLTE(png, pnginfo, &palette, &numpal) != PNG_INFO_PLTE)
			png_error(png, "could not get palette");
	}

	row_pointers = (png_bytep *) calloc(rd->height, sizeof(png_bytep));
	if (!row_pointers) png_error(png, "no memory");

	png_uint_32 rowbytes = png_get_rowbytes(png, pnginfo);
	for (y = 0; y < rd->height; y++)
	{
		row_pointers[y] = (png_byte *) malloc(rowbytes);
		if (!row_pointers[y]) png_error(png, "no memory");
	}
	
	/*fprintf(stderr, "numchan=%d coltype=%d width=%d rowbytes=%d numpal=%d\n", 
		numchan, coltype, rd->width, rowbytes, numpal);*/
	
	png_read_image(png, row_pointers);
	
	TUINT *buf = TAlloc(TNULL, rd->width * rd->height * sizeof(TUINT));
	if (!buf) png_error(png, "no memory");

	typedef enum { PNG_PALETTE, PNG_GRAY, PNG_GRAYALPHA, PNG_RGB, 
		PNG_RGB_ALPHA } pngtype_t;
	pngtype_t mode;
	if (palette)
		mode = PNG_PALETTE;
	else if (coltype == PNG_COLOR_TYPE_GRAY)
		mode = PNG_GRAY;
	else if (coltype == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		mode = PNG_GRAYALPHA;
		rd->flags |= PXMF_ALPHA;
	}
	else if (numchan == 4)
	{
		mode = PNG_RGB_ALPHA;
		rd->flags |= PXMF_ALPHA;
	}
	else
		mode = PNG_RGB;
	
	rd->buf = buf;
	for (y = 0; y < rd->height; ++y)
	{
		switch (mode)
		{
			case PNG_PALETTE:
				for (x = 0; x < rd->width; ++x)
				{
					TUINT8 idx = row_pointers[y][x];
					TUINT r = palette[idx].red;
					TUINT g = palette[idx].green;
					TUINT b = palette[idx].blue;
					*buf++ = 0xff000000 | (r << 16) | (g << 8) | b;
				}
				break;
			case PNG_GRAY:
				for (x = 0; x < rd->width; ++x)
				{
					TUINT8 c = row_pointers[y][x];
					*buf++ = 0xff000000 | (c << 16) | (c << 8) | c;
				}
				break;
			case PNG_GRAYALPHA:
				for (x = 0; x < rd->width * 2; x += 2)
				{
					TUINT8 c = row_pointers[y][x];
					TUINT8 a = row_pointers[y][x + 1];
					*buf++ = (a << 24) | (c << 16) | (c << 8) | c;
				}
				break;
			case PNG_RGB_ALPHA:
				for (x = 0; x < rd->width * 4; x += 4)
				{
					TUINT r = row_pointers[y][x];
					TUINT g = row_pointers[y][x + 1];
					TUINT b = row_pointers[y][x + 2];
					TUINT a = row_pointers[y][x + 3];
					*buf++ = (a << 24) | (r << 16) | (g << 8) | b;
				}
				break;
			case PNG_RGB:
				for (x = 0; x < rd->width * 3; x += 3)
				{
					TUINT r = row_pointers[y][x];
					TUINT g = row_pointers[y][x + 1];
					TUINT b = row_pointers[y][x + 2];
					*buf++ = (r << 16) | (g << 8) | b;
				}
				break;
		}
	}

	png_destroy_read_struct(&png, &pnginfo, NULL);
	
	for (y = 0; y < rd->height; y++)
		free(row_pointers[y]);
	free(row_pointers);
	return TTRUE;
}

static TINT tek_lib_visual_createpixmap_from_png(lua_State *L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	TEKVisual *vis = lua_touserdata(L, -1);
	lua_pop(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	struct pngmemread rd;
	rd.width = 0;
	rd.height = 0;
	rd.flags = 0;
	rd.src = (png_bytep) luaL_checklstring(L, 1, &rd.len);
	rd.left = rd.len;
	if (!tek_lib_visual_load_png(TExecBase, &rd))
		return 0;
	TEKPixmap *pm = lua_newuserdata(L, sizeof(TEKPixmap));
	luaL_newmetatable(L, TEK_LIB_VISUALPIXMAP_CLASSNAME);
	lua_setmetatable(L, -2);
	pm->pxm_Data = rd.buf;
	pm->pxm_Width = rd.width;
	pm->pxm_Height = rd.height;
	pm->pxm_Flags = rd.flags;
	pm->pxm_VisualBase = vis;
	lua_pushinteger(L, rd.width);
	lua_pushinteger(L, rd.height);
	lua_pushboolean(L, rd.flags & PXMF_ALPHA);
	return 4;
}
#endif

static TINT
tek_lib_visual_createpixmap_from_ppm(lua_State *L)
{
	size_t ppmlen;
	TEKVisual *vis;
	struct TExecBase *TExecBase;
	const char *ppm = luaL_checklstring(L, 1, &ppmlen);
	const char *srcbuf;
	int tw, th, maxv;
	TUINT *buf;
	TEKPixmap *bm;
	TUINT8 r, g, b;
	int x, y;
	
	if (!((sscanf(ppm, "P6\n%d %d\n%d\n", &tw, &th, &maxv) == 3 ||
		sscanf(ppm, "P6\n#%*80[^\n]\n%d %d\n%d\n", &tw, &th, &maxv) == 3) &&
		maxv > 0 && maxv < 256))
	{
		/*luaL_argerror(L, 2, "invalid bitmap");*/
		lua_pushnil(L);
		lua_pushstring(L, "Invalid format");
		return 2;
	}
	
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	lua_pop(L, 1);
	TExecBase = vis->vis_ExecBase;
	
	bm = lua_newuserdata(L, sizeof(TEKPixmap));
	luaL_newmetatable(L, TEK_LIB_VISUALPIXMAP_CLASSNAME);
	lua_setmetatable(L, -2);
	
	srcbuf = ppm + ppmlen - 3 * tw * th;
	buf = TAlloc(TNULL, tw * th * sizeof(TUINT));
	if (buf == TNULL)
	{
		lua_pushstring(L, "out of memory");
		lua_error(L);
	}
	
	bm->pxm_Data = buf;
	bm->pxm_Width = tw;
	bm->pxm_Height = th;
	bm->pxm_Flags = 0;
	bm->pxm_VisualBase = vis;
	
	for (y = 0; y < th; ++y)
	{
		for (x = 0; x < tw; ++x)
		{
			r = *srcbuf++;
			g = *srcbuf++;
			b = *srcbuf++;
			*buf++ = (r << 16) | (g << 8) | b;
		}
	}
	
	lua_pushinteger(L, tw);
	lua_pushinteger(L, th);
	return 3;
}

static TINT
tek_lib_visual_createpixmap_from_table(lua_State *L)
{
	TEKVisual *vis;
	struct TExecBase *TExecBase;
	TUINT *buf;
	TEKPixmap *bm;
	int x, y;
	int tw = luaL_checkinteger(L, 1);
	int th = luaL_checkinteger(L, 2);
	int i = luaL_optinteger(L, 4, 0);
	int lw = luaL_optinteger(L, 5, tw);
	
	lua_getfield(L, LUA_REGISTRYINDEX, TEK_LIB_VISUAL_BASECLASSNAME);
	vis = lua_touserdata(L, -1);
	lua_pop(L, 1);
	TExecBase = vis->vis_ExecBase;
	
	bm = lua_newuserdata(L, sizeof(TEKPixmap));
	luaL_newmetatable(L, TEK_LIB_VISUALPIXMAP_CLASSNAME);
	lua_setmetatable(L, -2);
	
	buf = TAlloc(TNULL, tw * th * sizeof(TUINT));
	if (buf == TNULL)
	{
		lua_pushstring(L, "out of memory");
		lua_error(L);
	}
	
	bm->pxm_Data = buf;
	bm->pxm_Width = tw;
	bm->pxm_Height = th;
	bm->pxm_Flags = 0;
	bm->pxm_VisualBase = vis;
	
	for (y = 0; y < th; ++y)
	{
		for (x = 0; x < tw; ++x)
		{
			lua_rawgeti(L, 3, i + x);
			*buf++ = lua_tointeger(L, -1);
			lua_pop(L, 1);
		}
		i += lw;
	}
	return 1;
}

LOCAL LUACFUNC TINT
tek_lib_visual_createpixmap(lua_State *L)
{
	if (lua_isstring(L, 1))
	{
#if defined(ENABLE_PNG)
		int nres = tek_lib_visual_createpixmap_from_png(L);
		if (nres != 0)
			return nres;
#endif
		return tek_lib_visual_createpixmap_from_ppm(L);
	}
	return tek_lib_visual_createpixmap_from_table(L);
}

LOCAL LUACFUNC TINT
tek_lib_visual_freepixmap(lua_State *L)
{
	TEKPixmap *bm = getpixmapptr(L, 1);
	if (bm->pxm_Data)
	{
		TEKVisual *vis = bm->pxm_VisualBase;
		struct TExecBase *TExecBase = vis->vis_ExecBase;
		TFree(bm->pxm_Data);
		bm->pxm_Data = TNULL;
	}
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_getpixmap(lua_State *L)
{
	TEKPixmap *bm = getpixmapptr(L, 1);
	if (bm->pxm_Data)
	{
		lua_Integer x = luaL_checkinteger(L, 2);
		lua_Integer y = luaL_checkinteger(L, 3);
		if (x < 0 || x >= bm->pxm_Width)
			luaL_argerror(L, 2, "Invalid position");
		if (y < 0 || y >= bm->pxm_Height)
			luaL_argerror(L, 3, "Invalid position");
		lua_pushinteger(L, bm->pxm_Data[x + y * bm->pxm_Width]);
		return 1;
	}
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_setpixmap(lua_State *L)
{
	TEKPixmap *bm = getpixmapptr(L, 1);
	if (bm->pxm_Data)
	{
		lua_Integer x = luaL_checkinteger(L, 2);
		lua_Integer y = luaL_checkinteger(L, 3);
		lua_Integer val = luaL_checkinteger(L, 4);
		if (x < 0 || x >= bm->pxm_Width)
			luaL_argerror(L, 2, "Invalid position");
		if (y < 0 || y >= bm->pxm_Height)
			luaL_argerror(L, 3, "Invalid position");
		bm->pxm_Data[x + y * bm->pxm_Width] = val;
		return 1;
	}
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_allocpen(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT a = luaL_checkinteger(L, 2);
	TINT r = luaL_checkinteger(L, 3);
	TINT g = luaL_checkinteger(L, 4);
	TINT b = luaL_checkinteger(L, 5);
	TUINT rgb;
	TEKPen *pen = lua_newuserdata(L, sizeof(TEKPen));
	/* s: pendata */
	pen->pen_Pen = TVPEN_UNDEFINED;
	/* attach class metatable to userdata object: */
	luaL_newmetatable(L, TEK_LIB_VISUALPEN_CLASSNAME);
	/* s: pendata, meta */
	lua_setmetatable(L, -2);
	/* s: pendata */
	a = TCLAMP(0, a, 255);
	r = TCLAMP(0, r, 255);
	g = TCLAMP(0, g, 255);
	b = TCLAMP(0, b, 255);
	rgb = ((TUINT)(a) << 24) | ((TUINT)(r) << 16) | ((TUINT)(g) << 8) | 
		(TUINT)(b);
	pen->pen_Pen = TVisualAllocPen(vis->vis_Visual, rgb);
	pen->pen_Visual = vis;
	return 1;
}

LOCAL LUACFUNC TINT
tek_lib_visual_freepen(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TEKPen *pen = getpenptr(L, 2);
	if (pen->pen_Pen != TVPEN_UNDEFINED)
	{
		if (vis != pen->pen_Visual)
			luaL_argerror(L, 2, "Pen not from visual");
		TVisualFreePen(vis->vis_Visual, pen->pen_Pen);
		pen->pen_Pen = TVPEN_UNDEFINED;
	}
	return 0;
}

/*****************************************************************************/

#if defined(TEK_VISUAL_DEBUG)
static void tek_lib_visual_debugwait(TEKVisual *vis)
{
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TTIME dt = { 1800 };
	TWaitTime(&dt, 0);
}
#endif

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_rect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT x1 = luaL_checkinteger(L, 4) + sx;
	TINT y1 = luaL_checkinteger(L, 5) + sy;
	TEKPen *pen = checklookuppen(L, vis->vis_refPens, 6);
	#if defined(TEK_VISUAL_DEBUG)
	if (vis->vis_VisBase->vis_Debug)
	{
		TVisualRect(vis->vis_Visual, x0, y0, x1 - x0 + 1, y1 - y0 + 1,
			vis->vis_DebugPen1);
		tek_lib_visual_debugwait(vis);
		TVisualRect(vis->vis_Visual, x0, y0, x1 - x0 + 1, y1 - y0 + 1,
			vis->vis_DebugPen2);
		tek_lib_visual_debugwait(vis);
	}
	#endif
	TVisualRect(vis->vis_Visual, x0, y0, x1 - x0 + 1, y1 - y0 + 1,
		pen->pen_Pen);
	vis->vis_Dirty = TTRUE;
	return 0;
}

static void
tek_lib_visual_frectpixmap(lua_State *L, TEKVisual *vis, TEKPixmap *pm,
	TINT x0, TINT y0, TINT w, TINT h, TINT ox, TINT oy)
{
	TINT tw = pm->pxm_Width;
	TINT th = pm->pxm_Height;
	TUINT *buf = pm->pxm_Data;
	TINT th0, yo;
	TINT y = y0;
	TTAGITEM tags[2];
	tags[0].tti_Tag = TVisual_AlphaChannel;
	tags[0].tti_Value = pm->pxm_Flags & PXMF_ALPHA;
	tags[1].tti_Tag = TTAG_DONE;
	
	yo = (y0 - oy) % th;
	if (yo < 0) yo += th;
	th0 = th - yo;
	
	while (h > 0)
	{
		int tw0;
		int x = x0;
		int ww = w;
		int dh = TMIN(h, th0);
		int xo = (x0 - ox) % tw;
		if (xo < 0) xo += tw;
		tw0 = tw - xo;
		
		while (ww > 0)
		{
			int dw = TMIN(ww, tw0);
			TVisualDrawBuffer(vis->vis_Visual, x, y, 
				buf + xo + yo * tw, dw, dh, tw, tags);
			ww -= dw;
			x += dw;
			tw0 = tw;
			xo = 0;
		}
		h -= dh;
		y += dh;
		th0 = th;
		yo = 0;
	}
}

/*****************************************************************************/
/*
**	Gradient support
*/

#if defined(ENABLE_GRADIENT)

static float mulVec(vec a, vec b)
{
	return a.x * b.x + a.y * b.y;
}

static vec subVec(vec a, vec b)
{
	vec c;
	c.x = a.x - b.x;
	c.y = a.y - b.y;
	return c;
}

static vec addVec(vec a, vec b)
{
	vec c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	return c;
}

static vec scaleVec(vec a, float b)
{
	vec c;
	c.x = a.x * b;
	c.y = a.y * b;
	return c;
}

static void getS2(vec r0, vec u, int Ar, int Ag, int Ab, int Dr, int Dg, int Db, 
	float *pr, float *pg, float *pb, float *pf, int x, int y)
{
	vec z = { x, y };
	float s = mulVec(subVec(z, r0), u) / mulVec(u, u);
	vec F = subVec(addVec(r0, scaleVec(u, s)), r0);
	float f = TABS(u.y) > TABS(u.x) ? F.y / u.y : F.x / u.x;
	*pr = Ar + Dr * f;
	*pg = Ag + Dg * f;
	*pb = Ab + Db * f;
	*pf = f * 256;
}

static void
tek_lib_visual_frectgradient(lua_State *L, TEKVisual *vis, TEKGradient *gr,
	TINT x0, TINT y0, TINT w, TINT h, TINT ox, TINT oy)
{
	int y, x;
	int Ar = gr->A.r;
	int Ag = gr->A.g;
	int Ab = gr->A.b;
	int Br = gr->B.r;
	int Bg = gr->B.g;
	int Bb = gr->B.b;
	int Dr = Br - Ar;
	int Dg = Bg - Ag;
	int Db = Bb - Ab;
	
#if 0
	TUINT rgb0 = (Ar << 16) | (Ag << 8) | Ab;
	TUINT rgb1 = (Br << 16) | (Bg << 8) | Bb;
	TUINT rgb2 = (Dr << 16) | (Dg << 8) | Db;
	if (rgb0 == rgb1 && rgb0 == rgb2)
	{
		TVisualFRectRGB(vis->vis_Visual, x0, y0, w, h, rgb0);
		return;
	}
#endif

	vec r0 = gr->A.vec;
	vec u = subVec(gr->B.vec, r0);
	
	x = x0 - ox;
	y = y0 - oy;

	float cr0, cg0, cb0, cx0;
	float cr1, cg1, cb1, cx1;
	float cr2, cg2, cb2, cx2;
	getS2(r0, u, Ar, Ag, Ab, Dr, Dg, Db, &cr0, &cg0, &cb0, &cx0, x, y);
	getS2(r0, u, Ar, Ag, Ab, Dr, Dg, Db, &cr1, &cg1, &cb1, &cx1, x + w - 1, y);
	getS2(r0, u, Ar, Ag, Ab, Dr, Dg, Db, &cr2, &cg2, &cb2, &cx2, x, y + h - 1);
	float dr0 = (cr2 - cr0) / (h - 1);
	float dg0 = (cg2 - cg0) / (h - 1);
	float db0 = (cb2 - cb0) / (h - 1);
	float dx0 = (cx2 - cx0) / (h - 1);
	float dr = (cr1 - cr0) / (w - 1);
	float dg = (cg1 - cg0) / (w - 1);
	float db = (cb1 - cb0) / (w - 1);
	float dx = (cx1 - cx0) / (w - 1);
	
	TUINT *buf = TExecAlloc(vis->vis_ExecBase, TNULL, w * h * sizeof(TUINT));
	if (!buf)
		return;
	
	for (y = 0; y < h; ++y)
	{
		float cr = cr0;
		float cg = cg0;
		float cb = cb0;
		float cx = cx0;
		cr0 += dr0;
		cg0 += dg0;
		cb0 += db0;
		cx0 += dx0;

		float ce = cx + dx * w;
		TUINT r, g, b;
		if (cx < 1 && ce < 1)
		{
			r = Ar;
			g = Ag;
			b = Ab;
		}
		else if (cx > 255 && ce > 255)
		{
			r = Br;
			g = Bg;
			b = Bb;
		}
		else if (cx >= 1 && cx <= 255 && ce >= 1 && ce <= 255)
		{
			if (cx == ce)
			{
				r = cr;
				g = cg;
				b = cb;
			}
			else
			{
				for (x = 0; x < w; ++x)
				{
					*buf++ = ((TUINT) cr << 16) | ((TUINT) cg << 8) | (TUINT) cb;
					cr += dr;
					cg += dg;
					cb += db;
				}
				continue;
			}
		}
		else if ((cx < 1 && ce <= 255) || (cx <= 255 && ce < 1))
		{
			for (x = 0; x < w; ++x)
			{
				if (cx < 1)
					*buf++ = ((TUINT) Ar << 16) | ((TUINT) Ag << 8) | (TUINT) Ab;
				else
					*buf++ = ((TUINT) cr << 16) | ((TUINT) cg << 8) | (TUINT) cb;
				cr += dr;
				cg += dg;
				cb += db;
				cx += dx;
			}
			continue;
		}
		else if ((cx >= 1 && ce > 255) || (cx > 255 && ce >= 1))
		{
			for (x = 0; x < w; ++x)
			{
				if (cx > 255)
					*buf++ = ((TUINT) Br << 16) | ((TUINT) Bg << 8) | (TUINT) Bb;
				else
					*buf++ = ((TUINT) cr << 16) | ((TUINT) cg << 8) | (TUINT) cb;
				cr += dr;
				cg += dg;
				cb += db;
				cx += dx;
			}
			continue;
		}
		else
		{
			for (x = 0; x < w; ++x)
			{
				if (cx < 1)
					*buf++ = ((TUINT) Ar << 16) | ((TUINT) Ag << 8) | (TUINT) Ab;
				else if (cx > 255)
					*buf++ = ((TUINT) Br << 16) | ((TUINT) Bg << 8) | (TUINT) Bb;
				else
					*buf++ = ((TUINT) cr << 16) | ((TUINT) cg << 8) | (TUINT) cb;
				cr += dr;
				cg += dg;
				cb += db;
				cx += dx;
			}
			continue;
		}
		
		for (x = 0; x < w; ++x)
			*buf++ = ((TUINT) r << 16) | ((TUINT) g << 8) | (TUINT) b;
	}

	buf -= w * h;
	TVisualDrawBuffer(vis->vis_Visual, x0, y0, buf, w, h, w, TNULL);
	TExecFree(vis->vis_ExecBase, buf);
}

#endif

/*****************************************************************************/


LOCAL LUACFUNC TINT
tek_lib_visual_frect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT x1 = luaL_checkinteger(L, 4) + sx;
	TINT y1 = luaL_checkinteger(L, 5) + sy;
	TINT w = x1 - x0 + 1;
	TINT h = y1 - y0 + 1;
	void *udata;
	
	if (x0 > x1 || y0 > y1)
		return 0;
	
	#if defined(TEK_VISUAL_DEBUG)
	if (vis->vis_VisBase->vis_Debug)
	{
		TVisualFRect(vis->vis_Visual, x0, y0, w, h, vis->vis_DebugPen1);
		tek_lib_visual_debugwait(vis);
		TVisualFRect(vis->vis_Visual, x0, y0, w, h, vis->vis_DebugPen2);
		tek_lib_visual_debugwait(vis);
	}
	#endif
	
	switch (getbgpaint(L, vis, 6, &udata))
	{
		case PAINT_PEN:
		{
			TEKPen *pen = udata;
			TVisualFRect(vis->vis_Visual, x0, y0, w, h, pen->pen_Pen);
			break;
		}
		case PAINT_PIXMAP:
		{
			TEKPixmap *pm = udata;
			TINT ox = vis->vis_TextureX + sx;
			TINT oy = vis->vis_TextureY + sy;
			tek_lib_visual_frectpixmap(L, vis, pm, x0, y0, w, h, ox, oy);
			break;
		}
#if defined(ENABLE_GRADIENT)
		case PAINT_GRADIENT:
		{
			TEKGradient *grad = udata;
			TINT ox = vis->vis_TextureX + sx;
			TINT oy = vis->vis_TextureY + sy;
			tek_lib_visual_frectgradient(L, vis, grad, x0, y0, w, h, ox, oy);
			break;
		}
#endif
	}
	
	vis->vis_Dirty = TTRUE;
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_line(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT x1 = luaL_checkinteger(L, 4) + sx;
	TINT y1 = luaL_checkinteger(L, 5) + sy;
	TEKPen *pen = checklookuppen(L, vis->vis_refPens, 6);
	#if defined(TEK_VISUAL_DEBUG)
	if (vis->vis_VisBase->vis_Debug)
	{
		TVisualLine(vis->vis_Visual, x0, y0, x1, y1, vis->vis_DebugPen1);
		tek_lib_visual_debugwait(vis);
		TVisualLine(vis->vis_Visual, x0, y0, x1, y1, vis->vis_DebugPen2);
		tek_lib_visual_debugwait(vis);
	}
	#endif
	TVisualLine(vis->vis_Visual, x0, y0, x1, y1, pen->pen_Pen);
	vis->vis_Dirty = TTRUE;
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_plot(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TEKPen *pen = checklookuppen(L, vis->vis_refPens, 4);
	#if defined(TEK_VISUAL_DEBUG)
	if (vis->vis_VisBase->vis_Debug)
	{
		TVisualPlot(vis->vis_Visual, x0, y0, vis->vis_DebugPen1);
		tek_lib_visual_debugwait(vis);
		TVisualPlot(vis->vis_Visual, x0, y0, vis->vis_DebugPen2);
		tek_lib_visual_debugwait(vis);
	}
	#endif
	TVisualPlot(vis->vis_Visual, x0, y0, pen->pen_Pen);
	vis->vis_Dirty = TTRUE;
	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_text(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT x1 = luaL_checkinteger(L, 4) + sx;
	TINT y1 = luaL_checkinteger(L, 5) + sy;
	size_t tlen;
	TSTRPTR text = (TSTRPTR) luaL_checklstring(L, 6, &tlen);
	TEKPen *fpen = checklookuppen(L, vis->vis_refPens, 7);
	void *udata;

	switch (lookuppaint(L, vis->vis_refPens, 8, &udata, TTRUE))
	{
		case PAINT_PEN:
		{
			TEKPen *pen = udata;
			TVisualFRect(vis->vis_Visual, x0, y0, x1 - x0 + 1, y1 - y0 + 1,
				pen->pen_Pen);
			break;
		}
		case PAINT_PIXMAP:
		{
			TEKPixmap *pm = udata;
			TINT ox = vis->vis_TextureX + sx;
			TINT oy = vis->vis_TextureY + sy;
			tek_lib_visual_frectpixmap(L, vis, pm, x0, y0, 
				x1 - x0 + 1, y1 - y0 + 1, ox, oy);
			break;
		}
#if defined(ENABLE_GRADIENT)
		case PAINT_GRADIENT:
		{
			TEKGradient *grad = udata;
			TINT ox = vis->vis_TextureX + sx;
			TINT oy = vis->vis_TextureY + sy;
			tek_lib_visual_frectgradient(L, vis, grad, x0, y0, 
				x1 - x0 + 1, y1 - y0 + 1, ox, oy);
			break;
		}
#endif
	}
	
	TVisualText(vis->vis_Visual, x0, y0, text, tlen, fpen->pen_Pen);
		
	vis->vis_Dirty = TTRUE;
	return 0;
}

/*****************************************************************************/
/*
**	drawimage(visual, image, r1, r2, r3, r4, pentab, override_pen)
**
**	Layout of vector image data structure:
**
**	{
**		[1] = { x0, y0, x1, y1, ... }, -- coordinates (x/y)
**		[4] = boolean -- is_transparent
**		[5] = {  -- primitives
**			{ [1]=fmtcode, [2]=numpts, [3]={ indices }, [4]=pen_or_pentable },
**			...
**		}
**	}
**
**	format codes:
**		0x1000 - strip
**		0x2000 - fan
*/

LOCAL LUACFUNC TINT
tek_lib_visual_drawimage(lua_State *L)
{
	TEKPen *pen_override = TNULL;
	TEKVisual *vis = checkvisptr(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	lua_Integer rect[4], scalex, scaley;
	size_t primcount, i, j;
	TTAGITEM tags[2];
	tags[1].tti_Tag = TTAG_DONE;
	
	if (lua_type(L, 7) != LUA_TTABLE)
		pen_override = lookuppen(L, vis->vis_refPens, 7);
	
	rect[0] = luaL_checkinteger(L, 3);
	rect[1] = luaL_checkinteger(L, 4);
	rect[2] = luaL_checkinteger(L, 5);
	rect[3] = luaL_checkinteger(L, 6);
	scalex = rect[2] - rect[0];
	scaley = rect[1] - rect[3];
	
	lua_getmetatable(L, 1);
	/* vismeta */
	lua_rawgeti(L, -1, vis->vis_refPens);
	/* vismeta, pentab */
	lua_remove(L, -2);
	/* pentab */
	
	lua_pushinteger(L, 1);
	lua_gettable(L, 2);
	/* s: pentab, coords */
	lua_pushinteger(L, 5);
	lua_gettable(L, 2);
	/* s: pentab, coords, primitives */
#if LUA_VERSION_NUM < 502
	primcount = lua_objlen(L, -1);
#else
	primcount = lua_rawlen(L, -1);
#endif

	for (i = 0; i < primcount; i++)
	{
		lua_Integer nump, fmt;
		size_t bufsize;
		void *buf;
		TINT *coord;
		TINT *pentab;
		
		lua_rawgeti(L, -1, i + 1);
		/* s: pentab, coords, primitives, prim[i] */
		lua_rawgeti(L, -1, 1);
		/* s: pentab, coords, primitives, prim[i], fmtcode */
		fmt = luaL_checkinteger(L, -1);
		lua_rawgeti(L, -2, 2);
		/* s: pentab, coords, primitives, prim[i], fmtcode, nump */
		nump = luaL_checkinteger(L, -1);
		
		bufsize = sizeof(TINT) * 3 * nump;
		buf = vis->vis_VisBase->vis_DrawBuffer;
		if (buf && TGetSize(buf) < bufsize)
		{
			TFree(buf);
			buf = TNULL;
		}
		if (buf == TNULL)
			buf = TAlloc(TNULL, bufsize);
		vis->vis_VisBase->vis_DrawBuffer = buf;
		if (buf == TNULL)
		{
			lua_pushstring(L, "out of memory");
			lua_error(L);
		}
		coord = buf;
		
		lua_rawgeti(L, -3, 3);
		/* s: pentab, coords, primitives, prim[i], fmtcode, nump, indices */
		lua_rawgeti(L, -4, 4);
		/* s: pentab, coords, primitives, prim[i], fmtcode, nump, indices, pt */
		
		pentab = lua_type(L, -1) == LUA_TTABLE ? coord + 2 * nump : TNULL;
		if (pentab)
			tags[0].tti_Tag = TVisual_PenArray;
		else
		{
			tags[0].tti_Tag = TVisual_Pen;
			if (pen_override)
				tags[0].tti_Value = pen_override->pen_Pen;
			else
			{
				lua_gettable(L, -8);
				tags[0].tti_Value = ((TEKPen *) checkpenptr(L, -1))->pen_Pen;
			}
		}
		
		for (j = 0; j < (size_t) nump; ++j)
		{
			lua_Integer idx;
			lua_rawgeti(L, -2, j + 1);
			idx = lua_tointeger(L, -1);
			lua_rawgeti(L, -8, idx * 2 - 1);
			lua_rawgeti(L, -9, idx * 2);
			/* index, x, y */
			coord[j * 2] = rect[0] + sx + 
				(lua_tointeger(L, -2) * scalex) / 0x10000;
			coord[j * 2 + 1] = rect[3] + sy +
				(lua_tointeger(L, -1) * scaley) / 0x10000;
			if (pentab)
			{
				lua_rawgeti(L, -7, idx + 1);
				pentab[j] = ((TEKPen *) checkpenptr(L, -1))->pen_Pen;
				lua_pop(L, 4);
			}
			else
				lua_pop(L, 3);
		}
		
		switch (fmt & 0xf000)
		{
			case 0x1000:
			case 0x4000:
				TVisualDrawStrip(vis->vis_Visual, coord, nump, tags);
				break;
			case 0x2000:
				TVisualDrawFan(vis->vis_Visual, coord, nump, tags);
				break;
		}
		
		lua_pop(L, 5);
	}
	
	vis->vis_Dirty = TTRUE;
	lua_pop(L, 3);
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_getattrs(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TTAGITEM tags[7];
	size_t narg = 0, i;
	const char *opts = lua_tolstring(L, 2, &narg);
	
	if (narg > 5)
		luaL_error(L, "too many attributes");
	
	if (narg == 0)
	{
		opts = "whxy";
		narg = 4;
	}
	
	for (i = 0; i < narg; ++i)
		tags[i].tti_Value = (TTAG) &tags[i].tti_Value;
	tags[narg].tti_Tag = TTAG_DONE;
	
	for (i = 0; i < narg; ++i)
	{
		switch (opts[i])
		{
			case 'w':
				tags[i].tti_Tag = TVisual_Width;
				break;
			case 'h':
				tags[i].tti_Tag = TVisual_Height;
				break;
			case 'x':
				tags[i].tti_Tag = TVisual_WinLeft;
				break;
			case 'y':
				tags[i].tti_Tag = TVisual_WinTop;
				break;
			case 's':
				tags[i].tti_Tag = TVisual_HaveSelection;
				break;
			case 'c':
				tags[i].tti_Tag = TVisual_HaveClipboard;
				break;
			default:
				luaL_error(L, "unknown attribute");
		}
	}
	TVisualGetAttrs(vis->vis_Visual, tags);
	
	for (i = 0; i < narg; ++i)
	{
		switch (opts[i])
		{
			case 'w':
			case 'h':
			case 'x':
			case 'y':
				lua_pushinteger(L, *((TINT *) &tags[i].tti_Value));
				break;
			case 's':
			case 'c':
				lua_pushboolean(L, *((TINT *) &tags[i].tti_Value));
				break;
		}
	}
	return narg;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_getuserdata(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	if (vis->vis_refUserData >= 0)
	{
		lua_getmetatable(L, 1);
		/* s: metatable */
		lua_rawgeti(L, -1, vis->vis_refUserData);
		/* s: metatable, userdata */
		lua_remove(L, -2);
	}
	else
		lua_pushnil(L);
	return 1;
}

/*****************************************************************************/

static TTAGITEM *getminmax(lua_State *L, TTAGITEM *tp, const char *keyname,
	TTAG tag)
{
	TBOOL isfalse;
	lua_getfield(L, 2, keyname);
	isfalse = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
	if (lua_isnumber(L, -1) || isfalse)
	{
		TINT val = isfalse ? -1 : lua_tointeger(L, -1);
		tp->tti_Tag = tag;
		tp->tti_Value = val;
		tp++;
	}
	lua_pop(L, 1);
	return tp;
}

LOCAL LUACFUNC TINT
tek_lib_visual_setattrs(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TTAGITEM tags[7], *tp = tags;
	tp = getminmax(L, tp, "MinWidth", TVisual_MinWidth);
	tp = getminmax(L, tp, "MinHeight", TVisual_MinHeight);
	tp = getminmax(L, tp, "MaxWidth", TVisual_MaxWidth);
	tp = getminmax(L, tp, "MaxHeight", TVisual_MaxHeight);
	lua_getfield(L, 2, "HaveSelection");
	if (!lua_isnoneornil(L, -1))
	{
		tp->tti_Tag = TVisual_HaveSelection;
		tp++->tti_Value = lua_toboolean(L, -1);
		lua_pop(L, 1);
	}
	lua_getfield(L, 2, "HaveClipboard");
	if (!lua_isnoneornil(L, -1))
	{
		tp->tti_Tag = TVisual_HaveClipboard;
		tp++->tti_Value = lua_toboolean(L, -1);
		lua_pop(L, 1);
	}
	tp->tti_Tag = TTAG_DONE;
	
	#if defined(TEK_VISUAL_DEBUG)
	lua_getfield(L, 2, "Debug");
	if (lua_isboolean(L, -1))
		vis->vis_VisBase->vis_Debug = lua_toboolean(L, -1);
	lua_pop(L, 1);
	#endif
	
	lua_pushnumber(L, TVisualSetAttrs(vis->vis_Visual, tags));
	return 1;
}

/*****************************************************************************/
/*
**	textsize_visual: return text width, height using the current font
*/

LOCAL LUACFUNC TINT
tek_lib_visual_textsize_visual(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	size_t len;
	TSTRPTR s = (TSTRPTR) luaL_checklstring(L, 2, &len);
	lua_pushinteger(L,
		TVisualTextSize(vis->vis_Base, vis->vis_Font, s, (TINT) len));
	lua_pushinteger(L, vis->vis_FontHeight);
	return 2;
}

/*****************************************************************************/
/*
**	setfont(font): attach a font to a visual
*/

LOCAL LUACFUNC TINT
tek_lib_visual_setfont(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TEKFont *font = checkfontptr(L, 2);
	if (font->font_Font && vis->vis_Font != font->font_Font)
	{
		lua_getmetatable(L, 1);
		/* s: vismeta */

		if (vis->vis_refFont != -1)
		{
			/* unreference old current font: */
			luaL_unref(L, -1, vis->vis_refFont);
			vis->vis_refFont = -1;
		}

		TVisualSetFont(vis->vis_Visual, font->font_Font);
		vis->vis_Font = font->font_Font;
		vis->vis_FontHeight = font->font_Height;

		/* reference new font: */
		lua_pushvalue(L, 2);
		/* s: vismeta, font */
		vis->vis_refFont = luaL_ref(L, -2);
		/* s: vismeta */
		lua_pop(L, 1);
	}
	return 0;
}

/*****************************************************************************/

static TTAG hookfunc(struct THook *hook, TAPTR obj, TTAG msg)
{
	TEKVisual *vis = hook->thk_Data;
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TINT *rect = (TINT *) msg;
	TINT *newbuf = vis->vis_RectBuffer ?
		TRealloc(vis->vis_RectBuffer,
			(vis->vis_RectBufferNum + 4) * sizeof(TINT)) :
		TAlloc(TNULL, sizeof(TINT) * 4);

	if (newbuf)
	{
		vis->vis_RectBuffer = newbuf;
		newbuf += vis->vis_RectBufferNum;
		vis->vis_RectBufferNum += 4;
		newbuf[0] = rect[0];
		newbuf[1] = rect[1];
		newbuf[2] = rect[2];
		newbuf[3] = rect[3];
	}

	return 0;
}

LOCAL LUACFUNC TINT
tek_lib_visual_copyarea(lua_State *L)
{
	TTAGITEM tags[2], *tp = TNULL;
	struct THook hook;
	TEKVisual *vis = checkvisptr(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TINT sx = vis->vis_ShiftX;
	TINT sy = vis->vis_ShiftY;
	TINT x = luaL_checkinteger(L, 2);
	TINT y = luaL_checkinteger(L, 3);
	TINT w = luaL_checkinteger(L, 4) - x + 1;
	TINT h = luaL_checkinteger(L, 5) - y + 1;
	TINT dx = luaL_checkinteger(L, 6) + sx;
	TINT dy = luaL_checkinteger(L, 7) + sy;
	x += sx;
	y += sy;

	if (lua_istable(L, 8))
	{
		vis->vis_RectBuffer = TNULL;
		vis->vis_RectBufferNum = 0;
		TInitHook(&hook, hookfunc, vis);
		tags[0].tti_Tag = TVisual_ExposeHook;
		tags[0].tti_Value = (TTAG) &hook;
		tags[1].tti_Tag = TTAG_DONE;
		tp = tags;
	}

	TVisualCopyArea(vis->vis_Visual, x, y, w, h, dx, dy, tp);

	if (tp)
	{
		TINT i;
		for (i = 0; i < vis->vis_RectBufferNum; ++i)
		{
			lua_pushinteger(L, vis->vis_RectBuffer[i]);
			lua_rawseti(L, 8, i + 1);
		}
		TFree(vis->vis_RectBuffer);
		vis->vis_RectBuffer = TNULL;
	}

	vis->vis_Dirty = TTRUE;
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_setcliprect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT x = luaL_checkinteger(L, 2);
	TINT y = luaL_checkinteger(L, 3);
	TINT w = luaL_checkinteger(L, 4);
	TINT h = luaL_checkinteger(L, 5);
	TVisualSetClipRect(vis->vis_Visual, x, y, w, h, TNULL);
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_unsetcliprect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TVisualUnsetClipRect(vis->vis_Visual);
	return 0;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT
tek_lib_visual_setshift(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT sx = vis->vis_ShiftX;
	TINT sy = vis->vis_ShiftY;
	vis->vis_ShiftX = sx + (TINT) luaL_optinteger(L, 2, 0);
	vis->vis_ShiftY = sy + (TINT) luaL_optinteger(L, 3, 0);
	lua_pushinteger(L, sx);
	lua_pushinteger(L, sy);
	return 2;
}

/*****************************************************************************/
/*
**	drawrgb(visual, x0, y0, table, width, height, pixwidth, pixheight)
*/

LOCAL LUACFUNC TINT
tek_lib_visual_drawrgb(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TINT sx = vis->vis_ShiftX, sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT w = luaL_checkinteger(L, 5);
	TINT h = luaL_checkinteger(L, 6);
	TINT pw = luaL_checkinteger(L, 7);
	TINT ph = luaL_checkinteger(L, 8);
	TUINT *buf;
	TINT bw = w * pw;
	TINT bh = h * ph;

	luaL_checktype(L, 4, LUA_TTABLE);

	buf = TAlloc(TNULL, bw * bh * sizeof(TUINT));
	if (buf)
	{
		TUINT rgb;
		TUINT *p = buf;
		TINT i = 0;
		TINT xx, yy, x, y;

		for (y = 0; y < h; ++y)
		{
			TUINT *lp = p;
			for (x = 0; x < w; ++x)
			{
				lua_rawgeti(L, 4, i++);
				rgb = lua_tointeger(L, -1);
				lua_pop(L, 1);
				for (xx = 0; xx < pw; ++xx)
					*p++ = rgb;
			}

			for (yy = 0; yy < ph - 1; ++yy)
			{
				TCopyMem(lp, p, bw * sizeof(TUINT));
				p += bw;
			}
		}

		TVisualDrawBuffer(vis->vis_Visual, x0, y0, buf, bw, bh, bw, TNULL);

		TFree(buf);
	}

	vis->vis_Dirty = TTRUE;
	return 0;
}

/*****************************************************************************/
/*
**	drawpixmap(visual, image, x0, y0, x1, y1)
*/

LOCAL LUACFUNC TINT
tek_lib_visual_drawpixmap(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TEKPixmap *img = checkpixmapptr(L, 2);
	TINT sx = vis->vis_ShiftX;
	TINT sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 3);
	TINT y0 = luaL_checkinteger(L, 4);
	TINT w = luaL_optinteger(L, 5, x0 + img->pxm_Width - 1) - x0 + 1;
	TINT h = luaL_optinteger(L, 6, y0 + img->pxm_Height - 1) - y0 + 1;
	TINT offs = 0; /*luaL_optinteger(L, 7, 0)*/;
	TTAGITEM tags[2];
	tags[0].tti_Tag = TVisual_AlphaChannel;
	tags[0].tti_Value = img->pxm_Flags & PXMF_ALPHA;
	tags[1].tti_Tag = TTAG_DONE;
	TVisualDrawBuffer(vis->vis_Visual, x0 + sx, y0 + sy, img->pxm_Data + offs,
		w, h, img->pxm_Width, tags);
	vis->vis_Dirty = TTRUE;
	return 0;
}

/*****************************************************************************/
/*
**	settextureorigin(visual, tx, ty)
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_settextureorigin(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT tx = vis->vis_TextureX;
	TINT ty = vis->vis_TextureY;
	vis->vis_TextureX = lua_tointeger(L, 2);
	vis->vis_TextureY = lua_tointeger(L, 3);
	lua_pushinteger(L, tx);
	lua_pushinteger(L, ty);
	return 2;
}

/*****************************************************************************/
/*
**	pushcliprect(x0, y0, x1, y1)
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_pushcliprect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	TINT sx = vis->vis_ShiftX;
	TINT sy = vis->vis_ShiftY;
	TINT x0 = luaL_checkinteger(L, 2) + sx;
	TINT y0 = luaL_checkinteger(L, 3) + sy;
	TINT x1 = luaL_checkinteger(L, 4) + sx;
	TINT y1 = luaL_checkinteger(L, 5) + sy;
	struct RectNode *clipnode = 
		(struct RectNode *) TRemHead(&vis->vis_FreeRects);
	if (clipnode == TNULL)
	{
		clipnode = TAlloc(TNULL, sizeof(struct RectNode));
		if (clipnode == TNULL)
			luaL_error(L, "Out of memory");
	}
	clipnode->rn_Rect[0] = x0;
	clipnode->rn_Rect[1] = y0;
	clipnode->rn_Rect[2] = x1;
	clipnode->rn_Rect[3] = y1;
	TAddTail(&vis->vis_ClipStack, &clipnode->rn_Node);
	
	if (vis->vis_HaveClipRect)
	{
		TINT c0 = vis->vis_ClipRect[0];
		TINT c1 = vis->vis_ClipRect[1];
		TINT c2 = vis->vis_ClipRect[2];
		TINT c3 = vis->vis_ClipRect[3];
		if (TEK_UI_OVERLAP(x0, y0, x1, y1, c0, c1, c2, c3))
		{
			x0 = TMAX(x0, c0);
			y0 = TMAX(y0, c1);
			x1 = TMIN(x1, c2);
			y1 = TMIN(y1, c3);
		}
		else
		{
			x0 = y0 = x1 = y1 = -1;
		}
	}
	
	vis->vis_ClipRect[0] = x0;
	vis->vis_ClipRect[1] = y0;
	vis->vis_ClipRect[2] = x1;
	vis->vis_ClipRect[3] = y1;
	vis->vis_HaveClipRect = TTRUE;
	TVisualSetClipRect(vis->vis_Visual, x0, y0, 
		x1 - x0 + 1, y1 - y0 + 1, TNULL);

	return 0;
}

/*****************************************************************************/
/*
**	popcliprect():
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_popcliprect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	TINT x0 = -1;
	TINT y0 = -1;
	TINT x1 = -1;
	TINT y1 = -1;
	TINT have_cliprect = TFALSE;
	TAddHead(&vis->vis_FreeRects, TRemTail(&vis->vis_ClipStack));
	if (!TISLISTEMPTY(&vis->vis_ClipStack))
	{
		struct TNode *next, *node = vis->vis_ClipStack.tlh_Head;
		x0 = 0;
		y0 = 0;
		x1 = TEKUI_HUGE;
		y1 = TEKUI_HUGE;
		have_cliprect = TTRUE;
		for (; (next = node->tln_Succ); node = next)
		{
			struct RectNode *rn = (struct RectNode *) node;
			TINT c0 = rn->rn_Rect[0];
			TINT c1 = rn->rn_Rect[1];
			TINT c2 = rn->rn_Rect[2];
			TINT c3 = rn->rn_Rect[3];
			if (!TEK_UI_OVERLAP(x0, y0, x1, y1, c0, c1, c2, c3))
			{
				x0 = y0 = x1 = y1 = -1;
				break;
			}
			x0 = TMAX(x0, c0);
			y0 = TMAX(y0, c1);
			x1 = TMIN(x1, c2);
			y1 = TMIN(y1, c3);
		}
	}
	vis->vis_HaveClipRect = have_cliprect;
	vis->vis_ClipRect[0] = x0;
	vis->vis_ClipRect[1] = y0;
	vis->vis_ClipRect[2] = x1;
	vis->vis_ClipRect[3] = y1;
	if (have_cliprect)
		TVisualSetClipRect(vis->vis_Visual, x0, y0, x1 - x0 + 1, y1 - y0 + 1,
			TNULL);
	else
		TVisualUnsetClipRect(vis->vis_Visual);
	return 0;
}

/*****************************************************************************/
/*
**	getcliprect:
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_getcliprect(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	if (vis->vis_HaveClipRect)
	{
		lua_pushinteger(L, vis->vis_ClipRect[0]);
		lua_pushinteger(L, vis->vis_ClipRect[1]);
		lua_pushinteger(L, vis->vis_ClipRect[2]);
		lua_pushinteger(L, vis->vis_ClipRect[3]);
		return 4;
	}
	return 0;
}

/*****************************************************************************/
/*
**	setpen:
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_setbgpen(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	if (vis->vis_refBGPen >= 0)
	{
		luaL_unref(L, lua_upvalueindex(1), vis->vis_refBGPen);
		vis->vis_refBGPen = -1;
	}
	vis->vis_BGPenType = lookuppaint(L, vis->vis_refPens, 2, 
		(void **) &vis->vis_BGPen, TTRUE);
	if (vis->vis_BGPen)
	{
		lua_pushvalue(L, 2);
		vis->vis_refBGPen = luaL_ref(L, lua_upvalueindex(1));
	}
	vis->vis_TextureX = lua_tointeger(L, 3);
	vis->vis_TextureY = lua_tointeger(L, 4);
	return 0;
}

/*****************************************************************************/
/*
**	getselection:
*/

LOCAL LUACFUNC TINT 
tek_lib_visual_getselection(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	struct TExecBase *TExecBase = vis->vis_ExecBase;
	struct TTagItem tags[2];
	TAPTR data;
	tags[0].tti_Tag = TVisual_SelectionType;
	tags[0].tti_Value = luaL_optinteger(L, 2, 1);
	tags[1].tti_Tag = TTAG_DONE;
	data = TVisualGetSelection(vis->vis_Visual, tags);
	if (data)
	{
		TSIZE len = TGetSize(data);
		lua_pushlstring(L, data, len);
		TFree(data);
		return 1;		
	}
	return 0;
}

/*****************************************************************************/
/*
**	creategradient(x0,y0,x1,y1,rgb0,rgb1)
*/

LOCAL LUACFUNC TINT tek_lib_visual_creategradient(lua_State *L)
{
#if defined(ENABLE_GRADIENT)
	TUINT rgb0 = luaL_checkinteger(L, 5);
	TUINT rgb1 = luaL_checkinteger(L, 6);
	TEKGradient *gr = lua_newuserdata(L, sizeof(TEKGradient));
	gr->A.vec.x = luaL_checkinteger(L, 1);
	gr->A.vec.y = luaL_checkinteger(L, 2);
	gr->B.vec.x = luaL_checkinteger(L, 3);
	gr->B.vec.y = luaL_checkinteger(L, 4);
	gr->A.r = (rgb0 & 0xff0000) >> 16;
	gr->A.g = (rgb0 & 0xff00) >> 8;
	gr->A.b = rgb0 & 0xff;
	gr->B.r = (rgb1 & 0xff0000) >> 16;
	gr->B.g = (rgb1 & 0xff00) >> 8;
	gr->B.b = rgb1 & 0xff;
	luaL_newmetatable(L, TEK_LIB_VISUALGRADIENT_CLASSNAME);
	lua_setmetatable(L, -2);
#else
	lua_pushnil(L);
#endif
	return 1;
}

/*****************************************************************************/

LOCAL LUACFUNC TINT tek_lib_visual_getpaintinfo(lua_State *L)
{
	TEKVisual *vis = checkvisptr(L, 1);
	void *udata;
	lua_pushinteger(L, getbgpaint(L, vis, 2, &udata));
	return 1;
}
