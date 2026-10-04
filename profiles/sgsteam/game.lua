-- STEINS;GATE, Steam (2016) release.
-- Built on the English PS3 profile: same game and script engine, but the Steam
-- data ships as little-endian MPK archives with textures redrawn at 1.5x. Logic
-- stays in the PS3's 1280x720 space; sheets declare PS3-sized design units so
-- the PS3 sprite coordinates map onto the bigger Steam textures.
--
-- gamedata/sgsteam is the game's install folder (the one with USRDIR/).

include(root.BasePaths.RootProfilesDir .. '/sgps3/game.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/vfs.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/sprites.lua');
include(root.BasePaths.RootProfilesDir .. '/sgsteam/font.lua');

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
root.Vm.GameInstructionSet = InstructionSet.SGHD;

root.SaveData.SaveFilePath = root.BasePaths.RootSavesDir .. "/sgsteam/SYSTEM.DAT";

-- The PS3 profile borrows CHLCC's title menu, which now needs CHLCC-only
-- parts (delusion intro). S;G needs its own; off until then.
root.TitleMenu.Type = TitleMenuType.None;
