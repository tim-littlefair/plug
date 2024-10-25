/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2024  offa
 * Copyright (C) 2010-2016  piorekf <piorek@piorekf.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "com/MustangProtocolBase.h"
#include "com/Mustang.h"
#include "com/PacketSerializer.h"
#include "com/CommunicationException.h"
#include "com/Packet.h"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace plug::com
{
    // Declarations of helper functions used by the V1V2 protocol - these are implemented in Mustang.cpp
    SignalChain decode_data(const std::array<PacketRawType, 7>& data);

    class MustangProtocolV1V2: public MustangProtocolBase
    {

        public:

        MustangProtocolV1V2(DeviceModel model);

        std::array<Packet<EmptyPayload>,2> serializeInitCommand();

        Packet<EmptyPayload> serializeV1V2LoadCommand();

        InitialData loadPresetData(const std::shared_ptr<Connection> conn);
    };
}

