#pragma once

#include <Windows.h>

namespace awm::ui {

/// RAII memory-buffer for flicker-free custom painting.
///
/// Usage:
///   DoubleBuffer buffer(hdc, width, height);
///   if (buffer.valid()) { ... draw into buffer.dc() ...; buffer.present(); }
class DoubleBuffer {
public:
    DoubleBuffer(HDC target, int width, int height);
    ~DoubleBuffer();

    DoubleBuffer(const DoubleBuffer&) = delete;
    DoubleBuffer& operator=(const DoubleBuffer&) = delete;

    bool valid() const { return memoryDc_ != nullptr; }
    HDC dc() const { return memoryDc_; }

    /// Copies the buffer to the target DC.
    void present();

private:
    void cleanup();

    HDC target_ = nullptr;
    HDC memoryDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HBITMAP oldBitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace awm::ui
