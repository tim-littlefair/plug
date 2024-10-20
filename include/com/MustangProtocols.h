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

#include <algorithm>
#include <fstream>
#include <iostream>

#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QByteArray>
#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QString>

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

        virtual std::array<Packet<EmptyPayload>,2> serializeInitCommand() = 0;
        virtual Packet<EmptyPayload> serializeLoadCommand() = 0;
        virtual InitialData decodePresetNamesAndSettings(std::vector<std::array<std::uint8_t, 64>> recieved_data) = 0;
    };

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

        InitialData decodePresetNamesAndSettings(std::vector<std::array<std::uint8_t, 64>> recieved_data)
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

    // Forward declaration of helper function which is used to unpack V3 JSON payloads
    static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<PacketRawType> packets, const std::string label);

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

#if 0
#endif
            return retval;
        }

        Packet<EmptyPayload> serializeLoadCommand()
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
            header2.fromBytes(header2Bytes);
            return Packet<EmptyPayload>{header2, EmptyPayload{}};
        }

        InitialData decodePresetNamesAndSettings(std::vector<std::array<std::uint8_t, 64>> recieved_data)
        {
            std::array<PacketRawType, 7> presetData{{}};
            std::vector<std::string>presetNames;
            extractResponsePayload_V3_USB(recieved_data, "initial_data");
            // decodePresetNamesAndSettings
            return {decode_data(presetData),presetNames};
        }
    };

    static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<PacketRawType> packets, const std::string label) {
        std::vector<uint8_t> retval = std::vector<uint8_t>();
        for (size_t i=0; i<packets.size(); ++i)
        {
            PacketRawType p = packets.at(i);
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

#ifdef INSTANTIATE_PROTOCOL_FACTORY_HERE
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
#endif


}

