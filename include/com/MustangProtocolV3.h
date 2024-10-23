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
#include "com/MustangProtocolBase.h"
#include "com/PacketSerializer.h"
#include "com/CommunicationException.h"
#include "com/Packet.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <cassert>

#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QByteArray>
#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QString>

// Forward declarations of helper functions
// definitions of these are at the end of the file, after the namespace closes
//static std::vector<uint8_t> hexStringVectorOfBytes(const std::string& hexString);
static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets, const std::string label);

namespace plug::com
{
    class MustangProtocolV3: public MustangProtocolBase {

        public:

        MustangProtocolV3(DeviceModel model):
        MustangProtocolBase(model)
        {

        };

        std::array<Packet<EmptyPayload>,2> serializeInitCommand()
        {
            std::array<Packet<EmptyPayload>,2> retval;

            Header header0{};
            std::array<uint8_t, 16> header0Bytes = {
                0x35,
                0x09,
                0x08,
                0x00,
                0x8a,
                0x07,
                0x04,
                0x08,
                0x00,
                0x10,
            };
            header0.fromBytes(header0Bytes);
            retval[0] = Packet<EmptyPayload>{header0, EmptyPayload{}};

            Header header1{};
            std::array<uint8_t, 16> header1Bytes = {
                0x35,
                0x07,
                0x08,
                0x00,
                0xb2,
                0x06,
                0x02,
                0x08,
                0x01,
                0x00,
                0x10,
            };
            header1.fromBytes(header1Bytes);
            retval[1] = Packet<EmptyPayload>{header1, EmptyPayload{}};

            return retval;
        }

        InitialData loadPresetData(const std::shared_ptr<Connection> conn)
        {
            std::array<PacketRawType, 7> presetData{{}};
            std::vector<std::string>presetNames;

            // TODO: number of presets to request should come from this->m_model
            for(int i=1; i<=60; ++i)
            {
                std::vector<std::array<std::uint8_t, 64>> recieved_data;

                const auto loadCommand = this->serializePresetRequestCommand(i);
                auto recieved = conn->send(loadCommand.getBytes());

                while (recieved != 0)
                {
                    const auto recvData = receivePacket(*conn);
                    recieved = recvData.size();
                    PacketRawType p{};
                    std::copy(recvData.cbegin(), recvData.cend(), p.begin());
                    recieved_data.push_back(p);
                }

                char presetFilename[20];

                snprintf(presetFilename,sizeof(presetFilename),"preset%02d",i);
                extractResponsePayload_V3_USB(recieved_data, presetFilename);
            }
            return {decode_data(presetData),presetNames};
        }

        private:

        Packet<EmptyPayload> serializePresetRequestCommand(int presetIndex)
        {
            Packet<EmptyPayload> retval;
            Header header2{};
            std::array<uint8_t, 16> header2Bytes = {
                0x35,
                0x07,
                0x08,
                0x00,
                0xca,
                0x06,
                0x02,
                0x08,
                0x01,
                0x01,
                0x00,
                0x10,
            };
            header2Bytes[8] = presetIndex;
            header2.fromBytes(header2Bytes);
            return Packet<EmptyPayload>{header2, EmptyPayload{}};
        }

    };
} // end of namespace

/*
// definitions of static helper functions
static std::vector<uint8_t> hexStringVectorOfBytes(const std::string& hexString)
{
    assert(hexString.length()%2==0);

    std::vector<uint8_t> retval;

    for (unsigned int i = 0; i < hexString.length(); i += 2)
    {
        std::string byteString = hexString.substr(i, 2);
        uint8_t nextByte = static_cast<uint8_t>(strtol(byteString.c_str(), NULL, 16));
        retval.push_back(nextByte);
    }

    return retval;
}
*/

static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets, const std::string label) {
    std::vector<uint8_t> retval = std::vector<uint8_t>();
    for (size_t i=0; i<packets.size(); ++i)
    {
        plug::com::PacketRawType p = packets.at(i);
        int json_start_offset =3;
        int json_length = p[2];

        // p[0] is always 0
        // p[1] is frame type
        // p[2] is signficant data in frame (after p[2])

        switch (p[1])
        {
            case 0x33:  // first frame of response
                // p[3] appears to hold number of bytes to be consumed
                // before JSON starts
                json_start_offset+= p[3] + 1;
                json_length -= ( p[3] + 1 ) ;
                break;

            case 0x34: // any frame other than first and last
                json_start_offset = 3;
                break;


            case 0x35: // last frame of response
                json_start_offset = 3;
                json_length -= 1;
                break;

            default:
                json_start_offset = 3;
                json_length=0;
                continue;
        }

        std::cout << "i=" << i << " p1[1:2]=" << static_cast<unsigned int>(p[1]) << " " << static_cast<unsigned int>(p[2]) << " " << json_start_offset << " " << json_length << std::endl;

        std::copy(
            p.cbegin() + json_start_offset,
            p.cbegin() + json_start_offset + json_length,
            std::back_inserter(retval)
        );
    }

#ifndef NDEBUG
    std::string json_dump_fname = label;
    json_dump_fname.append(".json");
    std::ofstream json_dump_stream(json_dump_fname);

    const char* jsonNullTerminatedCharString = reinterpret_cast<const char*>(&(retval.at(0)));


    QByteArray jsonQByteArray(jsonNullTerminatedCharString,retval.size()-1);
    QJsonParseError parseError;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonQByteArray, &parseError);
    if(jsonDocument.isNull())
    {
        json_dump_stream << "JSON parse error of type " << parseError.error
                            << " at offset " << parseError.offset  << std::endl << std::endl;
        json_dump_stream.write(jsonNullTerminatedCharString, retval.size()-1);
    }
    else
    {
        // dump a human-readable indented rendering of the single-line JSON retrieved from packets
        json_dump_stream << jsonDocument.toJson(QJsonDocument::Indented).data() << std::endl;
    }
    json_dump_stream.flush();
    json_dump_stream.close();
#endif

    return retval;
}

