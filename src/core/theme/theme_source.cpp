#include "theme/theme_source.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace awm::theme {

namespace {

constexpr const char* kDefaultBodyFont =
    "'Segoe UI','Microsoft YaHei UI','Microsoft YaHei',sans-serif";
constexpr const char* kDefaultCodeFont =
    "Consolas,'Cascadia Mono','Courier New',monospace";

char AsciiLower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

std::string_view Trim(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

int HexValue(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

ThemeColor Opaque(ThemeColor color)
{
    color.a = 255;
    return color;
}

TokenColors DeriveTokens(const ThemeSource& source)
{
    TokenColors tokens;
    const ThemeColor& fg = source.text;
    const ThemeColor& bg = source.background;
    const ThemeColor& accent = source.accent;
    tokens.comment = Blend(fg, bg, 0.55);
    tokens.punctuation = Blend(fg, bg, 0.10);
    tokens.tag = accent;
    tokens.property = Blend(accent, fg, 0.35);
    tokens.booleanNumber = Blend(accent, fg, 0.20);
    tokens.string = Blend(accent, fg, 0.55);
    tokens.oper = Blend(fg, bg, 0.20);
    tokens.function = accent;
    tokens.keyword = Blend(accent, fg, 0.10);
    tokens.regex = Blend(accent, bg, 0.25);

    tokens.comment = EnsureContrast(tokens.comment, source.codeBackground, 3.0);
    tokens.punctuation = EnsureContrast(tokens.punctuation, source.codeBackground, 3.0);
    tokens.tag = EnsureContrast(tokens.tag, source.codeBackground, 3.0);
    tokens.property = EnsureContrast(tokens.property, source.codeBackground, 3.0);
    tokens.booleanNumber =
        EnsureContrast(tokens.booleanNumber, source.codeBackground, 3.0);
    tokens.string = EnsureContrast(tokens.string, source.codeBackground, 3.0);
    tokens.oper = EnsureContrast(tokens.oper, source.codeBackground, 3.0);
    tokens.function = EnsureContrast(tokens.function, source.codeBackground, 3.0);
    tokens.keyword = EnsureContrast(tokens.keyword, source.codeBackground, 3.0);
    tokens.regex = EnsureContrast(tokens.regex, source.codeBackground, 3.0);
    return tokens;
}

void FillTypography(ThemeTypography& typography)
{
    if (typography.bodyFont.empty()) {
        typography.bodyFont = kDefaultBodyFont;
    }
    if (typography.codeFont.empty()) {
        typography.codeFont = kDefaultCodeFont;
    }
    typography.bodySize = std::clamp(typography.bodySize, 8, 48);
    typography.lineHeight = std::clamp(typography.lineHeight, 1.0, 3.0);
    typography.maxWidth = std::clamp(typography.maxWidth, 320, 2400);
    typography.padTop = std::clamp(typography.padTop, 0, 200);
    typography.padRight = std::clamp(typography.padRight, 0, 200);
    typography.padBottom = std::clamp(typography.padBottom, 0, 200);
    typography.codeSize = std::clamp(typography.codeSize, 8, 32);
}

double HueOf(const ThemeColor& color)
{
    const double r = static_cast<double>(color.r) / 255.0;
    const double g = static_cast<double>(color.g) / 255.0;
    const double b = static_cast<double>(color.b) / 255.0;
    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double delta = maximum - minimum;
    if (delta <= 0.0) {
        return 0.0;
    }
    double hue = 0.0;
    if (maximum == r) {
        hue = 60.0 * std::fmod((g - b) / delta, 6.0);
    } else if (maximum == g) {
        hue = 60.0 * (((b - r) / delta) + 2.0);
    } else {
        hue = 60.0 * (((r - g) / delta) + 4.0);
    }
    if (hue < 0.0) {
        hue += 360.0;
    }
    return hue;
}

double SaturationOf(const ThemeColor& color)
{
    const double r = static_cast<double>(color.r) / 255.0;
    const double g = static_cast<double>(color.g) / 255.0;
    const double b = static_cast<double>(color.b) / 255.0;
    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double lightness = (maximum + minimum) / 2.0;
    const double delta = maximum - minimum;
    if (delta <= 0.0) {
        return 0.0;
    }
    const double denominator = 1.0 - std::abs(2.0 * lightness - 1.0);
    return denominator <= 0.0 ? 0.0 : delta / denominator;
}

std::uint32_t Fnv1a(std::string_view text)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char ch : text) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

std::set<std::string> ExplicitFieldsOf(const ThemeSourceDraft& draft)
{
    std::set<std::string> explicitFields;
    // background/text are the roots of every derivation and always present.
    explicitFields.insert(field::kBackground);
    explicitFields.insert(field::kText);

    const auto mark = [&explicitFields](const std::optional<ThemeColor>& value,
                                        const char* key) {
        if (value) {
            explicitFields.insert(key);
        }
    };
    mark(draft.muted, field::kMuted);
    mark(draft.accent, field::kAccent);
    mark(draft.link, field::kLink);
    mark(draft.border, field::kBorder);
    mark(draft.hr, field::kHr);
    mark(draft.strong, field::kStrong);
    mark(draft.emphasis, field::kEmphasis);
    mark(draft.listMarker, field::kListMarker);
    for (const std::optional<ThemeColor>& heading : draft.headings) {
        if (heading) {
            explicitFields.insert(field::kHeadings);
            break;
        }
    }
    mark(draft.blockquoteText, field::kBlockquoteText);
    mark(draft.blockquoteBorder, field::kBlockquoteBorder);
    mark(draft.codeBackground, field::kCodeBackground);
    mark(draft.codeText, field::kCodeText);
    mark(draft.codeBorder, field::kCodeBorder);
    mark(draft.tableBorder, field::kTableBorder);
    mark(draft.tableHeadBackground, field::kTableHeadBackground);
    mark(draft.selection, field::kSelection);
    mark(draft.findMatch, field::kFindMatch);
    mark(draft.findCurrent, field::kFindCurrent);
    mark(draft.findText, field::kFindText);
    mark(draft.sidebarBackground, field::kSidebarBackground);
    mark(draft.sidebarHover, field::kSidebarHover);
    mark(draft.danger, field::kDanger);
    if (draft.tokens) {
        explicitFields.insert(field::kTokens);
    }
    return explicitFields;
}

} // namespace

bool UiOverrides::any() const
{
    return windowBg || surface || navBg || titleBarBg || titleBarInactive || tabHover ||
           buttonHover || buttonPressed || divider || textPrimary || textSecondary ||
           accent || accentSoft || gutterBg || currentLine || selection || danger ||
           dirtyDot;
}

ThemeSource FinalizeSource(ThemeSourceDraft draft)
{
    ThemeSource source;
    source.explicitFields = ExplicitFieldsOf(draft);
    source.schemaVersion = draft.schemaVersion > 0 ? draft.schemaVersion : 1;
    source.id = std::move(draft.id);
    source.name = std::move(draft.name);
    source.author = std::move(draft.author);
    source.license = std::move(draft.license);
    source.source = std::move(draft.source);

    source.background = draft.background.value_or(Rgb(255, 255, 255));
    source.text = draft.text.value_or(Rgb(31, 35, 40));
    source.dark = draft.dark.value_or(RelativeLuminance(source.background) < 0.5);
    source.darkExplicit = draft.dark.has_value();

    source.accent = draft.accent.value_or(draft.link.value_or(Rgb(9, 105, 218)));
    source.link = draft.link.value_or(source.accent);
    source.muted =
        draft.muted.value_or(Over(Rgba(source.text.r, source.text.g, source.text.b, 128),
                                  source.background));
    source.border = draft.border.value_or(Blend(source.background, source.text, 0.15));
    source.hr = draft.hr.value_or(source.border);
    source.strong = draft.strong.value_or(source.text);
    source.emphasis = draft.emphasis.value_or(source.text);
    source.listMarker = draft.listMarker.value_or(source.muted);

    for (int i = 0; i < 6; ++i) {
        source.headings[i] =
            draft.headings[i].value_or(i == 5 ? source.muted : source.text);
    }

    source.blockquoteText = draft.blockquoteText.value_or(source.muted);
    source.blockquoteBorder = draft.blockquoteBorder.value_or(source.border);
    source.codeBackground = draft.codeBackground.value_or(
        Blend(source.background, source.text, source.dark ? 0.10 : 0.06));
    source.codeText = draft.codeText.value_or(source.text);
    source.codeBorder =
        draft.codeBorder.value_or(Blend(source.codeBackground, source.text, 0.10));
    source.tableBorder = draft.tableBorder.value_or(source.border);
    source.tableHeadBackground = draft.tableHeadBackground.value_or(source.codeBackground);
    source.selection = draft.selection.value_or(
        Rgba(source.accent.r, source.accent.g, source.accent.b, 64));
    source.findMatch = draft.findMatch.value_or(
        Rgba(source.accent.r, source.accent.g, source.accent.b, 102));
    source.findCurrent = draft.findCurrent.value_or(
        Rgba(source.accent.r, source.accent.g, source.accent.b, 153));
    source.findText = draft.findText.value_or(source.text);
    source.sidebarBackground =
        draft.sidebarBackground.value_or(Blend(source.background, source.text, 0.04));
    source.sidebarHover =
        draft.sidebarHover.value_or(Blend(source.background, source.text, 0.08));
    source.danger = draft.danger.value_or(Rgb(214, 69, 69));

    source.family = draft.family.value_or(DeriveFamily(source.accent));
    source.familyExplicit = draft.family.has_value();

    source.tokens = draft.tokens.value_or(DeriveTokens(source));

    source.typography = draft.typography;
    FillTypography(source.typography);

    std::vector<std::string> warnings;
    source.extraCss = SanitizeThemeCss(draft.extraCss, warnings);

    source.ui = draft.ui;
    source.overrides = std::move(draft.overrides);
    return source;
}

void MinimizeExplicitFields(ThemeSource& source)
{
    // Derive the reference from the base colors alone. Any authored field that
    // happens to equal its default is indistinguishable from a materialized
    // default, so treat it as derived; the value itself never changes.
    ThemeSourceDraft referenceDraft;
    referenceDraft.background = source.background;
    referenceDraft.text = source.text;
    referenceDraft.accent = source.accent;
    referenceDraft.dark = source.dark;
    const ThemeSource reference = FinalizeSource(std::move(referenceDraft));

    const auto dropIfSame = [&source](const char* key, const ThemeColor& actual,
                                      const ThemeColor& derived) {
        if (actual == derived) {
            source.explicitFields.erase(key);
        }
    };
    dropIfSame(field::kMuted, source.muted, reference.muted);
    dropIfSame(field::kBorder, source.border, reference.border);
    dropIfSame(field::kHr, source.hr, reference.hr);
    dropIfSame(field::kStrong, source.strong, reference.strong);
    dropIfSame(field::kEmphasis, source.emphasis, reference.emphasis);
    dropIfSame(field::kListMarker, source.listMarker, reference.listMarker);
    dropIfSame(field::kBlockquoteText, source.blockquoteText, reference.blockquoteText);
    dropIfSame(field::kBlockquoteBorder, source.blockquoteBorder,
               reference.blockquoteBorder);
    dropIfSame(field::kCodeBackground, source.codeBackground, reference.codeBackground);
    dropIfSame(field::kCodeText, source.codeText, reference.codeText);
    dropIfSame(field::kCodeBorder, source.codeBorder, reference.codeBorder);
    dropIfSame(field::kTableBorder, source.tableBorder, reference.tableBorder);
    dropIfSame(field::kTableHeadBackground, source.tableHeadBackground,
               reference.tableHeadBackground);
    dropIfSame(field::kSelection, source.selection, reference.selection);
    dropIfSame(field::kFindMatch, source.findMatch, reference.findMatch);
    dropIfSame(field::kFindCurrent, source.findCurrent, reference.findCurrent);
    dropIfSame(field::kFindText, source.findText, reference.findText);
    dropIfSame(field::kSidebarBackground, source.sidebarBackground,
               reference.sidebarBackground);
    dropIfSame(field::kSidebarHover, source.sidebarHover, reference.sidebarHover);
    dropIfSame(field::kDanger, source.danger, reference.danger);

    bool headingsMatch = true;
    for (int i = 0; i < 6 && headingsMatch; ++i) {
        headingsMatch = source.headings[i] == reference.headings[i];
    }
    if (headingsMatch) {
        source.explicitFields.erase(field::kHeadings);
    }

    if (source.tokens.comment == reference.tokens.comment &&
        source.tokens.punctuation == reference.tokens.punctuation &&
        source.tokens.tag == reference.tokens.tag &&
        source.tokens.property == reference.tokens.property &&
        source.tokens.booleanNumber == reference.tokens.booleanNumber &&
        source.tokens.string == reference.tokens.string &&
        source.tokens.oper == reference.tokens.oper &&
        source.tokens.function == reference.tokens.function &&
        source.tokens.keyword == reference.tokens.keyword &&
        source.tokens.regex == reference.tokens.regex) {
        source.explicitFields.erase(field::kTokens);
    }

    // link defaults to accent; when both were materialized at the same value
    // it is safe to let link follow later accent edits again.
    if (source.explicitFields.count(field::kAccent) != 0 &&
        source.link == source.accent) {
        source.explicitFields.erase(field::kLink);
    }

    // dark/family are inferred from background/accent unless a theme really
    // curates them; old files always wrote the inferred values.
    if (source.darkExplicit &&
        source.dark == (RelativeLuminance(source.background) < 0.5)) {
        source.darkExplicit = false;
    }
    if (source.familyExplicit && source.family == DeriveFamily(source.accent)) {
        source.familyExplicit = false;
    }
}

ThemeSource RefreshDerivedFields(const ThemeSource& source)
{
    const auto has = [&source](const char* key) {
        return source.explicitFields.find(key) != source.explicitFields.end();
    };

    ThemeSourceDraft draft;
    draft.schemaVersion = source.schemaVersion;
    draft.id = source.id;
    draft.name = source.name;
    draft.author = source.author;
    draft.license = source.license;
    draft.source = source.source;
    draft.background = source.background;
    draft.text = source.text;
    if (source.darkExplicit) {
        draft.dark = source.dark;
    }
    if (source.familyExplicit) {
        draft.family = source.family;
    }

    if (has(field::kMuted)) {
        draft.muted = source.muted;
    }
    if (has(field::kAccent)) {
        draft.accent = source.accent;
    }
    if (has(field::kLink)) {
        draft.link = source.link;
    }
    if (has(field::kBorder)) {
        draft.border = source.border;
    }
    if (has(field::kHr)) {
        draft.hr = source.hr;
    }
    if (has(field::kStrong)) {
        draft.strong = source.strong;
    }
    if (has(field::kEmphasis)) {
        draft.emphasis = source.emphasis;
    }
    if (has(field::kListMarker)) {
        draft.listMarker = source.listMarker;
    }
    if (has(field::kHeadings)) {
        for (int i = 0; i < 6; ++i) {
            draft.headings[i] = source.headings[i];
        }
    }
    if (has(field::kBlockquoteText)) {
        draft.blockquoteText = source.blockquoteText;
    }
    if (has(field::kBlockquoteBorder)) {
        draft.blockquoteBorder = source.blockquoteBorder;
    }
    if (has(field::kCodeBackground)) {
        draft.codeBackground = source.codeBackground;
    }
    if (has(field::kCodeText)) {
        draft.codeText = source.codeText;
    }
    if (has(field::kCodeBorder)) {
        draft.codeBorder = source.codeBorder;
    }
    if (has(field::kTableBorder)) {
        draft.tableBorder = source.tableBorder;
    }
    if (has(field::kTableHeadBackground)) {
        draft.tableHeadBackground = source.tableHeadBackground;
    }
    if (has(field::kSelection)) {
        draft.selection = source.selection;
    }
    if (has(field::kFindMatch)) {
        draft.findMatch = source.findMatch;
    }
    if (has(field::kFindCurrent)) {
        draft.findCurrent = source.findCurrent;
    }
    if (has(field::kFindText)) {
        draft.findText = source.findText;
    }
    if (has(field::kSidebarBackground)) {
        draft.sidebarBackground = source.sidebarBackground;
    }
    if (has(field::kSidebarHover)) {
        draft.sidebarHover = source.sidebarHover;
    }
    if (has(field::kDanger)) {
        draft.danger = source.danger;
    }
    if (has(field::kTokens)) {
        draft.tokens = source.tokens;
    }

    draft.typography = source.typography;
    draft.extraCss = source.extraCss;
    draft.ui = source.ui;
    draft.overrides = source.overrides;

    ThemeSource result = FinalizeSource(std::move(draft));
    // FinalizeSource sanitizes extra CSS; while the dialog is still being
    // edited keep the raw text and only sanitize it on save.
    result.extraCss = source.extraCss;
    return result;
}

ThemeSource SourceFromDefinition(const ThemeDefinition& definition, bool includeOverrides)
{
    ThemeSource source;
    source.id = definition.id;
    source.name = definition.displayName;
    source.author = definition.author;
    source.license = definition.license;
    source.source = definition.source;
    source.dark = definition.dark;
    source.darkExplicit = true;
    source.family = definition.family;
    source.familyExplicit = true;

    source.background = definition.preview.pageBg;
    source.text = definition.preview.pageText;
    source.muted = definition.preview.mutedText;
    source.accent = definition.chrome.accent;
    source.link = definition.preview.link;
    source.border = definition.preview.border;
    source.hr = definition.preview.hr;
    source.strong = definition.preview.strong;
    source.emphasis = definition.preview.em;
    source.listMarker = definition.preview.listMarker;
    for (int i = 0; i < 6; ++i) {
        source.headings[i] = definition.preview.headings[i];
    }
    source.blockquoteText = definition.preview.blockquoteText;
    source.blockquoteBorder = definition.preview.blockquoteBorder;
    source.codeBackground = definition.preview.codeBlockBg;
    source.codeText = definition.preview.codeBlockText;
    source.codeBorder = definition.preview.codeBlockBorder;
    source.tableBorder = definition.preview.tableBorder;
    source.tableHeadBackground = definition.preview.tableHeadBg;
    source.selection = definition.preview.selectionBg;
    source.findMatch = definition.preview.findMatchBg;
    source.findCurrent = definition.preview.findCurrentBg;
    source.findText = definition.preview.findText;
    source.sidebarBackground = definition.chrome.windowBg;
    source.sidebarHover = definition.chrome.tabHover;
    source.danger = definition.chrome.danger;
    source.tokens = definition.tokens;

    source.typography = definition.typography;
    FillTypography(source.typography);
    source.extraCss = definition.extraCss;

    if (includeOverrides) {
        const auto add = [&source](const char* key, const ThemeColor& color) {
            source.overrides.emplace(key, color);
        };
        add("chrome.windowBg", definition.chrome.windowBg);
        add("chrome.surface", definition.chrome.surface);
        add("chrome.navBg", definition.chrome.navBg);
        add("chrome.titleBarBg", definition.chrome.titleBarBg);
        add("chrome.titleBarInactive", definition.chrome.titleBarInactive);
        add("chrome.tabHover", definition.chrome.tabHover);
        add("chrome.buttonHover", definition.chrome.buttonHover);
        add("chrome.buttonPressed", definition.chrome.buttonPressed);
        add("chrome.divider", definition.chrome.divider);
        add("chrome.textPrimary", definition.chrome.textPrimary);
        add("chrome.textSecondary", definition.chrome.textSecondary);
        add("chrome.accent", definition.chrome.accent);
        add("chrome.accentSoft", definition.chrome.accentSoft);
        add("chrome.gutterBg", definition.chrome.gutterBg);
        add("chrome.currentLine", definition.chrome.currentLine);
        add("chrome.selection", definition.chrome.selection);
        add("chrome.danger", definition.chrome.danger);
        add("chrome.dirtyDot", definition.chrome.dirtyDot);

        add("editor.background", definition.editor.background);
        add("editor.text", definition.editor.text);
        add("editor.heading", definition.editor.heading);
        add("editor.emphasis", definition.editor.emphasis);
        add("editor.strong", definition.editor.strong);
        add("editor.code", definition.editor.code);
        add("editor.codeBlock", definition.editor.codeBlock);
        add("editor.link", definition.editor.link);
        add("editor.image", definition.editor.image);
        add("editor.listMarker", definition.editor.listMarker);
        add("editor.blockQuote", definition.editor.blockQuote);
        add("editor.html", definition.editor.html);
        add("editor.hrule", definition.editor.hrule);
        add("editor.comment", definition.editor.comment);
        add("editor.reference", definition.editor.reference);
        add("editor.gutterBg", definition.editor.gutterBg);
        add("editor.gutterText", definition.editor.gutterText);
        add("editor.currentLine", definition.editor.currentLine);
        add("editor.selection", definition.editor.selection);
        add("editor.selectionText", definition.editor.selectionText);
        add("editor.caret", definition.editor.caret);

        add("preview.codeBlockBorder", definition.preview.codeBlockBorder);
    }

    // A definition is a full snapshot: every color is authored. Defaults that
    // still match the base colors are marked as derived so editing those base
    // colors in the settings dialog keeps the derived values in step.
    source.explicitFields = {
        field::kBackground,        field::kText,
        field::kMuted,             field::kAccent,
        field::kLink,              field::kBorder,
        field::kHr,                field::kStrong,
        field::kEmphasis,          field::kListMarker,
        field::kHeadings,          field::kBlockquoteText,
        field::kBlockquoteBorder,  field::kCodeBackground,
        field::kCodeText,          field::kCodeBorder,
        field::kTableBorder,       field::kTableHeadBackground,
        field::kSelection,         field::kFindMatch,
        field::kFindCurrent,       field::kFindText,
        field::kSidebarBackground, field::kSidebarHover,
        field::kDanger,            field::kTokens,
    };
    MinimizeExplicitFields(source);
    return source;
}

std::optional<ThemeColor> ParseColor(std::string_view text)
{
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::string lower;
    lower.reserve(trimmed.size());
    for (const char ch : trimmed) {
        lower.push_back(AsciiLower(ch));
    }

    static const std::pair<const char*, ThemeColor> kNamed[] = {
        {"black", Rgb(0, 0, 0)},       {"white", Rgb(255, 255, 255)},
        {"red", Rgb(255, 0, 0)},       {"green", Rgb(0, 128, 0)},
        {"blue", Rgb(0, 0, 255)},      {"yellow", Rgb(255, 255, 0)},
        {"orange", Rgb(255, 165, 0)},  {"purple", Rgb(128, 0, 128)},
        {"gray", Rgb(128, 128, 128)},  {"grey", Rgb(128, 128, 128)},
        {"silver", Rgb(192, 192, 192)}, {"navy", Rgb(0, 0, 128)},
        {"teal", Rgb(0, 128, 128)},    {"transparent", Rgba(0, 0, 0, 0)},
    };
    for (const auto& [name, color] : kNamed) {
        if (lower == name) {
            return color;
        }
    }

    if (lower.front() == '#') {
        // Keep the view anchored to `lower`; lower.substr(1) would return a
        // temporary std::string and leave the view dangling.
        const std::string_view lowerView(lower);
        const std::string_view digits = lowerView.substr(1);
        const auto hexAt = [&digits](std::size_t index) {
            return HexValue(digits[index]);
        };
        if (digits.size() == 3 || digits.size() == 4) {
            int values[4] = {};
            for (std::size_t i = 0; i < digits.size(); ++i) {
                values[i] = hexAt(i);
                if (values[i] < 0) {
                    return std::nullopt;
                }
            }
            const int alpha = digits.size() == 4 ? values[3] * 17 : 255;
            return Rgba(values[0] * 17, values[1] * 17, values[2] * 17, alpha);
        }
        if (digits.size() == 6 || digits.size() == 8) {
            int values[8] = {};
            for (std::size_t i = 0; i < digits.size(); ++i) {
                values[i] = hexAt(i);
                if (values[i] < 0) {
                    return std::nullopt;
                }
            }
            const int alpha = digits.size() == 8 ? values[6] * 16 + values[7] : 255;
            return Rgba(values[0] * 16 + values[1], values[2] * 16 + values[3],
                        values[4] * 16 + values[5], alpha);
        }
        return std::nullopt;
    }

    const bool isRgb = lower.rfind("rgb(", 0) == 0;
    const bool isRgba = lower.rfind("rgba(", 0) == 0;
    if (!isRgb && !isRgba) {
        return std::nullopt;
    }
    const std::size_t open = lower.find('(');
    const std::size_t close = lower.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return std::nullopt;
    }
    const std::string body = lower.substr(open + 1, close - open - 1);
    double values[4] = {0.0, 0.0, 0.0, 1.0};
    int count = 0;
    std::size_t start = 0;
    while (start <= body.size() && count < 4) {
        const std::size_t comma = body.find(',', start);
        const std::string piece = body.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start);
        const std::string_view pieceTrimmed = Trim(piece);
        if (pieceTrimmed.empty()) {
            break;
        }
        const std::string pieceString(pieceTrimmed);
        char* end = nullptr;
        const double value = std::strtod(pieceString.c_str(), &end);
        if (end == pieceString.c_str()) {
            return std::nullopt;
        }
        values[count++] = value;
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (count < 3) {
        return std::nullopt;
    }
    const auto channel = [](double value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0, 255.0)));
    };
    int alpha = 255;
    if (count >= 4) {
        const double value = values[3];
        alpha = value <= 1.0 ? static_cast<int>(std::lround(value * 255.0))
                             : channel(value);
        alpha = std::clamp(alpha, 0, 255);
    }
    return Rgba(channel(values[0]), channel(values[1]), channel(values[2]), alpha);
}

ThemeFamily DeriveFamily(const ThemeColor& accent)
{
    if (SaturationOf(accent) < 0.12) {
        return ThemeFamily::Neutral;
    }
    const double hue = HueOf(accent);
    if (hue < 15.0 || hue >= 345.0) {
        return ThemeFamily::RedPink;
    }
    if (hue < 70.0) {
        return ThemeFamily::Warm;
    }
    if (hue < 165.0) {
        return ThemeFamily::Green;
    }
    if (hue < 195.0) {
        return ThemeFamily::Cyan;
    }
    if (hue < 255.0) {
        return ThemeFamily::Blue;
    }
    if (hue < 290.0) {
        return ThemeFamily::Purple;
    }
    return ThemeFamily::RedPink;
}

std::string SanitizeThemeCss(const std::string& css, std::vector<std::string>& warnings)
{
    if (css.empty()) {
        return {};
    }
    constexpr std::size_t kMaxCssBytes = 16 * 1024;
    if (css.size() > kMaxCssBytes) {
        warnings.push_back("额外 CSS 超过 16 KB，已忽略。");
        return {};
    }

    static const char* const kForbidden[] = {"<", ">", "\\", "@", "(", ")", "/*", "*/"};
    for (const char* token : kForbidden) {
        if (css.find(token) != std::string::npos) {
            warnings.push_back(std::string("额外 CSS 含有不允许的片段“") + token +
                               "”，已忽略。");
            return {};
        }
    }
    for (const unsigned char ch : css) {
        if (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t') {
            warnings.push_back("额外 CSS 含有控制字符，已忽略。");
            return {};
        }
    }
    return css;
}

std::string ThemeSlug(std::string_view name)
{
    std::string slug;
    slug.reserve(name.size());
    bool pendingDash = false;
    for (const unsigned char raw : name) {
        const char ch = AsciiLower(static_cast<char>(raw));
        const bool accepted = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
        if (accepted) {
            if (pendingDash && !slug.empty()) {
                slug.push_back('-');
            }
            pendingDash = false;
            slug.push_back(ch);
            if (slug.size() >= 32) {
                break;
            }
        } else {
            pendingDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "theme" : slug;
}

std::string MakeThemeId(std::string_view name, std::string_view existingIds)
{
    char hashText[16] = {};
    const std::uint32_t hash = Fnv1a(name);
    std::snprintf(hashText, sizeof(hashText), "%08x", hash);
    const std::string base = "user-" + ThemeSlug(name) + "-" + hashText;

    const auto exists = [existingIds](const std::string& candidate) {
        std::size_t start = 0;
        while (start <= existingIds.size()) {
            const std::size_t end = existingIds.find('\n', start);
            const std::string_view line = existingIds.substr(
                start, end == std::string_view::npos ? std::string_view::npos : end - start);
            if (line == candidate) {
                return true;
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        return false;
    };

    if (!exists(base)) {
        return base;
    }
    for (int suffix = 2; suffix < 10000; ++suffix) {
        const std::string candidate = base + "-" + std::to_string(suffix);
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return base + "-x";
}

} // namespace awm::theme
