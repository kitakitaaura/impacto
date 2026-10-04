-- Steam FONT.PNG keeps the PS3 glyph ids (punctuation flags in sgps3/charset.lua
-- still line up) but has 46 rows, and the PS3 table never marks Latin letters,
-- so lines broke mid-word. Latin glyphs: 128-137 digits, 138-163 A-Z,
-- 164-189 a-z; 201 is the apostrophe (’).
for i = 64 * 37, (64 * 46) - 1 do root.Charset.Flags[i] = 0; end

local function alphabet(id)
    root.Charset.Flags[id] = root.Charset.Flags[id] | CharacterTypeFlags.Alphabet;
end
for id = 128, 189 do alphabet(id); end
alphabet(201);
