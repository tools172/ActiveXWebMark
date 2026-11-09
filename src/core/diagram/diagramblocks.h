#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace awm {

/// Fenced-block diagram renderers understood by the preview.
enum class DiagramKind {
    Mermaid,
    PlantUml,
};

struct DiagramTransformOptions {
    /// Per-kind availability. When false the original code block is kept.
    bool mermaidEnabled = false;
    bool plantumlEnabled = false;
    /// Directory holding cached SVG files. Must be non-empty when enabled.
    std::filesystem::path cacheDirectory;
    /// Mermaid theme and renderer version (cache key components).
    std::string mermaidTheme = "default";
    std::string mermaidVersion = "11.12.1";
    /// PlantUML backend: "remote" (Kroki) or "native" (bundled headless exe).
    std::string plantumlMode = "remote";
    /// Remote server and native executable. The mode selects which is used.
    std::string plantumlServer = "https://kroki.io";
    std::string plantumlExe;
    std::string plantumlVersion = "kroki-v1";
    /// Dark preview theme; the renderer uses a transparent background and a
    /// dark PlantUML theme when the diagram does not set its own styling.
    bool plantumlDark = false;
    /// Sources larger than this are left as plain code blocks.
    std::size_t maxSourceBytes = 512 * 1024;
};

/// A block that still needs rendering.
struct DiagramPendingBlock {
    DiagramKind kind = DiagramKind::Mermaid;
    /// DOM id of the placeholder element in the preview document.
    std::string elementId;
    /// Decoded source.
    std::string source;
    /// Mermaid theme (empty for PlantUML).
    std::string theme;
    /// PlantUML backend: "remote" or "native" (empty for Mermaid).
    std::string mode;
    /// PlantUML remote server (empty for Mermaid/native).
    std::string server;
    /// PlantUML native executable (empty for Mermaid/remote).
    std::string exe;
    /// PlantUML preview appearance (false for Mermaid).
    bool dark = false;
    /// Destination SVG path.
    std::filesystem::path cachePath;
    /// file:// URL for cachePath (used once the file exists).
    std::string url;
};

struct DiagramTransformResult {
    std::string html;
    std::vector<DiagramPendingBlock> pending;
};

/// Replaces cmark-rendered diagram fenced blocks.
///
/// Mermaid blocks use `language-mermaid`; PlantUML uses `language-plantuml` or
/// `language-puml`. Cached diagrams become <img> elements directly; uncached
/// ones become a placeholder that keeps the source visible until the sidecar
/// writes the SVG. All other markup is copied unchanged.
DiagramTransformResult TransformDiagramBlocks(const std::string& bodyHtml,
                                              const DiagramTransformOptions& options);

/// Stable cache key for a kind/source/variant/version combination.
std::string DiagramCacheKey(DiagramKind kind, const std::string& source,
                            const std::string& variant, const std::string& version);

/// "mermaid" or "plantuml".
const char* DiagramKindName(DiagramKind kind);

} // namespace awm
