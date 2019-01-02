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

#ifndef HHUOS_ATAPCIDRIVER_H
#define HHUOS_ATAPCIDRIVER_H

#include <devices/pci/PciDeviceDriver.h>
#include "AtaController.h"

class AtaPciDriver : public PciDeviceDriver {

public:

    AtaPciDriver() = default;

    ~AtaPciDriver() override = default;

    PCI_DEVICE_DRIVER_IMPLEMENT_CREATE_INSTANCE(AtaPciDriver);

    uint8_t getBaseClass() const override;

    uint8_t getSubClass() const override;

    PciDeviceDriver::SetupMethod getSetupMethod() const override;

    void setup(const Pci::Device &device) override;

private:

    Logger *log = nullptr;

    AtaController *primaryController = nullptr;
    AtaController *secondaryController = nullptr;

};

#endif
