#pragma once

#include <memory>
#include <string>
#include <system_error>

#include "document/idocument.h"
#include "document/outline.h"

namespace awm {

/// Editable state for one open document (one tab).
///
/// IDocument remains an immutable format-specific snapshot produced by a
/// loader. DocumentSession owns the mutable text the editor works on, tracks
/// whether it differs from what is on disk, and performs save/save-as. Keeping
/// this separate from both the loader and the UI makes it unit-testable and
/// lets a future JSONL viewer reuse the same tab model.
class DocumentSession {
public:
    /// Creates a session from a freshly loaded document.
    static std::unique_ptr<DocumentSession> fromDocument(std::wstring path,
                                                         const IDocument& document);

    /// Creates a session that has no path yet (used by a future "New" command).
    static std::unique_ptr<DocumentSession> untitled(std::wstring title);

    const std::wstring& path() const { return path_; }
    bool hasPath() const { return !path_.empty(); }

    /// Display title (first heading at load time, otherwise the file stem).
    const std::wstring& title() const { return title_; }

    const std::string& source() const { return source_; }
    const Outline& outline() const { return outline_; }
    bool isDirty() const { return dirty_; }

    /// Replaces the editable source and recomputes the dirty flag by comparing
    /// against the last saved snapshot (so undo-to-original clears it).
    void setSource(std::string source);

    /// Marks the session dirty without copying the editor buffer. The editor
    /// uses this on every keystroke; setSource() later re-evaluates precisely.
    void markDirty() { dirty_ = true; }

    void setOutline(Outline outline) { outline_ = std::move(outline); }

    /// Marks the current source as the saved baseline.
    void markClean();

    /// Writes the current source to `path_`. Fails when the session has no path.
    bool save(std::error_code& ec);

    /// Writes to `path` and, on success, rebinds the session to that file.
    bool saveAs(const std::wstring& path, std::error_code& ec);

private:
    DocumentSession(std::wstring path, std::wstring fallbackTitle, std::string source,
                    Outline outline);

    void updateTitleFromPath();

    std::wstring path_;
    std::wstring fallbackTitle_;
    std::wstring title_;
    std::string source_;
    std::string savedSource_;
    Outline outline_;
    bool dirty_ = false;
};

} // namespace awm
