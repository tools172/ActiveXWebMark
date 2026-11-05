#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace awm::theme {

/// Straight 8-bit sRGB color. `a` is 0..255 and is only meaningful for CSS
/// output; UI/editor consumers composite it over a background first.
struct ThemeColor {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

constexpr bool operator==(const ThemeColor& a, const ThemeColor& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

constexpr bool operator!=(const ThemeColor& a, const ThemeColor& b)
{
    return !(a == b);
}

constexpr ThemeColor Rgb(int r, int g, int b)
{
    return ThemeColor{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                      static_cast<unsigned char>(b), 255};
}

constexpr ThemeColor Rgba(int r, int g, int b, int a)
{
    return ThemeColor{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                      static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
}

/// Typography for the rendered Markdown document. Empty font names select the
/// built-in stacks. All sizes are CSS pixels.
struct ThemeTypography {
    std::string bodyFont;
    std::string codeFont;
    int bodySize = 15;
    double lineHeight = 1.75;
    int maxWidth = 980;
    int padTop = 32;
    int padRight = 40;
    int padBottom = 64;
    int codeSize = 13;
};

/// Colors used by the custom-drawn main window chrome.
struct ChromeColors {
    ThemeColor windowBg;
    ThemeColor surface;
    ThemeColor navBg;
    ThemeColor titleBarBg;
    ThemeColor titleBarInactive;
    ThemeColor tabHover;
    ThemeColor buttonHover;
    ThemeColor buttonPressed;
    ThemeColor divider;
    ThemeColor textPrimary;
    ThemeColor textSecondary;
    ThemeColor accent;
    ThemeColor accentSoft;
    ThemeColor gutterBg;
    ThemeColor currentLine;
    ThemeColor selection;
    ThemeColor danger;
    ThemeColor dirtyDot;
};

/// Colors used by the rendered Markdown document. Alpha is preserved in the
/// generated CSS (`rgba(...)`); IE11 supports rgba but not custom properties.
struct PreviewColors {
    ThemeColor pageBg;
    ThemeColor pageText;
    ThemeColor mutedText;
    ThemeColor border;
    ThemeColor link;
    ThemeColor inlineCodeBg;
    ThemeColor inlineCodeText;
    ThemeColor codeBlockBg;
    ThemeColor codeBlockText;
    ThemeColor codeBlockBorder;
    ThemeColor blockquoteText;
    ThemeColor blockquoteBorder;
    ThemeColor tableBorder;
    ThemeColor tableHeadBg;
    ThemeColor hr;
    ThemeColor strong;
    ThemeColor em;
    ThemeColor listMarker;
    ThemeColor selectionBg;
    ThemeColor findMatchBg;
    ThemeColor findCurrentBg;
    ThemeColor findText;
    ThemeColor headings[6];
};

/// Prism token groups. MarkText's Prism themes use these same groups, so a
/// dozen colors cover every language Prism emits.
struct TokenColors {
    ThemeColor comment;
    ThemeColor punctuation;
    ThemeColor tag;
    ThemeColor property;
    ThemeColor booleanNumber;
    ThemeColor string;
    ThemeColor oper;
    ThemeColor function;
    ThemeColor keyword;
    ThemeColor regex;
};

/// Colors for the Scintilla Markdown editor.
struct EditorScheme {
    ThemeColor background;
    ThemeColor text;
    ThemeColor heading;
    ThemeColor emphasis;
    ThemeColor strong;
    ThemeColor code;
    ThemeColor codeBlock;
    ThemeColor link;
    ThemeColor image;
    ThemeColor listMarker;
    ThemeColor blockQuote;
    ThemeColor html;
    ThemeColor hrule;
    ThemeColor comment;
    ThemeColor reference;
    ThemeColor gutterBg;
    ThemeColor gutterText;
    ThemeColor currentLine;
    ThemeColor selection;
    ThemeColor selectionText;
    ThemeColor caret;
};

/// Coarse color-family classification used by the "按色系" menu. The value is
/// curated per theme; it is not derived at runtime.
enum class ThemeFamily {
    Neutral,
    Blue,
    Cyan,
    Green,
    Warm,
    RedPink,
    Purple,
    Multicolor,
};

inline constexpr int kThemeFamilyCount = 8;

struct ThemeDefinition {
    std::string id;
    std::string displayName;
    bool dark = false;
    ChromeColors chrome;
    PreviewColors preview;
    TokenColors tokens;
    EditorScheme editor;
    /// Original author or organization (not the project the values were
    /// imported from).
    std::string author;
    ThemeFamily family = ThemeFamily::Neutral;
    /// True for themes loaded from the user's themes directory.
    bool userDefined = false;
    /// Import provenance ("VS Code Marketplace: publisher.extension@1.2.3").
    std::string source;
    /// License reported by the source package, shown for attribution.
    std::string license;
    ThemeTypography typography;
    /// Sanitized additional Markdown CSS authored by the user.
    std::string extraCss;
};

/// Themes grouped by a display label, in catalog-index order. Used by the
/// "按作者/来源" menu.
struct ThemeGroup {
    std::string label;
    std::vector<int> indices;
};

/// Themes grouped by color family, in family-enum order.
struct FamilyThemeGroup {
    ThemeFamily family;
    std::vector<int> indices;
};

inline constexpr const char* kDefaultThemeId = "github-light";

/// User themes are appended after the built-ins. The menu command range and
/// the catalog reserve space for this many of them.
inline constexpr std::size_t kMaxUserThemes = 459;

const std::vector<ThemeDefinition>& AllThemes();
const ThemeDefinition& DefaultTheme();
const ThemeDefinition* FindTheme(std::string_view id);
const ThemeDefinition& ResolveTheme(const std::string& id);
int IndexOfTheme(std::string_view id);

/// Number of compiled-in themes. User themes always follow them in
/// AllThemes(), so built-in indices stay stable.
std::size_t BuiltinThemeCount();

/// Replaces the user-theme tail of the catalog. Built-in themes are never
/// touched. Increments CatalogGeneration().
void SetUserThemes(std::vector<ThemeDefinition> themes);
void ClearUserThemes();
unsigned CatalogGeneration();
const std::vector<ThemeDefinition>& UserThemes();

/// Catalog indices for the light/dark menu lists, in catalog order.
std::vector<int> LightThemeIndices();
std::vector<int> DarkThemeIndices();

/// Author groups in the curated display order (unknown authors follow
/// alphabetically); each group's indices are ascending.
std::vector<ThemeGroup> ThemesGroupedByAuthor();

/// Non-empty color-family groups in ThemeFamily order; each group's indices
/// are ascending.
std::vector<FamilyThemeGroup> ThemesGroupedByFamily();

/// Formats a color for the preview stylesheet: `#rrggbb`, or `rgba(...)` when
/// the alpha channel is not fully opaque.
std::string ColorToCss(const ThemeColor& color);

/// Linear interpolation: t = 0 returns `a`, t = 1 returns `b`. The result is
/// opaque.
ThemeColor Blend(const ThemeColor& a, const ThemeColor& b, double t);

/// Composites `fg` over `bg` using `fg.a`. The result is opaque. GDI has no
/// alpha, so every UI/editor color is passed through this before use.
ThemeColor Over(const ThemeColor& fg, const ThemeColor& bg);

double RelativeLuminance(const ThemeColor& color);
double ContrastRatio(const ThemeColor& a, const ThemeColor& b);

/// Returns an opaque color at least `minRatio` away from `bg`. Imported theme
/// files are authored for their own renderer and occasionally land just below
/// WCAG AA after alpha flattening; this nudges only those colors.
ThemeColor EnsureContrast(ThemeColor fg, const ThemeColor& bg, double minRatio);

} // namespace awm::theme
