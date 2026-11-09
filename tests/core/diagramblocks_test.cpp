#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "diagram/diagramblocks.h"

using namespace awm;

namespace {

constexpr const char* kMermaidSource = "graph TD; A-->B;\n";
constexpr const char* kPlantUmlSource = "@startuml\nAlice -> Bob: hello\n@enduml\n";

std::string CodeBlock(const std::string& language, const std::string& escapedSource,
                      const std::string& sourcePos = "1:1-3:3")
{
    std::string html = "<pre";
    if (!sourcePos.empty()) {
        html += " data-sourcepos=\"" + sourcePos + "\"";
    }
    html += "><code class=\"language-" + language + "\">";
    html += escapedSource;
    html += "</code></pre>";
    return html;
}

std::string MermaidBlock(const std::string& escapedSource,
                         const std::string& sourcePos = "1:1-3:3")
{
    return CodeBlock("mermaid", escapedSource, sourcePos);
}

std::string PlantUmlBlock(const std::string& escapedSource,
                          const std::string& language = "plantuml",
                          const std::string& sourcePos = "1:1-4:7")
{
    return CodeBlock(language, escapedSource, sourcePos);
}

class TempDirectory {
public:
    TempDirectory()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("awm-diagram-test-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

DiagramTransformOptions OptionsFor(const std::filesystem::path& cacheDir)
{
    DiagramTransformOptions options;
    options.mermaidEnabled = true;
    options.plantumlEnabled = true;
    options.cacheDirectory = cacheDir;
    options.mermaidTheme = "default";
    options.mermaidVersion = "11.12.1";
    options.plantumlMode = "remote";
    options.plantumlServer = "https://kroki.io";
    options.plantumlVersion = "kroki-v1";
    options.plantumlDark = false;
    return options;
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
}

} // namespace

TEST(DiagramBlocks, LeavesOtherMarkupUntouched)
{
    TempDirectory temp;
    const std::string body =
        "<p>before</p><pre><code class=\"language-cpp\">int x = 1;</code></pre><p>after</p>";

    const auto result = TransformDiagramBlocks(body, OptionsFor(temp.path()));
    EXPECT_EQ(result.html, body);
    EXPECT_TRUE(result.pending.empty());
}

TEST(DiagramBlocks, ReplacesMermaidBlockWithPlaceholder)
{
    TempDirectory temp;
    const std::string body =
        "<p>before</p>" + MermaidBlock("graph TD; A--&gt;B;\n") + "<p>after</p>";

    const auto result = TransformDiagramBlocks(body, OptionsFor(temp.path()));
    ASSERT_EQ(result.pending.size(), 1u);

    EXPECT_NE(result.html.find("class=\"awm-diagram\""), std::string::npos);
    EXPECT_NE(result.html.find("data-diagram-kind=\"mermaid\""), std::string::npos);
    EXPECT_NE(result.html.find("id=\"awm-diagram-mermaid-"), std::string::npos);
    EXPECT_NE(result.html.find("data-sourcepos=\"1:1-3:3\""), std::string::npos);
    EXPECT_NE(result.html.find("正在渲染图表"), std::string::npos);
    EXPECT_NE(result.html.find("graph TD; A--&gt;B;"), std::string::npos);

    const DiagramPendingBlock& pending = result.pending.front();
    EXPECT_EQ(pending.kind, DiagramKind::Mermaid);
    EXPECT_EQ(pending.source, kMermaidSource);
    EXPECT_EQ(pending.theme, "default");
    EXPECT_TRUE(pending.server.empty());
    EXPECT_EQ(pending.cachePath.extension(), ".svg");
    EXPECT_EQ(pending.url.rfind("file:///", 0), 0u);
}

TEST(DiagramBlocks, ReplacesPlantUmlBlockWithPlaceholder)
{
    TempDirectory temp;
    const std::string body =
        PlantUmlBlock("@startuml\nAlice -&gt; Bob: hello\n@enduml\n");

    const auto result = TransformDiagramBlocks(body, OptionsFor(temp.path()));
    ASSERT_EQ(result.pending.size(), 1u);

    EXPECT_NE(result.html.find("data-diagram-kind=\"plantuml\""), std::string::npos);
    EXPECT_NE(result.html.find("id=\"awm-diagram-plantuml-"), std::string::npos);
    EXPECT_NE(result.html.find("language-plantuml"), std::string::npos);

    const DiagramPendingBlock& pending = result.pending.front();
    EXPECT_EQ(pending.kind, DiagramKind::PlantUml);
    EXPECT_EQ(pending.source, kPlantUmlSource);
    EXPECT_TRUE(pending.theme.empty());
    EXPECT_EQ(pending.mode, "remote");
    EXPECT_EQ(pending.server, "https://kroki.io");
    EXPECT_TRUE(pending.exe.empty());
    EXPECT_FALSE(pending.dark);
}

TEST(DiagramBlocks, NativePlantUmlBackendCarriesExe)
{
    TempDirectory temp;
    DiagramTransformOptions options = OptionsFor(temp.path());
    options.plantumlMode = "native";
    options.plantumlExe = "C:\\PlantUML\\plantuml-headless.exe";
    options.plantumlVersion = "kroki-v1:native:123";
    options.plantumlDark = true;

    const auto result = TransformDiagramBlocks(
        PlantUmlBlock("@startuml\nA -&gt; B\n@enduml\n"), options);
    ASSERT_EQ(result.pending.size(), 1u);

    const DiagramPendingBlock& pending = result.pending.front();
    EXPECT_EQ(pending.kind, DiagramKind::PlantUml);
    EXPECT_EQ(pending.mode, "native");
    EXPECT_EQ(pending.exe, "C:\\PlantUML\\plantuml-headless.exe");
    EXPECT_TRUE(pending.server.empty());
    EXPECT_TRUE(pending.dark);
}

TEST(DiagramBlocks, PlantUmlAppearanceChangesCacheKey)
{
    TempDirectory temp;
    DiagramTransformOptions light = OptionsFor(temp.path());
    DiagramTransformOptions dark = OptionsFor(temp.path());
    dark.plantumlDark = true;

    const std::string body = PlantUmlBlock("@startuml\nA -&gt; B\n@enduml\n");
    const auto lightResult = TransformDiagramBlocks(body, light);
    const auto darkResult = TransformDiagramBlocks(body, dark);
    ASSERT_EQ(lightResult.pending.size(), 1u);
    ASSERT_EQ(darkResult.pending.size(), 1u);
    EXPECT_FALSE(lightResult.pending.front().dark);
    EXPECT_TRUE(darkResult.pending.front().dark);
    EXPECT_NE(lightResult.pending.front().cachePath,
              darkResult.pending.front().cachePath);
}

TEST(DiagramBlocks, SupportsPumlAlias)
{
    TempDirectory temp;
    const auto result = TransformDiagramBlocks(
        PlantUmlBlock("A -&gt; B\n", "puml"), OptionsFor(temp.path()));
    ASSERT_EQ(result.pending.size(), 1u);
    EXPECT_EQ(result.pending.front().kind, DiagramKind::PlantUml);
    EXPECT_NE(result.html.find("class=\"language-plantuml\""), std::string::npos);
}

TEST(DiagramBlocks, DisabledKindsLeaveOriginalMarkup)
{
    TempDirectory temp;
    const std::string body =
        MermaidBlock("graph TD; A--&gt;B;\n") +
        PlantUmlBlock("@startuml\nA -&gt; B\n@enduml\n");

    DiagramTransformOptions mermaidOnly = OptionsFor(temp.path());
    mermaidOnly.plantumlEnabled = false;
    auto result = TransformDiagramBlocks(body, mermaidOnly);
    ASSERT_EQ(result.pending.size(), 1u);
    EXPECT_EQ(result.pending.front().kind, DiagramKind::Mermaid);
    EXPECT_NE(result.html.find("language-plantuml"), std::string::npos);

    DiagramTransformOptions plantUmlOnly = OptionsFor(temp.path());
    plantUmlOnly.mermaidEnabled = false;
    result = TransformDiagramBlocks(body, plantUmlOnly);
    ASSERT_EQ(result.pending.size(), 1u);
    EXPECT_EQ(result.pending.front().kind, DiagramKind::PlantUml);
    EXPECT_NE(result.html.find("language-mermaid"), std::string::npos);
}

TEST(DiagramBlocks, HandlesBothKindsInOneDocument)
{
    TempDirectory temp;
    const std::string body =
        MermaidBlock("graph TD; A--&gt;B;\n", "1:1-3:3") + "<p>middle</p>" +
        PlantUmlBlock("@startuml\nA -&gt; B\n@enduml\n", "plantuml", "5:1-7:3");

    const auto result = TransformDiagramBlocks(body, OptionsFor(temp.path()));
    ASSERT_EQ(result.pending.size(), 2u);
    EXPECT_EQ(result.pending[0].kind, DiagramKind::Mermaid);
    EXPECT_EQ(result.pending[1].kind, DiagramKind::PlantUml);
    EXPECT_NE(result.pending[0].elementId, result.pending[1].elementId);
    EXPECT_NE(result.pending[0].cachePath, result.pending[1].cachePath);
    EXPECT_NE(result.html.find("<p>middle</p>"), std::string::npos);
}

TEST(DiagramBlocks, UsesCachedSvgDirectly)
{
    TempDirectory temp;
    const std::string key =
        DiagramCacheKey(DiagramKind::Mermaid, kMermaidSource, "default", "11.12.1");
    const std::filesystem::path cachePath = temp.path() / (key + ".svg");
    WriteFile(cachePath, "<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>");

    const auto result = TransformDiagramBlocks(
        MermaidBlock("graph TD; A--&gt;B;\n"), OptionsFor(temp.path()));
    EXPECT_TRUE(result.pending.empty());
    EXPECT_NE(result.html.find("<img src=\"file:///"), std::string::npos);
    EXPECT_EQ(result.html.find("正在渲染图表"), std::string::npos);
}

TEST(DiagramBlocks, CacheKeyVariesByKindThemeAndServer)
{
    const std::string mermaid =
        DiagramCacheKey(DiagramKind::Mermaid, "x", "default", "1");
    const std::string mermaidDark =
        DiagramCacheKey(DiagramKind::Mermaid, "x", "dark", "1");
    const std::string plantUml =
        DiagramCacheKey(DiagramKind::PlantUml, "x", "https://kroki.io", "1");
    const std::string plantUmlOther =
        DiagramCacheKey(DiagramKind::PlantUml, "x", "https://kroki.example.com", "1");
    const std::string plantUmlNative =
        DiagramCacheKey(DiagramKind::PlantUml, "x",
                        "native:C:\\plantuml-headless.exe", "1");

    EXPECT_NE(mermaid, mermaidDark);
    EXPECT_NE(mermaid, plantUml);
    EXPECT_NE(plantUml, plantUmlOther);
    EXPECT_NE(plantUml, plantUmlNative);
    EXPECT_EQ(DiagramCacheKey(DiagramKind::Mermaid, "x", "default", "1"), mermaid);
}

TEST(DiagramBlocks, LeavesOversizedSourceAlone)
{
    TempDirectory temp;
    const std::string body = MermaidBlock("graph TD; A--&gt;B;\n");
    DiagramTransformOptions options = OptionsFor(temp.path());
    options.maxSourceBytes = 4;

    const auto result = TransformDiagramBlocks(body, options);
    EXPECT_EQ(result.html, body);
    EXPECT_TRUE(result.pending.empty());
}

TEST(DiagramBlocks, DecodesEscapedSource)
{
    TempDirectory temp;
    const std::string body = MermaidBlock("A[&lt;b&gt;] --&gt; B[&amp;]\n");

    const auto result = TransformDiagramBlocks(body, OptionsFor(temp.path()));
    ASSERT_EQ(result.pending.size(), 1u);
    EXPECT_EQ(result.pending.front().source, "A[<b>] --> B[&]\n");
    EXPECT_NE(result.html.find("A[&lt;b&gt;] --&gt; B[&amp;]"), std::string::npos);
}

TEST(DiagramBlocks, KindNames)
{
    EXPECT_STREQ(DiagramKindName(DiagramKind::Mermaid), "mermaid");
    EXPECT_STREQ(DiagramKindName(DiagramKind::PlantUml), "plantuml");
}
