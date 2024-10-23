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
static void hexStringToArrayOf16Bytes(const std::string& inHexString, std::array<uint8_t,16>& outHeaderBytes);
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
            std::string hexBytes0("350908008a0704080010");
            std::array<uint8_t, 16> header0Bytes;
            hexStringToArrayOf16Bytes(hexBytes0, header0Bytes);
            header0.fromBytes(header0Bytes);
            retval[0] = Packet<EmptyPayload>{header0, EmptyPayload{}};

            Header header1{};
#if 0
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
#else
            std::string hexBytes1("35070800b2060208010010");
            std::array<uint8_t, 16> header1Bytes;
            hexStringToArrayOf16Bytes(hexBytes1, header1Bytes);
#endif
            header1.fromBytes(header1Bytes);
            retval[1] = Packet<EmptyPayload>{header1, EmptyPayload{}};

            return retval;
        }

        InitialData loadPresetData(const std::shared_ptr<Connection> conn)
        {
            std::array<PacketRawType, 7> presetData{{}};
#if 0
        const auto name = decodeNameFromData(fromRawData<NamePayload>(data[0]));
        const auto amp = decodeAmpFromData(fromRawData<AmpPayload>(data[1]), fromRawData<AmpPayload>(data[6]));
        const auto effects = decodeEffectsFromData({{fromRawData<EffectPayload>(data[2]), fromRawData<EffectPayload>(data[3]),
                                                     fromRawData<EffectPayload>(data[4]), fromRawData<EffectPayload>(data[5])}});

        return SignalChain{name, amp, effects};
#endif

            std::vector<std::string>presetNames;

            for(size_t i=1; i<=m_model.numberOfPresets(); ++i)
            {
                const auto loadCommand = this->serializePresetRequestCommand(i);
                auto recieved = conn->send(loadCommand.getBytes());

                if(recieved==0)
                {
                    char exception_message[100];
                    snprintf(
                        exception_message,sizeof(exception_message),
                        "Empty response to request for preset %lu", i
                    );
                    throw CommunicationException(exception_message);
                }

                const auto receivedData = receiveResponse(conn, true);
                char presetFilename[20];

                snprintf(presetFilename,sizeof(presetFilename),"preset%02lu",i);
                extractResponsePayload_V3_USB(receivedData, presetFilename);
            }

            for(size_t i=1; i<5; ++i)
            {
                const auto loadCommand = this->serializeNextRequestCommand(i);
                auto recieved = conn->send(loadCommand.getBytes());

                if(recieved==0)
                {
                    char exception_message[100];
                    snprintf(
                        exception_message,sizeof(exception_message),
                        "Empty response to request for next %lu", i
                    );
                    throw CommunicationException(exception_message);
                }

                const auto receivedData = receiveResponse(conn, true);
                char dumpFilename[20];
                snprintf(dumpFilename,sizeof(dumpFilename),"next%02lu.dat",i);
#if 0
                std::ofstream dump_stream(dumpFilename);
                for(int i=0; i< receivedData.length())
                {
                    dump_stream.write(static_cast<uint8_t>(receivedData[i]),64);
                }
#endif
                std::cout << dumpFilename << std::endl;
            }

            return {decode_data(presetData),presetNames};
        }

        private:

        Packet<EmptyPayload> serializePresetRequestCommand(int presetIndex)
        {
            Packet<EmptyPayload> retval;
            Header header2{};
#if 0
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
#else
            std::string hexBytes2("35070800ca060208010110");
            std::array<uint8_t, 16> header2Bytes;
            hexStringToArrayOf16Bytes(hexBytes2, header2Bytes);
#endif
            header2Bytes[8] = presetIndex;
            header2.fromBytes(header2Bytes);
            return Packet<EmptyPayload>{header2, EmptyPayload{}};
        }

        Packet<EmptyPayload> serializeNextRequestCommand(int index)
        {
            Packet<EmptyPayload> retval;
            Header header{};
#if 0
            std::array<uint8_t, 16> headerBytes = {
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
            header.fromBytes(hexStringToArrayOf16Bytes(hexBytes));
#else
            // "07:08:00:c2:06:02:08:01:
            std::string hexBytes("070800c206020801");
            std::array<uint8_t, 16> headerBytes;
            hexStringToArrayOf16Bytes(hexBytes, headerBytes);
#endif
            headerBytes[8] = index;
            header.fromBytes(headerBytes);
            return Packet<EmptyPayload>{header, EmptyPayload{}};
        }
    };
} // end of namespace

// definitions of static helper functions

static void hexStringToArrayOf16Bytes(const std::string& inHexString, std::array<uint8_t,16>& outByteArray)
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

