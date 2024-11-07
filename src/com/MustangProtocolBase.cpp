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

#include "com/MustangProtocolBase.h"

#include "com/Mustang.h"
#include "com/PacketSerializer.h"
#include "com/CommunicationException.h"
#include "com/Packet.h"
#include "com/Connection.h"
#include "com/Packet.h"

// Factory function needs to see derived class definitions
#include "com/MustangProtocolV1V2.h"
#include "com/MustangProtocolV3.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <cctype>

#include <cassert>

namespace plug::com
{

    MustangProtocolBase::MustangProtocolBase(DeviceModel model) :
        m_model(model)
    {
    }

    MustangProtocolBase::~MustangProtocolBase()
    {
    }

    std::vector<std::uint8_t> MustangProtocolBase::receivePacket(Connection& conn)
    {
        return conn.receive(packetRawTypeSize);
    }

    plug::com::Packet< plug::com::EmptyPayload > plug::com::MustangProtocolBase::serializeCommand(std::string hex_bytes)
{
        Header header;
        std::array<uint8_t, 16> headerBytes;
        hexStringToArrayOf16Bytes(hex_bytes, headerBytes);
        header.fromBytes(headerBytes);
        return Packet<EmptyPayload>{header, EmptyPayload{}};
    }

    std::vector<std::array<std::uint8_t, 64>> MustangProtocolBase::receiveResponse(
        const std::shared_ptr<Connection> conn, bool lastPacketCheck
    )
    {
        std::vector<std::array<std::uint8_t, 64>> received_data;
        size_t received_bytes;
        do
        {
            const auto recvData = receivePacket(*conn);
            received_bytes = recvData.size();
            PacketRawType p{};
            std::copy(recvData.cbegin(), recvData.cend(), p.begin());
            received_data.push_back(p);

            // On Mustang LT40S the second byte of recvData
            // being equal to 0x35 provides a reliable way
            // of detecting the end of the response without
            // trying to receive another packet and waiting
            // to time out.
            // I don't have a V1 or V2 amplifier to test
            // with to determine whether the same applies
            // for them but I suspect it will so I'm making
            // this available to both protocols in the base class.
            if(
                (lastPacketCheck==true) &&
                (recvData.size()>0) &&
                (recvData[1]==0x35)
            )
            {
                break;
            }
        } while(received_bytes>0);
        return received_data;
    }

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

    void hexStringToArrayOf16Bytes(const std::string& inHexString, std::array<uint8_t,16>& outByteArray)
    {
        assert(inHexString.length()%2==0);

        for (size_t i = 0; i<sizeof(outByteArray); ++i)
        {
            if(2*i<inHexString.length())
            {
                std::string byteString = inHexString.substr(2*i, 2);
                uint8_t nextByte = static_cast<uint8_t>(strtol(byteString.c_str(), NULL, 16));
                outByteArray[i] = nextByte;
            }
            else
            {
                outByteArray[i] = static_cast<uint8_t>(0);
            }
        }
    }

}


