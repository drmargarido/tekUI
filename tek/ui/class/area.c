/*-----------------------------------------------------------------------------
--
--	tek.ui.class.area
--	Written by Timm S. Mueller <tmueller at schulze-mueller.de>
--	See copyright notice in COPYRIGHT
--
--	OVERVIEW::
--		[[#ClassOverview]] :
--		[[#tek.class : Class]] /
--		[[#tek.class.object : Object]] /
--		[[#tek.ui.class.element : Element]] / 
--		Area ${subclasses(Area)}
--
--		This is the base class of all visible user interface elements.
--		It implements an outer margin, layouting, drawing, and the
--		relationships to its neighbour elements.
--
--	ATTRIBUTES::
--		- {{AutoPosition [I]}} (boolean)
--			When the element receives the focus, this flag instructs it to
--			automatically position itself into the visible area of any Canvas
--			that may contain it. An affected [[#tek.ui.class.canvas : Canvas]]
--			must have its {{AutoPosition}} attribute enabled as well for this
--			option to take effect (but unlike the Area class, in a Canvas it
--			is disabled by default).
--			The boolean (default '''true''') will be translated to the flag
--			{{FL_AUTOPOSITION}}, and is meaningless after initialization.
--		- {{BGPen [G]}} (color specification)
--			The current color (or texture) for painting the element's
--			background. This value is set in Area:setState(), where it is
--			derived from the element's current state and the
--			''background-color'' style property. Valid are color names (e.g. 
--			{{"detail"}}, {{"fuchsia"}}, see also
--			[[#tek.ui.class.display : Display]] for more), a hexadecimal RGB
--			specification (e.g. {{"#334455"}}, {{"#f0f"}}), or an image URL
--			in the form {{"url(...)"}}.
--		- {{DamageRegion [G]}} ([[#tek.lib.region : Region]])
--			see {{TrackDamage}}
--		- {{Disabled [ISG]}} (boolean)
--			If '''true''', the element is in disabled state and loses its
--			ability to interact with the user. This state variable is handled
--			in the [[#tek.ui.class.widget : Widget]] class.
--		- {{EraseBG [I]}} (boolean)
--			If '''true''', the element's background is painted automatically
--			using the Area:erase() method. Set this attribute to '''false'''
--			if you intend to paint the background yourself in Area:draw().
--			The boolean (default '''true''') will be translated to the flag
--			{{FL_ERASEBG}}, and is meaningless after initialization.
--		- {{Flags [SG]}} (Flags field)
--			This attribute holds various status flags, among others:
--			- {{FL_SETUP}} - Set in Area:setup() and cleared in Area:cleanup()
--			- {{FL_LAYOUT}} - Set in Area:layout(), cleared in Area:cleanup()
--			- {{FL_SHOW}} - Set in Area:show(), cleared in Area:hide()
--			- {{FL_REDRAW}} - Set in Area:layout(), Area:damage(),
--			Area:setState() and possibly other places to indicate that the
--			element needs to be repainted. Cleared in Area:draw().
--			- {{FL_REDRAWBORDER}} - To indicate a repaint of the element's
--			borders. Handled in the [[#tek.ui.class.frame : Frame]] class.
--			- {{FL_CHANGED}} - This flag indicates that the contents of an
--			element have changed, i.e. when children were added to a group,
--			or when setting a new text or image should cause a recalculation
--			of its size.
--			- {{FL_POPITEM}} - Used to identify elements in popups, handled in
--			[[#tek.ui.class.popitem : PopItem]].
--		- {{Focus [SG]}} (boolean)
--			If '''true''', the element has the input focus. This state variable
--			is handled by the [[#tek.ui.class.widget : Widget]] class. Note:
--			This attribute represents only the current state. If you want to
--			place the initial focus on an element, use the {{InitialFocus}}
--			attribute in the [[#tek.ui.class.widget : Widget]] class.
--		- {{HAlign [IG]}} ({{"left"}}, {{"center"}}, {{"right"}})
--			Horizontal alignment of the element in its group. This attribute
--			can be controlled using the {{halign}} style property.
--		- {{Height [IG]}} (number, {{"auto"}}, {{"fill"}}, {{"free"}})
--			Height of the element, in pixels, or
--				- {{"auto"}} - Reserves the minimum required
--				- {{"free"}} - Allows the element's height to grow to any size.
--				- {{"fill"}} - To fill up the height that other elements in the
--				same group have claimed, without claiming more.
--			This attribute can be controlled using the {{height}} style
--			property.
--		- {{Hilite [SG]}} (boolean)
--			If '''true''', the element is in highlighted state. This
--			state variable is handled by the [[#tek.ui.class.widget : Widget]]
--			class.
--		- {{MaxHeight [IG]}} (number or {{"none"}})
--			Maximum height of the element, in pixels [default: {{"none"}}].
--			This attribute is controllable via the {{max-height}} style
--			property.
--		- {{MaxWidth [IG]}} (number or {{"none"}})
--			Maximum width of the element, in pixels [default: {{"none"}}].
--			This attribute is controllable via the {{max-width}} style
--			property.
--		- {{MinHeight [IG]}} (number)
--			Minimum height of the element, in pixels [default: {{0}}].
--			This attribute is controllable via the {{min-height}} style
--			property.
--		- {{MinWidth [IG]}} (number)
--			Minimum width of the element, in pixels [default: {{0}}].
--			This attribute is controllable via the {{min-width}} style
--			property.
--		- {{Selected [ISG]}} (boolean)
--			If '''true''', the element is in selected state. This state
--			variable is handled by the [[#tek.ui.class.widget : Widget]] class.
--		- {{TrackDamage [I]}} (boolean)
--			If '''true''', the element collects intra-area damages in a
--			[[#tek.lib.region : Region]] named {{DamageRegion}}, which can be
--			used by class writers to implement minimally invasive repaints.
--			Default: '''false''', the element is repainted in its entirety.
--			The boolean will be translated to the flag {{FL_TRACKDAMAGE}} and
--			is meaningless after initialization.
--		- {{VAlign [IG]}} ({{"top"}}, {{"center"}}, {{"bottom"}})
--			Vertical alignment of the element in its group. This attribute
--			can be controlled using the {{valign}} style property.
--		- {{Weight [IG]}} (number)
--			Specifies the weight that is attributed to the element relative
--			to its siblings in the same group. By recommendation, the weights
--			in a group should sum up to 0x10000.
--		- {{Width [IG]}} (number, {{"auto"}}, {{"fill"}}, {{"free"}})
--			Width of the element, in pixels, or
--				- {{"auto"}} - Reserves the minimum required
--				- {{"free"}} - Allows the element's width to grow to any size
--				- {{"fill"}} - To fill up the width that other elements in the
--				same group have claimed, without claiming more.
--			This attribute can be controlled using the {{width}} style
--			property.
--
--	STYLE PROPERTIES::
--		''background-attachment'' || {{"scollable"}} or {{"fixed"}}
--		''background-color'' || Controls {{Area.BGPen}}
--		''fixed'' || coordinates used by the fixed layouter: {{"x0 y0 x1 y1"}}
--		''halign'' || controls the {{Area.HAlign}} attribute
--		''height'' || controls the {{Area.Height}} attribute
--		''margin-bottom'' || the element's bottom margin, in pixels
--		''margin-left'' || the element's left margin, in pixels
--		''margin-right'' || the element's right margin, in pixels
--		''margin-top'' || the element's top margin, in pixels
--		''max-height'' || controls the {{Area.MaxHeight}} attribute
--		''max-width'' || controls the {{Area.MaxWidth}} attribute
--		''min-height'' || controls the {{Area.MinHeight}} attribute
--		''min-width'' || controls the {{Area.MinWidth}} attribute
--		''padding-bottom'' || the element's bottom padding
--		''padding-left'' || the element's left padding
--		''padding-right'' || the element's right padding
--		''padding-top'' || the element's top padding
--		''valign'' || controls the {{Area.VAlign}} attribute
--		''width'' || controls the {{Area.Width}} attribute
--
--		Note that repainting elements with a {{"fixed"}}
--		''background-attachment'' can be expensive. This variant should be
--		used sparingly, and some classes may implement it incompletely or
--		incorrectly.
--
--	IMPLEMENTS::
--		- Area:askMinMax() - Queries the element's minimum and maximum size
--		- Area:checkClearFlags() - Check and clear an element's flags
--		- Area:checkFlags() - Check an element's flags
--		- Area:checkFocus() - Checks if the element can receive the input focus
--		- Area:checkHilite() - Checks if the element can be highlighted
--		- Area:damage() - Notifies the element of a damage
--		- Area:draw() - Paints the element
--		- Area:drawBegin() - Prepares the rendering context
--		- Area:drawEnd() - Reverts the changes made in drawBegin()
--		- Area:erase() - Erases the element's background
--		- Area:focusRect() - Makes the element fully visible, if possible
--		- Area:getBG() - Gets the element's background properties
--		- Area:getBGElement() - Gets the element's background element
--		- Area:getByXY() - Checks if the element covers a coordinate
--		- Area:getChildren() - Gets the element's children
--		- Area:getDisplacement(): Get this element's coordinate displacement
--		- Area:getGroup() - Gets the element's group
--		- Area:getMargin() - Gets the element's margin
--		- Area:getMinMax() - Gets the element's calculated min/max sizes
--		- Area:getMsgFields() - Get fields of an input message
--		- Area:getNext() - Gets the element's successor in its group
--		- Area:getPadding() - Gets the element's paddings
--		- Area:getParent() - Gets the element's parent element
--		- Area:getPrev() - Gets the element's predecessor in its group
--		- Area:getRect() - Returns the element's layouted coordinates
--		- Area:getSiblings() - Gets the element's siblings
--		- Area:hide() - Gets called when the element is about to be hidden
--		- Area:layout() - Layouts the element into a rectangle
--		- Area:passMsg() - Passes an input message to the element
--		- Area:punch() - Subtracts the outline of the element from a
--		[[#tek.lib.region : Region]]
--		- Area:rethinkLayout() - Causes a relayout of the element and its group
--		- Area:setFlags() - Sets an element's flags
--		- Area:setState() - Sets the background attribute of an element
--		- Area:show() - Gets called when the element is about to be shown
--
--	OVERRIDES::
--		- Element:cleanup()
--		- Object.init()
--		- Class.new()
--		- Element:onSetStyle()
--		- Element:setup()
--
-------------------------------------------------------------------------------

module("tek.ui.class.area", tek.ui.class.element)
_VERSION = "Area 54.0"
local Area = _M
Element:newClass(Area)

-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>
#include <tek/lib/tek_lua.h>
#include <tek/lib/tekui.h>


/* Name of superclass: */
#define SUPERCLASS_NAME "tek.ui.class.element"

/* Name of this class: */
#define CLASS_NAME "tek.ui.class.area"

/* Version string: */
#define CLASS_VERSION "Area 54.0"

/* Required major version of the Region library: */
#define REGION_VERSION	10


/* Stack index of self argument: */
#define ISELF	1

/* Stack index of superclass: */
#define ISUPERCLASS	lua_upvalueindex(1)

/* Stack index of tek.ui: */
#define ITEKUI	lua_upvalueindex(2)

/* Stack index of Region library: */
#define IREGION	lua_upvalueindex(3)



static void setfieldbool(lua_State *L, int idx, const char *k, TBOOL b)
{
	lua_pushboolean(L, b);
	lua_setfield(L, idx > 0 ? idx : idx - 1, k);
}

static void setfieldbooliffalse(lua_State *L, int idx, const char *k, TBOOL b)
{
	lua_getfield(L, idx, k);
	if (!lua_toboolean(L, -1))
		setfieldbool(L, idx > 0 ? idx : idx - 1, k, b);
	lua_pop(L, 1);
}

static void setfieldboolifnil(lua_State *L, int idx, const char *k, TBOOL b)
{
	lua_getfield(L, idx, k);
	if (lua_isnil(L, -1))
		setfieldbool(L, idx, k, b);
	lua_pop(L, 1);
}

static int callfield(lua_State *L, int idx, const char *k, int narg, int nret)
{
	lua_getfield(L, idx, k);
	lua_insert(L, -narg - 1);
	lua_call(L, narg, nret);
	return nret;
}

static void replacewithfieldiffalse(lua_State *L, int idx, const char *k)
{
	if (lua_toboolean(L, -1))
		return;
	lua_getfield(L, idx, k);
	lua_remove(L, -2);
}

static void replacewithbooliffalse(lua_State *L, TBOOL b)
{
	if (lua_toboolean(L, -1))
		return;
	lua_pushboolean(L, b);
	lua_remove(L, -2);
}

static void replacewithnumberifkey(lua_State *L, const char *k, lua_Integer v)
{
	const char *key = lua_tostring(L, -1);
	if (!key || strcmp(key, k))
		return;
	lua_pop(L, 1);
	lua_pushinteger(L, v);
}

static void replaceidxwithinteger(lua_State *L, int idx, int idx2)
{
	if (lua_isnumber(L, idx))
	{
		lua_pushinteger(L, lua_tointeger(L, idx));
		lua_replace(L, idx - 1);
	}
	else if (lua_isnumber(L, idx2))
	{
		lua_pushinteger(L, lua_tointeger(L, idx2));
		lua_replace(L, idx - 1);
	}
}

static TBOOL getboolfield(lua_State *L, int idx, const char *key)
{
	lua_getfield(L, idx, key);
	TBOOL res = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return res;
}

static lua_Integer getnumfield(lua_State *L, int idx, const char *key)
{
	lua_getfield(L, idx, key);
	lua_Integer res = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return res;
}

#define TEKUI_FL_DRAWOK \
	(TEKUI_FL_LAYOUT | TEKUI_FL_SHOW | TEKUI_FL_SETUP)

#define TEKUI_FL_REDRAWOK \
	(TEKUI_FL_DRAWOK | TEKUI_FL_REDRAW)

#define TEKUI_FL_BUBBLEUP \
	(TEKUI_FL_REDRAW | TEKUI_FL_REDRAWBORDER | TEKUI_FL_CHANGED)

static lua_Integer clrsetflags(lua_State *L, lua_Integer clr, lua_Integer set,
	TBOOL bubbleup)
{
	lua_getfield(L, ISELF, "Flags");
	lua_Integer f = lua_tointeger(L, -1);
	lua_Integer nf = (f & ~clr) | set;
	if (nf != f)
	{
		lua_pushinteger(L, nf);
		lua_setfield(L, ISELF, "Flags");
	}
	lua_pop(L, 1);
	
	if (bubbleup && (set & TEKUI_FL_BUBBLEUP) && !(nf & TEKUI_FL_UPDATE))
	{
		lua_pushvalue(L, ISELF);
		do
		{
			f = getnumfield(L, -1, "Flags");
			lua_pushinteger(L, f | TEKUI_FL_UPDATE);
			lua_setfield(L, -2, "Flags");
			callfield(L, -1, "getParent", 1, 1);
		} while (lua_toboolean(L, -1));
		lua_pop(L, 1);
	}
	
	return f;
}

/*-----------------------------------------------------------------------------
--	new: overrides
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_new(lua_State *L)
{
	int narg = lua_gettop(L);
	lua_settop(L, 2);
	lua_pushvalue(L, ISELF);
	if (narg >= 2)
		lua_pushvalue(L, 2);
	else
		lua_newtable(L);
	/* class, self or { } */
	callfield(L, IREGION, "new", 0, 1);
	lua_setfield(L, -2, "MinMax");
	callfield(L, IREGION, "new", 0, 1);
	lua_setfield(L, -2, "Rect");
	
	setfieldbool(L, -1, "BGPen", TFALSE);
	setfieldbool(L, -1, "DamageRegion", TFALSE);
	setfieldbooliffalse(L, -1, "Disabled", TFALSE);
	setfieldbool(L, -1, "Focus", TFALSE);
	setfieldbooliffalse(L, -1, "HAlign", TFALSE);
	setfieldbooliffalse(L, -1, "Height", TFALSE);
	setfieldbool(L, -1, "Hilite", TFALSE);
	setfieldbooliffalse(L, -1, "MaxHeight", TFALSE);
	setfieldbooliffalse(L, -1, "MaxWidth", TFALSE);
	setfieldbooliffalse(L, -1, "MinHeight", TFALSE);
	setfieldbooliffalse(L, -1, "MinWidth", TFALSE);
	setfieldbooliffalse(L, -1, "Selected", TFALSE);
	setfieldbooliffalse(L, -1, "VAlign", TFALSE);
	setfieldbooliffalse(L, -1, "Weight", TFALSE);
	setfieldbooliffalse(L, -1, "Width", TFALSE);
	
	lua_Integer flags = getnumfield(L, -1, "Flags");
	lua_getfield(L, -1, "AutoPosition");
	if (lua_isnil(L, -1) || lua_toboolean(L, -1))
		flags |= TEKUI_FL_AUTOPOSITION;
	lua_pop(L, 1);
	lua_getfield(L, -1, "EraseBG");
	if (lua_isnil(L, -1) || lua_toboolean(L, -1))
		flags |= TEKUI_FL_ERASEBG;
	lua_pop(L, 1);
	lua_getfield(L, -1, "TrackDamage");
	if (lua_toboolean(L, -1))
		flags |= TEKUI_FL_TRACKDAMAGE;
	lua_pop(L, 1);
	lua_pushinteger(L, flags);
	lua_setfield(L, -2, "Flags");
	
	callfield(L, ISUPERCLASS, "new", 2, 1);
	return 1;
}

/*-----------------------------------------------------------------------------
--	Area:setup(app, win): After passing the call on to Element:setup(),
--	initializes fields which are being used by Area:layout(), and sets
--	{{FL_SETUP}} in the {{Flags}} field to indicate that the element is
--	ready for getting layouted and displayed.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_setup(lua_State *L)
{
	lua_settop(L, 3);
	
	lua_getfield(L, ISUPERCLASS, "setup");
	lua_pushvalue(L, ISELF);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, 3);
	lua_call(L, 3, 0);
	
	lua_getfield(L, ISELF, "Properties");
	
	lua_getfield(L, ISELF, "HAlign");
	replacewithfieldiffalse(L, -2, "halign");
	replacewithbooliffalse(L, TFALSE);
	lua_setfield(L, ISELF, "HAlign");
	
	lua_getfield(L, ISELF, "VAlign");
	replacewithfieldiffalse(L, -2, "valign");
	replacewithbooliffalse(L, TFALSE);
	lua_setfield(L, ISELF, "VAlign");
	
	/* p */
	lua_getfield(L, ISELF, "MinWidth");
	replacewithfieldiffalse(L, -2, "min-width");
	/* p, minw */
	lua_getfield(L, ISELF, "MinHeight");
	replacewithfieldiffalse(L, -3, "min-height");
	/* p, minw, minh */
	
	lua_getfield(L, ISELF, "MaxWidth");
	replacewithfieldiffalse(L, -4, "max-width");
	replacewithnumberifkey(L, "none", TEKUI_HUGE);
	/* p, minw, minh, maxw */
	lua_getfield(L, ISELF, "MaxHeight");
	replacewithfieldiffalse(L, -5, "max-height");
	replacewithnumberifkey(L, "none", TEKUI_HUGE);
	/* p, minw, minh, maxw, maxh */
	
	lua_getfield(L, ISELF, "Width");
	replacewithfieldiffalse(L, -6, "width");
	const char *key = lua_tostring(L, -1);
	if (key && !strcmp(key, "auto"))	
	{
		lua_pushinteger(L, 0);
		/* p, minw, minh, maxw, maxh, w, 0 */
		lua_replace(L, -4);
	}
	/* p, minw, minh, maxw, maxh, w */

	lua_getfield(L, ISELF, "Height");
	replacewithfieldiffalse(L, -7, "height");
	key = lua_tostring(L, -1);
	if (key && !strcmp(key, "auto"))	
	{
		lua_pushinteger(L, 0);
		/* p, minw, minh, maxw, maxh, w, h, 0 */
		lua_replace(L, -4);
	}
	/* p, minw, minh, maxw, maxh, w, h */
	
	replaceidxwithinteger(L, -6, -2);
	replaceidxwithinteger(L, -5, -1);
	replaceidxwithinteger(L, -4, -2);
	replaceidxwithinteger(L, -3, -1);

	if (lua_isnumber(L, -1))
	{
		lua_pushinteger(L, lua_tointeger(L, -1));
		lua_replace(L, -2);
	}
	replacewithbooliffalse(L, TFALSE);
	lua_setfield(L, ISELF, "Height");
	
	if (lua_isnumber(L, -1))
	{
		lua_pushinteger(L, lua_tointeger(L, -1));
		lua_replace(L, -2);
	}
	replacewithbooliffalse(L, TFALSE);
	lua_setfield(L, ISELF, "Width");
	/* p, minw, minh, maxw, maxh */

	lua_pushinteger(L, 
		lua_isnumber(L, -1) ? lua_tointeger(L, -1) : TEKUI_HUGE);
	lua_setfield(L, ISELF, "MaxHeight");
	lua_pop(L, 1);
	lua_pushinteger(L, 
		lua_isnumber(L, -1) ? lua_tointeger(L, -1) : TEKUI_HUGE);
	lua_setfield(L, ISELF, "MaxWidth");
	lua_pop(L, 1);
	lua_pushinteger(L, lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0);
	lua_setfield(L, ISELF, "MinHeight");
	lua_pop(L, 1);
	lua_pushinteger(L, lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0);
	lua_setfield(L, ISELF, "MinWidth");
	lua_pop(L, 2);
	
	clrsetflags(L, 0, TEKUI_FL_SETUP, TFALSE);
	
	return 0;
}
	
/*-----------------------------------------------------------------------------
--	Area:cleanup(): Clears all temporary layouting data and the {{FL_LAYOUT}}
--	and {{FL_SETUP}} flags, before passing on the call to Element:cleanup().
-----------------------------------------------------------------------------*/
	
static int tek_ui_class_area_cleanup(lua_State *L)
{
	lua_getfield(L, ISUPERCLASS, "cleanup");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 0);
	setfieldbool(L, ISELF, "DamageRegion", TFALSE);
	lua_getfield(L, IREGION, "new");
	lua_call(L, 0, 1);
	lua_setfield(L, ISELF, "MinMax");
	lua_getfield(L, IREGION, "new");
	lua_call(L, 0, 1);
	lua_setfield(L, ISELF, "Rect");
	clrsetflags(L, TEKUI_FL_LAYOUT | TEKUI_FL_SETUP | TEKUI_FL_REDRAW, 0,
		TFALSE);
	return 0;
}

/*-----------------------------------------------------------------------------
--	Area:damage(x0, y0, x1, y1): If the element overlaps with the given
--	rectangle, this function marks it as damaged by setting {{ui.FL_REDRAW}}
--	in the element's {{Flag}} field. Additionally, if the element's
--	{{FL_TRACKDAMAGE}} flag is set, intra-area damage rectangles are
--	collected in {{DamageRegion}}.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_damage(lua_State *L)
{
	lua_settop(L, 5);

	if ((clrsetflags(L, 0, 0, TFALSE) & (TEKUI_FL_LAYOUT | TEKUI_FL_SHOW)) ==
		(TEKUI_FL_LAYOUT | TEKUI_FL_SHOW))
	{
		lua_getfield(L, ISELF, "getRect");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 4);
		lua_Integer r1 = lua_tointeger(L, 2);
		lua_Integer r2 = lua_tointeger(L, 3);
		lua_Integer r3 = lua_tointeger(L, 4);
		lua_Integer r4 = lua_tointeger(L, 5);
		lua_Integer s1 = lua_tointeger(L, -4);
		lua_Integer s2 = lua_tointeger(L, -3);
		lua_Integer s3 = lua_tointeger(L, -2);
		lua_Integer s4 = lua_tointeger(L, -1);
		lua_pop(L, 4);
		if (TEK_UI_OVERLAP(r1, r2, r3, r4, s1, s2, s3, s4))
		{
			r1 = TMAX(s1, r1);
			r2 = TMAX(s2, r2);
			r3 = TMIN(s3, r3);
			r4 = TMIN(s4, r4);
			TBOOL track = 
				getnumfield(L, ISELF, "Flags") & TEKUI_FL_TRACKDAMAGE;
			TBOOL redraw = TFALSE;
			if (!track)
				redraw = clrsetflags(L, 0, 0, TFALSE) & TEKUI_FL_REDRAW;
			if (track || !redraw)
			{
				lua_getfield(L, ISELF, "DamageRegion");
				if (lua_toboolean(L, -1))
				{
					lua_pushinteger(L, r1);
					lua_pushinteger(L, r2);
					lua_pushinteger(L, r3);
					lua_pushinteger(L, r4);
					callfield(L, -5, "orRect", 5, 0);
				}
				else if (track)
				{
					lua_pop(L, 1);
					lua_getfield(L, IREGION, "new");
					lua_pushinteger(L, r1);
					lua_pushinteger(L, r2);
					lua_pushinteger(L, r3);
					lua_pushinteger(L, r4);
					lua_call(L, 4, 1);
					lua_setfield(L, ISELF, "DamageRegion");
				}
				clrsetflags(L, 0, TEKUI_FL_REDRAW, TTRUE);
			}
		}
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	changed = Area:layout(x0, y0, x1, y1[, markdamage]): Layouts the element
--	into the specified rectangle. If the element's (or any of its childrens')
--	coordinates change, returns '''true''' and marks the element as damaged,
--	unless the optional argument {{markdamage}} is set to '''false'''.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_layout(lua_State *L)
{
	lua_settop(L, 6);
	
	lua_pushvalue(L, ISELF);	
	callfield(L, ISELF, "getMargin", 1, 4);
	lua_Integer m1 = lua_tointeger(L, -4);
	lua_Integer m2 = lua_tointeger(L, -3);
	lua_Integer m3 = lua_tointeger(L, -2);
	lua_Integer m4 = lua_tointeger(L, -1);
	lua_pop(L, 4);
	
	lua_Integer x0 = lua_tointeger(L, 2) + m1;
	lua_Integer y0 = lua_tointeger(L, 3) + m2;
	lua_Integer x1 = lua_tointeger(L, 4) - m3;
	lua_Integer y1 = lua_tointeger(L, 5) - m4;

	lua_getfield(L, ISELF, "Rect");
	/* r */
	
	lua_getfield(L, -1, "get");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 4);
	lua_Integer r1 = lua_tointeger(L, -4);
	lua_Integer r2 = lua_tointeger(L, -3);
	lua_Integer r3 = lua_tointeger(L, -2);
	lua_Integer r4 = lua_tointeger(L, -1);
	TBOOL not_r1 = !lua_toboolean(L, -4);
	lua_pop(L, 4);
	
	if (!not_r1 && r1 == x0 && r2 == y0 && r3 == x1 && r4 == y1)
	{
		lua_pop(L, 1);
		return 0;
	}
	
	lua_pushinteger(L, x0);
	lua_pushinteger(L, y0);
	lua_pushinteger(L, x1);
	lua_pushinteger(L, y1);
	callfield(L, -5, "setRect", 5, 0);
	
	/* */
	
	clrsetflags(L, 0, TEKUI_FL_LAYOUT, TFALSE);
	
	TBOOL markdamage = !(lua_isboolean(L, 6) && !lua_toboolean(L, 6));
	
	if (not_r1)
	{
		if (markdamage)
			clrsetflags(L, 0, TEKUI_FL_REDRAW, TTRUE);
		lua_pushboolean(L, TTRUE);
		return 1;
	}
	
	lua_Integer dx = x0 - r1;
	lua_Integer dy = y0 - r2;
	lua_Integer dw = x1 - x0 - r3 + r1;
	lua_Integer dh = y1 - y0 - r4 + r2;
	
	lua_getfield(L, ISELF, "Window");
	lua_getfield(L, -1, "Drawable");
	/* win, d */
	lua_getfield(L, -1, "setShift");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 2);
	lua_Integer sx = lua_tointeger(L, -2);
	lua_Integer sy = lua_tointeger(L, -1);
	lua_pop(L, 2);
	
	TBOOL track = getnumfield(L, ISELF, "Flags") & TEKUI_FL_TRACKDAMAGE;
	TBOOL samesize = (dw == 0) && (dh == 0);
	TBOOL validmove = (dx == 0) != (dy == 0);
	
	TBOOL can_copy = TFALSE;
	if (validmove && (samesize || track))
	{
		lua_getfield(L, -2, "BlitObjects");
		lua_pushvalue(L, ISELF);
		lua_gettable(L, -2);
		TBOOL not_blitobject = !lua_toboolean(L, -1);
		lua_pop(L, 2);
		if (not_blitobject)
		{
			lua_pushvalue(L, ISELF);
			callfield(L, ISELF, "getBG", 1, 4);
			can_copy = lua_toboolean(L, -1);
			lua_pop(L, 4);
		}
	}
	if (can_copy)
	{
		lua_Integer s1 = x0 - dx - m1;
		lua_Integer s2 = y0 - dy - m2;
		lua_Integer s3 = x1 - dx + m3;
		lua_Integer s4 = y1 - dy + m4;
		
		can_copy = TFALSE;
		
		lua_getfield(L, -1, "getClipRect");
		lua_pushvalue(L, -2);
		lua_call(L, 1, 4);
		TBOOL is_c1 = lua_toboolean(L, -4);
		lua_Integer c1 = lua_tointeger(L, -4);
		lua_Integer c2 = lua_tointeger(L, -3);
		lua_Integer c3 = lua_tointeger(L, -2);
		lua_Integer c4 = lua_tointeger(L, -1);
		lua_pop(L, 4);
		if (is_c1)
		{
			lua_getfield(L, IREGION, "new");
			lua_pushinteger(L, r1 + sx - m1);
			lua_pushinteger(L, r2 + sy - m2);
			lua_pushinteger(L, r3 + sx + m3);
			lua_pushinteger(L, r4 + sy + m4);
			lua_call(L, 4, 1);
			/* win, d, r */
			lua_getfield(L, -1, "subRect");
			lua_pushvalue(L, -2);
			lua_pushinteger(L, c1);
			lua_pushinteger(L, c2);
			lua_pushinteger(L, c3);
			lua_pushinteger(L, c4);
			lua_call(L, 5, 0);
			lua_getfield(L, -1, "shift");
			lua_pushvalue(L, -2);
			lua_pushinteger(L, dx);
			lua_pushinteger(L, dy);
			lua_call(L, 3, 0);
			lua_getfield(L, -1, "andRect");
			lua_pushvalue(L, -2);
			lua_pushinteger(L, c1);
			lua_pushinteger(L, c2);
			lua_pushinteger(L, c3);
			lua_pushinteger(L, c4);
			lua_call(L, 5, 0);
			lua_getfield(L, -1, "isEmpty");
			lua_pushvalue(L, -2);
			lua_call(L, 1, 1);
			if (lua_toboolean(L, -1))
				can_copy = TTRUE;
			lua_pop(L, 2);
		}
		else
			can_copy = TTRUE;
		
		if (can_copy)
		{
			/* win, d */
			lua_getfield(L, -2, "BlitObjects");
			lua_pushvalue(L, ISELF);
			lua_pushboolean(L, TTRUE);
			/* win, d, blitobjects, self, true */
			lua_settable(L, -3);
			lua_pop(L, 1);
			/* win, d */
			lua_getfield(L, -2, "addBlit");
			lua_pushvalue(L, -3);
			lua_pushinteger(L, s1 + sx);
			lua_pushinteger(L, s2 + sy);
			lua_pushinteger(L, s3 + sx);
			lua_pushinteger(L, s4 + sy);
			lua_pushinteger(L, dx);
			lua_pushinteger(L, dy);
			if (is_c1)
			{
				lua_pushinteger(L, c1);
				lua_pushinteger(L, c2);
				lua_pushinteger(L, c3);
				lua_pushinteger(L, c4);
				lua_call(L, 11, 0);
			}
			else
				lua_call(L, 7, 0);
			if (samesize)
			{
				lua_pop(L, 2);
				lua_pushboolean(L, TTRUE);
				lua_pushboolean(L, TTRUE);
				return 2;
			}
			r1 += dx;
			r2 += dy;
			r3 += dx;
			r4 += dy;
		}
	}
		
	/* win, d */
	if ((x0 == r1 && y0 == r2) && markdamage && track)
	{
		lua_getfield(L, IREGION, "new");
		lua_pushinteger(L, x0);
		lua_pushinteger(L, y0);
		lua_pushinteger(L, x1);
		lua_pushinteger(L, y1);
		lua_call(L, 4, 1);
		lua_getfield(L, -1, "subRect");
		lua_insert(L, -2);
		lua_pushinteger(L, r1);
		lua_pushinteger(L, r2);
		lua_pushinteger(L, r3);
		lua_pushinteger(L, r4);
		lua_call(L, 5, 1);
		/* win, d, r */
		lua_getfield(L, -2,	"getClipRect");
		lua_pushvalue(L, -3);
		lua_call(L, 1, 4);
		TBOOL is_c1 = lua_toboolean(L, -4);
		lua_Integer c1 = lua_tointeger(L, -4);
		lua_Integer c2 = lua_tointeger(L, -3);
		lua_Integer c3 = lua_tointeger(L, -2);
		lua_Integer c4 = lua_tointeger(L, -1);
		lua_pop(L, 4);
		if (is_c1)
		{
			lua_getfield(L, -1, "andRect");
			lua_pushvalue(L, -2);
			lua_pushinteger(L, c1 - sx);
			lua_pushinteger(L, c2 - sy);
			lua_pushinteger(L, c3 - sx);
			lua_pushinteger(L, c4 - sy);
			lua_call(L, 5, 0);
		}
		
		lua_getfield(L, -1, "forEach");
		lua_pushvalue(L, -2);
		lua_getfield(L, ISELF, "damage");
		lua_pushvalue(L, ISELF);
		lua_call(L, 3, 0);
		lua_pop(L, 1);
	}
	
	if (markdamage)
		clrsetflags(L, 0, TEKUI_FL_REDRAW, TTRUE);
	
	/* win, d */
	lua_pop(L, 2);
	lua_pushboolean(L, TTRUE);
	return 1;
}

/*-----------------------------------------------------------------------------
--	Area:punch(region): Subtracts the element from (by punching a hole into)
--	the specified Region. This function is called by the layouter.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_punch(lua_State *L)
{
	lua_getfield(L, 2, "subRegion");
	lua_pushvalue(L, 2);
	lua_getfield(L, ISELF, "Rect");
	lua_call(L, 2, 0);
	return 0;
}

/*-----------------------------------------------------------------------------
--	success = Area:draw(): If the element is marked for a repaint (indicated
--	by the presence of the flag {{ui.FL_REDRAW}} in the {{Flags}} field),
--	draws the element into the rectangle that was assigned to it by the
--	layouter, clears {{ui.FL_REDRAW}}, and returns '''true'''. If the
--	flag {{FL_ERASEBG}} is set, this function also clears the element's
--	background by calling Area:erase().
--
--	When overriding this function, the control flow is roughly as follows:
--
--			function ElementClass:draw()
--			  if SuperClass.draw(self) then
--			    -- your rendering here
--			    return true
--			  end
--			end
--
--	There are rare occasions in which a class modifies the drawing context,
--	e.g. by setting a coordinate displacement. Such modifications must
--	be performed in Area:drawBegin() and undone in Area:drawEnd(). In this
--	case, the control flow looks like this:
--
--			function ElementClass:draw()
--			  if SuperClass.draw(self) and self:drawBegin() then
--			    -- your rendering here
--			    self:drawEnd()
--			    return true
--			  end
--			end
--
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_draw(lua_State *L)
{
	TBOOL res = TFALSE;
	if ((clrsetflags(L, TEKUI_FL_REDRAW, 0, TFALSE) & TEKUI_FL_REDRAWOK) == 
		TEKUI_FL_REDRAWOK)
	{
		TBOOL erasebg = getnumfield(L, ISELF, "Flags") & TEKUI_FL_ERASEBG;
		if (erasebg)
		{
			lua_getfield(L, ISELF, "drawBegin");
			lua_pushvalue(L, ISELF);
			lua_call(L, 1, 1);
			TBOOL drawbegin = lua_toboolean(L, -1);
			lua_pop(L, 1);
			if (drawbegin)
			{
				lua_getfield(L, ISELF, "erase");
				lua_pushvalue(L, ISELF);
				lua_call(L, 1, 1);
				lua_getfield(L, ISELF, "drawEnd");
				lua_pushvalue(L, ISELF);
				lua_call(L, 1, 0);
			}
		}
		lua_pushboolean(L, TFALSE);
		lua_setfield(L, ISELF, "DamageRegion");
		res = TTRUE;
	}
	lua_pushboolean(L, res);
	return 1;
}

/*-----------------------------------------------------------------------------
--	self = Area:getByXY(x, y): Returns {{self}} if the element covers
--	the specified coordinate.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getbyxy(lua_State *L)
{
	lua_settop(L, 3);
	lua_getfield(L, ISELF, "getRect");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 4);
	if (lua_toboolean(L, -4))
	{
		lua_Integer r1 = lua_tointeger(L, -4);
		lua_Integer r2 = lua_tointeger(L, -3);
		lua_Integer r3 = lua_tointeger(L, -2);
		lua_Integer r4 = lua_tointeger(L, -1);
		lua_Integer x = lua_tointeger(L, 2);
		lua_Integer y = lua_tointeger(L, 3);
		lua_pop(L, 4);
		if (x >= r1 && x <= r3 && y >= r2 && y <= r4)
		{
			lua_pushvalue(L, ISELF);
			return 1;
		}
	}
	else
		lua_pop(L, 4);
	return 0;
}

/*-----------------------------------------------------------------------------
--	left, top, right, bottom = Area:getMargin(): Returns the element's
--	margins in the order left, top, right, bottom.
-----------------------------------------------------------------------------*/

typedef struct { int r1, r2, r3, r4; } rect;

static rect tek_ui_class_area_getmargin_int(lua_State *L)
{
	rect res;
	lua_getfield(L, ISELF, "Properties");
	lua_getfield(L, -1, "margin-left");
	lua_getfield(L, -2, "margin-top");
	lua_getfield(L, -3, "margin-right");
	lua_getfield(L, -4, "margin-bottom");
	res.r1 = lua_tointeger(L, -4);
	res.r2 = lua_tointeger(L, -3);
	res.r3 = lua_tointeger(L, -2);
	res.r4 = lua_tointeger(L, -1);
	lua_pop(L, 5);
	return res;
}

static int tek_ui_class_area_getmargin(lua_State *L)
{
	rect res = tek_ui_class_area_getmargin_int(L);
	lua_pushinteger(L, res.r1);
	lua_pushinteger(L, res.r2);
	lua_pushinteger(L, res.r3);
	lua_pushinteger(L, res.r4);
	return 4;
}

/*-----------------------------------------------------------------------------
--	onSetStyle: overrides
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_onsetstyle(lua_State *L)
{
	lua_getfield(L, ISUPERCLASS, "onSetStyle");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 0);
	lua_getfield(L, ISELF, "setState");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 0);
	lua_getfield(L, ISELF, "rethinkLayout");
	lua_pushvalue(L, ISELF);
	lua_pushinteger(L, 2);
	lua_pushboolean(L, 1);
	lua_call(L, 3, 0);
	return 0;
}

/*-----------------------------------------------------------------------------
--	Area:setState(bg): Sets the {{BGPen}} attribute according to
--	the state of the element, and if it changed, marks the element
--	for repainting.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_setstate(lua_State *L)
{
	if (!lua_toboolean(L, 2))
	{
		lua_getfield(L, ISELF, "Properties");
		lua_getfield(L, -1, "background-color");
		lua_remove(L, -2);
		if (!lua_toboolean(L, -1))
		{
			lua_pop(L, 1);
			lua_pushliteral(L, "background");
		}
	}
	size_t bglen;
	const char *bg = lua_tolstring(L, -1, &bglen);
	if (bglen == 11 && !strcmp(bg, "transparent"))
	{
		lua_pop(L, 1);
		lua_getfield(L, ISELF, "getBGElement");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 1);
		lua_getfield(L, -1, "BGPen");
		lua_remove(L, -2);
	}
	lua_getfield(L, ISELF, "BGPen");
	int equal = tek_lua_equal(L, -1, -2);
	lua_pop(L, 1);
	if (!equal)
	{
		lua_setfield(L, ISELF, "BGPen");
		clrsetflags(L, 0, TEKUI_FL_REDRAW, TTRUE);
	}
	else
		lua_pop(L, 1);
	return 0;
}

/*-----------------------------------------------------------------------------
--	dx, dy = Area:getDisplacement(): Get the element's coordinate displacement
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getdisplacement(lua_State *L)
{
	lua_pushinteger(L, 0);
	lua_pushinteger(L, 0);
	return 2;
}

/*-----------------------------------------------------------------------------
--	... = Area:getMsgFields(msg, field): Get specified field(s) from an input
--	message. Possible fields include:
--	* {{"mousexy"}}: Returns the message's mouse coordinates (x, y)
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getmsgfields(lua_State *L)
{
	size_t modelen;
	const char *mode;
	lua_settop(L, 3);
	mode = lua_tolstring(L, 3, &modelen);
	if (modelen == 7 && !strcmp(mode, "mousexy"))
	{
		lua_Integer mx, my;
		lua_pushinteger(L, 4);
		lua_gettable(L, 2);
		lua_pushinteger(L, 5);
		lua_gettable(L, 2);
		mx = lua_tointeger(L, -2);
		my = lua_tointeger(L, -1);
		lua_pop(L, 2);
		lua_getfield(L, ISELF, "getParent");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 1);
		while (!lua_isnil(L, -1))
		{
			lua_getfield(L, -1, "getDisplacement");
			lua_pushvalue(L, -2);
			lua_call(L, 1, 2);
			mx -= lua_tointeger(L, -2);
			my -= lua_tointeger(L, -1);
			lua_pop(L, 2);
			lua_getfield(L, -1, "getParent");
			lua_insert(L, -2);
			lua_call(L, 1, 1);
		}
		lua_pop(L, 1);
		lua_pushinteger(L, mx);
		lua_pushinteger(L, my);
		return 2;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	all_present = Area:checkFlags(flags): Check for presence of all of the
--	specified {{flags}} in the element.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_checkflags(lua_State *L)
{
	lua_Integer chkf = lua_tointeger(L, 2);
	lua_pushboolean(L, (clrsetflags(L, 0, 0, TFALSE) & chkf) == chkf);
	return 1;
}

/*-----------------------------------------------------------------------------
--	all_present = Area:checkClearFlags(chkflags[, clearflags]): Check for
--	presence of all of the specified {{chkflags}} in the element. Optionally
--	clears the {{clearflags}} set from the element.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_checkclearflags(lua_State *L)
{
	lua_Integer chkf = lua_tointeger(L, 2);
	lua_Integer clrf = luaL_optinteger(L, 3, chkf);
	lua_pushboolean(L, (clrsetflags(L, clrf, 0, TFALSE) & chkf) == chkf);
	return 1;
}

/*-----------------------------------------------------------------------------
--	setFlags(flags): Set element's flags. The flags {{FL_REDRAW}},
--	{{FL_CHANGED}}, and {{FL_REDRAWBORDER}} will additionally cause the flag
--	{{FL_UPDATE}} to be set, which will also bubble up in the element
--	hierarchy until it reaches to topmost element.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_setflags(lua_State *L)
{
	lua_Integer flags = lua_tointeger(L, 2);
	clrsetflags(L, 0, flags, TTRUE);
	return 0;
}

/*-----------------------------------------------------------------------------
--	minx, miny, maxx, maxy = Area:getMinMax(): Returns the element's
--	calculated minimum and maximum size requirements, which are available
--	after Area:askMinMax().
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getminmax(lua_State *L)
{
	lua_getfield(L, ISELF, "MinMax");
	lua_getfield(L, -1, "get");
	lua_insert(L, -2);
	lua_call(L, 1, 4);
	return 4;
}

/*-----------------------------------------------------------------------------
--	left, top, right, bottom = Area:getPadding(): Returns the element's
--	padding style properties.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getpadding(lua_State *L)
{
	lua_getfield(L, ISELF, "Properties");
	lua_getfield(L, -1, "padding-left");
	lua_getfield(L, -2, "padding-top");
	lua_getfield(L, -3, "padding-right");
	lua_getfield(L, -4, "padding-bottom");
	lua_Integer p1 = lua_tointeger(L, -4);
	lua_Integer p2 = lua_tointeger(L, -3);
	lua_Integer p3 = lua_tointeger(L, -2);
	lua_Integer p4 = lua_tointeger(L, -1);
	lua_pop(L, 5);
	lua_pushinteger(L, p1);
	lua_pushinteger(L, p2);
	lua_pushinteger(L, p3);
	lua_pushinteger(L, p4);
	return 4;
}

/*-----------------------------------------------------------------------------
--	can_draw = Area:drawBegin(): Prepares the drawing context, returning a
--	boolean indicating success. This function must be overridden if a class
--	wishes to modify the drawing context, e.g. for installing a coordinate
--	displacement.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_drawbegin(lua_State *L)
{
	lua_pushboolean(L, 
		(clrsetflags(L, 0, 0, TFALSE) & TEKUI_FL_DRAWOK) == TEKUI_FL_DRAWOK);
	return 1;
}

/*-----------------------------------------------------------------------------
--	Area:drawEnd(): Reverts the changes made to the drawing context during
--	Area:drawBegin().
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_drawend(lua_State *L)
{
	return 0;
}

/*-----------------------------------------------------------------------------
--	element = Area:getChildren([mode]): Returns a table containing the
--	element's children, or '''nil''' if this element cannot have children.
--	The optional argument {{mode}} is the string {{"init"}} when this function
--	is called during initialization or deinitialization.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getchildren(lua_State *L)
{
	return 0;
}

/*-----------------------------------------------------------------------------
--	element = Area:getParent(): Returns the element's parent element, or
--	'''false''' if it currently has no parent.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getparent(lua_State *L)
{
	lua_getfield(L, ISELF, "Parent");
	return 1;
}

/*-----------------------------------------------------------------------------
--	x0, y0, x1, y1 = Area:getRect(): This function returns the
--	rectangle which the element has been layouted to, or '''nil'''
--	if the element has not been layouted yet.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getrect(lua_State *L)
{
	if ((clrsetflags(L, 0, 0, TFALSE) & TEKUI_FL_DRAWOK) == TEKUI_FL_DRAWOK)
	{
		lua_getfield(L, ISELF, "Rect");
		lua_getfield(L, -1, "get");
		lua_insert(L, -2);
		lua_call(L, 1, 4);
		return 4;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	moved = Area:focusRect([x0, y0, x1, y1]): Tries to shift any Canvas
--	containing the element into a position that makes the element fully
--	visible. Optionally, a rectangle can be specified that is to be made
--	visible. Returns '''true''' to indicate that some kind of repositioning
--	has taken place.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_focusrect(lua_State *L)
{
	lua_Integer r1, r2, r3, r4;
	lua_settop(L, 5);
	if (!lua_toboolean(L, 2))
	{
		lua_getfield(L, ISELF, "Rect");
		lua_getfield(L, -1, "get");
		lua_insert(L, -2);
		lua_call(L, 1, 4);
		int have_rect = lua_toboolean(L, -4);
		r1 = lua_tointeger(L, -4);
		r2 = lua_tointeger(L, -3);
		r3 = lua_tointeger(L, -2);
		r4 = lua_tointeger(L, -1);
		lua_pop(L, 4);
		if (!have_rect)
			return 0;
	}
	else
	{
		if (!lua_toboolean(L, 2))
			return 0;
		r1 = lua_tointeger(L, 2);
		r2 = lua_tointeger(L, 3);
		r3 = lua_tointeger(L, 4);
		r4 = lua_tointeger(L, 5);
	}
	
	lua_getfield(L, ISELF, "getParent");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 1);
	if (lua_toboolean(L, -1))
	{
		rect m = tek_ui_class_area_getmargin_int(L);
		lua_getfield(L, -1, "focusRect");
		lua_insert(L, -2);
		lua_pushinteger(L, r1 - m.r1);
		lua_pushinteger(L, r2 - m.r2);
		lua_pushinteger(L, r3 - m.r3);
		lua_pushinteger(L, r4 - m.r4);
		lua_call(L, 5, 1);
		return 1;
	}
	lua_pop(L, 1);
	return 0;
}

/*-----------------------------------------------------------------------------
--	table = Area:getSiblings(): Returns a table containing the element's
--	siblings, which includes the element itself. Returns '''nil''' if the
--	element is not currently connected. Note: The returned table must be
--	treated read-only.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getsiblings(lua_State *L)
{
	lua_getfield(L, ISELF, "getParent");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 1);
	if (lua_toboolean(L, -1))
	{
		lua_getfield(L, -1, "getChildren");
		lua_insert(L, -2);
		lua_call(L, 1, 1);
		return 1;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	element = Area:getGroup(parent): Returns the closest
--	[[#tek.ui.class.group : Group]] containing the element. If the {{parent}}
--	argument is '''true''', this function will start looking for the closest
--	group at its parent - otherwise, the element itself is returned if it is
--	a group already. Returns '''nil''' if the element is not currently
--	connected.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getgroup(lua_State *L)
{
	lua_getfield(L, ISELF, "getParent");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 1);
	if (lua_toboolean(L, -1))
	{
		lua_getfield(L, -1, "getGroup");
		lua_pushvalue(L, -2);
		lua_call(L, 1, 1);
		int equal = tek_lua_equal(L, -1, -2);
		lua_pop(L, 1);
		if (equal)
			return 1;
		lua_getfield(L, -1, "getGroup");
		lua_insert(L, -2);
		lua_pushboolean(L, 1);
		lua_call(L, 2, 1);
		return 1;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	element = Area:getNext([mode]): Returns the next element in its group.
--	If the element has no successor and the optional argument {{mode}} is
--	{{"recursive"}}, returns the next element in the parent group (and so
--	forth, until it reaches the topmost group).
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_findelement(lua_State *L, int *pn, int *pi)
{
	lua_getfield(L, ISELF, "getSiblings");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 1);
	if (lua_toboolean(L, -1))
	{
		int i;
		int n = tek_lua_len(L, -1);
		for (i = 1; i <= n; ++i)
		{
			lua_rawgeti(L, -1, i);
			int equal = tek_lua_equal(L, ISELF, -1);
			lua_pop(L, 1);
			if (equal)
			{
				*pn = n;
				*pi = i;
				return 1;
			}
		}
	}
	lua_pop(L, 1);
	return 0;
}

static int tek_ui_class_area_getnext(lua_State *L)
{
	int n, i;
	if (tek_ui_class_area_findelement(L, &n, &i) == 0)
		return 0;
	if (i == n)
	{
		lua_pop(L, 1);
		size_t modelen;
		const char *mode = lua_tolstring(L, 2, &modelen);
		if (modelen == 9 && !strcmp(mode, "recursive"))
		{
			lua_getfield(L, ISELF, "getParent");
			lua_pushvalue(L, ISELF);
			lua_call(L, 1, 1);
			lua_getfield(L, -1, "getNext");
			lua_insert(L, -2);
			lua_pushliteral(L, "recursive");
			lua_call(L, 2, 1);
			return 1;			
		}
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_rawgeti(L, -1, i % n + 1);
	lua_remove(L, -2);
	return 1;
}

/*-----------------------------------------------------------------------------
--	element = Area:getPrev([mode]): Returns the previous element in its group.
--	If the element has no predecessor and the optional argument {{mode}} is
--	{{"recursive"}}, returns the previous element in the parent group (and so
--	forth, until it reaches the topmost group).
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getprev(lua_State *L)
{
	int n, i;
	if (tek_ui_class_area_findelement(L, &n, &i) == 0)
		return 0;
	if (i == 1)
	{
		lua_pop(L, 1);
		size_t modelen;
		const char *mode = lua_tolstring(L, 2, &modelen);
		if (modelen == 9 && !strcmp(mode, "recursive"))
		{
			lua_getfield(L, ISELF, "getParent");
			lua_pushvalue(L, ISELF);
			lua_call(L, 1, 1);
			lua_getfield(L, -1, "getPrev");
			lua_insert(L, -2);
			lua_pushliteral(L, "recursive");
			lua_call(L, 2, 1);
			return 1;			
		}
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_rawgeti(L, -1, (i - 2) % n + 1);
	lua_remove(L, -2);
	return 1;
}

/*-----------------------------------------------------------------------------
--	can_receive = Area:checkFocus(): Returns '''true''' if this element can
--	receive the input focus.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_checkfocus(lua_State *L)
{
	return 0;
}

/*-----------------------------------------------------------------------------
--	can_hilite = Area:checkHilite(): Returns '''true''' if this element can
--	be highlighted, e.g by being hovered over with the pointing device.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_checkhilite(lua_State *L)
{
	return 0;
}

/*-----------------------------------------------------------------------------
--	msg = Area:passMsg(msg): If the element has the ui.FL_RECVINPUT flag set,
--	this function receives input messages. Additionally, to receive messages
--	of the type ui.MSG_MOUSEMOVE, the flag ui.FL_RECVMOUSEMOVE must be set.
--	After processing, it is free to return the message unmodified (thus
--	allowing it to be passed on to other elements), or to absorb the message
--	by returning '''false'''. You are not allowed to modify any data
--	inside the original message; if you alter it, you must return a copy.
--	The message types ui.MSG_INTERVAL, ui.MSG_USER, and ui.MSG_REQSELECTION
--	are not received by this function. To receive these, you must register an
--	input handler using Window:addInputHandler().
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_passmsg(lua_State *L)
{
	lua_pushvalue(L, 2);
	return 1;
}

/*-----------------------------------------------------------------------------
--	Area:erase(): Clears the element's background. This method is invoked by
--	Area:draw() if the {{FL_ERASEBG}} flag is set, and when a repaint is
--	possible and necessary. Area:drawBegin() has been invoked when this
--	function is called, and Area:drawEnd() will be called afterwards.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_erase(lua_State *L)
{
	lua_getfield(L, ISELF, "Window");
	lua_getfield(L, -1, "Drawable");
	lua_remove(L, -2);
	/* d */
	lua_getfield(L, -1, "setBGPen");
	lua_pushvalue(L, -2);
	/* d, setBGPen(), d */
	lua_getfield(L, ISELF, "getBG");
	/* d, setBGPen(), d, getBG() */
	lua_pushvalue(L, ISELF);
	/* d, setBGPen(), d, getBG(), self */
	lua_call(L, 1, 3);
	/* d, setBGPen(), d, bg, tx, ty */
	lua_call(L, 4, 0);
	/* d */
	lua_getfield(L, ISELF, "DamageRegion");
	/* d, dr */
	if (lua_toboolean(L, -1))
	{
		lua_getfield(L, -1, "forEach");
		lua_insert(L, -2);
		/* d, foreach(), dr */
		lua_getfield(L, -3, "fillRect");
		/* d, foreach(), dr, fillrect() */
		lua_pushvalue(L, -4);
		/* d, foreach(), dr, fillrect(), d */
		lua_call(L, 3, 0);
		/* d */
	}
	else
	{
		lua_pop(L, 1);
		lua_getfield(L, -1, "fillRect");
		/* d, fillrect() */
		lua_insert(L, -2);
		/* fillrect, d */
		lua_getfield(L, ISELF, "getRect");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 4);
		/* fillrect, d, r1, r2, r3, r4 */
		lua_call(L, 5, 0);
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	element = Area:getBGElement(): Returns the element that is responsible for
--	painting the surroundings (or the background) of the element. This
--	information is useful for painting transparent or translucent parts of
--	the element, e.g. an inactive focus border.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getbgelement(lua_State *L)
{
	lua_getfield(L, ISELF, "getParent");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 1);
	lua_getfield(L, -1, "getBGElement");
	lua_insert(L, -2);
	lua_call(L, 1, 1);
	return 1;
}

/*-----------------------------------------------------------------------------
--	bgpen, tx, ty, pos_independent = Area:getBG(): Gets the element's
--	background properties. {{bgpen}} is the background paint (which may be a
--	solid color, texture, or gradient). If the element's
--	''background-attachment'' is not {{"fixed"}}, then {{tx}} and {{ty}} are
--	the coordinates of the texture origin. {{pos_independent}} indicates
--	whether the background is independent of the element's position.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_getbg(lua_State *L)
{
	TBOOL scrollable = TFALSE;
	lua_getfield(L, ISELF, "Properties");
	lua_getfield(L, ISELF, "BGPen");
	/* p, bgpen */
	TBOOL istransparent = TFALSE;
	TBOOL bgpen = lua_toboolean(L, -1);
	if (bgpen)
	{
		luaL_Buffer buf;
		luaL_buffinit(L, &buf);
		luaL_addlstring(&buf, "background-color", 16);
		lua_getfield(L, ISELF, "getPseudoClass");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 1);
		luaL_addvalue(&buf);
		luaL_pushresult(&buf);
		/* p, bgpen, "background-color..." */
		lua_gettable(L, -3);
		/* p, bgpen, p["background-color..."] */
		size_t len;
		const char *val = lua_tolstring(L, -1, &len);
		istransparent = (len == 11 && !strcmp(val, "transparent"));
		lua_pop(L, 1);
		/* p, bgpen */
	}
	if (!bgpen || istransparent)
	{
		lua_pop(L, 1);
		lua_getfield(L, ISELF, "getParent");
		lua_pushvalue(L, ISELF);
		lua_call(L, 1, 1);
		/* p, parent */
		lua_getfield(L, -1, "getBG");
		lua_insert(L, -2);
		/* p, getBG(), parent */
		lua_call(L, 1, 3);
		/* p, bgpen, tx, ty */
	}
	else
	{
		/* p, bgpen */
		lua_getfield(L, -2, "background-attachment");
		/* p, bgpen, p["background-attachment"] */
		size_t len;
		const char *val = lua_tolstring(L, -1, &len);
		scrollable = len != 5 || strcmp(val, "fixed");
		lua_pop(L, 1);
		/* p, bgpen */
		if (scrollable)
		{
			lua_getfield(L, ISELF, "getRect");
			lua_pushvalue(L, ISELF);
			lua_call(L, 1, 2);
			/* p, bgpen, tx, ty */
		}
		else
		{
			lua_pushnil(L);
			lua_pushnil(L);
			/* p, bgpen, tx, tx */
		}
	}
	lua_remove(L, -4);
	/* bgpen, tx, tx */
	lua_getfield(L, ISELF, "Window");
	lua_getfield(L, -1, "isSolidPen");
	lua_insert(L, -2);
	/* bgpen, tx, tx, isSolidPen(), Window */
	lua_pushvalue(L, -5);
	/* bgpen, tx, tx, isSolidPen(), Window, bgpen */
	lua_call(L, 2, 1);
	/* bgpen, tx, tx, result */
	if (!lua_toboolean(L, -1))
	{
		lua_pop(L, 1);
		lua_pushboolean(L, scrollable);
	}
	/* bgpen, tx, tx, result */
	return 4;
}

/*-----------------------------------------------------------------------------
--	minw, minh, maxw, maxh = Area:askMinMax(minw, minh, maxw, maxh): This
--	method is called during the layouting process for adding the required
--	width and height to the minimum and maximum size requirements of the
--	element, before passing the result on to its super class. {{minw}},
--	{{minh}} are cumulative of the minimal size of the element, while
--	{{maxw}}, {{maxw}} collect the size the element is allowed to expand to.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_askminmax(lua_State *L)
{
	lua_Integer m1 = lua_tonumber(L, 2);
	lua_Integer m2 = lua_tonumber(L, 3);
	lua_Integer m3 = lua_tonumber(L, 4);
	lua_Integer m4 = lua_tonumber(L, 5);
	lua_getfield(L, ISELF, "getPadding");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 4);
	lua_Integer p1 = lua_tonumber(L, -4);
	lua_Integer p2 = lua_tonumber(L, -3);
	lua_Integer p3 = lua_tonumber(L, -2);
	lua_Integer p4 = lua_tonumber(L, -1);
	lua_getfield(L, ISELF, "MinWidth");
	lua_getfield(L, ISELF, "MinHeight");
	lua_getfield(L, ISELF, "MaxWidth");
	lua_getfield(L, ISELF, "MaxHeight");
	lua_Integer minw = lua_tonumber(L, -4);
	lua_Integer minh = lua_tonumber(L, -3);
	lua_Integer maxw = lua_tonumber(L, -2);
	lua_Integer maxh = lua_tonumber(L, -1);
	m1 = TMAX(minw, m1 + p1 + p3);
	m2 = TMAX(minh, m2 + p2 + p4);
	m3 = TMAX(TMIN(maxw, m3 + p1 + p3), m1);
	m4 = TMAX(TMIN(maxh, m4 + p2 + p4), m2);
	lua_getfield(L, ISELF, "getMargin");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 4);
	lua_Integer ma1 = lua_tonumber(L, -4);
	lua_Integer ma2 = lua_tonumber(L, -3);
	lua_Integer ma3 = lua_tonumber(L, -2);
	lua_Integer ma4 = lua_tonumber(L, -1);
	lua_pop(L, 12);
	m1 += ma1 + ma3;
	m2 += ma2 + ma4;
	m3 += ma1 + ma3;
	m4 += ma2 + ma4;
	lua_getfield(L, ISELF, "MinMax");
	lua_getfield(L, -1, "setRect");
	lua_insert(L, -2);
	lua_pushinteger(L, m1);
	lua_pushinteger(L, m2);
	lua_pushinteger(L, m3);
	lua_pushinteger(L, m4);
	lua_call(L, 5, 0);
	lua_pushinteger(L, m1);
	lua_pushinteger(L, m2);
	lua_pushinteger(L, m3);
	lua_pushinteger(L, m4);
	return 4;
}

/*-----------------------------------------------------------------------------
--	Area:rethinkLayout([repaint[, check_size]]): Slates an element (and its
--	children) for relayouting, which takes place during the next Window update
--	cycle. If the element's coordinates change, this will cause it to be
--	repainted. The parent element (usually a Group) will be checked as well,
--	so that it has the opportunity to update its FreeRegion.
--	The optional argument {{repaint}} can be used to specify additional hints:
--		- {{1}} - marks the element for repainting unconditionally (not
--		implying possible children)
--		- {{2}} - marks the element (and all possible children) for repainting
--		unconditionally
--	The optional argument {{check_size}} (a boolean) can be used to
--	recalculate the element's minimum and maximum size requirements.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_rethinklayout(lua_State *L)
{
	if (clrsetflags(L, 0, 0, TFALSE) & TEKUI_FL_SETUP)
	{
		TBOOL check_size = lua_toboolean(L, 3);
		if (check_size)
		{
			lua_getfield(L, ISELF, "getGroup");
			lua_pushvalue(L, ISELF);
			lua_call(L, 1, 1);
			clrsetflags(L, 0, TEKUI_FL_CHANGED, TTRUE);
		}
		lua_getfield(L, ISELF, "Window");
		lua_getfield(L, -1, "addLayout");
		lua_insert(L, -2);
		lua_pushvalue(L, ISELF);
		lua_pushinteger(L, lua_tointeger(L, 2));
		lua_pushboolean(L, check_size);
		lua_call(L, 4, 0);
	}
	return 0;
}

/*-----------------------------------------------------------------------------
--	Area:hide(): Override this method to free all display-related resources
--	previously allocated in Area:show().
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_hide(lua_State *L)
{
	clrsetflags(L, TEKUI_FL_SHOW | TEKUI_FL_REDRAW | TEKUI_FL_REDRAWBORDER, 0,
		TFALSE);
	return 0;
}

/*-----------------------------------------------------------------------------
--	Area:show(): This function is called when the element's window is opened.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_show(lua_State *L)
{
	lua_getfield(L, ISELF, "setState");
	lua_pushvalue(L, ISELF);
	lua_call(L, 1, 0);
	clrsetflags(L, 0, TEKUI_FL_SHOW, TFALSE);
	return 0;
}

/*-----------------------------------------------------------------------------
--	beginPopup([baseitem]): Prepare element for being used in a popup.
-----------------------------------------------------------------------------*/

static int tek_ui_class_area_beginpopup(lua_State *L)
{
	setfieldbool(L, ISELF, "Hilite", TFALSE);
	setfieldbool(L, ISELF, "Focus", TFALSE);
	return 0;
}

/*****************************************************************************/

static const luaL_Reg classfuncs[] =
{
	{ "new", tek_ui_class_area_new },
	{ "setup", tek_ui_class_area_setup },
	{ "cleanup", tek_ui_class_area_cleanup },
	{ "damage", tek_ui_class_area_damage },
	{ "layout", tek_ui_class_area_layout },
	{ "punch", tek_ui_class_area_punch },
	{ "draw", tek_ui_class_area_draw },
	{ "getByXY", tek_ui_class_area_getbyxy },
	{ "getMargin", tek_ui_class_area_getmargin },
	{ "onSetStyle", tek_ui_class_area_onsetstyle },
	{ "setState", tek_ui_class_area_setstate },
	{ "getDisplacement", tek_ui_class_area_getdisplacement },
	{ "getMsgFields", tek_ui_class_area_getmsgfields },
	{ "checkFlags", tek_ui_class_area_checkflags },
	{ "checkClearFlags", tek_ui_class_area_checkclearflags },
	{ "setFlags", tek_ui_class_area_setflags },
	{ "getMinMax", tek_ui_class_area_getminmax },
	{ "getPadding", tek_ui_class_area_getpadding },
	{ "drawBegin", tek_ui_class_area_drawbegin },
	{ "drawEnd", tek_ui_class_area_drawend },
	{ "getChildren", tek_ui_class_area_getchildren },
	{ "getParent", tek_ui_class_area_getparent },
	{ "getRect", tek_ui_class_area_getrect },
	{ "focusRect", tek_ui_class_area_focusrect },
	{ "getSiblings", tek_ui_class_area_getsiblings },
	{ "getGroup", tek_ui_class_area_getgroup },
	{ "getNext", tek_ui_class_area_getnext },
	{ "getPrev", tek_ui_class_area_getprev },
	{ "checkFocus", tek_ui_class_area_checkfocus },
	{ "checkHilite", tek_ui_class_area_checkhilite },
	{ "passMsg", tek_ui_class_area_passmsg },
	{ "erase", tek_ui_class_area_erase },
	{ "getBGElement", tek_ui_class_area_getbgelement },
	{ "getBG", tek_ui_class_area_getbg },
	{ "askMinMax", tek_ui_class_area_askminmax },
	{ "rethinkLayout", tek_ui_class_area_rethinklayout },
	{ "hide", tek_ui_class_area_hide },
	{ "show", tek_ui_class_area_show },
	{ "beginPopup", tek_ui_class_area_beginpopup },
	{ NULL, NULL }
};

/*****************************************************************************/

int luaopen_tek_ui_class_area(lua_State *L)
{
	lua_getglobal(L, "require");
	lua_pushliteral(L, SUPERCLASS_NAME);
	lua_call(L, 1, 1);
	lua_getfield(L, -1, "newClass");
	lua_insert(L, -2);
	/* s: newClass(), superclass */
	
	/* pass superclass, tek.ui and required libraries as upvalues: */
	lua_pushvalue(L, -1);
	lua_getglobal(L, "require");
	lua_pushliteral(L, "tek.ui");
	lua_call(L, 1, 1);
	lua_getfield(L, -1, "loadLibrary");
	lua_pushliteral(L, "region");	
	lua_pushinteger(L, REGION_VERSION);
	lua_call(L, 2, 1);
	/* s: newClass(), superclass, superclass, tek.ui, Region */
	tek_lua_register(L, CLASS_NAME, classfuncs, 3);
	/* s: newClass(), superclass, class */
	
	/* insert name and version: */
	lua_pushliteral(L, CLASS_NAME);
	lua_setfield(L, -2, "_NAME");
	lua_pushliteral(L, CLASS_VERSION);
	lua_setfield(L, -2, "_VERSION");

	/* inherit: class = superclass.newClass(superclass, class) */
	lua_call(L, 2, 1); 
	/* s: class */
	return 1;
}
