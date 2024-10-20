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

        MustangProtocolV1V2(DeviceModel model):
        MustangProtocolBase(model)
        {

        };

        std::array<Packet<EmptyPayload>,2> serializeInitCommand()
        {
            return plug::com::serializeInitCommand();
        }

        Packet<EmptyPayload> serializeLoadCommand()
        {
            return plug::com::serializeLoadCommand();
        }

        InitialData decodeLoadResponsePackets(std::vector<std::array<std::uint8_t, 64>> recieved_data)
        {
            const std::size_t numPresetPackets = m_model.numberOfPresets() > 0 ? (m_model.numberOfPresets() * 2) : (recieved_data.size() > 143 ? 200 : 48);
            std::vector<Packet<NamePayload>> presetListData;
            presetListData.reserve(numPresetPackets);
            std::transform(recieved_data.cbegin(), std::next(recieved_data.cbegin(), numPresetPackets), std::back_inserter(presetListData), [](const auto& p)
                        {
                Packet<NamePayload> packet{};
                packet.fromBytes(p);
                return packet; });
            auto presetNames = decodePresetListFromData(presetListData);

            std::array<PacketRawType, 7> presetData{{}};
            std::copy(std::next(recieved_data.cbegin(), numPresetPackets), std::next(recieved_data.cbegin(), numPresetPackets + 7), presetData.begin());

            return {decode_data(presetData), presetNames};
        }
    };
}

