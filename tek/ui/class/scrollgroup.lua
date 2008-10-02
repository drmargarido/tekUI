-------------------------------------------------------------------------------
--
--	tek.ui.class.scrollgroup
--	Written by Timm S. Mueller <tmueller at schulze-mueller.de>
--	See copyright notice in COPYRIGHT
--
--	LINEAGE::
--		[[#ClassOverview]] :
--		[[#tek.class : Class]] /
--		[[#tek.class.object : Object]] /
--		[[#tek.ui.class.element : Element]] /
--		[[#tek.ui.class.area : Area]] /
--		[[#tek.ui.class.frame : Frame]] /
--		[[#tek.ui.class.gadget : Gadget]] /
--		[[#tek.ui.class.group : Group]] /
--		ScrollGroup
--
--	OVERVIEW::
--		This class implements a group containing a scrollable
--		container and accompanying elements such as horizontal and vertical
--		[[#tek.ui.class.scrollbar : ScrollBars]].
--
--	ATTRIBUTES::
--		- {{Child [IG]}} ([[#tek.ui.class.canvas : Canvas]])
--			Specifies the Canvas which encapsulates the scrollable
--			area and children.
--		- {{HSliderMode [IG]}} (string)
--			Specifies when the horizontal
--			[[#tek.ui.class.scrollbar : ScrollBar]] should be visible:
--				- "on" - The horizontal scrollbar is displayed
--				- "off" - The horizontal scrollbar is not displayed
--				- "auto" - The horizontal scrollbar is displayed when
--				the ListGadget is wider than the currently visible area.
--			Note: The use of the "auto" mode is currently (v8.0) discouraged.
--		- {{VSliderMode [IG]}} (string)
--			Specifies when the vertical
--			[[#tek.ui.class.scrollbar : ScrollBar]] should be visible:
--				- "on" - The vertical scrollbar is displayed
--				- "off" - The vertical scrollbar is not displayed
--				- "auto" - The vertical scrollbar is displayed when
--				the ListGadget is taller than the currently visible area.
--			Note: The use of the "auto" mode is currently (v8.0) discouraged.
--
--	OVERRIDES::
--		- Element:cleanup()
--		- Area:layout()
--		- Class.new()
--		- Area:refresh()
--		- Area:relayout()
--		- Element:setup()
--
-------------------------------------------------------------------------------

local ui = require "tek.ui"
local Region = require "tek.lib.region"
local Group = ui.Group
local Image = ui.Image
local ScrollBar = ui.ScrollBar
local floor = math.floor
local insert = table.insert
local ipairs = ipairs
local min = math.min
local max = math.max
local overlap = Region.overlapCoords
local remove = table.remove
local type = type
local unpack = unpack

module("tek.ui.class.scrollgroup", tek.ui.class.group)
_VERSION = "ScrollGroup 9.3"

-------------------------------------------------------------------------------
--	ScrollGroup:
-------------------------------------------------------------------------------

local ScrollGroup = _M

function ScrollGroup.new(class, self)
	self = self or { }

	self.NotifyLeft =
		{ self, "onSetCanvasLeft", ui.NOTIFY_VALUE, ui.NOTIFY_OLDVALUE }
	self.NotifyTop =
		{ self, "onSetCanvasTop", ui.NOTIFY_VALUE, ui.NOTIFY_OLDVALUE }

	self.CopyAreaList = { }
	self.Orientation = "vertical"
	self.HSliderEnabled = false
	self.HSliderGroup = self.HSliderGroup or false
	self.HSliderMode = self.HSliderMode or "off"
	self.HSliderNotify = { self, "onSetSliderLeft", ui.NOTIFY_VALUE }
	self.VSliderMode = self.VSliderMode or "off"
	self.VSliderEnabled = false
	self.VSliderGroup = self.VSliderGroup or false
	self.VSliderNotify = { self, "onSetSliderTop", ui.NOTIFY_VALUE }

	local hslider, vslider

	if self.HSliderMode ~= "off" and not self.HSliderGroup then
		hslider = ScrollBar:new
		{
			Orientation = "horizontal",
			Min = 0,
			Step = 10,
		}
		self.HSliderGroup = hslider
		self.HSliderEnabled = true
	end

	if self.VSliderMode ~= "off" and not self.VSliderGroup then
		vslider = ScrollBar:new
		{
			Orientation = "vertical",
			Min = 0,
			Step = 10,
		}
		self.VSliderGroup = vslider
		self.VSliderEnabled = true
	end

	self.Children =
	{
		Group:new
		{
			Children =
			{
				self.Child,
				vslider,
			},
		},
		hslider,
	}

	return Group.new(class, self)
end

function ScrollGroup:askMinMax(m1, m2, m3, m4)
	m1, m2, m3, m4 = Group.askMinMax(self, m1, m2, m3, m4)
	if self.HSliderMode == "auto" and self.Child.MinWidth == 0 then
		self.Child.MinWidth = m1
	end
	if self.VSliderMode == "auto" and self.Child.MinHeight == 0 then
		self.Child.MinHeight = m2
	end
	return m1, m2, m3, m4
end

-------------------------------------------------------------------------------
--	setup: overrides
-------------------------------------------------------------------------------

function ScrollGroup:setup(app, window)
	Group.setup(self, app, window)
	local c = self.Child
	c:addNotify("CanvasLeft", ui.NOTIFY_CHANGE, self.NotifyLeft, 1)
	c:addNotify("CanvasTop", ui.NOTIFY_CHANGE, self.NotifyTop, 1)
	if self.HSliderGroup then
		self.HSliderGroup.Slider:addNotify("Value", ui.NOTIFY_CHANGE,
			self.HSliderNotify)
	end
	if self.VSliderGroup then
		self.VSliderGroup.Slider:addNotify("Value", ui.NOTIFY_CHANGE,
			self.VSliderNotify)
	end
end

-------------------------------------------------------------------------------
--	cleanup: overrides
-------------------------------------------------------------------------------

function ScrollGroup:cleanup()
	if self.VSliderGroup then
		self.VSliderGroup.Slider:remNotify("Value", ui.NOTIFY_CHANGE,
			self.VSliderNotify)
	end
	if self.HSliderGroup then
		self.HSliderGroup.Slider:remNotify("Value", ui.NOTIFY_CHANGE,
			self.HSliderNotify)
	end
	local c = self.Child
	c:remNotify("CanvasTop", ui.NOTIFY_CHANGE, self.NotifyTop)
	c:remNotify("CanvasLeft", ui.NOTIFY_CHANGE, self.NotifyLeft)
	Group.cleanup(self)
end

-------------------------------------------------------------------------------
--	enableHSlider:
-------------------------------------------------------------------------------

function ScrollGroup:enableHSlider(onoff)
	local enabled = self.HSliderEnabled
	if onoff and not enabled then
		self:addMember(self.HSliderGroup, 2)
		enabled = true
	elseif not onoff and enabled then
		self:remMember(self.HSliderGroup, 2)
		enabled = false
	end
	self.HSliderEnabled = enabled
	return enabled
end

-------------------------------------------------------------------------------
--	enableVSlider:
-------------------------------------------------------------------------------

function ScrollGroup:enableVSlider(onoff)
	local enabled = self.VSliderEnabled
	if onoff and not enabled then
		self.Children[1]:addMember(self.VSliderGroup, 2)
		enabled = true
	elseif not onoff and enabled then
		self.Children[1]:remMember(self.VSliderGroup, 2)
		enabled = false
	end
	self.VSliderEnabled = enabled
	return enabled
end

-------------------------------------------------------------------------------
--	onSetCanvasWidth:
-------------------------------------------------------------------------------

function ScrollGroup:onSetCanvasWidth(w)
	local c = self.Child
	local r = c.Rect
	local sw = r[3] - r[1] + 1
	local g = self.HSliderGroup
	if g then
		g.Slider:setValue("Range", w)
		g.Slider:setValue("Max", w - sw)
	end
	self.Child:setValue("CanvasWidth", w)

	self:enableHSlider(self.HSliderMode == "on"
		or self.HSliderMode == "auto" and (sw < w))
end

-------------------------------------------------------------------------------
--	onSetCanvasHeight:
-------------------------------------------------------------------------------

function ScrollGroup:onSetCanvasHeight(h)
	local c = self.Child
	local r = c.Rect
	local sh = r[4] - r[2] + 1
	local g = self.VSliderGroup
	if g then
		g.Slider:setValue("Range", h)
		g.Slider:setValue("Max", h - sh)
	end
	self.Child:setValue("CanvasHeight", h)
	self:enableVSlider(self.VSliderMode == "on"
		or self.VSliderMode == "auto" and (sh < h))
end

-------------------------------------------------------------------------------
--	onSetCanvasLeft:
-------------------------------------------------------------------------------

function ScrollGroup:onSetCanvasLeft(x, ox)
	local c = self.Child
	local r1, _, r3 = c:getRectangle()
	if r1 then
		ox = ox or c.CanvasLeft
		x = max(0, min(c.CanvasWidth - (r3 - r1 + 1), floor(x)))
		local dx = ox - x
		c.CanvasLeft = x
		if self.HSliderGroup then
			self.HSliderGroup.Slider:setValue("Value", x)
		end
		self.Child:setValue("CanvasLeft", x)
		if dx ~= 0 then
			insert(self.CopyAreaList, { dx, 0 })
		end
	end
end

-------------------------------------------------------------------------------
--	onSetCanvasTop:
-------------------------------------------------------------------------------

function ScrollGroup:onSetCanvasTop(y, oy)
	local c = self.Child
	local _, r2, _, r4 = c:getRectangle()
	if r2 then
		oy = oy or c.CanvasTop
		y = max(0, min(c.CanvasHeight - (r4 - r2 + 1), floor(y)))
		local dy = oy - y
		c.CanvasTop = y
		if self.VSliderGroup then
			self.VSliderGroup.Slider:setValue("Value", y)
		end
		self.Child:setValue("CanvasTop", y)
		if dy ~= 0 then
			insert(self.CopyAreaList, { 0, dy })
		end
	end
end

-------------------------------------------------------------------------------
--	exposeArea:
-------------------------------------------------------------------------------

function ScrollGroup:exposeArea(r1, r2, r3, r4)
	self.Child:markDamage(r1, r2, r3, r4)
end

-------------------------------------------------------------------------------
--	copyArea:
-------------------------------------------------------------------------------

function ScrollGroup:copyArea(...)
	self.Drawable:copyArea(...)
end

-------------------------------------------------------------------------------
--	layout: overrides
-------------------------------------------------------------------------------

function ScrollGroup:layout(r1, r2, r3, r4, markdamage)
	local res = Group.layout(self, r1, r2, r3, r4, markdamage)
	local c = self.Child
	self:onSetCanvasWidth(c.CanvasWidth)
	self:onSetCanvasHeight(c.CanvasHeight)
	self:onSetCanvasTop(c.CanvasTop)
	self:onSetCanvasLeft(c.CanvasLeft)
	return res
end

-------------------------------------------------------------------------------
--	relayout: overrides
-------------------------------------------------------------------------------

function ScrollGroup:relayout(e, r1, r2, r3, r4)
	local res, changed = self.Child:relayout(e, r1, r2, r3, r4)
	if res then
		return res, changed
	end
	return Group.relayout(self, e, r1, r2, r3, r4)
end

-------------------------------------------------------------------------------
--	refresh: overrides
-------------------------------------------------------------------------------

function ScrollGroup:refresh()

	-- handle scrolling:
	local cs = self.Window.CanvasStack
	insert(cs, self.Child)

	-- determine cumulative copyarea shift:
	local dx, dy = 0, 0
	while #self.CopyAreaList > 0 do
		local c = remove(self.CopyAreaList, 1)
		dx = dx + c[1]
		dy = dy + c[2]
	end

	if dx ~=0 or dy ~= 0 then

		-- determine own and parent canvas:
		local canvas = self.Child
		local parent = cs[#cs - 1] or cs[1]

		-- calc total canvas shift for self and parent:
		local ax, ay = 0, 0
		local bx, by = 0, 0
		for i = 1, #cs - 1 do
			bx, by = ax, ay
			ax = ax + cs[i].Rect[1] - cs[i].CanvasLeft
			ay = ay + cs[i].Rect[2] - cs[i].CanvasTop
		end

		-- overlap self and parent:
		local a1, a2, a3, a4 = unpack(canvas.Rect)
		local b1, b2, b3, b4 = unpack(parent.Rect)
		a1, a2, a3, a4 = overlap(a1 + ax, a2 + ay, a3 + ax, a4 + ay,
			b1 + bx, b2 + by, b3 + bx, b4 + by)
		if a1 then

			-- overlap with top rect:
			a1, a2, a3, a4 = overlap(a1, a2, a3, a4, unpack(cs[1].Rect))
			if a1 then

				local d = self.Drawable

				-- make relative:
				local sx, sy = d:getShift()
				a1 = a1 - sx
				a2 = a2 - sy
				a3 = a3 - sx
				a4 = a4 - sy

				-- region that needs to get refreshed (relative):
				local dr = Region.new(a1, a2, a3, a4)

				local x0, y0, x1, y1 = unpack(canvas.Rect)
				x0, y0, x1, y1 = overlap(x0, y0, x1, y1,
					a1 + dx, a2 + dy, a3 + dx, a4 + dy)
				if x0 then

					dr:subRect(x0, y0, x1, y1)

					d:pushClipRect(a1, a2, a3, a4)

					-- copy area, collecting exposures from obscured regions:

					local t = { }

					self:copyArea(a1, a2, a3, a4, a1 + dx, a2 + dy, t)

					-- exposures resulting from obscured areas (make relative):
					for i = 1, #t, 4 do
						self:exposeArea(t[i] - sx, t[i+1] - sy,
							t[i+2] - sx, t[i+3] - sy)
					end

					-- exposures resulting from areas shifting into canvas:
					for _, r1, r2, r3, r4 in dr:getRects() do
						self:exposeArea(r1, r2, r3, r4)
					end

					d:popClipRect()

				else
					-- refresh all:
					self:exposeArea(a1, a2, a3, a4)
				end
			end
		end
	end

	-- refresh group contents (including damages caused by scrolling):

	Group.refresh(self)

	remove(cs)
end

function ScrollGroup:onSetSliderTop(val)
	self:onSetCanvasTop(val)
end

function ScrollGroup:onSetSliderLeft(val)
	self:onSetCanvasLeft(val)
end
