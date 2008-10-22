-------------------------------------------------------------------------------
--
--	tek.ui.class.checkmark
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
--		[[#tek.ui.class.text : Text]] /
--		CheckMark
--
--	OVERVIEW::
--		Specialization of a [[#tek.ui.class.text : Text]] for placing
--		checkmarks.
--
--	OVERRIDES::
--		- Area:askMinMax()
--		- Area:draw()
--		- Area:hide()
--		- Object.init()
--		- Area:layout()
--		- Class.new()
--		- Area:setState()
--		- Area:show()
--
-------------------------------------------------------------------------------

local ui = require "tek.ui"
local Text = ui.Text
local VectorImage = ui.VectorImage

local floor = math.floor
local ipairs = ipairs
local max = math.max

module("tek.ui.class.checkmark", tek.ui.class.text)
_VERSION = "CheckMark 2.20"

-------------------------------------------------------------------------------
--	Constants & Class data:
-------------------------------------------------------------------------------

local coords =
{
	-- shadow:
	-3,5, -5,3, -3,-2, -5,-3, -2,-3, -3,-5, 5,-3, 3,-5,
	-- shine:
	-3,3, 3,5, 2,3, 5,3, 3,2, 3,-3,
	-- check:
	0,0, -2,3, -3,2, -1,0, -3,-2, -2,-3, 0,-1, 2,-3, 3,-2, 1,0, 3,2, 2,3, 0,1
}

-- shadow:
local points1 = { 1, 9, 10, 11, 12, 13, 7, 14 }
-- shine:
local points2 = { 1, 2, 3, 4, 5, 6, 7, 8 }
-- check:
local points3 = { 15,16,17,18,19,20,21,22,23,24,25,26,27,16 }

local CheckImage1 = VectorImage:new
{
	ImageData =
	{
		Coords = coords,
		Primitives =
		{
			{ 0x1000, 8, Points = points1, Pen = ui.PEN_BORDERSHINE },
			{ 0x1000, 8, Points = points2, Pen = ui.PEN_BORDERSHADOW },
		},
		MinMax = { -5, 5, 5, -5 },
	}
}

local CheckImage2 = VectorImage:new
{
	ImageData =
	{
		Coords = coords,
		Primitives = {
			{ 0x1000, 8, Points = points1, Pen = ui.PEN_BORDERSHINE },
			{ 0x1000, 8, Points = points2, Pen = ui.PEN_BORDERSHADOW },
			{ 0x2000, 14, Points = points3, Pen = ui.PEN_DETAIL },
		},
		MinMax = { -5, 5, 5, -5 },
	}
}

local DEF_IMAGEMINHEIGHT = 18

-------------------------------------------------------------------------------
--	Class implementation:
-------------------------------------------------------------------------------

local CheckMark = _M

function CheckMark.new(class, self)
	self = self or { }
	self.ImageRect = { 0, 0, 0, 0 }
	return Text.new(class, self)
end

function CheckMark.init(self)
	self.AltImage = self.AltImage or CheckImage2
	self.Image = self.Image or CheckImage1
	self.ImageHeight = false
	self.ImageMinHeight = self.ImageMinHeight or DEF_IMAGEMINHEIGHT
	self.ImageWidth = false
	self.Mode = self.Mode or "toggle"
	self.OldSelected = false
	return Text.init(self)
end

-------------------------------------------------------------------------------
--	hide:
-------------------------------------------------------------------------------

function CheckMark:hide()
	if self.TextRecords then
		self.TextRecords[1] = false
	end
	Text.hide(self)
end

-------------------------------------------------------------------------------
--	askMinMax:
-------------------------------------------------------------------------------

function CheckMark:askMinMax(m1, m2, m3, m4)
	local tr = self.TextRecords and self.TextRecords[1]
	if tr then
		local w, h = tr[9], tr[10]
		self.ImageHeight = max(h, self.ImageMinHeight or h)
		self.ImageWidth = self.ImageHeight * self.Drawable.AspectX /
			self.Drawable.AspectY
		h = max(0, self.ImageHeight - h)
		local h2 = floor(h / 2)
		tr[5] = self.ImageWidth
		tr[6] = h2
		tr[7] = 0
		tr[8] = h - h2
	end
	return Text.askMinMax(self, m1, m2, m3, m4)
end

-------------------------------------------------------------------------------
--	layout:
-------------------------------------------------------------------------------

function CheckMark:layout(x0, y0, x1, y1, markdamage)
	if Text.layout(self, x0, y0, x1, y1, markdamage) then
		local i = self.ImageRect
		local r = self.Rect
		local p = self.Padding
		local eh = r[4] - r[2] - p[4] - p[2] + 1
		local iw = self.ImageWidth
		local ih = self.ImageHeight
		i[1] = r[1] + p[1]
		i[2] = r[2] + p[2] + floor((eh - ih) / 2)
		i[3] = i[1] + iw - 1
		i[4] = i[2] + ih - 1
		return true
	end
end

-------------------------------------------------------------------------------
--	draw:
-------------------------------------------------------------------------------

function CheckMark:draw()
	Text.draw(self)
	local img = self.Selected and self.AltImage or self.Image
	if img then
		img:draw(self.Drawable, self.ImageRect)
	end
end

-------------------------------------------------------------------------------
--	setState:
-------------------------------------------------------------------------------

function CheckMark:setState(bg, fg)
	if self.Selected ~= self.OldSelected then
		self.OldSelected = self.Selected
		self.Redraw = true
	end
	Text.setState(self, bg, fg)
end
