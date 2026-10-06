-- Steam DATA01 moved the system message art, so the PS3 rectangles from
-- sgps3/hud/sysmesboxdisplay.lua land on the menu icons. Rectangles here are
-- Steam pixels / 1.5 (the sheet's design size).
local sheet = "Data";
local name = "SysMesBox";

local function px(x, y, w, h)
    return { X = x / 1.5, Y = y / 1.5, Width = w / 1.5, Height = h / 1.5 };
end

-- Full-width dark band (darker towards the bottom).
root.Sprites[name .. "Box"] = { Sheet = sheet, Bounds = px(1150, 1207, 1920, 328) };

-- The selected choice gets the glowing underline from the top-left corner,
-- stretched over the text; no separate end caps.
root.Sprites[name .. "SelectionMiddlePart"] = { Sheet = sheet, Bounds = px(66, 9, 92, 23) };
root.Sprites[name .. "SelectionLeftPart"] = { Sheet = sheet, Bounds = { X = 0, Y = 0, Width = 0, Height = 1 } };
root.Sprites[name .. "SelectionRightPart"] = { Sheet = sheet, Bounds = { X = 0, Y = 0, Width = 0, Height = 1 } };

root.SysMesBoxDisplay.BoxY = 200;
root.SysMesBoxDisplay.HighlightXOffset = 0;
root.SysMesBoxDisplay.HighlightYOffset = 22;
