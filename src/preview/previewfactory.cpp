#include "ipreviewview.h"

#include "ie/iepreviewview.h"

namespace awm {

std::unique_ptr<IPreviewView> CreatePreviewView(PreviewEngine engine)
{
    switch (engine) {
    case PreviewEngine::Ie:
        return std::make_unique<IePreviewView>();
    // Miniblink / WebView2 / litehtml adapters are added here later.
    default:
        return nullptr;
    }
}

} // namespace awm
