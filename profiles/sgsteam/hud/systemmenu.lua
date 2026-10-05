-- STEINS;GATE Steam in-game menu, from DATA01 (system.mpk 6): each item is
-- an icon + label strip at x=927 (up to 290 px wide); order is the script's
-- SW_SYSMENUCNO jump table: BACK LOG, QUICK SAVE, QUICK LOAD, SAVE, LOAD,
-- TIPS LIST, CONFIG, FULL SCREEN, HELP, TITLE MENU, CLOSE MENU, EXIT GAME.
local rows = { {7, 49}, {73, 49}, {140, 48}, {204, 48}, {271, 47}, {335, 49},
               {400, 50}, {468, 45}, {532, 49}, {594, 57}, {663, 49}, {728, 51} };
local items = {};
for i, r in ipairs(rows) do
    local name = "SGSysMenuItem" .. i;
    root.Sprites[name] = { Sheet = "Data",
        Bounds = { X = 927 / 1.5, Y = r[1] / 1.5, Width = 290 / 1.5, Height = r[2] / 1.5 } };
    items[i] = name;
end
root.Sprites["SGSysMenuFrame"] = { Sheet = "Data",
    Bounds = { X = 1272 / 1.5, Y = 43 / 1.5, Width = 61 / 1.5, Height = 708 / 1.5 } };

root.SystemMenu = {
    Type = SystemMenuType.SGHD,
    DrawType = DrawComponentType.SystemMenu,
    MenuEntriesNum = 0,
    MenuEntriesHNum = 0,
    ItemSprites = items,
    FrameSprite = "SGSysMenuFrame",
    FramePos = { X = 846, Y = 104 },
    ItemsPos = { X = 884, Y = 110 },
    ItemSpacing = 42,
    UnselectedBrightness = 0.55,
};
