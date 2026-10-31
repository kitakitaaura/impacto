-- STEINS;GATE Steam backlog, from BACKLOG.DDS (system.mpk 2). The sheet's left
-- 1920x1080 is the screen: the panel spans x 88-1823 with the scroll track
-- built in at x 1790-1823; the voice icon and scrollbar thumb sit to the
-- right. Rectangles are sheet pixels / 1.5 (design units, see sprites.lua).
-- _STARTUP_WIN.SCX opens it as sub menu 1 with MesRev DispInit/AllCls, the
-- same flow as MO6TW, so it uses that implementation.
local function px(x, y, w, h)
    return { X = x / 1.5, Y = y / 1.5, Width = w / 1.5, Height = h / 1.5 };
end

root.BacklogMenu = {
    Type = BacklogMenuType.MO6TW,
    DrawType = DrawComponentType.SystemMenu,

    BacklogBackgroundSprite = "SGBacklogBackground",

    EntryHighlightSprite = "SGBacklogHighlight",
    EntryHighlightLocation = EntryHighlightLocationType.None,
    EntryHighlightOffset = { X = 0, Y = 0 },
    EntryHighlightPadding = 0.0,

    VoiceIconSprite = "SGBacklogVoiceIcon",
    VoiceIconOffset = { X = -42, Y = 0 },

    ScrollbarTrackSprite = "SGBacklogTrack",
    ScrollbarThumbSprite = "SGBacklogThumb",
    ScrollbarPosition = { X = 1200, Y = 88 },
    ScrollbarThumbLength = 68,

    EntriesStart = { X = 0, Y = 96 },
    RenderingBounds = { X = 60, Y = 84, Width = 1130, Height = 630 },
    EntryYPadding = 22,

    HoverBounds = { X = 60, Y = 84, Width = 1130, Height = 630 },

    -- The backlog covers the message window (game.cpp fades it out), so the
    -- line on screen has to be its newest entry.
    AddLineOnDisplay = true,

    FadeInDuration = 0.25,
    FadeOutDuration = 0.25,

    ScrollingSpeed = 600,
    PageUpDownHeight = 560
};

root.Sprites["SGBacklogBackground"] = { Sheet = "Backlog", Bounds = px(0, 0, 1920, 1080) };
root.Sprites["SGBacklogVoiceIcon"] = { Sheet = "Backlog", Bounds = px(1976, 7, 56, 52) };
root.Sprites["SGBacklogThumb"] = { Sheet = "Backlog", Bounds = px(1940, 58, 12, 103) };
-- The track is part of the panel; the scrollbar only needs its length, so
-- point it at a transparent strip of the sheet (x < 88 is empty).
root.Sprites["SGBacklogTrack"] = { Sheet = "Backlog", Bounds = px(0, 132, 12, 930) };
-- No highlight art in the Steam sheet; an empty strip keeps the widget happy.
root.Sprites["SGBacklogHighlight"] = { Sheet = "Backlog", Bounds = px(0, 0, 12, 12) };
