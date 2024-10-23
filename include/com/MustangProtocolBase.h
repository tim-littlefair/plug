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

#include "com/Mustang.h"
#include "com/PacketSerializer.h"
#include "com/CommunicationException.h"
#include "com/Packet.h"
#include "com/Connection.h"
#include "com/Packet.h"


#include <algorithm>
#include <fstream>
#include <iostream>

namespace plug::com
{

    class MustangProtocolBase {
    protected:
        MustangProtocolBase(DeviceModel model) :
            m_model(model)
        {
        }

        virtual ~MustangProtocolBase()
        {
        }

        DeviceModel m_model;

    public:

        static MustangProtocolBase* factory(DeviceModel model);

        std::vector<std::uint8_t> receivePacket(Connection& conn)
        {
            return conn.receive(packetRawTypeSize);
        }

        virtual std::array<Packet<EmptyPayload>,2> serializeInitCommand() = 0;
        virtual InitialData loadPresetData(const std::shared_ptr<Connection> conn) = 0;
    };
}


#ifdef INSTANTIATE_PROTOCOL_FACTORY_HERE

#include "com/MustangProtocolV1V2.h"
#include "com/MustangProtocolV3.h"

namespace plug::com
{
    MustangProtocolBase* MustangProtocolBase::factory(DeviceModel model)
    {
        switch ( model.category() )
        {
            case DeviceModel::Category::MustangV1:
            case DeviceModel::Category::MustangV2:
                return new MustangProtocolV1V2(model);

            case DeviceModel::Category::MustangV3_USB:
                return new MustangProtocolV3(model);

            default:
                return NULL;
        }
    }
}
#endif



