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

} // namespace awm
