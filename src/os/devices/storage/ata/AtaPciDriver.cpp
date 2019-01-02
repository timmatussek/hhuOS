/*
 * Copyright (C) 2018 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 * Heinrich-Heine University
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "AtaPciDriver.h"
#include "AtaIsaDriver.h"

uint8_t AtaPciDriver::getBaseClass() const {
    return Pci::CLASS_MASS_STORAGE_DEVICE;
}

uint8_t AtaPciDriver::getSubClass() const {
    return Pci::SUBCLASS_IDE;
}

PciDeviceDriver::SetupMethod AtaPciDriver::getSetupMethod() const {
    return BY_CLASS;
}

void AtaPciDriver::setup(const Pci::Device &device) {
    log = &Logger::get("ATA");

    log->trace("Setting up ATA device on the PCI bus");

    uint32_t bar0 = Pci::readDoubleWord(device.bus, device.device, device.function, Pci::PCI_HEADER_BAR0);
    uint32_t bar1 = Pci::readDoubleWord(device.bus, device.device, device.function, Pci::PCI_HEADER_BAR1);
    uint32_t bar2 = Pci::readDoubleWord(device.bus, device.device, device.function, Pci::PCI_HEADER_BAR2);
    uint32_t bar3 = Pci::readDoubleWord(device.bus, device.device, device.function, Pci::PCI_HEADER_BAR3);

    bool primaryUseMmio = (bar0 & 0x01) != 0x01;
    bool secondaryUseMmio = (bar2 & 0x01) != 0x01;

    uint32_t primaryCommandPort = primaryUseMmio ? (bar0 & 0xfffffff0) : (bar0 & 0xffffffc);
    uint32_t primaryControlPort = primaryUseMmio ? (bar1 & 0xfffffff0) : (bar1 & 0xffffffc);
    uint32_t secondaryCommandPort = secondaryUseMmio ? (bar2 & 0xfffffff0) : (bar2 & 0xffffffc);
    uint32_t secondaryControlPort = secondaryUseMmio ? (bar3 & 0xfffffff0) : (bar3 & 0xffffffc);

    if(primaryCommandPort == 0x00 || primaryControlPort == 0x00) {
        // Primary controller is running ISA compatibility mode
        // We have to check manually, which ports the controller is using
        if(AtaIsaDriver::checkDrive(AtaIsaDriver::COMMAND_BASE_PORT_1, AtaIsaDriver::CONTROL_BASE_PORT_1, 0)||
           AtaIsaDriver::checkDrive(AtaIsaDriver::COMMAND_BASE_PORT_1, AtaIsaDriver::CONTROL_BASE_PORT_1, 1)) {

            log->info("Found primary controller running in ISA compatibility mode; command port: 0x%08x,"
                      "control port: 0x%08x, mmio: %s", AtaIsaDriver::COMMAND_BASE_PORT_1,
                      AtaIsaDriver::CONTROL_BASE_PORT_1, "false");

            primaryController = new AtaController(AtaIsaDriver::COMMAND_BASE_PORT_1,
                    AtaIsaDriver::CONTROL_BASE_PORT_1, false);
        }
    } else {
        log->info("Found primary controller running in PCI native mode; command port: 0x%08x, control port: 0x%08x,"
                  "mmio: %s", primaryCommandPort, primaryControlPort, primaryUseMmio ? "true" : "false");

        primaryController = new AtaController(primaryCommandPort, primaryControlPort, primaryUseMmio);
    }

    if(secondaryCommandPort == 0x00 || secondaryControlPort == 0x00) {
        // Secondary controller is running ISA compatibility mode
        // We have to check manually, which ports the controller is using
        if(AtaIsaDriver::checkDrive(AtaIsaDriver::COMMAND_BASE_PORT_2, AtaIsaDriver::CONTROL_BASE_PORT_2, 0)||
           AtaIsaDriver::checkDrive(AtaIsaDriver::COMMAND_BASE_PORT_2, AtaIsaDriver::CONTROL_BASE_PORT_2, 1)) {

            log->info("Found secondary controller running in ISA compatibility mode; command port: 0x%08x,"
                      "control port: 0x%08x, mmio: %s", AtaIsaDriver::COMMAND_BASE_PORT_2,
                      AtaIsaDriver::CONTROL_BASE_PORT_2, "false");

            secondaryController = new AtaController(AtaIsaDriver::COMMAND_BASE_PORT_2,
                    AtaIsaDriver::CONTROL_BASE_PORT_2, false);
        }
    } else {
        log->info("Found secondary controller running in PCI native mode; command port: 0x%08x, control port: 0x%08x,"
                  "mmio: %s", secondaryCommandPort, secondaryControlPort, secondaryUseMmio ? "true" : "false");

        secondaryController = new AtaController(secondaryCommandPort, secondaryControlPort, secondaryUseMmio);
    }

    log->trace("Finished setting up ATA device on the PCI bus");
}
