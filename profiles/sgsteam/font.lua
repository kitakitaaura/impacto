-- Steam FONT.PNG keeps the PS3's 64 columns but has 46 rows (adds kanji).
-- Glyphs beyond the PS3 table use the em width until we have Steam's widths.
local font = root.Fonts["Default"];
font.GridSize = { X = 64, Y = 46 };
for i = #font.AdvanceWidthsTable + 1, 64 * 46 do
    font.AdvanceWidthsTable[i] = font.AdvanceWidthsEmWidth;
end
