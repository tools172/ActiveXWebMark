#include "doublebuffer.h"

namespace awm::ui {

DoubleBuffer::DoubleBuffer(HDC target, int width, int height)
    : target_(target), width_(width), height_(height)
{
    if (target_ == nullptr || width_ <= 0 || height_ <= 0) {
        return;
    }

    memoryDc_ = ::CreateCompatibleDC(target_);
    if (memoryDc_ == nullptr) {
        return;
    }

    bitmap_ = ::CreateCompatibleBitmap(target_, width_, height_);
    if (bitmap_ == nullptr) {
        cleanup();
        return;
    }

    oldBitmap_ = static_cast<HBITMAP>(::SelectObject(memoryDc_, bitmap_));
}

DoubleBuffer::~DoubleBuffer()
{
    cleanup();
}

void DoubleBuffer::present()
{
    if (memoryDc_ != nullptr && target_ != nullptr) {
        ::BitBlt(target_, 0, 0, width_, height_, memoryDc_, 0, 0, SRCCOPY);
    }
}

void DoubleBuffer::cleanup()
{
    if (memoryDc_ != nullptr && oldBitmap_ != nullptr) {
        ::SelectObject(memoryDc_, oldBitmap_);
        oldBitmap_ = nullptr;
    }
    if (bitmap_ != nullptr) {
        ::DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (memoryDc_ != nullptr) {
        ::DeleteDC(memoryDc_);
        memoryDc_ = nullptr;
    }
}

} // namespace awm::ui
