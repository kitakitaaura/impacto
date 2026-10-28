-- STEINS;GATE, Steam (2016) release.
-- Built on the English PS3 profile: same game and script engine, but the Steam
-- data ships as little-endian MPK archives with textures redrawn at 1.5x. Logic
-- stays in the PS3's 1280x720 space; sheets declare PS3-sized design units so
-- the PS3 sprite coordinates map onto the bigger Steam textures.
--
-- gamedata/sgsteam is the game's install folder (the one with USRDIR/).

include(root.BasePaths.RootProfilesDir .. '/sgps3/game.lua');
-- Script variables: Steam S;G uses the same map as Chaos;Head Noah (common +
-- chn overrides), not the PS3 one. Its BG macros write SW[4500 + 40*bg] (main),
-- SW[2400 + 10*bg] (offsets), SW[5000 + 20*bg] (captures) and the logo fades
-- use SW 4400/4401 (MASK1COLOR/ALPHA).
include(root.BasePaths.RootProfilesDir .. '/sgsteam/scriptvars.lua');
include(root.BasePaths.RootProfilesDir .. '/chn/scriptvars.lua');
-- Steam S;G has no message-window type flags; the common index (4422) is a
-- mask field here (MACROSYS sets SW 4422 = 100), which read as DontDrawText.
-- Point it at a block no Steam script references so the flags stay 0.
root.ScriptVars.SW_MESWIN0TYPE = 7000;
include(root.BasePaths.RootProfilesDir .. '/sgsteam/vfs.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/sprites.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/font.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/charset.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/dialogue.lua');

-- Keys from the game's own HELP pages (manual.mpk): 1 system menu, Shift hide
-- text, Backspace back, Ctrl force skip, E skip mode, Z/C phone, Insert/F2
-- backlog, Delete auto. Shift shares B with Back, as on the pad ("BACK / HIDE
-- WINDOW"). The original quits on Escape; here Escape works like right click
-- (open the menu, or back out), which is what browser players reach for.
for _, pad in ipairs({ root.PADinput.PAD1START, root.PADinput.PAD1B }) do
    table.insert(root.Input.PADtoKBcustom[pad], { Id = 60, Mode = KbInputMode.Any });
end
-- F2 (and the wheel, see vm/interface/input.cpp) opens the backlog, which
-- _STARTUP_WIN.SCX reads as PADcustom 12 (Y, also Insert).
table.insert(root.Input.PADtoKBcustom[root.PADinput.PAD1Y], { Id = 52, Mode = KbInputMode.Tap });
table.insert(root.Input.KBcustom[5], KeyboardScanCode._LSHIFT);
table.insert(root.Input.KBcustom[5], KeyboardScanCode._RSHIFT);
-- Steam keys: hold Ctrl to skip (fast-forward, L1), F3 toggles Auto (X).
-- Ctrl counts only while held, so it never flips save pages like L1 taps do.
table.insert(root.Input.PADtoKBcustom[root.PADinput.PAD1L1], { Id = 50, Mode = KbInputMode.Held });
table.insert(root.Input.PADtoKBcustom[root.PADinput.PAD1X], { Id = 53, Mode = KbInputMode.Tap });

-- Phone trigger: _STARTUP_WIN.SCX opens the phone on PADcustom slot 22, which
-- the shared table leaves empty. Bind it to L3 (unused), Z/C (the manual's
-- keys), Tab/P and middle click.
for _, list in ipairs({ root.Input.PADcustomA, root.Input.PADcustomB }) do
    list[23] = root.PADinput.PAD1L3;  -- Lua is 1-based: slot 22
end
root.Input.KBcustom[10] = { KeyboardScanCode._Z, KeyboardScanCode._C,
                            KeyboardScanCode._TAB, KeyboardScanCode._P };
root.Input.PADtoMS[root.PADinput.PAD1L3] = 2;

root.WindowName = "STEINS;GATE";

-- Steam .lay files: little-endian, texture coordinates already in pixels,
-- plus one trailing byte per chunk that the loader doesn't need.
root.AssetScale = 1.5;
root.LayFileBigEndian = false;
root.LayFileTexXMultiplier = 1;
root.LayFileTexYMultiplier = 1;

-- Steam scripts store a u16 return id after every Call/CallFar target
-- (_STARTUP_WIN.SCX: CallFar at 0x62e is followed by 00 00, then the next CallFar).
root.Vm.UseReturnIds = true;
root.Vm.ScrWorkChaStructSize = 40;
root.Vm.ScrWorkChaOffsetStructSize = 10;
root.Vm.ScrWorkBgStructSize = 40;
root.Vm.ScrWorkBgOffsetStructSize = 10;
root.Vm.ScrWorkCaptureStructSize = 20;
root.Vm.ScrWorkCaptureOffsetStructSize = 10;
root.Vm.ScrWorkCaptureEffectInfoStructSize = 3;
root.Vm.ScrWorkBgEffStructSize = 30;
root.Vm.ScrWorkBgEffOffsetStructSize = 20;
root.Vm.GameInstructionSet = InstructionSet.SGHD;

root.SaveData.Type = SaveDataType.SGHD;
root.SaveData.SaveFilePath = root.BasePaths.RootSavesDir .. "/sgsteam/SAVEDATA.DAT";
include(root.BasePaths.RootProfilesDir .. '/sgsteam/messagedata.lua');

-- The PS3 profile borrows CHLCC's title menu; Steam gets its own.
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/titlemenu.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/savemenu.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/systemmenu.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/sysmesboxdisplay.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/backlogmenu.lua');
