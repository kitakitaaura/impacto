-- Dev paths for running the native build from the repo (see noberu-dev/run.sh).
-- run.sh substitutes @ROOT@ with the impacto checkout.
root.BasePaths = {
  RootInstallDir = "@ROOT@/install/Release",
  RootGamedataDir = "@ROOT@/gamedata",
  RootProfilesDir = "@ROOT@/profiles",
  RootPatchesDir = "@ROOT@/patches",
  RootSavesDir = "@ROOT@/noberu-dev/saves",
};
