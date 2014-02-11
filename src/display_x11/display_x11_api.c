
#include <string.h>
#include "display_x11_mod.h"

static void freepen(X11DISPLAY *mod, X11WINDOW *v, struct X11Pen *pen);
static int x11_seteventmask(X11DISPLAY *mod, X11WINDOW *v, TUINT eventmask);
static void x11_freeimage(X11DISPLAY *mod, X11WINDOW *v);

#define DEF_WINWIDTH 600
#define DEF_WINHEIGHT 400

/*****************************************************************************/
/*
**	This is awkward, since we are in a shared library and must
**	check for availability of the extension in an error handler using
**	a global variable. TODO: To fully work around this mess, we would
**	additionally have to enclose XShmAttach() in a mutex.
*/

static TBOOL x11_shm_available = TTRUE;

static int shm_errhandler(Display *d, XErrorEvent *evt)
{
	TDBPRINTF(TDB_ERROR,("Remote display - fallback to normal XPutImage\n"));
	x11_shm_available = TFALSE;
	return 0;
}

static void x11_releasesharedmemory(X11DISPLAY *mod, X11WINDOW *v)
{
	if (v->shmsize > 0)
	{
		XShmDetach(mod->x11_Display, &v->shminfo);
		shmdt(v->shminfo.shmaddr);
		shmctl(v->shminfo.shmid, IPC_RMID, 0);
	}
}

static TAPTR x11_getsharedmemory(X11DISPLAY *mod, X11WINDOW *v, size_t size)
{
	if (!mod->x11_ShmAvail)
		return TNULL;
	if (v->shmsize > 0 && size <= v->shmsize)
		return v->shminfo.shmaddr;
	x11_releasesharedmemory(mod, v);
	v->shminfo.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
	if (v->shminfo.shmid != -1)
	{
		XErrorHandler oldhnd;
		v->shminfo.readOnly = False;
		XSync(mod->x11_Display, 0);
		oldhnd = XSetErrorHandler(shm_errhandler);
		x11_shm_available = TTRUE;
		XShmAttach(mod->x11_Display, &v->shminfo);
		TDBPRINTF(TDB_TRACE,("shmattach size=%d\n", (int) size));
		XSync(mod->x11_Display, 0);
		XSetErrorHandler(oldhnd);
		if (x11_shm_available)
		{
			v->shminfo.shmaddr = shmat(v->shminfo.shmid, 0, 0);
			v->shmsize = size;
		}
		else
		{
			shmdt(v->shminfo.shmaddr);
			shmctl(v->shminfo.shmid, IPC_RMID, 0);
			/* ah, just forget it altogether: */
			mod->x11_ShmAvail = TFALSE;
			v->shmsize = 0;
			return TNULL;
		}
	}
	return v->shminfo.shmaddr;
}

/*****************************************************************************/

LOCAL void x11_openvisual(X11DISPLAY *mod, struct TVRequest *req)
{
	TAPTR TExecBase = TGetExecBase(mod);
	TTAGITEM *tags = req->tvr_Op.OpenWindow.Tags;
	X11WINDOW *v = TAlloc0(mod->x11_MemMgr, sizeof(X11WINDOW));
	XTextProperty title_prop;
	TBOOL save_under = TFALSE;

	req->tvr_Op.OpenWindow.Window = v;
	if (v == TNULL) return;

	v->userdata = TGetTag(tags, TVisual_UserData, TNULL);
	
	for (;;)
	{
		XSetWindowAttributes swa;
		TUINT swa_mask;
		XGCValues gcv;
		TUINT gcv_mask;
		struct FontNode *fn;
		XWindowAttributes rootwa;
		TBOOL setfocus = TFALSE;
		TINT sw, sh;
		TBOOL borderless = TGetTag(tags, TVisual_Borderless, TFALSE);
		TINT minw = (TINT) TGetTag(tags, TVisual_MinWidth, -1);
		TINT minh = (TINT) TGetTag(tags, TVisual_MinHeight, -1);
		TINT maxw = (TINT) TGetTag(tags, TVisual_MaxWidth, 1000000);
		TINT maxh = (TINT) TGetTag(tags, TVisual_MaxHeight, 1000000);
		
		/* gain access to root window properties: */
		XGetWindowAttributes(mod->x11_Display,
			DefaultRootWindow(mod->x11_Display), &rootwa);
		
		if (mod->x11_FullScreenWidth == 0)
		{
			sw = WidthOfScreen(rootwa.screen);
			sh = HeightOfScreen(rootwa.screen);
		}
		else
		{
			sw = mod->x11_FullScreenWidth;
			sh = mod->x11_FullScreenHeight;
		}

		swa_mask = CWColormap | CWEventMask;
		
		fn = mod->x11_fm.deffont;
		v->curfont = fn;
		mod->x11_fm.defref++;

		TInitList(&v->penlist);

		TInitList(&v->imsgqueue);
		v->imsgport = req->tvr_Op.OpenWindow.IMsgPort;

		v->sizehints = XAllocSizeHints();
		if (v->sizehints == TNULL)
			break;

		v->sizehints->flags = 0;

		v->title = (TSTRPTR)
			TGetTag(tags, TVisual_Title, (TTAG) "TEKlib visual");

		/* size/position calculation: */

		v->winwidth = (TINT) TGetTag(tags, TVisual_Width, DEF_WINWIDTH);
		v->winheight = (TINT) TGetTag(tags, TVisual_Height, DEF_WINHEIGHT);
		
		v->winwidth = TCLAMP(minw, v->winwidth, maxw);
		v->winheight = TCLAMP(minh, v->winheight, maxh);
		v->winwidth = TMIN(v->winwidth, sw);
		v->winheight = TMIN(v->winheight, sh);

		v->changevidmode = TFALSE;
		
		if (TGetTag(tags, TVisual_FullScreen, TFALSE))
		{
			#if defined(ENABLE_XVID)
			if (mod->x11_FullScreenWidth == 0)
			{
				XF86VidModeModeInfo **modes;
				int modeNum;
				int i;
				XF86VidModeGetAllModeLines(mod->x11_Display, 
					mod->x11_Screen, &modeNum, &modes);
				for (i = 0; i < modeNum; i++)
				{
					if ((modes[i]->hdisplay == v->winwidth) && 
						(modes[i]->vdisplay == v->winheight))
					{
						mod->x11_OldMode = *modes[0];
						mod->x11_VidMode = *modes[i];
						v->changevidmode = TTRUE;
						break;
					}
				}
				XFree(modes);
			}
			#endif
			
			if (!v->changevidmode)
			{
				v->winwidth = sw;
				v->winheight = sh;
			}
			v->winleft = 0;
			v->wintop = 0;			
			borderless = TTRUE;
			setfocus = TTRUE;
			mod->x11_FullScreen = TTRUE;
		}
		else
		{
			if (TGetTag(tags, TVisual_Center, TFALSE))
			{
				v->winleft = (sw - v->winwidth) / 2;
				v->wintop = (sh - v->winheight) / 2;
			}
			else
			{
				v->winleft = (int) TGetTag(tags, TVisual_WinLeft, (TTAG) -1);
				v->wintop = (int) TGetTag(tags, TVisual_WinTop, (TTAG) -1);
			}
			
			if (mod->x11_FullScreen)
			{
				borderless = TTRUE;
				if (!TGetTag(tags, TVisual_Borderless, TFALSE))
					setfocus = TTRUE;
				if (v->winleft == -1)
					v->winleft = (sw - v->winwidth) / 2;
				if (v->wintop == -1)
					v->wintop = (sh - v->winheight) / 2;
			}
		}
		
		if (v->winleft >= 0 || v->wintop >= 0)
			v->sizehints->flags |= USPosition | USSize;

		if (!borderless)
		{
			v->sizehints->min_width = (TINT)
				TGetTag(tags, TVisual_MinWidth, (TTAG) -1);
			v->sizehints->min_height = (TINT)
				TGetTag(tags, TVisual_MinHeight, (TTAG) -1);
			v->sizehints->max_width = (TINT)
				TGetTag(tags, TVisual_MaxWidth, (TTAG) -1);
			v->sizehints->max_height = (TINT)
				TGetTag(tags, TVisual_MaxHeight, (TTAG) -1);

			if (v->sizehints->max_width > 0)
				v->winwidth = TMIN(v->winwidth, v->sizehints->max_width);
			if (v->sizehints->max_height > 0)
				v->winheight = TMIN(v->winheight, v->sizehints->max_height);
			if (v->sizehints->min_width > 0)
				v->winwidth = TMAX(v->winwidth, v->sizehints->min_width);
			if (v->sizehints->min_height > 0)
				v->winheight = TMAX(v->winheight, v->sizehints->min_height);

			v->sizehints->min_width =
				v->sizehints->min_width <= 0 ? 1 : v->sizehints->min_width;
			v->sizehints->min_height =
				v->sizehints->min_height <= 0 ? 1 : v->sizehints->min_height;
			v->sizehints->max_width = v->sizehints->max_width <= 0 ?
				1000000 : v->sizehints->max_width;
			v->sizehints->max_height = v->sizehints->max_height <= 0 ?
				1000000 : v->sizehints->max_height;

			v->sizehints->flags |= PMinSize | PMaxSize;
		}

		v->winleft = TMAX(v->winleft, 0);
		v->wintop = TMAX(v->wintop, 0);

		if (borderless)
		{
			swa_mask |= CWOverrideRedirect;
			swa.override_redirect = True;
			/* is borderless, and area smaller than half screen area */
			save_under = v->winwidth * v->winheight < sw * sh / 2;
		}

		if (save_under)
		{
			swa_mask |= CWSaveUnder;
			swa.save_under = True;
		}
#if 0
		else
		{
			swa_mask |= CWBackingStore;
			swa.backing_store = True;
		}
#endif
		
		v->colormap = DefaultColormap(mod->x11_Display, mod->x11_Screen);
		if (v->colormap == TNULL)
			break;

		swa.colormap = v->colormap;

		v->base_mask = StructureNotifyMask | ExposureMask | FocusChangeMask;
		swa.event_mask = x11_seteventmask(mod, v,
			(TUINT) TGetTag(tags, TVisual_EventMask, 0));

		if (TGetTag(tags, TVisual_BlankCursor, TFALSE))
		{
			swa.cursor = mod->x11_NullCursor;
			swa_mask |= CWCursor;
		}
		#if defined(ENABLE_DEFAULTCURSOR)
		else
		{
			swa.cursor = mod->x11_DefaultCursor;
			swa_mask |= CWCursor;
		}
		#endif

		v->window = XCreateWindow(mod->x11_Display,
			RootWindow(mod->x11_Display, mod->x11_Screen),
			v->winleft, v->wintop, v->winwidth, v->winheight,
			0, CopyFromParent, CopyFromParent, CopyFromParent,
			swa_mask, &swa);

		if (v->window == TNULL)
			break;

		if (v->sizehints->flags)
			XSetWMNormalHints(mod->x11_Display, v->window, v->sizehints);

		if (Xutf8TextListToTextProperty(mod->x11_Display, 
			(char **) &v->title, 1, XUTF8StringStyle, &title_prop) ==
			Success)
		{
			XSetWMProperties(mod->x11_Display, v->window, &title_prop,
				NULL, NULL, 0, NULL, NULL, NULL);
			XFree(title_prop.value);
		}

		v->atom_wm_delete_win = XInternAtom(mod->x11_Display,
			"WM_DELETE_WINDOW", True);
		XSetWMProtocols(mod->x11_Display, v->window,
			&v->atom_wm_delete_win, 1);

		gcv.function = GXcopy;
		gcv.fill_style = FillSolid;
		gcv.graphics_exposures = True;
		gcv_mask = GCFunction | GCFillStyle | GCGraphicsExposures;

		v->gc = XCreateGC(mod->x11_Display, v->window, gcv_mask, &gcv);
		XCopyGC(mod->x11_Display,
			DefaultGC(mod->x11_Display, mod->x11_Screen),
			GCForeground | GCBackground, v->gc);

		XMapWindow(mod->x11_Display, v->window);

		#if defined(ENABLE_XFT)
		if (mod->x11_use_xft)
		{
			v->draw = (*mod->x11_xftiface.XftDrawCreate)(mod->x11_Display,
				v->window, mod->x11_Visual, v->colormap);
			if (!v->draw) break;
		}
		#endif

		v->bgpen = TVPEN_UNDEFINED;
		v->fgpen = TVPEN_UNDEFINED;

		#if defined(ENABLE_XVID)
		/* enter fullscreen vidmode? */
		if (v->changevidmode)
		{
			XF86VidModeSwitchToMode(mod->x11_Display, mod->x11_Screen,
				&mod->x11_VidMode);
			XF86VidModeSetViewPort(mod->x11_Display, mod->x11_Screen, 0, 0);
			XWarpPointer(mod->x11_Display, None, v->window,
				0, 0, 0, 0, 0, 0);
			XGrabPointer(mod->x11_Display, v->window, True, 
				ButtonPressMask, GrabModeAsync, GrabModeAsync,
				v->window, None, CurrentTime);
			mod->x11_FullScreenWidth = v->winwidth;
			mod->x11_FullScreenHeight = v->winheight;
		}
		#endif

		if (setfocus)
			XSetInputFocus(mod->x11_Display, v->window, RevertToParent,
				CurrentTime);

		TDBPRINTF(TDB_TRACE,("Created new window: %p\n", v->window));
		TAddTail(&mod->x11_vlist, &v->node);

		/* not yet mapped; register request in progress: */
		mod->x11_RequestInProgress = req;

		/* success: */
		return;
	}

	/* failure: */
	x11_closevisual(mod, req);
	req->tvr_Op.OpenWindow.Window = TNULL;
}

LOCAL void x11_closevisual(X11DISPLAY *mod, struct TVRequest *req)
{
	TAPTR TExecBase = TGetExecBase(mod);
	X11WINDOW *v = req->tvr_Op.OpenWindow.Window;
	struct X11Pen *pen;
	if (v == TNULL) return;

	#if defined(ENABLE_XVID)
	if (v->changevidmode)
	{
		XF86VidModeSwitchToMode(mod->x11_Display, mod->x11_Screen,
			&mod->x11_OldMode);
		XF86VidModeSetViewPort(mod->x11_Display, mod->x11_Screen, 0, 0);
		mod->x11_FullScreenWidth = 0;
		mod->x11_FullScreenHeight = 0;
	}
	#endif

	TRemove(&v->node);
	if (TISLISTEMPTY(&mod->x11_vlist))
	{
		/* last window closed - clear global fullscreen state */
		mod->x11_FullScreen = TFALSE;
	}

	x11_freeimage(mod, v);
	TFree(v->tempbuf);
	x11_releasesharedmemory(mod, v);

	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft && v->draw)
		(*mod->x11_xftiface.XftDrawDestroy)(v->draw);
	#endif

	if (v->window)
		XUnmapWindow(mod->x11_Display, v->window);
	if (v->gc)
		XFreeGC(mod->x11_Display, v->gc);
	if (v->window)
		XDestroyWindow(mod->x11_Display, v->window);

	while ((pen = (struct X11Pen *) TRemHead(&v->penlist)))
		freepen(mod, v, pen);

	if (v->colormap)
		XFreeColormap(mod->x11_Display, v->colormap);
	if (v->sizehints)
		XFree(v->sizehints);

	mod->x11_fm.defref--;

	TFree(v);
}

static int x11_seteventmask(X11DISPLAY *mod, X11WINDOW *v, TUINT eventmask)
{
	int x11_mask = v->base_mask;
	if (eventmask & TITYPE_REFRESH)
		x11_mask |= StructureNotifyMask | ExposureMask;
	if (eventmask & TITYPE_MOUSEOVER)
		x11_mask |= LeaveWindowMask | EnterWindowMask;
	if (eventmask & TITYPE_NEWSIZE)
		x11_mask |= StructureNotifyMask;
	if (eventmask & TITYPE_KEYDOWN)
		x11_mask |= KeyPressMask | KeyReleaseMask;
	if (eventmask & TITYPE_KEYUP)
		x11_mask |= KeyPressMask | KeyReleaseMask;
	if (eventmask & TITYPE_MOUSEMOVE)
		x11_mask |= PointerMotionMask | OwnerGrabButtonMask |
			ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
	if (eventmask & TITYPE_MOUSEBUTTON)
		x11_mask |= ButtonPressMask | ButtonReleaseMask | OwnerGrabButtonMask;
	v->eventmask = eventmask;
	return x11_mask;
}

LOCAL void x11_setinput(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.SetInput.Window;
	TUINT eventmask = req->tvr_Op.SetInput.Mask;
	XSelectInput(mod->x11_Display, v->window,
		x11_seteventmask(mod, v, eventmask));
	/* spool out possible remaining messages: */
	x11_sendimessages(mod, TFALSE);

}

LOCAL void x11_allocpen(X11DISPLAY *mod, struct TVRequest *req)
{
	TAPTR TExecBase = TGetExecBase(mod);
	X11WINDOW *v = req->tvr_Op.AllocPen.Window;
	TUINT rgb = req->tvr_Op.AllocPen.RGB;
	struct X11Pen *pen = TAlloc(mod->x11_MemMgr, sizeof(struct X11Pen));
	if (pen)
	{
		TUINT r = (rgb >> 16) & 0xff;
		TUINT g = (rgb >> 8) & 0xff;
		TUINT b = rgb & 0xff;
		
		r = (r << 8) | r;
		g = (g << 8) | g;
		b = (b << 8) | b;
		
		pen->color.red = r;
		pen->color.green = g;
		pen->color.blue = b;
		pen->color.flags = DoRed | DoGreen | DoBlue;
		if (XAllocColor(mod->x11_Display, v->colormap, &pen->color))
		{
			TBOOL success = TTRUE;
			#if defined(ENABLE_XFT)
			if (mod->x11_use_xft)
			{
				XRenderColor xrcolor;
				xrcolor.red = r;
				xrcolor.green = g;
				xrcolor.blue = b;
				xrcolor.alpha = 0xffff;
				success = (*mod->x11_xftiface.XftColorAllocValue)
					(mod->x11_Display, mod->x11_Visual, v->colormap, &xrcolor,
					&pen->xftcolor);
			}
			#endif
			if (success)
			{
				TAddTail(&v->penlist, &pen->node);
				req->tvr_Op.AllocPen.Pen = (TVPEN) pen;
				return;
			}
			XFreeColors(mod->x11_Display, v->colormap, &pen->color.pixel,
				1, 0);
		}
		TFree(pen);
	}
	req->tvr_Op.AllocPen.Pen = TVPEN_UNDEFINED;
}

static void freepen(X11DISPLAY *mod, X11WINDOW *v, struct X11Pen *pen)
{
	TAPTR TExecBase = TGetExecBase(mod);
	TRemove(&pen->node);
	XFreeColors(mod->x11_Display, v->colormap, &pen->color.pixel, 1, 0);
	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
		(*mod->x11_xftiface.XftColorFree)(mod->x11_Display, mod->x11_Visual,
			v->colormap, &pen->xftcolor);
	#endif
	TFree(pen);
}

LOCAL void x11_freepen(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.FreePen.Window;
	struct X11Pen *pen = (struct X11Pen *) req->tvr_Op.FreePen.Pen;
	freepen(mod, v, pen);
}

/*****************************************************************************/

static void setbgpen(X11DISPLAY *mod, X11WINDOW *v, TVPEN pen)
{
	if (pen != v->bgpen && pen != TVPEN_UNDEFINED)
	{
		XGCValues gcv;
		gcv.background = ((struct X11Pen *) pen)->color.pixel;
		XChangeGC(mod->x11_Display, v->gc, GCBackground, &gcv);
		v->bgpen = pen;
	}
}

static TVPEN setfgpen(X11DISPLAY *mod, X11WINDOW *v, TVPEN pen)
{
	TVPEN oldpen = v->fgpen;
	if (pen != oldpen && pen != TVPEN_UNDEFINED)
	{
		XGCValues gcv;
		gcv.foreground = ((struct X11Pen *) pen)->color.pixel;
		XChangeGC(mod->x11_Display, v->gc, GCForeground, &gcv);
		v->fgpen = pen;
		if (oldpen == (TVPEN) 0xffffffff) oldpen = pen;
	}
	return oldpen;
}

/*****************************************************************************/

#define OVERLAP(d0, d1, d2, d3, s0, s1, s2, s3) \
((s2) >= (d0) && (s0) <= (d2) && (s3) >= (d1) && (s1) <= (d3))

LOCAL void x11_frect(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.FRect.Window;
	TINT x0 = req->tvr_Op.FRect.Rect[0];
	TINT y0 = req->tvr_Op.FRect.Rect[1];
	TINT x1 = x0 + req->tvr_Op.FRect.Rect[2] - 1;
	TINT y1 = y0 + req->tvr_Op.FRect.Rect[3] - 1;
	if (!OVERLAP(x0, y0, x1, y1, 0, 0, v->winwidth - 1, v->winheight - 1))
		return;
	
	x0 = TMAX(x0, 0);
	y0 = TMAX(y0, 0);
	x1 = TMIN(x1, v->winwidth - 1);
	y1 = TMIN(y1, v->winheight - 1);
	
	setfgpen(mod, v, req->tvr_Op.FRect.Pen);
	XFillRectangle(mod->x11_Display, v->window, v->gc,
		x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

/*****************************************************************************/

LOCAL void x11_line(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Line.Window;
	TINT x0 = req->tvr_Op.Line.Rect[0];
	TINT y0 = req->tvr_Op.Line.Rect[1];
	TINT x1 = req->tvr_Op.Line.Rect[2];
	TINT y1 = req->tvr_Op.Line.Rect[3];
	if (!OVERLAP(x0, y0, x1, y1, 0, 0, v->winwidth - 1, v->winheight - 1))
		return;
	setfgpen(mod, v, req->tvr_Op.Line.Pen);
	XDrawLine(mod->x11_Display, v->window, v->gc,
		x0, y0, x1, y1);
}

/*****************************************************************************/

LOCAL void x11_rect(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Rect.Window;
	TINT x0 = req->tvr_Op.FRect.Rect[0];
	TINT y0 = req->tvr_Op.FRect.Rect[1];
	TINT x1 = x0 + req->tvr_Op.FRect.Rect[2] - 1;
	TINT y1 = y0 + req->tvr_Op.FRect.Rect[3] - 1;
	if (!OVERLAP(x0, y0, x1, y1, 0, 0, v->winwidth - 1, v->winheight - 1))
		return;
	setfgpen(mod, v, req->tvr_Op.Rect.Pen);
	XDrawRectangle(mod->x11_Display, v->window, v->gc,
		x0, y0, x1 - x0, y1 - y0);
}

/*****************************************************************************/

LOCAL void x11_plot(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Plot.Window;
	TUINT x0 = req->tvr_Op.Plot.Rect[0];
	TUINT y0 = req->tvr_Op.Plot.Rect[1];
	setfgpen(mod, v, req->tvr_Op.Plot.Pen);
	XDrawPoint(mod->x11_Display, v->window, v->gc, x0, y0);
}

/*****************************************************************************/

LOCAL void x11_drawstrip(X11DISPLAY *mod, struct TVRequest *req)
{
	TINT i;
	XPoint tri[3];
	X11WINDOW *v = req->tvr_Op.Strip.Window;
	TINT *array = req->tvr_Op.Strip.Array;
	TINT num = req->tvr_Op.Strip.Num;
	TTAGITEM *tags = req->tvr_Op.Strip.Tags;
	TVPEN *penarray = (TVPEN *) TGetTag(tags, TVisual_PenArray, TNULL);

	if (num < 3) return;

	if (penarray)
	{
		setfgpen(mod, v, penarray[2]);
	}
	else
	{
		TVPEN pen = (TVPEN) TGetTag(tags, TVisual_Pen, TVPEN_UNDEFINED);
		setfgpen(mod, v, pen);
	}

	tri[0].x = (TINT16) array[0];
	tri[0].y = (TINT16) array[1];
	tri[1].x = (TINT16) array[2];
	tri[1].y = (TINT16) array[3];
	tri[2].x = (TINT16) array[4];
	tri[2].y = (TINT16) array[5];

	XFillPolygon(mod->x11_Display, v->window, v->gc, tri, 3,
		Convex, CoordModeOrigin);

	for (i = 3; i < num; i++)
	{
		tri[0].x = tri[1].x;
		tri[0].y = tri[1].y;
		tri[1].x = tri[2].x;
		tri[1].y = tri[2].y;
		tri[2].x = (TINT16) array[i*2];
		tri[2].y = (TINT16) array[i*2+1];

		if (penarray)
			setfgpen(mod, v, penarray[i]);

		XFillPolygon(mod->x11_Display, v->window, v->gc, tri, 3,
			Convex, CoordModeOrigin);
	}
}

/*****************************************************************************/

LOCAL void x11_drawfan(X11DISPLAY *mod, struct TVRequest *req)
{
	TINT i;
	XPoint tri[3];
	X11WINDOW *v = req->tvr_Op.Fan.Window;
	TINT *array = req->tvr_Op.Fan.Array;
	TINT num = req->tvr_Op.Fan.Num;
	TTAGITEM *tags = req->tvr_Op.Fan.Tags;
	TVPEN pen = (TVPEN) TGetTag(tags, TVisual_Pen, TVPEN_UNDEFINED);
	TVPEN *penarray = (TVPEN *) TGetTag(tags, TVisual_PenArray, TNULL);

	if (num < 3) return;

	if (penarray)
		setfgpen(mod, v, penarray[2]);
	else
		setfgpen(mod, v, pen);

	tri[0].x = (TINT16) array[0];
	tri[0].y = (TINT16) array[1];
	tri[1].x = (TINT16) array[2];
	tri[1].y = (TINT16) array[3];
	tri[2].x = (TINT16) array[4];
	tri[2].y = (TINT16) array[5];

	XFillPolygon(mod->x11_Display, v->window, v->gc, tri, 3,
		Convex, CoordModeOrigin);

	for (i = 3; i < num; i++)
	{
		tri[1].x = tri[2].x;
		tri[1].y = tri[2].y;
		tri[2].x = (TINT16) array[i*2];
		tri[2].y = (TINT16) array[i*2+1];

		if (penarray)
			setfgpen(mod, v, penarray[i]);

		XFillPolygon(mod->x11_Display, v->window, v->gc, tri, 3,
			Convex, CoordModeOrigin);
	}
}

/*****************************************************************************/

LOCAL void x11_copyarea(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.CopyArea.Window;
	TINT x = req->tvr_Op.CopyArea.Rect[0];
	TINT y = req->tvr_Op.CopyArea.Rect[1];
	TINT w = req->tvr_Op.CopyArea.Rect[2];
	TINT h = req->tvr_Op.CopyArea.Rect[3];
	TINT dx = req->tvr_Op.CopyArea.DestX;
	TINT dy = req->tvr_Op.CopyArea.DestY;

	XCopyArea(mod->x11_Display, v->window, v->window, v->gc,
		x, y, w, h, dx, dy);

	mod->x11_CopyExposeHook = (struct THook *)
		TGetTag(req->tvr_Op.CopyArea.Tags, TVisual_ExposeHook, TNULL);
	if (mod->x11_CopyExposeHook)
	{
		/* register request in progress: */
		mod->x11_RequestInProgress = req;
	}
}

/*****************************************************************************/

LOCAL void x11_setcliprect(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.ClipRect.Window;
	TINT x = req->tvr_Op.ClipRect.Rect[0];
	TINT y = req->tvr_Op.ClipRect.Rect[1];
	TINT w = req->tvr_Op.ClipRect.Rect[2];
	TINT h = req->tvr_Op.ClipRect.Rect[3];
	Region region;
	XRectangle rectangle;

	region = XCreateRegion();

	rectangle.x = (short) x;
	rectangle.y = (short) y;
	rectangle.width = (unsigned short) w;
	rectangle.height = (unsigned short) h;

	/* union rect into region */
	XUnionRectWithRegion(&rectangle, region, region);
	/* set clip region */
	XSetRegion(mod->x11_Display, v->gc, region);

	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
		(*mod->x11_xftiface.XftDrawSetClip)(v->draw, region);
	#endif

	XDestroyRegion(region);
}

/*****************************************************************************/

LOCAL void x11_unsetcliprect(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.ClipRect.Window;
	/*XSetClipMask(mod->x11_Display, v->gc, None);*/
	XSetRegion(mod->x11_Display, v->gc, mod->x11_HugeRegion);
	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
		(*mod->x11_xftiface.XftDrawSetClip)(v->draw, mod->x11_HugeRegion);
	#endif
}

/*****************************************************************************/

LOCAL void x11_clear(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Clear.Window;
	setfgpen(mod, v, req->tvr_Op.Clear.Pen);
	XFillRectangle(mod->x11_Display, v->window, v->gc,
		0, 0, v->winwidth, v->winheight);
}

/*****************************************************************************/

static THOOKENTRY TTAG getattrfunc(struct THook *hook, TAPTR obj, TTAG msg)
{
	struct attrdata *data = hook->thk_Data;
	TTAGITEM *item = obj;
	X11WINDOW *v = data->v;

	switch (item->tti_Tag)
	{
		default:
			return TTRUE;
		case TVisual_UserData:
			*((TTAG *) item->tti_Value) = v->userdata;
			break;
		case TVisual_Width:
			*((TINT *) item->tti_Value) = v->winwidth;
			break;
		case TVisual_Height:
			*((TINT *) item->tti_Value) = v->winheight;
			break;
		case TVisual_WinLeft:
			*((TINT *) item->tti_Value) = v->winleft;
			break;
		case TVisual_WinTop:
			*((TINT *) item->tti_Value) = v->wintop;
			break;
		case TVisual_MinWidth:
			*((TINT *) item->tti_Value) = v->sizehints->min_width;
			break;
		case TVisual_MinHeight:
			*((TINT *) item->tti_Value) = v->sizehints->min_height;
			break;
		case TVisual_MaxWidth:
			*((TINT *) item->tti_Value) = v->sizehints->max_width;
			break;
		case TVisual_MaxHeight:
			*((TINT *) item->tti_Value) = v->sizehints->max_height;
			break;
		case TVisual_HaveClipboard:
		{
			X11DISPLAY *mod = data->mod;
			*((TINT *) item->tti_Value) = XGetSelectionOwner(mod->x11_Display, 
				mod->x11_XA_CLIPBOARD) == v->window;
			break;
		}
		case TVisual_HaveSelection:
		{
			X11DISPLAY *mod = data->mod;
			*((TINT *) item->tti_Value) = XGetSelectionOwner(mod->x11_Display, 
				mod->x11_XA_PRIMARY) == v->window;
			break;
		}
		case TVisual_Device:
			*((TAPTR *) item->tti_Value) = data->mod;
			break;
		case TVisual_Window:
			*((TAPTR *) item->tti_Value) = v;
			break;
	}
	data->num++;
	return TTRUE;
}

static THOOKENTRY TTAG setattrfunc(struct THook *hook, TAPTR obj, TTAG msg)
{
	struct attrdata *data = hook->thk_Data;
	TTAGITEM *item = obj;
	X11WINDOW *v = data->v;

	switch (item->tti_Tag)
	{
		default:
			return TTRUE;
		case TVisual_Width:
			data->neww = (TINT) item->tti_Value;
			break;
		case TVisual_Height:
			data->newh = (TINT) item->tti_Value;
			break;
		case TVisual_MinWidth:
			v->sizehints->min_width = (TINT) item->tti_Value;
			break;
		case TVisual_MinHeight:
			v->sizehints->min_height = (TINT) item->tti_Value;
			break;
		case TVisual_MaxWidth:
			v->sizehints->max_width = (TINT) item->tti_Value;
			break;
		case TVisual_MaxHeight:
			v->sizehints->max_height = (TINT) item->tti_Value;
			break;
		case TVisual_HaveSelection:
		{
			X11DISPLAY *mod = data->mod;
			XSetSelectionOwner(mod->x11_Display, mod->x11_XA_PRIMARY,
				item->tti_Value ? v->window : None, CurrentTime);
			break;
		}
		case TVisual_HaveClipboard:
		{
			X11DISPLAY *mod = data->mod;
			XSetSelectionOwner(mod->x11_Display, mod->x11_XA_CLIPBOARD,
				item->tti_Value ? v->window : None, CurrentTime);
			break;
		}
	}
	data->num++;
	return TTRUE;
}

/*****************************************************************************/

LOCAL void x11_getattrs(X11DISPLAY *mod, struct TVRequest *req)
{
	struct attrdata data;
	struct THook hook;

	data.v = req->tvr_Op.GetAttrs.Window;
	data.num = 0;
	data.mod = mod;
	TInitHook(&hook, getattrfunc, &data);

	TForEachTag(req->tvr_Op.GetAttrs.Tags, &hook);
	req->tvr_Op.GetAttrs.Num = data.num;
}

/*****************************************************************************/

LOCAL void x11_setattrs(X11DISPLAY *mod, struct TVRequest *req)
{
	struct attrdata data;
	struct THook hook;
	X11WINDOW *v = req->tvr_Op.SetAttrs.Window;
	TINT neww, newh;

	data.v = v;
	data.num = 0;
	data.mod = mod;
	data.neww = -1;
	data.newh = -1;
	TInitHook(&hook, setattrfunc, &data);

	TForEachTag(req->tvr_Op.SetAttrs.Tags, &hook);
	req->tvr_Op.SetAttrs.Num = data.num;

	if (v->sizehints->max_width < 0) v->sizehints->max_width = 1000000;
	if (v->sizehints->max_height < 0) v->sizehints->max_height = 1000000;

	v->sizehints->min_width = TMAX(v->sizehints->min_width, 0);
	v->sizehints->max_width = TMAX(v->sizehints->max_width,
		v->sizehints->min_width);
	v->sizehints->min_height = TMAX(v->sizehints->min_height, 0);
	v->sizehints->max_height = TMAX(v->sizehints->max_height,
		v->sizehints->min_height);

	neww = data.neww < 0 ? v->winwidth : data.neww;
	newh = data.newh < 0 ? v->winheight : data.newh;
	if (neww < v->sizehints->min_width || newh < v->sizehints->min_height)
	{
		neww = TMAX(neww, v->sizehints->min_width);
		newh = TMAX(newh, v->sizehints->min_height);
		XResizeWindow(mod->x11_Display, v->window, neww, newh);
		mod->x11_RequestInProgress = req;
		v->waitforresize = TTRUE;
	}

	XSetWMNormalHints(mod->x11_Display, v->window, v->sizehints);
}

/*****************************************************************************/

LOCAL void x11_drawtext(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Text.Window;
	TSTRPTR text = req->tvr_Op.Text.Text;
	TINT len = req->tvr_Op.Text.Length;
	TUINT x = req->tvr_Op.Text.X;
	TUINT y = req->tvr_Op.Text.Y;
	struct X11Pen *fgpen = (struct X11Pen *) req->tvr_Op.Text.FgPen;
	setfgpen(mod, v, (TVPEN) fgpen);

	#if defined(ENABLE_XFT)
	if (mod->x11_use_xft)
	{
		XftFont *f = ((struct FontNode *) v->curfont)->xftfont;
		(*mod->x11_xftiface.XftDrawStringUtf8)(v->draw, &fgpen->xftcolor,
			f, x, y + f->ascent, (FcChar8 *)text, len);
	}
	else
	#endif
	{
		TSTRPTR latin = x11_utf8tolatin(mod, text, len, &len);
		if (latin)
		{
			XFontStruct *f = ((struct FontNode *) v->curfont)->font;
			XDrawString(mod->x11_Display, v->window, v->gc,
				x, y + f->ascent, (char *) latin, len);
		}
	}
}

/*****************************************************************************/

LOCAL void x11_openfont(X11DISPLAY *mod, struct TVRequest *req)
{
	req->tvr_Op.OpenFont.Font =
		x11_hostopenfont(mod, req->tvr_Op.OpenFont.Tags);
}

/*****************************************************************************/

LOCAL void x11_textsize(X11DISPLAY *mod, struct TVRequest *req)
{
	req->tvr_Op.TextSize.Width =
		x11_hosttextsize(mod, req->tvr_Op.TextSize.Font,
			req->tvr_Op.TextSize.Text, req->tvr_Op.TextSize.NumChars);
}

/*****************************************************************************/

LOCAL void x11_getfontattrs(X11DISPLAY *mod, struct TVRequest *req)
{
	struct attrdata data;
	struct THook hook;

	data.mod = mod;
	data.font = req->tvr_Op.GetFontAttrs.Font;
	data.num = 0;
	TInitHook(&hook, x11_hostgetfattrfunc, &data);

	TForEachTag(req->tvr_Op.GetFontAttrs.Tags, &hook);
	req->tvr_Op.GetFontAttrs.Num = data.num;
}

/*****************************************************************************/

LOCAL void x11_setfont(X11DISPLAY *mod, struct TVRequest *req)
{
	x11_hostsetfont(mod, req->tvr_Op.SetFont.Window,
		req->tvr_Op.SetFont.Font);
}

/*****************************************************************************/

LOCAL void x11_closefont(X11DISPLAY *mod, struct TVRequest *req)
{
	x11_hostclosefont(mod, req->tvr_Op.CloseFont.Font);
}

/*****************************************************************************/

LOCAL void x11_queryfonts(X11DISPLAY *mod, struct TVRequest *req)
{
	req->tvr_Op.QueryFonts.Handle =
		x11_hostqueryfonts(mod, req->tvr_Op.QueryFonts.Tags);
}

/*****************************************************************************/

LOCAL void x11_getnextfont(X11DISPLAY *mod, struct TVRequest *req)
{
	req->tvr_Op.GetNextFont.Attrs =
		x11_hostgetnextfont(mod, req->tvr_Op.GetNextFont.Handle);
}

/*****************************************************************************/

struct drawdata
{
	X11WINDOW *v;
	X11DISPLAY *mod;
	Display *display;
	Window window;
	GC gc;
	TINT x0, x1, y0, y1;
};

static THOOKENTRY TTAG drawtagfunc(struct THook *hook, TAPTR obj, TTAG msg)
{
	struct drawdata *data = hook->thk_Data;
	TTAGITEM *item = obj;

	switch (item->tti_Tag)
	{
		case TVisualDraw_X0:
			data->x0 = item->tti_Value;
			break;
		case TVisualDraw_Y0:
			data->y0 = item->tti_Value;
			break;
		case TVisualDraw_X1:
			data->x1 = item->tti_Value;
			break;
		case TVisualDraw_Y1:
			data->y1 = item->tti_Value;
			break;
		case TVisualDraw_NewX:
			data->x0 = data->x1;
			data->x1 = item->tti_Value;
			break;
		case TVisualDraw_NewY:
			data->y0 = data->y1;
			data->y1 = item->tti_Value;
			break;
		case TVisualDraw_FgPen:
			setfgpen(data->mod, data->v, item->tti_Value);
			break;
		case TVisualDraw_BgPen:
			setbgpen(data->mod, data->v, item->tti_Value);
			break;
		case TVisualDraw_Command:
			switch (item->tti_Value)
			{
				case TVCMD_FRECT:
					XFillRectangle(data->display, data->window, data->gc,
						data->x0, data->y0, data->x1, data->y1);
					break;
				case TVCMD_RECT:
					XDrawRectangle(data->display, data->window, data->gc,
						data->x0, data->y0, data->x1 - 1, data->y1 - 1);
					break;
				case TVCMD_LINE:
					XDrawLine(data->display, data->window, data->gc,
						data->x0, data->y0, data->x1, data->y1);
					break;
			}
			break;
	}
	return TTRUE;
}

LOCAL void x11_drawtags(X11DISPLAY *mod, struct TVRequest *req)
{
	struct THook hook;
	struct drawdata data;
	data.v = req->tvr_Op.DrawTags.Window;
	data.mod = mod;
	data.display = mod->x11_Display;
	data.window = data.v->window;
	data.gc = data.v->gc;

	TInitHook(&hook, drawtagfunc, &data);
	TForEachTag(req->tvr_Op.DrawTags.Tags, &hook);
}

/*****************************************************************************/

static void x11_freeimage(X11DISPLAY *mod, X11WINDOW *v)
{
	if (v->image)
	{
		v->image->data = NULL;
		XDestroyImage(v->image);
		v->image = TNULL;
	}
}

static XImage *x11_getdrawimage(X11DISPLAY *mod, X11WINDOW *v, TINT w, TINT h,
	TUINT8 **bufptr, TINT *bytes_per_line)
{
	if (w <= 0 || h <= 0 || mod->x11_Depth <= 0)
		return TNULL;
	
	while (!v->image || w != v->imw || h != v->imh)
	{
		x11_freeimage(mod, v);
		
		if (mod->x11_ShmAvail)
		{
			/* TODO: buffer more images, not just 1 */
			v->image = XShmCreateImage(mod->x11_Display, mod->x11_Visual,
				mod->x11_Depth, ZPixmap, TNULL, &v->shminfo, w, h);
			if (v->image)
			{
				v->image->data = x11_getsharedmemory(mod, v, 
					v->image->bytes_per_line * v->image->height);
				if (v->image->data)
				{
					v->imw = w;
					v->imh = h;
					v->image_shm = TTRUE;
					break;
				}
				x11_freeimage(mod, v);
			}
		}

		if (!v->image)
		{
			TAPTR TExecBase = TGetExecBase(mod);
			
			if (v->tempbuf) 
				TFree(v->tempbuf);
			v->tempbuf = TAlloc(TNULL, w * h * mod->x11_BPP);
			if (v->tempbuf)
			{
				v->image = XCreateImage(mod->x11_Display, mod->x11_Visual,
					mod->x11_Depth, ZPixmap, 0, NULL, w, h, mod->x11_BPP * 8, 
					mod->x11_BPP * w);
				if (v->image)
				{
					v->image->data = v->tempbuf;
					v->imw = w;
					v->imh = h;
					break;
				}
				TFree(v->tempbuf);
				v->tempbuf = TNULL;
			}
		}
		
		return TNULL;
	}

	if (v->tempbuf)
	{
		*bufptr = (TUINT8 *) v->tempbuf;
		*bytes_per_line = v->imw * mod->x11_BPP;
	}
	else
	{
		*bufptr = (TUINT8 *) v->image->data;
		*bytes_per_line = v->image->bytes_per_line;
	}
	return v->image;
}

static void x11_putimage(X11DISPLAY *mod, X11WINDOW *v, struct TVRequest *req,
	TINT x0, TINT y0, TINT w, TINT h)
{
	if (v->image_shm)
	{
		XShmPutImage(mod->x11_Display, v->window, v->gc, v->image, 0, 0,
			x0, y0, w, h, 1);
		mod->x11_RequestInProgress = req;
	}
	else
		XPutImage(mod->x11_Display, v->window, v->gc, v->image, 0, 0,
			x0, y0, w, h);
}

static void x11_drawbuffer_argb32(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.DrawBuffer.Window;
	TINT x0 = req->tvr_Op.DrawBuffer.RRect[0];
	TINT y0 = req->tvr_Op.DrawBuffer.RRect[1];
	TINT w = req->tvr_Op.DrawBuffer.RRect[2];
	TINT h = req->tvr_Op.DrawBuffer.RRect[3];
	TINT totw = req->tvr_Op.DrawBuffer.TotWidth;
	TAPTR buf = req->tvr_Op.DrawBuffer.Buf;
	int xx, yy;
	TUINT p;
	TUINT *sp = buf;
	TUINT8 *dp;
	TINT dtw;

	if (!x11_getdrawimage(mod, v, w, h, &dp, &dtw))
		return;

	switch (mod->x11_DstFmt)
	{
		default:
			TDBPRINTF(TDB_ERROR,("Cannot render to screen mode\n"));
			break;

		case (24 << 8) + (0 << 1) + 0:
		case (32 << 8) + (0 << 1) + 0:
			if (dtw == totw * 4 && !v->image_shm)
			{
				v->image->data = (char *) buf;
			}
			else
			{
				for (yy = 0; yy < h; ++yy)
				{
					memcpy(dp, sp, w * 4);
					sp += totw;
					dp += dtw;
				}
			}
			break;

		case (15 << 8) + (0 << 1) + 0:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					((TUINT16 *)dp)[xx] = ((p & 0xf80000) >> 9) |
						((p & 0x00f800) >> 6) |
						((p & 0x0000f8) >> 3);

				}
				sp += totw;
				dp += dtw;
			}
			break;

		case (15 << 8) + (0 << 1) + 1:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					/*		24->15 bit, host-swapped
					**		........rrrrrrrrGGggggggbbbbbbbb
					** ->	................gggbbbbb0rrrrrGG */
					((TUINT16 *)dp)[xx] = ((p & 0xf80000) >> 17) |
						((p & 0x00c000) >> 14) |
						((p & 0x003800) << 2) |
						((p & 0x0000f8) << 5);
				}
				sp += totw;
				dp += dtw;
			}
			break;

		case (16 << 8) + (0 << 1) + 0:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					((TUINT16 *)dp)[xx] = ((p & 0xf80000) >> 8) |
						((p & 0x00fc00) >> 5) |
						((p & 0x0000f8) >> 3);

				}
				sp += totw;
				dp += dtw;
			}
			break;

		case (16 << 8) + (0 << 1) + 1:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					/*		24->16 bit, host-swapped
					**		........rrrrrrrrGGGgggggbbbbbbbb
					** ->	................gggbbbbbrrrrrGGG */
					((TUINT16 *) dp)[xx] = ((p & 0xf80000) >> 16) |
						((p & 0x0000f8) << 5) |
						((p & 0x00e000) >> 13) |
						((p & 0x001c00) << 3);
				}
				sp += totw;
				dp += dtw;
			}
			break;

		case (24 << 8) + (0 << 1) + 1:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					/*	24->24 bit, host-swapped */
					((TUINT *) dp)[xx] = ((p & 0x00ff0000) >> 8) |
						((p & 0x0000ff00) << 8) |
						((p & 0x000000ff) << 24);
				}
				sp += totw;
				dp += dtw;
			}
			break;
	}

	x11_putimage(mod, v, req, x0, y0, w, h);
}


static void x11_drawbuffer_rgb16(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.DrawBuffer.Window;
	TINT x0 = req->tvr_Op.DrawBuffer.RRect[0];
	TINT y0 = req->tvr_Op.DrawBuffer.RRect[1];
	TINT w = req->tvr_Op.DrawBuffer.RRect[2];
	TINT h = req->tvr_Op.DrawBuffer.RRect[3];
	TINT totw = req->tvr_Op.DrawBuffer.TotWidth;
	TAPTR buf = req->tvr_Op.DrawBuffer.Buf;
	int xx, yy;
	TUINT p;
	TUINT16 *sp = buf;
	TUINT8 *dp;
	TINT dtw;

	if (!x11_getdrawimage(mod, v, w, h, &dp, &dtw))
		return;
	
	switch (mod->x11_DstFmt)
	{
		default:
			TDBPRINTF(TDB_ERROR,("Render to screen mode not implemented\n"));
			break;
			
		case (16 << 8) + (0 << 1) + 0:
			for (yy = 0; yy < h; ++yy)
			{
				// 		...c...8...4...0
				// 		.bbbbbgggggrrrrr
				//	->	rrrrrgggggGbbbbb
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					/*	16->16 bit */
					((TUINT16 *) dp)[xx] = 
						((p & 0x7c00) >> 10) |
						((p & 0x03e0) << 1) |
						((p & 0x001f) << 11);
				}
				sp += totw;
				dp += dtw;
			}
			break;
			
		case (24 << 8) + (0 << 1) + 0:
			for (yy = 0; yy < h; ++yy)
			{
				for (xx = 0; xx < w; xx++)
				{
					p = sp[xx];
					/*	16->24 bit, not swapped */
					((TUINT *) dp)[xx] = 
						((p & 0x1f) << 19) | ((p & 0x1c) << 14) |
						((p & 0x3e0) << 6) | ((p & 0x380) << 1) |
						((p & 0x7c00) >> 7) | ((p & 0x7000) >> 12);
				}
				sp += totw;
				dp += dtw;
			}
			break;
	}
	
	x11_putimage(mod, v, req, x0, y0, w, h);
}


LOCAL void x11_drawbuffer(X11DISPLAY *mod, struct TVRequest *req)
{
	TINT x0 = req->tvr_Op.DrawBuffer.RRect[0];
	TINT y0 = req->tvr_Op.DrawBuffer.RRect[1];
	TINT w = req->tvr_Op.DrawBuffer.RRect[2];
	TINT h = req->tvr_Op.DrawBuffer.RRect[3];
	TINT totw = req->tvr_Op.DrawBuffer.TotWidth;
	if (x0 < 0 || y0 < 0 || w <= 0 || h <= 0 || w > totw)
	{
		TDBPRINTF(TDB_WARN,("illegal arguments to x11_drawbuffer: x0=%d y0=%d w=%d h=%d tw=%d\n", x0, y0, w, h, totw));
		return;
	}
	
	TUINT pixfmt = TGetTag(req->tvr_Op.DrawBuffer.Tags, 
		TVisual_PixelFormat, TVPIXFMT_ARGB32);
	switch (pixfmt)
	{
		case TVPIXFMT_ARGB32:
			x11_drawbuffer_argb32(mod, req);
			break;
		case TVPIXFMT_RGB16:
			x11_drawbuffer_rgb16(mod, req);
			break;
	}
}


/*****************************************************************************/
/*
**	getselection:
*/

LOCAL void x11_getselection(X11DISPLAY *mod, struct TVRequest *req)
{
	X11WINDOW *v = req->tvr_Op.Rect.Window;
	TAPTR TExecBase = TGetExecBase(mod);
	Display *display = mod->x11_Display;
	Window window = v->window;
	TUINT8 *clip = NULL;
	
	Atom selatom = req->tvr_Op.GetSelection.Type == 2 ? 
		mod->x11_XA_PRIMARY : mod->x11_XA_CLIPBOARD;
	
	Window selectionowner = XGetSelectionOwner(display, selatom);
	
	req->tvr_Op.GetSelection.Data = TNULL;
	req->tvr_Op.GetSelection.Length = 0;
	
	if (selectionowner != None && selectionowner != window)
	{
		unsigned char *data = NULL;
		Atom type;
		int format;
		unsigned long len, bytesleft;
	
		XConvertSelection(display, selatom, mod->x11_XA_UTF8_STRING,
			selatom, window, CurrentTime);
		XFlush(display);
		for (;;)
		{
			XEvent evt;
			if (XCheckTypedEvent(display, SelectionNotify, &evt))
			{
				if (evt.xselection.requestor == window)
					break;
			}
		}
		
		XGetWindowProperty(display, window, selatom, 0, 0, False,
			AnyPropertyType, &type, &format, &len, &bytesleft, &data);
	
		if (data)
		{
			XFree(data);
			data = NULL;		
		}
		
		if (bytesleft)
		{
			if (XGetWindowProperty(display, window, selatom, 0, 
				bytesleft, False, AnyPropertyType,
				&type, &format, &len, &bytesleft, &data) == Success)
			{
				clip = TAlloc(TNULL, len);
				if (clip)
				{
					TCopyMem(data, clip, len);
					req->tvr_Op.GetSelection.Data = clip;
					req->tvr_Op.GetSelection.Length = len;
				}
				XFree(data);
			}
		}
		
		XDeleteProperty(display, window, selatom);
	}
}
