#pragma once

#include <string>

#include "document/outline.h"

namespace awm {

/// Unified document abstraction.
///
/// The UI, preview and navigation only depend on this interface, so a future
/// JSONL (agent conversation) document type can be added without touching any
/// rendering or navigation code.
class IDocument {
public:
    virtual ~IDocument() = default;

    /// Human-readable document title (first H1, or file name).
    virtual std::string title() const = 0;

    /// Navigation outline (heading tree for Markdown; records for JSONL).
    virtual const Outline& outline() const = 0;

    /// Raw Markdown source (UTF-8) used for rendering and editing.
    virtual std::string markdownSource() const = 0;
};

} // namespace awm
