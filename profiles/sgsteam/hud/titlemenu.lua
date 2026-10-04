-- STEINS;GATE Steam title screen, built from TITLE_CHIP (system.mpk id 30).
-- Rectangles are measured in sheet pixels and divided by 1.5 to get the
-- sheet's design units (see sgsteam/sprites.lua).
local function px(x, y, w, h)
    return { X = x / 1.5, Y = y / 1.5, Width = w / 1.5, Height = h / 1.5 };
end

local function sprite(name, x, y, w, h)
    root.Sprites[name] = { Sheet = "Title", Bounds = px(x, y, w, h) };
    return name;
end

-- Main items: white-on-light (normal) at x=1939, dark (highlighted) at 2140,
-- rows 49 px apart. HELP sits to the right of START.
local function main(name, row)
    return sprite("SGTitle" .. name, 1939, 1 + row * 49.5, 180, 48),
           sprite("SGTitle" .. name .. "Hi", 2140, 1 + row * 49.5, 180, 48);
end

-- Sub items: labels at x=2245, rows 39 px apart from y=199; config rows at 511.
local function sub(name, y)
    local s = sprite("SGTitleSub" .. name, 2245, y, 249, 37);
    return s, s;
end

local startN, startH = main("Start", 0);
local loadN, loadH = main("Load", 1);
local extraN, extraH = main("Extra", 2);
local configN, configH = main("Config", 3);
local helpN = sprite("SGTitleHelp", 2341, 1, 180, 48);
local helpH = sprite("SGTitleHelpHi", 2542, 1, 180, 48);

local function item(id, n, h) return { Id = id, Sprite = n, HighlightSprite = h }; end
local function subitem(id, name, y)
    local n, h = sub(name, y);
    return item(id, n, h);
end

root.TitleMenu = {
    Type = TitleMenuType.SGHD,
    DrawType = DrawComponentType.SystemMenu,
    MenuEntriesNum = 0,

    BackgroundSprite = sprite("SGTitleBackground", 0, 0, 1920, 1060),
    BackgroundPos = { X = 0, Y = 0 },

    PressToStartSprite = sprite("SGTitlePressEnter", 190, 1078, 240, 50),
    PressToStartPos = { X = 110, Y = 600 },
    PressToStartAnimDurationIn = 0.8,
    PressToStartAnimDurationOut = 0.8,
    FadeDuration = 0.3,

    ArrowSprite = sprite("SGTitleArrow", 1930, 199, 34, 37),
    ArrowOffset = { X = -26, Y = 0 },

    MainItemPos = { X = 60, Y = 500 },
    MainItemSpacing = 34,
    SubItemOffset = { X = 150, Y = 0 },
    SubItemSpacing = 26,

    Items = {
        item(0, startN, startH),
        { Id = -1, Sprite = loadN, HighlightSprite = loadH, Sub = {
            subitem(10, "QuickLoad", 199),
            subitem(11, "Load", 238),
        } },
        { Id = -1, Sprite = extraN, HighlightSprite = extraH, Sub = {
            subitem(20, "ClearList", 277),
            subitem(21, "CGLibrary", 316),
            subitem(22, "SoundLibrary", 355),
            subitem(23, "MovieLibrary", 394),
            subitem(24, "Tips", 433),
        } },
        { Id = -1, Sprite = configN, HighlightSprite = configH, Sub = {
            subitem(30, "Config", 511),
            subitem(31, "SystemSave", 550),
        } },
        item(40, helpN, helpH),
    },
};
