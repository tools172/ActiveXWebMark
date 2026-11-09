#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "theme/theme_catalog.h"

namespace awm::theme {

/// Optional explicit overrides for the derived window chrome. Any field left
/// empty keeps the value computed from the Markdown colors.
struct UiOverrides {
    std::optional<ThemeColor> windowBg;
    std::optional<ThemeColor> surface;
    std::optional<ThemeColor> navBg;
    std::optional<ThemeColor> titleBarBg;
    std::optional<ThemeColor> titleBarInactive;
    std::optional<ThemeColor> tabHover;
    std::optional<ThemeColor> buttonHover;
    std::optional<ThemeColor> buttonPressed;
    std::optional<ThemeColor> divider;
    std::optional<ThemeColor> textPrimary;
    std::optional<ThemeColor> textSecondary;
    std::optional<ThemeColor> accent;
    std::optional<ThemeColor> accentSoft;
    std::optional<ThemeColor> gutterBg;
    std::optional<ThemeColor> currentLine;
    std::optional<ThemeColor> selection;
    std::optional<ThemeColor> danger;
    std::optional<ThemeColor> dirtyDot;

    bool any() const;
};

/// Canonical names of the color fields tracked in ThemeSource::explicitFields.
/// They double as the corresponding native-JSON keys.
namespace field {
inline constexpr char kBackground[] = "background";
inline constexpr char kText[] = "text";
inline constexpr char kMuted[] = "muted";
inline constexpr char kAccent[] = "accent";
inline constexpr char kLink[] = "link";
inline constexpr char kBorder[] = "border";
inline constexpr char kHr[] = "hr";
inline constexpr char kStrong[] = "strong";
inline constexpr char kEmphasis[] = "emphasis";
inline constexpr char kListMarker[] = "listMarker";
inline constexpr char kHeadings[] = "headings";
inline constexpr char kBlockquoteText[] = "blockquoteText";
inline constexpr char kBlockquoteBorder[] = "blockquoteBorder";
inline constexpr char kCodeBackground[] = "codeBackground";
inline constexpr char kCodeText[] = "codeText";
inline constexpr char kCodeBorder[] = "codeBorder";
inline constexpr char kTableBorder[] = "tableBorder";
inline constexpr char kTableHeadBackground[] = "tableHeadBackground";
inline constexpr char kSelection[] = "selection";
inline constexpr char kFindMatch[] = "findMatch";
inline constexpr char kFindCurrent[] = "findCurrent";
inline constexpr char kFindText[] = "findText";
inline constexpr char kSidebarBackground[] = "sidebarBackground";
inline constexpr char kSidebarHover[] = "sidebarHover";
inline constexpr char kDanger[] = "danger";
inline constexpr char kTokens[] = "tokens";
} // namespace field

/// Parsed-but-not-yet-normalized theme. Importers and the native JSON loader
/// fill only what they know; FinalizeSource() derives the rest.
struct ThemeSourceDraft {
    int schemaVersion = 1;
    std::string id;
    std::string name;
    std::string author;
    std::string license;
    std::string source;

    std::optional<bool> dark;
    std::optional<ThemeFamily> family;

    std::optional<ThemeColor> background;
    std::optional<ThemeColor> text;
    std::optional<ThemeColor> muted;
    std::optional<ThemeColor> accent;
    std::optional<ThemeColor> link;
    std::optional<ThemeColor> border;
    std::optional<ThemeColor> hr;
    std::optional<ThemeColor> strong;
    std::optional<ThemeColor> emphasis;
    std::optional<ThemeColor> listMarker;
    std::optional<ThemeColor> headings[6];
    std::optional<ThemeColor> blockquoteText;
    std::optional<ThemeColor> blockquoteBorder;
    std::optional<ThemeColor> codeBackground;
    std::optional<ThemeColor> codeText;
    std::optional<ThemeColor> codeBorder;
    std::optional<ThemeColor> tableBorder;
    std::optional<ThemeColor> tableHeadBackground;
    std::optional<ThemeColor> selection;
    std::optional<ThemeColor> findMatch;
    std::optional<ThemeColor> findCurrent;
    std::optional<ThemeColor> findText;
    std::optional<ThemeColor> sidebarBackground;
    std::optional<ThemeColor> sidebarHover;
    std::optional<ThemeColor> danger;
    std::optional<TokenColors> tokens;

    ThemeTypography typography;
    std::string extraCss;
    UiOverrides ui;
    /// Advanced sparse overrides, mostly used to export built-in themes with
    /// their hand-tuned chrome/editor values. Keys: "chrome.*", "preview.*",
    /// "editor.*".
    std::map<std::string, ThemeColor> overrides;
};

/// Fully normalized theme data: every color is present, so BuildDefinition()
/// has no ambiguity.
struct ThemeSource {
    int schemaVersion = 1;
    std::string id;
    std::string name;
    std::string author;
    std::string license;
    std::string source;

    bool dark = false;
    bool darkExplicit = false;
    ThemeFamily family = ThemeFamily::Neutral;
    bool familyExplicit = false;

    ThemeColor background;
    ThemeColor text;
    ThemeColor muted;
    ThemeColor accent;
    ThemeColor link;
    ThemeColor border;
    ThemeColor hr;
    ThemeColor strong;
    ThemeColor emphasis;
    ThemeColor listMarker;
    ThemeColor headings[6];
    ThemeColor blockquoteText;
    ThemeColor blockquoteBorder;
    ThemeColor codeBackground;
    ThemeColor codeText;
    ThemeColor codeBorder;
    ThemeColor tableBorder;
    ThemeColor tableHeadBackground;
    ThemeColor selection;
    ThemeColor findMatch;
    ThemeColor findCurrent;
    ThemeColor findText;
    ThemeColor sidebarBackground;
    ThemeColor sidebarHover;
    ThemeColor danger;
    TokenColors tokens;

    ThemeTypography typography;
    std::string extraCss;
    UiOverrides ui;
    std::map<std::string, ThemeColor> overrides;

    /// Names of the color fields that were authored explicitly (present in a
    /// file, set by an importer or edited in the settings dialog). Fields not
    /// in this set are derivation defaults and follow later edits of
    /// background/text/accent. Keys match the native JSON field names.
    std::set<std::string> explicitFields;
};

/// Fills every missing color with a deterministic value derived from
/// background/text/accent. Throws std::invalid_argument only when the draft
/// has neither a usable background nor text (callers should validate first).
ThemeSource FinalizeSource(ThemeSourceDraft draft);

/// Drops explicit markers for colors that match what FinalizeSource() would
/// derive from background/text/accent alone. Files written by older versions
/// materialized every derived color; running this after loading lets those
/// defaults follow later edits again.
void MinimizeExplicitFields(ThemeSource& source);

/// Rebuilds the source so every field that is not in `explicitFields` is
/// re-derived from the current background/text/accent. Explicitly authored
/// fields keep their values. Typography, extraCss, ui and overrides are
/// carried over unchanged.
ThemeSource RefreshDerivedFields(const ThemeSource& source);

/// Converts a fully resolved definition back into editable seed data.
/// includeOverrides=true also records the resolved chrome/editor/preview
/// values, which makes an exported built-in theme reproduce exactly.
ThemeSource SourceFromDefinition(const ThemeDefinition& definition,
                                 bool includeOverrides = false);

/// Expands a ThemeSource into the full palette used by the preview, editor and
/// window chrome. Implemented in theme_catalog.cpp next to BuildTheme().
ThemeDefinition BuildDefinition(const ThemeSource& source, bool userDefined = true);

/// Parses CSS color syntax: #rgb, #rgba, #rrggbb, #rrggbbaa, rgb(), rgba(),
/// and a small set of named colors.
std::optional<ThemeColor> ParseColor(std::string_view text);

/// Coarse color-family classification from the accent hue.
ThemeFamily DeriveFamily(const ThemeColor& accent);

/// Strict extra-CSS check for the IE11 preview. Returns the CSS when safe,
/// otherwise an empty string and appends a human-readable reason to warnings.
/// v1 rejects characters/functions that make injection or network access
/// possible rather than trying to rewrite the stylesheet.
std::string SanitizeThemeCss(const std::string& css, std::vector<std::string>& warnings);

/// ASCII slug for a theme id/file name. Non-ASCII characters are dropped.
std::string ThemeSlug(std::string_view name);

/// Returns a "user-..." id that does not collide with any id in
/// `existingIds` (a newline-separated list).
std::string MakeThemeId(std::string_view name, std::string_view existingIds);

} // namespace awm::theme
