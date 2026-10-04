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

root.SaveData.SaveFilePath = root.BasePaths.RootSavesDir .. "/sgsteam/SYSTEM.DAT";

-- The PS3 profile borrows CHLCC's title menu; Steam gets its own.
include(root.BasePaths.RootProfilesDir .. '/sgsteam/hud/titlemenu.lua');
