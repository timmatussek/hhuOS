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

#include "AtaModule.h"
#include "AtaPciDriver.h"

MODULE_PROVIDER {

    return new AtaModule();
};

int32_t AtaModule::initialize() {

    log = &Logger::get("ATA");

    AtaPciDriver pciDriver;

    Pci::setupDeviceDriver(pciDriver);

    if(!AtaController::DEFAULT_PRIMARY_PORTS_IN_USE || !AtaController::DEFAULT_SECONDARY_PORTS_IN_USE) {
        log->trace("Searching for ATA controllers using the default ISA ports");

        if(AtaIsaDriver::isAvailable(!AtaController::DEFAULT_PRIMARY_PORTS_IN_USE,
                                     !AtaController::DEFAULT_SECONDARY_PORTS_IN_USE)) {

            isaDriver = new AtaIsaDriver(!AtaController::DEFAULT_PRIMARY_PORTS_IN_USE,
                                         !AtaController::DEFAULT_SECONDARY_PORTS_IN_USE);
        } else {
            log->info("No ATA controller using the default ISA ports available");
        }
    }

    return 0;
}

int32_t AtaModule::finalize() {

    return 0;
}

String AtaModule::getName() {

    return NAME;
}

Util::Array<String> AtaModule::getDependencies() {

    return Util::Array<String>(0);
}
