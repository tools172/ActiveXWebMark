#include "theme/theme_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

#include "theme/theme_source.h"

namespace awm::theme {

namespace {

struct ThemeSpec {
    const char* id;
    const char* name;
    bool dark;

    ThemeColor pageBg;
    ThemeColor editorColor80;
    ThemeColor editorColor50;
    ThemeColor editorColor10;
    ThemeColor editorColor04;
    ThemeColor accent;
    ThemeColor danger;
    ThemeColor codeBlockBg;
    ThemeColor codeBlockColor;
    ThemeColor tableBorder;
    ThemeColor hr;
    ThemeColor link;
    ThemeColor strong;
    ThemeColor em;
    ThemeColor listMarker;
    ThemeColor headings[6];
    ThemeColor blockquoteText;
    ThemeColor blockquoteBorder;
    ThemeColor sideBarBg;
    ThemeColor sideBarItemHover;
    ThemeColor selectionColor;
    ThemeColor findMatchBg;
    ThemeColor findCurrentBg;
    ThemeColor findText;
    TokenColors tokens;
    const char* author = "";
    ThemeFamily family = ThemeFamily::Neutral;
};

EditorScheme DeriveEditor(const PreviewColors& preview, const ChromeColors& chrome)
{
    EditorScheme editor;
    editor.background = preview.pageBg;
    editor.text = preview.pageText;
    editor.heading = preview.headings[1];
    editor.emphasis = preview.pageText;
    editor.strong = preview.strong;
    editor.code = preview.inlineCodeText;
    editor.codeBlock = preview.codeBlockText;
    editor.link = preview.link;
    editor.image = preview.link;
    editor.listMarker = preview.listMarker;
    editor.blockQuote = preview.blockquoteText;
    editor.html = preview.mutedText;
    editor.hrule = preview.hr;
    editor.comment = preview.mutedText;
    editor.reference = chrome.accent;
    editor.gutterBg = chrome.gutterBg;
    editor.gutterText = chrome.textSecondary;
    editor.currentLine = chrome.currentLine;
    editor.selection = chrome.selection;
    editor.selectionText = preview.pageText;
    editor.caret = chrome.accent;
    return editor;
}

ThemeDefinition BuildTheme(const ThemeSpec& spec)
{
    ThemeDefinition theme;
    theme.id = spec.id;
    theme.displayName = spec.name;
    theme.dark = spec.dark;
    theme.author = spec.author;
    theme.family = spec.family;

    PreviewColors& preview = theme.preview;
    preview.pageBg = spec.pageBg;
    preview.pageText = EnsureContrast(Over(spec.editorColor80, spec.pageBg), spec.pageBg, 4.0);
    preview.mutedText = Over(spec.editorColor50, spec.pageBg);
    preview.border = Over(spec.editorColor10, spec.pageBg);
    preview.link = spec.link;
    preview.inlineCodeBg = spec.codeBlockBg;
    preview.inlineCodeText = spec.codeBlockColor;
    preview.codeBlockBg = spec.codeBlockBg;
    preview.codeBlockText = spec.codeBlockColor;
    preview.codeBlockBorder = Blend(spec.codeBlockBg, preview.pageText, 0.10);
    preview.blockquoteText = Over(spec.blockquoteText, spec.pageBg);
    preview.blockquoteBorder = spec.blockquoteBorder;
    preview.tableBorder = spec.tableBorder;
    preview.tableHeadBg = spec.codeBlockBg;
    preview.hr = Over(spec.hr, spec.pageBg);
    preview.strong = Over(spec.strong, spec.pageBg);
    preview.em = Over(spec.em, spec.pageBg);
    preview.listMarker = spec.listMarker;
    preview.selectionBg = Over(spec.selectionColor, spec.pageBg);
    preview.findMatchBg = Over(spec.findMatchBg, spec.pageBg);
    preview.findCurrentBg = Over(spec.findCurrentBg, spec.pageBg);
    preview.findText = ContrastRatio(spec.findText, preview.findMatchBg) >= 4.5
                           ? spec.findText
                           : EnsureContrast(preview.pageText, preview.findMatchBg, 4.5);
    for (int i = 0; i < 6; ++i) {
        preview.headings[i] = spec.headings[i];
    }

    ChromeColors& chrome = theme.chrome;
    chrome.surface = spec.pageBg;
    chrome.windowBg = Over(spec.sideBarBg, spec.pageBg);
    chrome.navBg = chrome.windowBg;
    chrome.titleBarBg = chrome.windowBg;
    chrome.titleBarInactive = Blend(chrome.windowBg, spec.pageBg, 0.35);
    chrome.tabHover = Over(spec.sideBarItemHover, chrome.windowBg);
    chrome.buttonHover = chrome.tabHover;
    chrome.buttonPressed = Blend(chrome.tabHover, preview.pageText, 0.08);
    chrome.divider = spec.tableBorder;
    chrome.textPrimary = preview.pageText;
    // MarkText's editorColor50 is intentionally faint for prose; for inactive
    // tabs, nav hints and status text use 85% text over the page background so
    // it stays readable in both light and dark themes.
    chrome.textSecondary = Blend(preview.pageText, spec.pageBg, 0.15);
    chrome.accent = spec.accent;
    chrome.accentSoft = Blend(spec.pageBg, spec.accent, 0.12);
    chrome.gutterBg = Blend(spec.pageBg, preview.pageText, 0.03);
    chrome.currentLine = Blend(spec.pageBg, spec.accent, 0.06);
    chrome.selection = Over(spec.selectionColor, spec.pageBg);
    chrome.danger = spec.danger;
    chrome.dirtyDot = spec.accent;

    theme.tokens = spec.tokens;
    theme.editor = DeriveEditor(preview, chrome);
    return theme;
}

TokenColors GithubTokens()
{
    return TokenColors{
        Rgb(110, 119, 129), // comment
        Rgb(36, 41, 47),    // punctuation
        Rgb(17, 99, 41),    // tag / selector
        Rgb(5, 80, 174),    // property / constant
        Rgb(5, 80, 174),    // boolean / number
        Rgb(10, 48, 105),   // string
        Rgb(207, 34, 46),   // operator
        Rgb(130, 80, 223),  // function / class-name
        Rgb(207, 34, 46),   // keyword
        Rgb(17, 99, 41),    // regex / variable
    };
}

ThemeDefinition GithubLight()
{
    ThemeSpec spec{
        "github-light",
        "GitHub Light",
        false,
        Rgb(255, 255, 255), // pageBg
        Rgb(31, 35, 40),    // editorColor80
        Rgb(87, 96, 106),   // editorColor50
        Rgb(234, 236, 239), // editorColor10
        Rgb(246, 248, 250), // editorColor04
        Rgb(9, 105, 218),   // accent
        Rgb(214, 69, 69),   // danger
        Rgb(246, 248, 250), // codeBlockBg
        Rgb(31, 35, 40),    // codeBlockColor
        Rgb(208, 215, 222), // tableBorder
        Rgb(229, 231, 235), // hr
        Rgb(9, 105, 218),   // link
        Rgb(31, 35, 40),    // strong
        Rgb(31, 35, 40),    // em
        Rgb(31, 35, 40),    // listMarker
        {Rgb(31, 35, 40), Rgb(31, 35, 40), Rgb(31, 35, 40), Rgb(31, 35, 40),
         Rgb(31, 35, 40), Rgb(87, 96, 106)},
        Rgb(87, 96, 106),   // blockquoteText
        Rgb(208, 215, 222), // blockquoteBorder
        Rgb(245, 246, 248), // sideBarBg
        Rgb(237, 239, 242), // sideBarItemHover
        Rgb(216, 232, 255), // selection
        Rgb(255, 224, 130), // findMatch
        Rgb(255, 183, 77),  // findCurrent
        Rgb(31, 35, 40),    // findText
        GithubTokens(),
        "GitHub",
        ThemeFamily::Blue,
    };

    ThemeDefinition theme = BuildTheme(spec);
    // Keep the existing light chrome exactly as it was before themes existed.
    ChromeColors& chrome = theme.chrome;
    chrome.windowBg = Rgb(245, 246, 248);
    chrome.surface = Rgb(255, 255, 255);
    chrome.navBg = Rgb(250, 251, 252);
    chrome.titleBarBg = Rgb(248, 249, 251);
    chrome.titleBarInactive = Rgb(251, 252, 253);
    chrome.tabHover = Rgb(237, 239, 242);
    chrome.buttonHover = Rgb(231, 234, 238);
    chrome.buttonPressed = Rgb(219, 223, 228);
    chrome.divider = Rgb(225, 228, 232);
    chrome.textPrimary = Rgb(31, 35, 40);
    chrome.textSecondary = Rgb(110, 119, 129);
    chrome.accent = Rgb(47, 111, 235);
    chrome.accentSoft = Rgb(232, 240, 254);
    chrome.gutterBg = Rgb(247, 248, 250);
    chrome.currentLine = Rgb(246, 249, 255);
    chrome.selection = Rgb(216, 232, 255);
    chrome.danger = Rgb(214, 69, 69);
    chrome.dirtyDot = Rgb(47, 111, 235);
    theme.preview.codeBlockBorder = Rgb(238, 240, 243);

    EditorScheme& editor = theme.editor;
    editor.background = Rgb(255, 255, 255);
    editor.text = Rgb(31, 35, 40);
    editor.heading = Rgb(11, 87, 208);
    editor.emphasis = Rgb(31, 35, 40);
    editor.strong = Rgb(0, 0, 0);
    editor.code = Rgb(194, 24, 91);
    editor.codeBlock = Rgb(55, 65, 81);
    editor.link = Rgb(9, 105, 218);
    editor.image = Rgb(0, 128, 0);
    editor.listMarker = Rgb(156, 107, 0);
    editor.blockQuote = Rgb(87, 96, 106);
    editor.html = Rgb(122, 77, 0);
    editor.hrule = Rgb(153, 153, 153);
    editor.comment = Rgb(0, 128, 0);
    editor.reference = Rgb(138, 43, 226);
    editor.gutterBg = Rgb(247, 248, 250);
    editor.gutterText = Rgb(154, 160, 166);
    editor.currentLine = Rgb(246, 249, 255);
    editor.selection = Rgb(216, 232, 255);
    editor.selectionText = Rgb(31, 35, 40);
    editor.caret = Rgb(47, 111, 235);
    theme.editor = editor;
    return theme;
}

ThemeDefinition Ulysses()
{
    return BuildTheme(ThemeSpec{
        "ulysses",
        "Ulysses",
        false,
        Rgb(243, 243, 243),
        Rgb(101, 101, 101),
        Rgb(153, 153, 153),
        Rgb(233, 233, 233),
        Rgb(247, 247, 247),
        Rgb(12, 139, 186),
        Rgb(255, 105, 105),
        Rgb(233, 233, 233),
        Rgb(101, 101, 101),
        Rgb(229, 229, 229),
        Rgb(233, 233, 233),
        Rgb(12, 139, 186),
        Rgb(101, 101, 101),
        Rgb(26, 159, 127),
        Rgb(12, 139, 186),
        {Rgb(12, 139, 186), Rgb(26, 159, 127), Rgb(209, 106, 61), Rgb(122, 112, 186),
         Rgb(199, 89, 145), Rgb(112, 128, 144)},
        Rgb(153, 153, 153),
        Rgb(12, 139, 186),
        Rgb(245, 245, 245),
        Rgb(236, 236, 236),
        Rgba(0, 0, 0, 26),
        Rgb(255, 224, 130),
        Rgb(255, 183, 77),
        Rgb(31, 35, 40),
        TokenColors{Rgb(153, 153, 153), Rgb(101, 101, 101), Rgb(199, 89, 145),
                    Rgb(199, 89, 145), Rgb(122, 112, 186), Rgb(26, 159, 127),
                    Rgb(112, 128, 144), Rgb(209, 106, 61), Rgb(12, 139, 186),
                    Rgb(199, 89, 145)},
        "Ran Luo（MarkText）",
        ThemeFamily::Cyan,
    });
}

ThemeDefinition SolarizedLight()
{
    return BuildTheme(ThemeSpec{
        "solarized-light",
        "Solarized Light",
        false,
        Rgb(253, 246, 227),
        Rgb(101, 123, 131),
        Rgba(101, 123, 131, 128),
        Rgba(101, 123, 131, 26),
        Rgba(101, 123, 131, 10),
        Rgb(38, 139, 210),
        Rgb(220, 50, 47),
        Rgb(238, 232, 213),
        Rgb(101, 123, 131),
        Rgb(220, 214, 195),
        Rgb(220, 214, 195),
        Rgb(38, 139, 210),
        Rgb(88, 110, 117),
        Rgb(181, 137, 0),
        Rgb(203, 75, 22),
        {Rgb(220, 50, 47), Rgb(203, 75, 22), Rgb(181, 137, 0), Rgb(133, 153, 0),
         Rgb(42, 161, 152), Rgb(38, 139, 210)},
        Rgba(101, 123, 131, 153),
        Rgb(147, 161, 161),
        Rgb(245, 239, 220),
        Rgba(101, 123, 131, 13),
        Rgba(238, 232, 213, 204),
        Rgb(255, 224, 130),
        Rgb(255, 183, 77),
        Rgb(31, 35, 40),
        TokenColors{Rgb(147, 161, 161), Rgb(88, 110, 117), Rgb(38, 139, 210),
                    Rgb(38, 139, 210), Rgb(211, 54, 130), Rgb(42, 161, 152),
                    Rgb(203, 75, 22), Rgb(181, 137, 0), Rgb(133, 153, 0),
                    Rgb(203, 75, 22)},
        "Ethan Schoonover",
        ThemeFamily::Blue,
    });
}

ThemeDefinition OneDark()
{
    return BuildTheme(ThemeSpec{
        "one-dark",
        "One Dark",
        true,
        Rgb(40, 44, 52),
        Rgb(157, 165, 180),
        Rgba(157, 165, 180, 128),
        Rgba(157, 165, 180, 26),
        Rgba(157, 165, 180, 10),
        Rgb(77, 120, 204),
        Rgb(255, 105, 105),
        Rgb(58, 63, 75),
        Rgb(171, 178, 191),
        Rgb(54, 56, 57),
        Rgb(75, 83, 98),
        Rgb(97, 175, 239),
        Rgb(226, 192, 141),
        Rgb(198, 120, 221),
        Rgb(224, 108, 117),
        {Rgb(224, 108, 117), Rgb(97, 175, 239), Rgb(152, 195, 121), Rgb(86, 182, 194),
         Rgb(198, 120, 221), Rgb(229, 192, 123)},
        Rgba(157, 165, 180, 153),
        Rgb(226, 192, 141),
        Rgb(36, 40, 47),
        Rgb(58, 63, 75),
        Rgba(103, 118, 150, 96),
        Rgb(92, 74, 0),
        Rgb(138, 109, 0),
        Rgb(255, 255, 255),
        TokenColors{Rgb(92, 99, 112), Rgb(171, 178, 191), Rgb(224, 108, 117),
                    Rgb(209, 154, 102), Rgb(209, 154, 102), Rgb(152, 195, 121),
                    Rgb(86, 182, 194), Rgb(97, 175, 239), Rgb(198, 120, 221),
                    Rgb(198, 120, 221)},
        "Atom（GitHub）",
        ThemeFamily::Blue,
    });
}

ThemeDefinition Dracula()
{
    return BuildTheme(ThemeSpec{
        "dracula",
        "Dracula",
        true,
        Rgb(40, 42, 54),
        Rgb(248, 248, 242),
        Rgba(248, 248, 242, 128),
        Rgba(248, 248, 242, 26),
        Rgba(248, 248, 242, 10),
        Rgb(189, 147, 249),
        Rgb(255, 85, 85),
        Rgb(68, 71, 90),
        Rgb(248, 248, 242),
        Rgb(68, 71, 90),
        Rgb(68, 71, 90),
        Rgb(139, 233, 253),
        Rgb(248, 248, 242),
        Rgb(241, 250, 140),
        Rgb(189, 147, 249),
        {Rgb(255, 121, 198), Rgb(189, 147, 249), Rgb(139, 233, 253), Rgb(80, 250, 123),
         Rgb(255, 184, 108), Rgb(241, 250, 140)},
        Rgba(248, 248, 242, 153),
        Rgb(98, 114, 164),
        Rgb(36, 38, 49),
        Rgba(248, 248, 242, 8),
        Rgba(68, 71, 90, 204),
        Rgb(92, 74, 0),
        Rgb(138, 109, 0),
        Rgb(255, 255, 255),
        TokenColors{Rgb(98, 114, 164), Rgb(248, 248, 242), Rgb(255, 121, 198),
                    Rgb(255, 121, 198), Rgb(189, 147, 249), Rgb(80, 250, 123),
                    Rgb(248, 248, 242), Rgb(241, 250, 140), Rgb(255, 121, 198),
                    Rgb(255, 184, 108)},
        "Dracula Theme",
        ThemeFamily::Purple,
    });
}

ThemeDefinition Nord()
{
    return BuildTheme(ThemeSpec{
        "nord",
        "Nord",
        true,
        Rgb(46, 52, 64),
        Rgb(216, 222, 233),
        Rgba(216, 222, 233, 128),
        Rgba(216, 222, 233, 26),
        Rgba(216, 222, 233, 10),
        Rgb(129, 161, 193),
        Rgb(191, 97, 106),
        Rgb(59, 66, 82),
        Rgb(216, 222, 233),
        Rgb(59, 66, 82),
        Rgb(76, 86, 106),
        Rgb(136, 192, 208),
        Rgb(236, 239, 244),
        Rgb(235, 203, 139),
        Rgb(129, 161, 193),
        {Rgb(136, 192, 208), Rgb(129, 161, 193), Rgb(94, 129, 172), Rgb(180, 142, 173),
         Rgb(163, 190, 140), Rgb(235, 203, 139)},
        Rgba(216, 222, 233, 153),
        Rgb(94, 129, 172),
        Rgb(43, 49, 60),
        Rgba(216, 222, 233, 8),
        Rgba(76, 86, 106, 153),
        Rgb(92, 74, 0),
        Rgb(138, 109, 0),
        Rgb(255, 255, 255),
        TokenColors{Rgb(97, 110, 136), Rgb(236, 239, 244), Rgb(129, 161, 193),
                    Rgb(129, 161, 193), Rgb(180, 142, 173), Rgb(163, 190, 140),
                    Rgb(129, 161, 193), Rgb(136, 192, 208), Rgb(129, 161, 193),
                    Rgb(235, 203, 139)},
        "Arctic Ice Studio",
        ThemeFamily::Blue,
    });
}

#include "theme/theme_catalog_generated.inc"

} // namespace

ThemeColor EnsureContrast(ThemeColor fg, const ThemeColor& bg, double minRatio)
{
    fg.a = 255;
    if (ContrastRatio(fg, bg) >= minRatio) {
        return fg;
    }
    // Pick the direction with the most headroom. For mid-gray backgrounds
    // neither pure white nor pure black may reach the requested ratio; the
    // higher-contrast direction gives the best chance (and a sane fallback).
    const ThemeColor black = Rgb(0, 0, 0);
    const ThemeColor white = Rgb(255, 255, 255);
    const ThemeColor toward =
        ContrastRatio(white, bg) >= ContrastRatio(black, bg) ? white : black;
    for (int step = 1; step <= 100; ++step) {
        const ThemeColor candidate = Blend(fg, toward, static_cast<double>(step) / 100.0);
        if (ContrastRatio(candidate, bg) >= minRatio) {
            return candidate;
        }
    }
    return toward;
}

namespace {

std::vector<ThemeDefinition> BuildBuiltinThemes()
{
    std::vector<ThemeDefinition> result{
        GithubLight(), Ulysses(), SolarizedLight(), OneDark(), Dracula(), Nord(),
    };
    result.reserve(53);
    AppendGeneratedThemes(result);
    return result;
}

const std::vector<ThemeDefinition>& BuiltinStorage()
{
    static const std::vector<ThemeDefinition> themes = BuildBuiltinThemes();
    return themes;
}

std::vector<ThemeDefinition>& UserStorage()
{
    static std::vector<ThemeDefinition> themes;
    return themes;
}

std::vector<ThemeDefinition>& CombinedStorage()
{
    static std::vector<ThemeDefinition> themes;
    return themes;
}

unsigned& GenerationStorage()
{
    static unsigned generation = 0;
    return generation;
}

void ApplyUiOverrides(ThemeDefinition& theme, const UiOverrides& ui)
{
    if (ui.windowBg) {
        theme.chrome.windowBg = *ui.windowBg;
    }
    if (ui.surface) {
        theme.chrome.surface = *ui.surface;
    }
    if (ui.navBg) {
        theme.chrome.navBg = *ui.navBg;
    }
    if (ui.titleBarBg) {
        theme.chrome.titleBarBg = *ui.titleBarBg;
    }
    if (ui.titleBarInactive) {
        theme.chrome.titleBarInactive = *ui.titleBarInactive;
    }
    if (ui.tabHover) {
        theme.chrome.tabHover = *ui.tabHover;
    }
    if (ui.buttonHover) {
        theme.chrome.buttonHover = *ui.buttonHover;
    }
    if (ui.buttonPressed) {
        theme.chrome.buttonPressed = *ui.buttonPressed;
    }
    if (ui.divider) {
        theme.chrome.divider = *ui.divider;
    }
    if (ui.textPrimary) {
        theme.chrome.textPrimary = *ui.textPrimary;
    }
    if (ui.textSecondary) {
        theme.chrome.textSecondary = *ui.textSecondary;
    }
    if (ui.accent) {
        theme.chrome.accent = *ui.accent;
    }
    if (ui.accentSoft) {
        theme.chrome.accentSoft = *ui.accentSoft;
    }
    if (ui.gutterBg) {
        theme.chrome.gutterBg = *ui.gutterBg;
    }
    if (ui.currentLine) {
        theme.chrome.currentLine = *ui.currentLine;
    }
    if (ui.selection) {
        theme.chrome.selection = *ui.selection;
    }
    if (ui.danger) {
        theme.chrome.danger = *ui.danger;
    }
    if (ui.dirtyDot) {
        theme.chrome.dirtyDot = *ui.dirtyDot;
    }
}

void ApplyOverrides(ThemeDefinition& theme,
                    const std::map<std::string, ThemeColor>& overrides)
{
    const auto apply = [&overrides](const char* key, ThemeColor& target) {
        const auto it = overrides.find(key);
        if (it != overrides.end()) {
            target = it->second;
        }
    };

    apply("preview.codeBlockBorder", theme.preview.codeBlockBorder);

    apply("chrome.windowBg", theme.chrome.windowBg);
    apply("chrome.surface", theme.chrome.surface);
    apply("chrome.navBg", theme.chrome.navBg);
    apply("chrome.titleBarBg", theme.chrome.titleBarBg);
    apply("chrome.titleBarInactive", theme.chrome.titleBarInactive);
    apply("chrome.tabHover", theme.chrome.tabHover);
    apply("chrome.buttonHover", theme.chrome.buttonHover);
    apply("chrome.buttonPressed", theme.chrome.buttonPressed);
    apply("chrome.divider", theme.chrome.divider);
    apply("chrome.textPrimary", theme.chrome.textPrimary);
    apply("chrome.textSecondary", theme.chrome.textSecondary);
    apply("chrome.accent", theme.chrome.accent);
    apply("chrome.accentSoft", theme.chrome.accentSoft);
    apply("chrome.gutterBg", theme.chrome.gutterBg);
    apply("chrome.currentLine", theme.chrome.currentLine);
    apply("chrome.selection", theme.chrome.selection);
    apply("chrome.danger", theme.chrome.danger);
    apply("chrome.dirtyDot", theme.chrome.dirtyDot);

    apply("editor.background", theme.editor.background);
    apply("editor.text", theme.editor.text);
    apply("editor.heading", theme.editor.heading);
    apply("editor.emphasis", theme.editor.emphasis);
    apply("editor.strong", theme.editor.strong);
    apply("editor.code", theme.editor.code);
    apply("editor.codeBlock", theme.editor.codeBlock);
    apply("editor.link", theme.editor.link);
    apply("editor.image", theme.editor.image);
    apply("editor.listMarker", theme.editor.listMarker);
    apply("editor.blockQuote", theme.editor.blockQuote);
    apply("editor.html", theme.editor.html);
    apply("editor.hrule", theme.editor.hrule);
    apply("editor.comment", theme.editor.comment);
    apply("editor.reference", theme.editor.reference);
    apply("editor.gutterBg", theme.editor.gutterBg);
    apply("editor.gutterText", theme.editor.gutterText);
    apply("editor.currentLine", theme.editor.currentLine);
    apply("editor.selection", theme.editor.selection);
    apply("editor.selectionText", theme.editor.selectionText);
    apply("editor.caret", theme.editor.caret);
}

} // namespace

const std::vector<ThemeDefinition>& AllThemes()
{
    std::vector<ThemeDefinition>& combined = CombinedStorage();
    if (combined.empty()) {
        combined = BuiltinStorage();
    }
    return combined;
}

const ThemeDefinition& DefaultTheme()
{
    return AllThemes().front();
}

const ThemeDefinition* FindTheme(std::string_view id)
{
    for (const ThemeDefinition& theme : AllThemes()) {
        if (id == std::string_view(theme.id)) {
            return &theme;
        }
    }
    return nullptr;
}

const ThemeDefinition& ResolveTheme(const std::string& id)
{
    if (const ThemeDefinition* theme = FindTheme(id)) {
        return *theme;
    }
    return DefaultTheme();
}

int IndexOfTheme(std::string_view id)
{
    const std::vector<ThemeDefinition>& themes = AllThemes();
    for (std::size_t i = 0; i < themes.size(); ++i) {
        if (id == std::string_view(themes[i].id)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::size_t BuiltinThemeCount()
{
    return BuiltinStorage().size();
}

void SetUserThemes(std::vector<ThemeDefinition> themes)
{
    if (themes.size() > kMaxUserThemes) {
        themes.resize(kMaxUserThemes);
    }

    UserStorage() = std::move(themes);

    std::vector<ThemeDefinition>& combined = CombinedStorage();
    combined = BuiltinStorage();
    combined.reserve(BuiltinThemeCount() + kMaxUserThemes);
    combined.insert(combined.end(), UserStorage().begin(), UserStorage().end());
    ++GenerationStorage();
}

void ClearUserThemes()
{
    SetUserThemes({});
}

unsigned CatalogGeneration()
{
    return GenerationStorage();
}

const std::vector<ThemeDefinition>& UserThemes()
{
    return UserStorage();
}

ThemeDefinition BuildDefinition(const ThemeSource& source, bool userDefined)
{
    ThemeSpec spec{};
    spec.id = source.id.c_str();
    spec.name = source.name.c_str();
    spec.dark = source.dark;
    spec.pageBg = source.background;
    spec.editorColor80 = source.text;
    spec.editorColor50 = source.muted;
    spec.editorColor10 = source.border;
    spec.editorColor04 = Blend(source.background, source.text, 0.04);
    spec.accent = source.accent;
    spec.danger = source.danger;
    spec.codeBlockBg = source.codeBackground;
    spec.codeBlockColor = source.codeText;
    spec.tableBorder = source.tableBorder;
    spec.hr = source.hr;
    spec.link = source.link;
    spec.strong = source.strong;
    spec.em = source.emphasis;
    spec.listMarker = source.listMarker;
    for (int i = 0; i < 6; ++i) {
        spec.headings[i] = source.headings[i];
    }
    spec.blockquoteText = source.blockquoteText;
    spec.blockquoteBorder = source.blockquoteBorder;
    spec.sideBarBg = source.sidebarBackground;
    spec.sideBarItemHover = source.sidebarHover;
    spec.selectionColor = source.selection;
    spec.findMatchBg = source.findMatch;
    spec.findCurrentBg = source.findCurrent;
    spec.findText = source.findText;
    spec.tokens = source.tokens;
    spec.author = source.author.c_str();
    spec.family = source.family;

    ThemeDefinition theme = BuildTheme(spec);
    theme.userDefined = userDefined;
    theme.source = source.source;
    theme.license = source.license;
    theme.typography = source.typography;
    std::vector<std::string> warnings;
    theme.extraCss = SanitizeThemeCss(source.extraCss, warnings);

    ApplyUiOverrides(theme, source.ui);
    // UI overrides can change the accent/chrome; keep the editor scheme in
    // step before applying explicit editor overrides.
    theme.editor = DeriveEditor(theme.preview, theme.chrome);
    ApplyOverrides(theme, source.overrides);

    if (userDefined) {
        const ThemeColor codeBg = theme.preview.codeBlockBg;
        theme.preview.pageText =
            EnsureContrast(theme.preview.pageText, theme.preview.pageBg, 4.5);
        theme.preview.mutedText =
            EnsureContrast(theme.preview.mutedText, theme.preview.pageBg, 3.0);
        theme.preview.blockquoteText =
            EnsureContrast(theme.preview.blockquoteText, theme.preview.pageBg, 3.0);
        theme.chrome.textPrimary =
            EnsureContrast(theme.chrome.textPrimary, theme.chrome.surface, 4.5);
        theme.chrome.textSecondary =
            EnsureContrast(theme.chrome.textSecondary, theme.chrome.surface, 3.0);
        theme.editor.text =
            EnsureContrast(theme.editor.text, theme.editor.background, 4.5);
        theme.tokens.comment = EnsureContrast(theme.tokens.comment, codeBg, 3.0);
        theme.tokens.punctuation = EnsureContrast(theme.tokens.punctuation, codeBg, 3.0);
        theme.tokens.tag = EnsureContrast(theme.tokens.tag, codeBg, 3.0);
        theme.tokens.property = EnsureContrast(theme.tokens.property, codeBg, 3.0);
        theme.tokens.booleanNumber =
            EnsureContrast(theme.tokens.booleanNumber, codeBg, 3.0);
        theme.tokens.string = EnsureContrast(theme.tokens.string, codeBg, 3.0);
        theme.tokens.oper = EnsureContrast(theme.tokens.oper, codeBg, 3.0);
        theme.tokens.function = EnsureContrast(theme.tokens.function, codeBg, 3.0);
        theme.tokens.keyword = EnsureContrast(theme.tokens.keyword, codeBg, 3.0);
        theme.tokens.regex = EnsureContrast(theme.tokens.regex, codeBg, 3.0);
        for (int i = 0; i < 6; ++i) {
            theme.preview.headings[i] =
                EnsureContrast(theme.preview.headings[i], theme.preview.pageBg, 3.0);
        }
    }
    return theme;
}

namespace {

const char* const kAuthorOrder[] = {
    "GitHub",
    "Microsoft",
    "Atom（GitHub）",
    "Ran Luo（MarkText）",
    "Ethan Schoonover",
    "Catppuccin",
    "Dracula Theme",
    "Arctic Ice Studio",
    "Rosé Pine",
    "Monokai（Wimer Hazenberg）",
    "Mattia Astorino（equinusocio）",
    "Dempfi（Ayu）",
    "Enkia（Tokyo Night）",
    "Pavel Pertsev（morhetz）",
    "Sainnhe（Everforest）",
    "Rebelot（Kanagawa）",
    "Nyoom Engineering（IBM Carbon）",
    "Jonathan Olaleye",
    "EdenEast",
    "Scott McKendry",
    "Robb Owen",
    "Jan T. Sott",
    "Chris Kempson",
    "Ian Beck（One Crayon）",
    "Ofer Reichman",
    "1ClickComputing",
    "本地导入",
};

int AuthorRank(std::string_view author)
{
    const int count = static_cast<int>(sizeof(kAuthorOrder) / sizeof(kAuthorOrder[0]));
    for (int i = 0; i < count; ++i) {
        if (author == kAuthorOrder[i]) {
            return i;
        }
    }
    return std::numeric_limits<int>::max();
}

char AsciiLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool AsciiLess(std::string_view a, std::string_view b)
{
    const std::size_t count = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i) {
        const char left = AsciiLower(a[i]);
        const char right = AsciiLower(b[i]);
        if (left != right) {
            return left < right;
        }
    }
    return a.size() < b.size();
}

} // namespace

std::vector<int> LightThemeIndices()
{
    std::vector<int> result;
    const std::vector<ThemeDefinition>& themes = AllThemes();
    for (std::size_t i = 0; i < themes.size(); ++i) {
        if (!themes[i].dark) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

std::vector<int> DarkThemeIndices()
{
    std::vector<int> result;
    const std::vector<ThemeDefinition>& themes = AllThemes();
    for (std::size_t i = 0; i < themes.size(); ++i) {
        if (themes[i].dark) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

std::vector<ThemeGroup> ThemesGroupedByAuthor()
{
    std::vector<ThemeGroup> groups;
    const std::vector<ThemeDefinition>& themes = AllThemes();
    for (std::size_t i = 0; i < themes.size(); ++i) {
        const std::string& author = themes[i].author;
        auto it = std::find_if(groups.begin(), groups.end(), [&author](const ThemeGroup& group) {
            return group.label == author;
        });
        if (it == groups.end()) {
            groups.push_back(ThemeGroup{author, {}});
            it = groups.end() - 1;
        }
        it->indices.push_back(static_cast<int>(i));
    }

    std::sort(groups.begin(), groups.end(), [](const ThemeGroup& a, const ThemeGroup& b) {
        const int rankA = AuthorRank(std::string_view(a.label));
        const int rankB = AuthorRank(std::string_view(b.label));
        if (rankA != rankB) {
            return rankA < rankB;
        }
        return AsciiLess(std::string_view(a.label), std::string_view(b.label));
    });
    return groups;
}

std::vector<FamilyThemeGroup> ThemesGroupedByFamily()
{
    constexpr ThemeFamily kFamilyOrder[] = {
        ThemeFamily::Neutral, ThemeFamily::Blue,  ThemeFamily::Cyan, ThemeFamily::Green,
        ThemeFamily::Warm,    ThemeFamily::RedPink, ThemeFamily::Purple,
        ThemeFamily::Multicolor,
    };

    std::vector<FamilyThemeGroup> groups;
    const std::vector<ThemeDefinition>& themes = AllThemes();
    for (ThemeFamily family : kFamilyOrder) {
        FamilyThemeGroup group{family, {}};
        for (std::size_t i = 0; i < themes.size(); ++i) {
            if (themes[i].family == family) {
                group.indices.push_back(static_cast<int>(i));
            }
        }
        if (!group.indices.empty()) {
            groups.push_back(group);
        }
    }
    return groups;
}

std::string ColorToCss(const ThemeColor& color)
{
    char buffer[48] = {};
    if (color.a == 255) {
        std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", color.r, color.g, color.b);
    } else {
        const double alpha =
            std::round((static_cast<double>(color.a) / 255.0) * 1000.0) / 1000.0;
        std::snprintf(buffer, sizeof(buffer), "rgba(%u,%u,%u,%g)",
                      static_cast<unsigned>(color.r), static_cast<unsigned>(color.g),
                      static_cast<unsigned>(color.b), alpha);
    }
    return buffer;
}

ThemeColor Blend(const ThemeColor& a, const ThemeColor& b, double t)
{
    const double amount = std::clamp(t, 0.0, 1.0);
    const auto mix = [amount](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(
            std::lround(static_cast<double>(x) * (1.0 - amount) +
                        static_cast<double>(y) * amount));
    };
    return ThemeColor{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), 255};
}

ThemeColor Over(const ThemeColor& fg, const ThemeColor& bg)
{
    if (fg.a == 255) {
        return ThemeColor{fg.r, fg.g, fg.b, 255};
    }
    if (fg.a == 0) {
        return ThemeColor{bg.r, bg.g, bg.b, 255};
    }
    const double alpha = static_cast<double>(fg.a) / 255.0;
    const auto mix = [alpha](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(
            std::lround(static_cast<double>(x) * alpha +
                        static_cast<double>(y) * (1.0 - alpha)));
    };
    return ThemeColor{mix(fg.r, bg.r), mix(fg.g, bg.g), mix(fg.b, bg.b), 255};
}

double RelativeLuminance(const ThemeColor& color)
{
    const auto channel = [](unsigned char value) {
        const double c = static_cast<double>(value) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.r) + 0.7152 * channel(color.g) +
           0.0722 * channel(color.b);
}

double ContrastRatio(const ThemeColor& a, const ThemeColor& b)
{
    const double la = RelativeLuminance(a);
    const double lb = RelativeLuminance(b);
    const double lighter = std::max(la, lb);
    const double darker = std::min(la, lb);
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace awm::theme
