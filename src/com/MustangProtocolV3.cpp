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

#include "com/MustangProtocolV3.h"

#include "com/Mustang.h"

#include <algorithm>

#include <iostream>
#include <iomanip>
#include <fstream>

#include <cassert>

#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QByteArray>
#include <qt6/QtCore/QJsonParseError>
#include <qt6/QtCore/QString>
#include <qt6/QtCore/QJsonObject>

// Forward declarations of helper functions
// definitions of these are at the end of the file, after the namespace closes
static void hexStringToArrayOf16Bytes(const std::string& inHexString, std::array<uint8_t,16>& outHeaderBytes);
static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets);
static void parse_preset_json(
    std::vector<uint8_t> response_bytes,
    const std::string& label,
    std::string& presetName,
    plug::amp_settings& presetAmpSettings,
    std::vector<plug::fx_pedal_settings>& presetEffects
);
static void debug_dump_hex(std::vector<uint8_t> retval, const std::string& label);

namespace plug::com
{

    MustangProtocolV3::MustangProtocolV3(DeviceModel model):
    MustangProtocolBase(model)
    {

    };

    std::array<Packet<EmptyPayload>,2> MustangProtocolV3::serializeInitCommand()
    {
        std::array<Packet<EmptyPayload>,2> retval;

        Header header0{};
        std::string hexBytes0("350908008a0704080010");
        std::array<uint8_t, 16> header0Bytes;
        hexStringToArrayOf16Bytes(hexBytes0, header0Bytes);
        header0.fromBytes(header0Bytes);
        retval[0] = Packet<EmptyPayload>{header0, EmptyPayload{}};

        Header header1{};
        std::string hexBytes1("35070800b2060208010010");
        std::array<uint8_t, 16> header1Bytes;
        hexStringToArrayOf16Bytes(hexBytes1, header1Bytes);
        header1.fromBytes(header1Bytes);
        retval[1] = Packet<EmptyPayload>{header1, EmptyPayload{}};

        return retval;
    }

    InitialData MustangProtocolV3::loadPresetData(const std::shared_ptr<Connection> conn)
    {
        m_ppConn = &conn;
        std::vector<uint8_t> current_preset_response_bytes = sendCommandAndReceiveResponse("current_preset","35070800c206020801");
        debug_dump_hex(current_preset_response_bytes,"current_preset");
        m_ppConn = NULL;
        std::string currentPresetName;

        std::vector<std::string> presetNames;
        amp_settings presetAmpSettings;
        std::vector<plug::fx_pedal_settings> presetEffects;
        for(int i=1; i<=60; ++i)
        {
            presetNames.push_back("x");
        }
        for(int i=1; i<=8; ++i)
        {
            fx_pedal_settings ps{FxSlot{0}, effects::EMPTY, 0, 0, 0, 0, 0, 0, false};
            presetEffects.push_back(ps);
        }

        parse_preset_json(current_preset_response_bytes, "current_preset", currentPresetName, presetAmpSettings, presetEffects);

        return InitialData{SignalChain{currentPresetName, presetAmpSettings, presetEffects},presetNames};
    }

    std::vector<uint8_t> MustangProtocolV3::sendCommandAndReceiveResponse(
        const char *command_description,
        const char *command_hex_bytes
    )
    {
        Header header;
        std::array<uint8_t, 16> headerBytes;
        hexStringToArrayOf16Bytes(command_hex_bytes, headerBytes);
        header.fromBytes(headerBytes);
        const auto command = Packet<EmptyPayload>{header, EmptyPayload{}};

        auto recieved = (*m_ppConn)->send(command.getBytes());

        if(recieved==0)
        {
            char exception_message[100];
            snprintf(
                exception_message,sizeof(exception_message),
                "Empty response to %s request",
                command_description
            );
            throw CommunicationException(exception_message);
        }

        const auto receivedData = receiveResponse((*m_ppConn), true);

        std::vector<uint8_t> response_bytes = extractResponsePayload_V3_USB(receivedData);

        return response_bytes;
    }



    Packet<EmptyPayload> MustangProtocolV3::serializePresetRequestCommand(int presetIndex)
    {
        Packet<EmptyPayload> retval;
        Header header2{};
        std::string hexBytes2("35070800ca060208010110");
        std::array<uint8_t, 16> header2Bytes;
        hexStringToArrayOf16Bytes(hexBytes2, header2Bytes);
        header2Bytes[8] = presetIndex;
        header2.fromBytes(header2Bytes);
        return Packet<EmptyPayload>{header2, EmptyPayload{}};
    }

    Packet<EmptyPayload> MustangProtocolV3::serializeNextRequestCommand(int index)
    {
        Packet<EmptyPayload> retval;
        Header header{};
        // "35:07:08:00:c2:06:02:08:01:
        std::string hexBytes("35070800c206020801");
        std::array<uint8_t, 16> headerBytes;
        hexStringToArrayOf16Bytes(hexBytes, headerBytes);
        headerBytes[8] = index;
        header.fromBytes(headerBytes);
        return Packet<EmptyPayload>{header, EmptyPayload{}};
    }
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

static std::vector<uint8_t> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets) {
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
                json_start_offset+= (p[3] + 1);
                json_length -= ( p[3] + 1 ) ;
                break;

            case 0x34: // any frame other than first and last
                json_start_offset = 3;
                break;

            case 0x35: // last frame of response
                json_start_offset = 3;
                json_length -= 1; // terminating null?
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
    return retval;
}

static void parse_preset_json(
    std::vector<uint8_t> response_bytes,
    const std::string& label,
    std::string& presetName,
    plug::amp_settings& presetAmpSettings,
    std::vector<plug::fx_pedal_settings>& presetEffects
){
    std::string json_dump_fname = label;
    json_dump_fname.append(".json");
    std::ofstream json_dump_stream(json_dump_fname);

    const char* jsonNullTerminatedCharString = reinterpret_cast<const char*>(&(response_bytes.at(0)));


    QByteArray jsonQByteArray(jsonNullTerminatedCharString,response_bytes.size()-1);
    QJsonParseError parseError;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonQByteArray, &parseError);
    if(parseError.error==14)
    {
        // valid JSON followed by extra stuff - reparse to the end of the valid JSON
        QByteArray jsonQByteArray2(jsonNullTerminatedCharString,parseError.offset);
        jsonDocument = QJsonDocument::fromJson(jsonQByteArray2, &parseError);
    }

    if(jsonDocument.isNull())
    {
        json_dump_stream << "JSON parse error of type " << parseError.error
                            << " at offset " << parseError.offset  << std::endl << std::endl;
        return;
    }

#ifndef NDEBUG
        // dump a human-readable indented rendering of the single-line JSON retrieved from packets
    json_dump_stream << jsonDocument.toJson(QJsonDocument::Indented).data() << std::endl;
    json_dump_stream.flush();
    json_dump_stream.close();
#endif

    QString qName = jsonDocument.object().value(QStringLiteral("info")).toObject().value(QStringLiteral("displayName")).toString();
    presetName = qPrintable(qName);
    presetAmpSettings.amp_num = plug::amps::MUSTANG_V3_AMP_NOT_IDENTIFIED;
    assert(presetEffects.size()>=1);
}

static void debug_dump_hex(std::vector<uint8_t> retval, const std::string& label)
{
#ifndef NDEBUG
    std::string hex_dump_fname = label;
    hex_dump_fname.append(".hex");
    std::ofstream hex_dump_stream(hex_dump_fname);

    std::string raw_dump_fname = label;
    raw_dump_fname.append(".raw");
    std::ofstream raw_dump_stream(raw_dump_fname);

    size_t hex_line_offset = 0;
    while(hex_line_offset<retval.size())
    {
        std::ostringstream line_hex_string;
        line_hex_string << std::hex << std::setfill('0');
        //line_hex_string.width(2);
        //line_hex_string.fill('0');
        std::ostringstream line_char_string;
        for(size_t line_byte_index = 0; line_byte_index<16; ++line_byte_index)
        {
            size_t vector_byte_offset = hex_line_offset + line_byte_index;
            if (vector_byte_offset>retval.size())
            {
                line_hex_string << "   ";
                line_char_string << " ";
                continue;
            }
            uint8_t byte_value = retval[vector_byte_offset];
            line_hex_string << " " << std::setw(2) << static_cast<unsigned int>(byte_value);
            if( (byte_value>=0x20) && (byte_value < 0x80) )
            {
                line_char_string << static_cast<char>(byte_value);
            }
            else
            {
                line_char_string << '.';
            }
            raw_dump_stream << static_cast<char>(byte_value);
        }
        //line_hex_string << std::ends;
        //line_char_string << std::ends;
        hex_dump_stream << line_hex_string.str() << "   " << line_char_string.str() << std::endl;
        hex_line_offset += 16;
    }
    raw_dump_stream.close();
    hex_dump_stream.close();
#endif
}
