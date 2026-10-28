local dir = root.BasePaths.RootGamedataDir .. "/sgsteam/USRDIR";

-- Movie ids are indexes into Game.exe's name table (file offset 0x943698),
-- alphabetical but listing IMV049, which the game doesn't ship.
local movies = {
    "ar", "ending_c", "ending_f", "ending_m", "ending_r", "ending_s",
    "imv001", "imv002", "imv003", "imv004", "imv005", "imv006", "imv007",
    "imv008", "imv009", "imv010", "imv018", "imv023", "imv027", "imv034",
    "imv034b", "imv036", "imv037", "imv038", "imv039", "imv041", "imv042",
    "imv043", "imv047", "imv049", "imv050", "imv051", "op", "op2",
    "prologue01", "prologue02", "timeleapbg", "title",
};
local function hasFile(path)
    local file = io.open(path, "rb");
    if file then file:close(); end
    return file ~= nil;
end
-- Only mounted where the files are on disk (the browser build has no movie
-- folder yet).
local movieMounts = {};
if hasFile(dir .. "/movie/1920x1080/title.bk2") then
    table.insert(movieMounts, { Path = dir .. "/movie/1920x1080", Names = movies });
end
-- The English patch (LanguageBarrier) ships subtitled replacements for a
-- few movies; later mounts win, so they take over where present.
local patched = root.BasePaths.RootGamedataDir .. "/sgsteam/languagebarrier/videos/1080p";
if hasFile(patched .. "/imv041.bk2") then
    table.insert(movieMounts, { Path = patched, Names = movies });
end

root.Vfs = {
    Mounts = {
        ["script"] = {dir .. "/script.mpk"},
        ["system"] = {dir .. "/system.mpk"},
        ["bgm"] = {dir .. "/bgm.mpk"},
        ["se"] = {dir .. "/se.mpk"},
        -- No separate system-SE archive on Steam; UI code asks for "sysse".
        ["sysse"] = {dir .. "/se.mpk"},
        ["voice"] = {dir .. "/voice.mpk"},
        ["bg"] = {dir .. "/bg.mpk"},
        ["chara"] = {dir .. "/chara.mpk"},
        ["mask"] = {dir .. "/mask.mpk"},
        -- Loose Bink 2 files; portfiles/ffmpeg adds the Bink 2 decoder.
        ["movie"] = movieMounts,
    }
};
