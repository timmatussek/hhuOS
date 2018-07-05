#include <kernel/memory/SystemManagement.h>
#include <kernel/services/TimeService.h>
#include <kernel/Kernel.h>
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

    ioRegisters = new IOport(ioAddress & (~0xFFU));

    readMac();

    log.trace("MAC : %s", (char*) macAddress);

    log.trace("Hardware Version is 0b%b", (ioRegisters->indw(0x40) & TCR_HWVERID_MASK));

    log.trace("Powering on device");

    ioRegisters->outb(0x52, 0x00);

    reset();

    initReceiveBuffer();

    enableInterrupts();

    configureReceiveBuffer();

    configureTransmitBuffer();

    enable();

    plugin();

    while(true) {

        log.trace("Sending data packet");

        const unsigned char *message[80];

        const unsigned char destination[6] { 0x90, 0x1b , 0x0e, 0x9a, 0xc6, 0x90};

        const unsigned char content[] = "Hello from hhuOS!";

        uint16_t type = 0x0806;

        memcpy((void*) message, mac, 6);
        memcpy((void*) (message + 6), destination, 6);
        memcpy((void*) (message + 12), &type, 2);
        memcpy((void*) (message + 14), content, strlen((char*) content));

        send((uint8_t*) message, 80);

        Kernel::getService<TimeService>()->msleep(1000);
    }


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

    ioRegisters->outdw(0x44, 0x00000000);
}

void Realtek8139::configureTransmitBuffer() {

    IOMemInfo info{};

    for (uint8_t i = 0; i < BUFFER_COUNT; i++) {

        // TODO(krakowski)
        //  Get contiguous pages
        info = SystemManagement::getInstance()->mapIO(TRANSMIT_BUFFER_SIZE);

        transmitBuffers[i] = (uint8_t*) info.virtStartAddress;

        physTransmitBuffers[i] = (uint8_t*) info.physAddresses[0];
    }
}

void Realtek8139::enable() {

    log.trace("Enabling NIC");

    ioRegisters->outb(0x37, 0x0C);
}

void Realtek8139::advanceBuffer() {

    currentBuffer++;

    currentBuffer %= 4;
}

void Realtek8139::send(uint8_t *data, uint32_t length) {

    memcpy(transmitBuffers[currentBuffer], data, length);

    ioRegisters->outdw(0x20 + (currentBuffer * 4), (uint32_t) physTransmitBuffers[currentBuffer]);

    ioRegisters->outdw(0x10 + (currentBuffer * 4), length);

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

    return registers->CR & CR_BUFE == CR_BUFE;
}

void Realtek8139::receive() {

}




