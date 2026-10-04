-- PS3 sheet name -> Steam system.mpk entry. Design sizes are the Steam texture
-- size divided by 1.5, so PS3 sprite rectangles land on the same artwork.
local function sheet(id, w, h)
    return { Path = { Mount = "system", Id = id }, DesignWidth = w / 1.5, DesignHeight = h / 1.5 };
end

root.SpriteSheets["Backlog"]  = sheet(2, 2048, 1080);   -- BACKLOG.DDS
root.SpriteSheets["Data"]     = sheet(6, 3072, 1788);   -- DATA01.DDS
root.SpriteSheets["Font"]     = sheet(9, 3072, 2208);   -- FONT.PNG
root.SpriteSheets["Menu"]     = sheet(5, 3072, 2048);   -- CONFIG.DDS (guess)
-- Steam merged the PS3's Title/TitleBg1/TitleBg2 into one TITLE_CHIP sheet,
-- so the title menu sprites need remapping before it looks right.
root.SpriteSheets["Title"]    = sheet(30, 3072, 1536);
root.SpriteSheets["TitleBg1"] = sheet(30, 3072, 1536);
root.SpriteSheets["TitleBg2"] = sheet(30, 3072, 1536);
