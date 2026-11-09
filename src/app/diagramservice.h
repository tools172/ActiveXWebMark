#pragma once

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "diagram/diagramblocks.h"

namespace awm {

/// Result of one asynchronous diagram render (Mermaid or PlantUML).
struct DiagramRenderResult {
    std::string elementId;
    bool ok = false;
    /// file:// URL of the rendered SVG when ok is true.
    std::string url;
    std::string error;
};

/// Owns the diagram sidecar (Node + Edge for Mermaid, remote Kroki for
/// PlantUML) and serializes render requests.
///
/// The sidecar is started lazily on the first request and exits after a period
/// of inactivity. If Node.js is missing or older than 18, or the vendored
/// sidecar files are absent, Available() returns false and the caller leaves
/// the original fenced blocks in the preview.
class DiagramService {
public:
    /// Base version must match tools/mermaid-sidecar/package.json; the suffix
    /// invalidates cached SVGs when the appearance handling changes.
    static constexpr const char* kMermaidVersion = "11.12.1:palette-v1";
    /// Bump when the PlantUML request or appearance handling changes.
    static constexpr const char* kPlantUmlVersion = "kroki-v3";
    static constexpr int kMinimumNodeMajor = 18;
    static constexpr int kIdleTimeoutMs = 300000;

    DiagramService();
    ~DiagramService();

    DiagramService(const DiagramService&) = delete;
    DiagramService& operator=(const DiagramService&) = delete;

    /// True when Node.js (>= 18) and the vendored sidecar are available.
    /// The detection runs once and is cached.
    bool Available();

    /// Mermaid also needs the bundled mermaid.min.js; env can force it off.
    bool MermaidEnabled() const;
    /// PlantUML uses the bundled native exe or the remote server; env can
    /// force it off.
    bool PlantUmlEnabled() const;
    void SetPlantUmlEnabled(bool enabled) { plantUmlEnabled_ = enabled; }
    const std::string& PlantUmlServer() const { return plantUmlServer_; }
    /// Ignored when AWM_PLANTUML_SERVER is set.
    void SetPlantUmlServer(std::string server);

    /// Effective backend: "native" when the headless executable is available,
    /// otherwise "remote".
    std::string PlantUmlBackend() const;
    std::string PlantUmlExe() const;
    /// Cache-key component; includes the exe signature for native mode.
    std::string PlantUmlVersion() const;

    /// "auto" (default), "local" or "remote"; "local" means the bundled native
    /// executable. Ignored when AWM_PLANTUML_MODE is set.
    void SetPlantUmlMode(std::string mode);
    /// Ignored when AWM_PLANTUML_EXE is set.
    void SetPlantUmlExe(std::string exe);

    const std::filesystem::path& cacheDirectory() const { return cacheDirectory_; }

    /// Queues a render. The callback runs on the service's reader thread; the
    /// caller is responsible for marshalling to the UI thread.
    ///
    /// If a render for the same cache file is already in flight, the callback
    /// is attached to that render instead of starting a duplicate.
    bool Request(const DiagramPendingBlock& block,
                 std::function<void(DiagramRenderResult)> callback);

    void Shutdown();

private:
    struct CallbackEntry {
        std::string elementId;
        std::function<void(DiagramRenderResult)> callback;
    };

    struct Pending {
        DiagramPendingBlock block;
        std::vector<CallbackEntry> callbacks;
    };

    bool Detect();
    bool ResolveNativePlantUml();
    bool Start();
    void Stop();
    void ReaderLoop();
    bool WriteLine(const std::string& line);
    void Complete(int id, const std::string& error);
    void FailAllPending(const std::string& error);

    std::wstring nodePath_;
    std::wstring sidecarPath_;
    std::wstring mermaidPath_;
    std::filesystem::path cacheDirectory_;
    bool detected_ = false;
    bool mermaidBundlePresent_ = false;
    bool mermaidDisabled_ = false;
    bool plantUmlEnabled_ = true;
    bool plantUmlDisabled_ = false;
    bool plantUmlServerFromEnv_ = false;
    std::string plantUmlServer_ = "https://kroki.io";
    std::string plantUmlMode_ = "auto";
    bool plantUmlModeFromEnv_ = false;
    std::wstring plantUmlExe_;
    bool plantUmlExeFromEnv_ = false;
    std::string plantUmlExeSignature_;
    std::atomic<bool> available_{false};
    std::atomic<bool> running_{false};

    HANDLE process_ = nullptr;
    HANDLE job_ = nullptr;
    HANDLE input_ = nullptr;
    HANDLE output_ = nullptr;
    std::thread reader_;

    std::mutex mutex_;
    std::map<int, Pending> pending_;
    std::atomic<int> nextId_{1};
};

} // namespace awm
