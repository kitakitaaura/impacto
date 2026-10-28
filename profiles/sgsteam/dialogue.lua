-- Dialogue tweaks for Steam S;G (see the Steam store screenshots: the name is
-- centered under the text box between two tab lines, "-- Mayuri --").
-- The script's mode 0 data is FixedPos, centered, NamePos.x 200, width 880,
-- i.e. centered on x=640; impacto's FixedPos centers on NamePos.x + width.
root.Dialogue.TextModesInfo = {
    [0] = { NamePos = { X = 640 - 880, Y = 674 } },
};
root.Dialogue.NametagCenterX = 640;
root.Dialogue.NametagPosition = { X = 0, Y = 680 };
-- Steam strings write the colour tag as an expression (04 82 00 00 = colour
-- 2), not the PS3's single byte; reading one byte left "00 00" as two line
-- breaks and, for variable colours, logged "unrecognized token 0x14".
root.Dialogue.ColorTagIsUint8 = false;
