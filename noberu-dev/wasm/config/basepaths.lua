-- Paths inside the browser build's virtual filesystem.
-- /gamedata/<profile>/... are the user's files (WebFileStream), /saves is IDBFS.
root.BasePaths = {
  RootInstallDir = "/",
  RootGamedataDir = "/gamedata",
  RootProfilesDir = "/profiles",
  RootPatchesDir = "/patches",
  RootSavesDir = "/saves",
};
