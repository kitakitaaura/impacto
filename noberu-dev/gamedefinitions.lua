-- Upstream game list plus the Steins;Gate profiles noberu is working on.
include(root.BasePaths.RootInstallDir .. "/gamedefinitions.lua");

root.GameDefinitions.sgps3 = {
  Name = "STEINS;GATE (PS3, English)",
  LauncherOrderId = 9,
  GameProfile = root.BasePaths.RootProfilesDir .. "/sgps3/game.lua",
};
