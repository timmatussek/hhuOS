#include <device/bios/Bios.h>
#include "ColorGraphicsArrayProvider.h"
#include "ColorGraphicsArray.h"

namespace Device::Graphic {

ColorGraphicsArrayProvider::ColorGraphicsArrayProvider(bool prototypeInstance) : supportedModes(2) {
    if (prototypeInstance) {
        return;
    }

    supportedModes[0] = {40, 25, 4, 0x01};
    supportedModes[1] = {80, 25, 4, 0x03};

    Bios::CallParameters biosParameters{};
    biosParameters.ax = BiosFunction::CHECK_VIDEO_CARD;

    Bios::interrupt(0x10, biosParameters);
    auto cardType = static_cast<VideoCardType>(biosParameters.bx);

    switch (cardType) {
        case MONOCHROME:
            deviceName = "Generic MDA";
            videoMemorySize = 4096;
            break;
        case CGA_COLOR:
            deviceName = "Generic CGA";
            videoMemorySize = 16384;
            break;
        case EGA_COLOR:
        case EGA_MONOCHROME:
            deviceName = "Generic EGA";
            videoMemorySize = 131072;
            break;
        case PGA_COLOR:
            deviceName = "Generic PGA";
            videoMemorySize = 262144;
            break;
        case VGA_MONOCHROME:
        case VGA_COLOR:
            deviceName = "Generic VGA";
            videoMemorySize = 262144;
            break;
        case MCGA_COLOR_DIGITAL:
        case MCGA_MONOCHROME:
        case MCGA_COLOR:
            deviceName = "Generic MCGA";
            videoMemorySize = 65536;
            break;
        default:
            break;
    }
}

bool ColorGraphicsArrayProvider::isAvailable() {
    Bios::CallParameters biosParameters{};
    biosParameters.ax = BiosFunction::CHECK_VIDEO_CARD;

    Bios::interrupt(0x10, biosParameters);
    auto cardType = static_cast<VideoCardType>(biosParameters.bx);

    return cardType > CGA_COLOR && cardType != UNKNOWN;
}

Util::Graphic::Terminal &ColorGraphicsArrayProvider::initializeTerminal(TerminalProvider::ModeInfo &modeInfo) {
    if (!isAvailable()) {
        Device::Cpu::throwException(Device::Cpu::Exception::UNSUPPORTED_OPERATION, "CGA is not available on this machine!");
    }

    // Set video mode
    Bios::CallParameters biosParameters{};
    biosParameters.ax = modeInfo.modeNumber;
    Bios::interrupt(0x10, biosParameters);

    // Set cursor options
    biosParameters.ax = BiosFunction::SET_CURSOR_SHAPE;
    biosParameters.cx = CURSOR_SHAPE_OPTIONS;
    Bios::interrupt(0x10, biosParameters);

    return *new ColorGraphicsArray(modeInfo.columns, modeInfo.rows);
}

void ColorGraphicsArrayProvider::destroyTerminal(Util::Graphic::Terminal &terminal) {
    delete &terminal;
}

Util::Data::Array<ColorGraphicsArrayProvider::ModeInfo> ColorGraphicsArrayProvider::getAvailableModes() const {
    return supportedModes;
}

uint32_t ColorGraphicsArrayProvider::getVideoMemorySize() const {
    return videoMemorySize;
}

Util::Memory::String ColorGraphicsArrayProvider::getDeviceName() const {
    return deviceName;
}

Util::Memory::String ColorGraphicsArrayProvider::getClassName() {
    return CLASS_NAME;
}

}