#include "document/headingparser.h"

#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace awm {

namespace {

// Returns the ATX heading level (1..6) if `line` is an ATX heading, else 0.
// Per CommonMark, the '#' run must be followed by a space/tab or end of line.
int atxHeadingLevel(const std::string& line)
{
    std::size_t i = 0;
    while (i < line.size() && i < 7 && line[i] == '#') {
        ++i;
    }
    if (i == 0 || i > 6) {
        return 0;
    }
    if (i < line.size() && line[i] != ' ' && line[i] != '\t') {
        return 0;
    }
    return static_cast<int>(i);
}

std::string trim(std::string s)
{
    constexpr const char* ws = " \t\r\n";
    const std::size_t first = s.find_first_not_of(ws);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
}

// Strips an optional trailing `###...` sequence and surrounding whitespace.
std::string stripClosingHashes(std::string s)
{
    s = trim(s);
    const std::size_t last = s.find_last_not_of('#');
    if (last == std::string::npos) {
        return s;
    }
    s.erase(last + 1);
    return trim(s);
}

} // namespace

Outline extractHeadings(const std::string& markdown)
{
    Outline outline;
    std::size_t pos = 0;
    int lineNumber = 1;

    while (pos <= markdown.size()) {
        const std::size_t nl = markdown.find('\n', pos);
        const std::string line = (nl == std::string::npos)
                                     ? markdown.substr(pos)
                                     : markdown.substr(pos, nl - pos);

        if (const int level = atxHeadingLevel(line); level > 0) {
            OutlineItem item;
            item.text = stripClosingHashes(line.substr(level));
            item.level = level;
            item.line = lineNumber;
            item.anchor = slugifyAnchor(item.text);
            outline.push_back(std::move(item));
        }

        if (nl == std::string::npos) {
            break;
        }
        pos = nl + 1;
        ++lineNumber;
    }

    return outline;
}

std::string slugifyAnchor(const std::string& headingText)
{
    // GitHub-style slug: lower-case ASCII, keep Unicode (UTF-8) bytes, replace
    // whitespace with '-', drop other ASCII punctuation, collapse runs of '-'.
    std::string out;
    out.reserve(headingText.size());
    bool pendingDash = false;

    for (const unsigned char ch : headingText) {
        if (ch >= 0x80 || std::isalnum(ch)) {
            if (pendingDash && !out.empty()) {
                out.push_back('-');
                pendingDash = false;
            }
            out.push_back(ch < 0x80 ? static_cast<char>(std::tolower(ch))
                                    : static_cast<char>(ch));
        } else if (ch == ' ' || ch == '\t') {
            pendingDash = true;
        }
    }
    while (!out.empty() && out.back() == '-') {
        out.pop_back();
    }
    return out.empty() ? "section" : out;
}

void makeAnchorsUnique(Outline& outline)
{
    // Two passes: first reserve anchors whose text is already unique so a
    // generated suffix can never collide with a later literal heading such as
    // "Foo 1" (slug "foo-1") or "Foo 2" (slug "foo-2").
    std::unordered_map<std::string, int> totals;
    totals.reserve(outline.size());
    for (const OutlineItem& item : outline) {
        const std::string base = item.anchor.empty() ? "section" : item.anchor;
        ++totals[base];
    }

    std::unordered_set<std::string> used;
    used.reserve(outline.size());
    for (const OutlineItem& item : outline) {
        const std::string base = item.anchor.empty() ? "section" : item.anchor;
        if (totals[base] == 1) {
            used.insert(base);
        }
    }

    for (OutlineItem& item : outline) {
        // slugifyAnchor() never returns an empty string, but the guard keeps
        // this helper safe for callers that build outlines by hand.
        const std::string base = item.anchor.empty() ? "section" : item.anchor;
        if (totals[base] == 1) {
            item.anchor = base; // Reserved in the first pass.
            continue;
        }

        std::string candidate = base;
        int suffix = 1;
        while (used.count(candidate) != 0) {
            candidate = base + "-" + std::to_string(suffix);
            ++suffix;
        }
        item.anchor = std::move(candidate);
        used.insert(item.anchor);
    }
}

} // namespace awm
