#pragma once

#include <string>

#include "document/outline.h"

namespace awm {

/// Extracts ATX headings (`#`, `##`, ...) from Markdown source into an outline.
///
/// Each entry carries its display text, heading level and 1-based source line.
/// Implementation is independent of the Markdown rendering engine so that the
/// navigation outline and the rendered HTML stay decoupled.
Outline extractHeadings(const std::string& markdown);

/// Produces a preview anchor from a heading's display text.
///
/// ASCII letters are lower-cased, whitespace becomes '-', other ASCII
/// punctuation is dropped, and non-ASCII (UTF-8) bytes are preserved verbatim.
std::string slugifyAnchor(const std::string& headingText);

/// Makes every anchor in `outline` unique, appending -1, -2, ... to duplicates.
///
/// Markdown allows repeated heading text; without this, every outline entry
/// would resolve to the first matching id in the rendered preview.
void makeAnchorsUnique(Outline& outline);

} // namespace awm
