#include "MultibootLinearFrameBufferProvider.h"
#include "Structure.h"

namespace Kernel::Multiboot {

MultibootLinearFrameBufferProvider::MultibootLinearFrameBufferProvider() : frameBufferInfo(), supportedModes(1) {
    supportedModes[0] = {frameBufferInfo.width, frameBufferInfo.height, frameBufferInfo.bpp, frameBufferInfo.pitch, 0};
}

bool MultibootLinearFrameBufferProvider::isAvailable() {
    const auto &frameBufferInfo = Structure::getFrameBufferInfo();
    return frameBufferInfo.type == FRAMEBUFFER_TYPE_RGB && frameBufferInfo.bpp >= 15;
}

Util::Graphic::LinearFrameBuffer& MultibootLinearFrameBufferProvider::initializeLinearFrameBuffer(MultibootLinearFrameBufferProvider::ModeInfo &modeInfo) {
    if (!isAvailable()) {
        Device::Cpu::throwException(Device::Cpu::Exception::UNSUPPORTED_OPERATION, "LFB mode has not been setup correctly by the bootloader!");
    }

    // TODO: Map framebuffer into IO memory
    return *new Util::Graphic::LinearFrameBuffer(reinterpret_cast<void*>(frameBufferInfo.address), frameBufferInfo.width, frameBufferInfo.height, frameBufferInfo.bpp, frameBufferInfo.pitch);
}

void MultibootLinearFrameBufferProvider::destroyLinearFrameBuffer(Util::Graphic::LinearFrameBuffer &lfb) {
    delete &lfb;
}

Util::Data::Array<MultibootLinearFrameBufferProvider::ModeInfo> MultibootLinearFrameBufferProvider::getAvailableModes() const {
    return supportedModes;
}

Util::Memory::String MultibootLinearFrameBufferProvider::getClassName() {
    return CLASS_NAME;
}

}