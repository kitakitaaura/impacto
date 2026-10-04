-- Upstream game list plus the Steins;Gate profiles noberu is working on.
include(root.BasePaths.RootInstallDir .. "/gamedefinitions.lua");

root.GameDefinitions.sgps3 = {
  Name = "STEINS;GATE (PS3, English)",
  LauncherOrderId = 9,
  GameProfile = root.BasePaths.RootProfilesDir .. "/sgps3/game.lua",
};

root.GameDefinitions.sgsteam = {
  Name = "STEINS;GATE (Steam)",
  LauncherOrderId = 10,
  GameProfile = root.BasePaths.RootProfilesDir .. "/sgsteam/game.lua",
};
