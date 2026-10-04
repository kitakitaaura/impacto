local dir = root.BasePaths.RootGamedataDir .. "/sgsteam/USRDIR";

root.Vfs = {
    Mounts = {
        ["script"] = {dir .. "/script.mpk"},
        ["system"] = {dir .. "/system.mpk"},
        ["bgm"] = {dir .. "/bgm.mpk"},
        ["se"] = {dir .. "/se.mpk"},
        ["voice"] = {dir .. "/voice.mpk"},
        ["bg"] = {dir .. "/bg.mpk"},
        ["chara"] = {dir .. "/chara.mpk"},
        ["mask"] = {dir .. "/mask.mpk"},
        -- Movies are loose Bink 2 files under USRDIR/movie/, which ffmpeg
        -- can't decode. Not mounted yet.
    }
};
