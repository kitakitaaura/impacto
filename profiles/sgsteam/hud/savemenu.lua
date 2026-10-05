-- STEINS;GATE Steam save / load / quick-load screen. SAVEMENU, LOADMENU and
-- QLOADMENU (system.mpk 26/12/25) share one layout: a 1920x1080 backdrop and,
-- to its right, the slot plate, NO DATA thumbnail, digits and "/6".
local function px(x, y, w, h)
    return { X = x / 1.5, Y = y / 1.5, Width = w / 1.5, Height = h / 1.5 };
end
local function sheet(id) return { Path = { Mount = "system", Id = id }, DesignWidth = 2048, DesignHeight = 720 }; end
root.SpriteSheets["SGSave"] = sheet(26);
root.SpriteSheets["SGLoad"] = sheet(12);
root.SpriteSheets["SGQuickLoad"] = sheet(25);

local function sprite(name, sheetName, x, y, w, h)
    root.Sprites[name] = { Sheet = sheetName, Bounds = px(x, y, w, h) };
    return name;
end

local digits = {};
for d = 0, 9 do
    digits[d + 1] = sprite("SGSaveDigit" .. d, "SGSave", 1923 + d * 47.4, 172, 46, 62);
end

root.SaveMenu = {
    Type = SaveMenuType.SGHD,
    DrawType = DrawComponentType.SaveMenu,
    FadeInDuration = 0.25,
    FadeOutDuration = 0.25,
    EmptyThumbnailSprite = sprite("SGSaveNoData", "SGSave", 2714, 0, 240, 137),

    SaveBackground = sprite("SGSaveBg", "SGSave", 0, 0, 1920, 1080),
    LoadBackground = sprite("SGLoadBg", "SGLoad", 0, 0, 1920, 1080),
    QuickLoadBackground = sprite("SGQuickLoadBg", "SGQuickLoad", 0, 0, 1920, 1080),
    SlotSprite = sprite("SGSaveSlot", "SGSave", 1920, 0, 794, 170),
    NoDataSprite = "SGSaveNoData",
    CursorSprite = "SGTitleArrow",
    DigitSprites = digits,
    PageTotalSprite = sprite("SGSavePageTotal", "SGSave", 2406, 190, 48, 44),

    SlotsPos = { X = 100, Y = 110 },
    SlotSpacing = { X = 559, Y = 127 },
    Columns = 2,
    Rows = 4,
    Pages = 6,
    ThumbnailOffset = { X = 10, Y = 11 },
    ThumbnailSize = { X = 160, Y = 90 },
    SlotNumberOffset = { X = 182, Y = 14 },
    SlotNumberScale = 0.6,
    PagePos = { X = 1150, Y = 24 },
};
