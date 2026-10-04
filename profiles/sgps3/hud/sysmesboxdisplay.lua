local sheet = "Data";
local name = "SysMesBox";

root.SysMesBoxDisplay = {
    Type = SysMesBoxType.CHLCC,
    DrawType = DrawComponentType.SystemMessage,
    BoxX = 0,
    BoxY = 230,
    TextFontSize = 32,
    TextMiddleY = 236,
    TextX = 640,
    TextLineHeight = 34,
    TextMarginY = 14,
    ChoicePadding = 40,
    ChoiceY = 365,
    ChoiceXBase = 680,
    MinMaxMesWidth = 294,
    MinHighlightWidth = 48,
    HighlightBaseWidth = 144,
    HighlightYOffset = 0,
    HighlightXOffset = 0,
    HighlightXBase = 658,
    HighlightXStep = 132,
    HighlightRightPartSpriteWidth = 24,
    AnimationSpeed = 55,
    FadeInDuration = 0.33,
    FadeOutDuration = 0.25,
    -- Required by the shared CHLCC SysMesBox; S;G has no loading stars.
    LoadingStarsPosition = { X = 0, Y = 0 },
    LoadingStarsFadeDuration = 0
};

root.Sprites[name .. "Box"] = {
    Sheet = sheet,
    Bounds = {
        X = 767,
        Y = 637,
        Width = 1280,
        Height = 172
    }
};
root.SysMesBoxDisplay.Box = name .. "Box";

root.Sprites[name .. "BoxDecoration"] = {
    Sheet = sheet,
    Bounds = {
        X = 1383,
        Y = 117,
        Width = 592,
        Height = 8
    }
};
root.SysMesBoxDisplay.BoxDecoration = name .. "BoxDecoration";

root.Sprites[name .. "SelectionLeftPart"] = {
    Sheet = sheet,
    Bounds = {
        X = 502,
        Y = 51,
        Width = 144,
        Height = 38
    }
};
root.SysMesBoxDisplay.SelectionLeftPart = name .. "SelectionLeftPart";

root.Sprites[name .. "SelectionRightPart"] = {
    Sheet = sheet,
    Bounds = {
        X = 634,
        Y = 51,
        Width = 24,
        Height = 38
    }
};
root.SysMesBoxDisplay.SelectionRightPart = name .. "SelectionRightPart";

root.Sprites[name .. "SelectionMiddlePart"] = {
    Sheet = sheet,
    Bounds = {
        X = 515,
        Y = 51,
        Width = 132,
        Height = 38
    }
};
root.SysMesBoxDisplay.SelectionMiddlePart = name .. "SelectionMiddlePart";
root.Sprites[name .. "LoadingStar"] = {
    Sheet = sheet,
    Bounds = { X = 0, Y = 0, Width = 0, Height = 0 }
};
root.SysMesBoxDisplay.LoadingStar = name .. "LoadingStar";
