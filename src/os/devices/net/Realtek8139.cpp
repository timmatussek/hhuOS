#include <kernel/memory/SystemManagement.h>
#include <kernel/services/TimeService.h>
#include <kernel/interrupts/IntDispatcher.h>
#include <kernel/services/DebugService.h>
#include "Realtek8139.h"

Realtek8139::Realtek8139() : log(Logger::get("RTL8139")) {

}

void Realtek8139::setup(const Pci::Device &dev) {

    pciDevice = dev;
    Pci::enableBusMaster(pciDevice.bus, pciDevice.device, pciDevice.function);
    Pci::enableIoSpace(pciDevice.bus, pciDevice.device, pciDevice.function);

    uint32_t ioAddress = Pci::readDoubleWord(pciDevice.bus, pciDevice.device, pciDevice.function, Pci::PCI_HEADER_BAR0);

    ioRegisters = new IOport((uint16_t) (ioAddress & ~0xFFU));

    readMac();

    log.trace("MAC : %s", (char*) macAddress);

    log.trace("Hardware version is 0b%b", (ioRegisters->indw(0x40) & TCR_HWVERID_MASK));

    log.trace("Powering on device");

    ioRegisters->outb(0x52, 0x00);

    reset();

    initReceiveBuffer();

    enableInterrupts();

    configureReceiveBuffer();

    configureTransmitBuffer();

    enable();

    plugin();
}

void Realtek8139::readMac() {

    uint32_t firstPart = ioRegisters->indw();

    uint16_t secondPart = ioRegisters->inw(0x04);

    mac = new uint8_t[6]{
        (uint8_t) (firstPart >> 0U),
        (uint8_t) (firstPart >> 8U),
        (uint8_t) (firstPart >> 16U),
        (uint8_t) (firstPart >> 24U),
        (uint8_t) (secondPart >> 0U),
        (uint8_t) (secondPart >> 8U)
    };

    macAddress = String::format("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void Realtek8139::reset() {

    log.trace("Resetting NIC");

    ioRegisters->outb(0x37, 0x10U);

    while((ioRegisters->inb(0x37) & 0x10U) != 0);

    log.trace("Reset complete");
}

void Realtek8139::initReceiveBuffer() {

    log.trace("Initializing receive buffer");

    IOMemInfo info = SystemManagement::getInstance()->mapIO(4096);

    ioRegisters->outdw(0x30, info.physAddresses[0]);
}

void Realtek8139::enableInterrupts() {

    log.trace("Enabling interrupts");

    ioRegisters->outw(0x3C, 0x0005);
}

void Realtek8139::configureReceiveBuffer() {

    log.trace("Configuring receive buffer");

    ioRegisters->outdw(0x44, 0x0000070A);

    //TODO(krakowski)
    // ~8KB of contiguous physical memory is required here
}

void Realtek8139::configureTransmitBuffer() {

    IOMemInfo info{};

    for (uint8_t i = 0; i < BUFFER_COUNT; i++) {

        // TODO(krakowski)
        //  Get contiguous pages
        info = SystemManagement::getInstance()->mapIO(TRANSMIT_BUFFER_SIZE);

        txBuffers[i] = (uint8_t*) info.virtStartAddress;

        physTxBuffers[i] = (uint8_t*) info.physAddresses[0];
    }
}

void Realtek8139::enable() {

    log.trace("Enabling NIC");

    ioRegisters->outb(0x37, 0x0C);
}

void Realtek8139::advanceBuffer() {

    currentTxBuffer++;

    currentTxBuffer %= 4;
}

void Realtek8139::send(uint8_t *data, uint32_t length) {

    memcpy(txBuffers[currentTxBuffer], data, length);

    ioRegisters->outdw((uint16_t) (0x20 + (currentTxBuffer * 4)), (uint32_t) physTxBuffers[currentTxBuffer]);

    ioRegisters->outdw((uint16_t) (0x10 + (currentTxBuffer * 4)), length);

    advanceBuffer();
}

void Realtek8139::trigger() {

    uint16_t interruptStatus = ioRegisters->inw(0x3E);

    if (interruptStatus & 0x04U) {

        ioRegisters->outw(0x3E, 0x05);
    }
}

void Realtek8139::plugin() {

    log.trace("Assigning interrupt %d", pciDevice.intr);

    IntDispatcher::getInstance().assign((uint8_t) pciDevice.intr + (uint8_t) 32, *this);

    Pic::getInstance()->allow(pciDevice.intr);
}

bool Realtek8139::isBufferEmpty() {

    return (ioRegisters->inb(0x37) & CR_BUFE) == CR_BUFE;
}

void Realtek8139::receive() {

    while (!isBufferEmpty()) {

        uint32_t length = rxBuffer[rxBufferOffset + 3] << 8U | rxBuffer[rxBufferOffset + 2];

        rxBufferOffset += length + 4;

        if (rxBufferOffset % 4 != 0) {

            rxBufferOffset = (uint16_t) ((rxBufferOffset + (4 - 1)) & -4);
        }

        rxBufferOffset %= RECEIVE_BUFFER_SIZE;

        // TODO(krakowski)
        //  Read Rx buffer and send event using EventBus

        ioRegisters->outw(0x38, rxBufferOffset - (uint16_t) 16U);
    }
}