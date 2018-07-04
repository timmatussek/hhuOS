#include <kernel/memory/SystemManagement.h>
#include <kernel/services/TimeService.h>
#include <kernel/Kernel.h>
#include "Realtek8139.h"

Realtek8139::Realtek8139() : log(Logger::get("RTL8139")) {

}

void Realtek8139::setup(const Pci::Device &dev) {

    pciDevice = dev;
    Pci::enableBusMaster(pciDevice.bus, pciDevice.device, pciDevice.function);
    Pci::enableMemorySpace(pciDevice.bus, pciDevice.device, pciDevice.function);

    uint32_t tmpAhciBase = Pci::readDoubleWord(pciDevice.bus, pciDevice.device, pciDevice.function, Pci::PCI_HEADER_BAR1);

    IOMemInfo memInfo = SystemManagement::getInstance()->mapIO(tmpAhciBase, 4096);
    registers = (Registers*) memInfo.virtStartAddress;

    log.trace("Powering on device");

    registers->CONFIG0 = 0x00;

    macAddress = String::format("%02x:%02x:%02x:%02x:%02x:%02x", registers->IDR0, registers->IDR1,
                                registers->IDR2, registers->IDR3, registers->IDR4, registers->IDR5);

    reset();

    initReceiveBuffer();

    enableInterrupts();

    configureReceiveBuffer();

    configureTransmitBuffer();

    enable();

    const char *message = "HELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLDHELLOWORLD";

    send((uint8_t*) message, 70);
}

void Realtek8139::reset() {

    log.trace("Resetting NIC");

    registers->CR = 0x10;

    while ((registers->CR & 0x10) != 0);
}

void Realtek8139::initReceiveBuffer() {

    log.trace("Initializing receive buffer");

    IOMemInfo info = SystemManagement::getInstance()->mapIO(4096);

    registers->RBSTART = info.physAddresses[0];
}

void Realtek8139::enableInterrupts() {

    log.trace("Enabling interrupts");

    registers->IMR = 0x0005;
}

void Realtek8139::configureReceiveBuffer() {

    log.trace("Configuring receive buffer");

    registers->RCR = 0x0F;
}

void Realtek8139::configureTransmitBuffer() {

    IOMemInfo info;

    for (uint8_t i = 0; i < BUFFER_COUNT; i++) {

        info = SystemManagement::getInstance()->mapIO(4096);

        transmitBuffers[i] = (uint8_t*) info.virtStartAddress;

        physTransmitBuffers[i] = (uint8_t*) info.physAddresses[0];
    }
}

void Realtek8139::enable() {

    log.trace("Enabling NIC");

    registers->CR = 0x0C;
}

void Realtek8139::advanceBuffer() {

    currentBuffer++;

    currentBuffer %= 4;
}

void Realtek8139::send(uint8_t *data, uint32_t length) {

    memcpy(transmitBuffers[currentBuffer], data, length);

    *(&registers->TSAD0 + currentBuffer) = (uint32_t) physTransmitBuffers[currentBuffer];

    *(&registers->TSD0 + currentBuffer) = length | (48 << 16);

    advanceBuffer();
}

void Realtek8139::trigger() {

}


