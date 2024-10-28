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
#include <qt6/QtCore/QStringLiteral>
#include <qt6/QtCore/QJsonObject>
#include <qt6/QtCore/QJsonArray>

// Forward declarations of helper functions
// definitions of these are at the end of the file, after the namespace closes
static void hexStringToArrayOf16Bytes(const std::string& inHexString, std::array<uint8_t,16>& outHeaderBytes);
static std::vector<std::vector<uint8_t>> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets, int& fender_message_type);
static void parse_preset_json(
    std::vector<uint8_t> response_bytes,
    const std::string& label,
    std::string& presetName,
    plug::amp_settings& presetAmpSettings,
    std::vector<plug::fx_pedal_settings>& presetEffects
);
static void debug_dump_hex(std::vector<uint8_t> retval, const std::string& label);
static std::vector<uint8_t> array64_to_vector(std::array<uint8_t,64> a);
static unsigned int protobuf_read_varint(std::vector<uint8_t>p, size_t& protobuf_read_offset);
static const plug::amps* jsonNameToAmpId(std::string jsonName);

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
        int response_type_received;
        std::vector<std::vector<uint8_t>> current_preset_response_bytes = sendCommandAndReceiveResponse("current_preset","35070800c206020801", response_type_received);
        debug_dump_hex(current_preset_response_bytes[0],"current_preset");
        m_ppConn = NULL;
        std::string currentPresetName;

        std::vector<std::string> presetNames;
        amp_settings presetAmpSettings;
        presetAmpSettings.amp_num = plug::amps::STUDIO_PREAMP;
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

        parse_preset_json(current_preset_response_bytes[1], "current_preset", currentPresetName, presetAmpSettings, presetEffects);

        return InitialData{SignalChain{currentPresetName, presetAmpSettings, presetEffects},presetNames};
    }

    std::vector<std::vector<uint8_t>> MustangProtocolV3::sendCommandAndReceiveResponse(
        const char *command_description,
        const char *command_hex_bytes,
        int& response_message_type
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

        auto response_fields = extractResponsePayload_V3_USB(receivedData, response_message_type);

        return response_fields;
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

/* This function returns a vector of vectors of bytes
 * element 0 is the whole protobuf std::vector<std::vector<uint8_t>> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets)
 * elements 1.. are the top-level fields of the response
 */
static std::vector<std::vector<uint8_t>> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets, int& fender_message_type)
{
    std::vector<std::vector<uint8_t>> retval;
    retval.push_back(std::vector<uint8_t>());

    // See https://github.com/brentmaxwell/LtAmp/blob/d62fd958cebe231723b160b1d53814754ffe9fbb/Schema/protobuf/FenderMessageLT.proto#L75
    fender_message_type = -1;
    size_t protobuf_read_offset = -1;

    for (size_t i=0; i<packets.size(); ++i)
    {
        plug::com::PacketRawType p = packets.at(i);

        if(fender_message_type==-1)
        {
            // first frame
            // May or may not be the last frame too

            // Refer to
            // https://protobuf.dev/programming-guides/encoding/#structure
            // for information about protobuf types and their encoding
            assert(p[3]==0x08); // magic number for protobuf
            assert(p[4]==0x02); // always protobuf v2
            protobuf_read_offset = 5;
            // protobuf requires that the next item in the stream is the
            // 'tag' of the message structure, which is a variable-length-encoded integer
            // which combines the protobuf type of the message in the three least
            // significant bits with a magic number assigned for the message
            // in higher bits
            unsigned int fender_message_tag = protobuf_read_varint(array64_to_vector(p), protobuf_read_offset);
            assert( (fender_message_tag & 0x07) == 2); // protobuf type of whole message is 'LEN'
            fender_message_type = protobuf_read_offset + (fender_message_tag >> 3);
        }
        assert(fender_message_type!=-1);

#if 0
        // p[0] is always 0
        // p[1] is frame type (0x33=first-of-many, 0x34=middle-of-many, 0x35=last-of-one-or-many)
        // p[2] is length of signficant data in this packet (after p[2])
        // The rest of the packet is the whole or a fragment of a protobuf message,
        // which may wrap a JSON jsonDocument
        switch (p[1])
        {
            case 0x33:

                // p[3] appears to hold number of bytes of raw data to be consumed
                for (int j=0; j<=p[3]; ++j)
                {
                    preamble.push_back(p[3+j]);
                }
                json_start_offset+= (p[3] + 1);
                json_length -= ( p[3] + 1 ) ;
                break;

            case 0x34: // any frame other than first and last
                json_start_offset = 3;
                break;

            case 0x35: // last frame of response
                json_start_offset = 3;
                //json_length -= 1; // terminating null?
                // p[3] appears to hold number of bytes of raw data to be consumed
                for (int j=0; j<=p[3]; ++j)
                {
                    preamble.push_back(p[json_start_offset + json_length + j]);
                }
                json_start_offset+= (p[3] + 1);
                json_length -= ( p[3] + 1 ) ;

                break;


            default:
                json_start_offset = 3;
                json_length=0;
                continue;
        }
#endif
        int pb_start_offset = 3;
        int pb_length = p[2];

        std::copy(
            p.cbegin() + pb_start_offset,
            p.cbegin() + pb_start_offset + pb_length,
            std::back_inserter(retval[0])
        );
    }
    std::ofstream raw_dump_stream("response.raw");
    for (size_t i = 0; i<retval[0].size(); ++i)
    {
        raw_dump_stream << static_cast<char>(retval[0][i]);
    }
    raw_dump_stream.close();

    do
    {
        unsigned int next_field_length = protobuf_read_varint(retval[0],protobuf_read_offset);
        //unsigned int next_field_tag = protobuf_read_varint(retval[0],protobuf_read_offset);
        unsigned int next_field_type = 2; //next_field_tag & 0x07;
        switch(next_field_type)
        {
            case 2: // length in varint followed by sequence of bytes
                {
                    std::vector<uint8_t> next_field_bytes;
                    std::copy(
                        retval[0].cbegin() + protobuf_read_offset,
                        retval[0].cbegin() + protobuf_read_offset + next_field_length,
                        std::back_inserter(next_field_bytes)
                    );
                    protobuf_read_offset += next_field_length;
                    retval.push_back(next_field_bytes);
                }
                next_field_type = 0;
                break;

            default:
                // For the moment we are only interested in the first field.
                // and only if it is of type LEN
                protobuf_read_offset = retval[0].size();
        }
    } while(protobuf_read_offset<retval[0].size());

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

    response_bytes.push_back(static_cast<uint8_t>(0));
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
    QJsonArray audioGraphNodes = jsonDocument.object().value(QStringLiteral("audioGraph")).toObject().value(QStringLiteral("nodes")).toArray();
    for(qsizetype i=0; i<audioGraphNodes.count(); ++i)
    {
        auto node = audioGraphNodes[i].toObject();
        QString nodeFenderId = qPrintable(node.value(QStringLiteral("nodeFenderId")).toString());
        auto pAmpId = jsonNameToAmpId(std::string(qPrintable(nodeFenderId)));
        if(pAmpId != NULL)
        {
            presetAmpSettings.amp_num = *pAmpId;
            presetAmpSettings.bass = node.value(QStringLiteral("bass")).toDouble();
            presetAmpSettings.bias = node.value(QStringLiteral("bias")).toInt();
/*
            //presentAmpSettings.brightness = node.value(QStringLiteral("brightness")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();

            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
            presentAmpSettings.bass = node.value(QStringLiteral("base")).toDouble();
*/
        }

    }
    presetAmpSettings.amp_num = plug::amps::STUDIO_PREAMP;
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

static std::vector<uint8_t> array64_to_vector(std::array<uint8_t,64> a)
{
    std::vector<uint8_t> v;
    std::copy(a.cbegin(),a.cend(),std::back_inserter(v));
    return v;
}

static unsigned int protobuf_read_varint(std::vector<uint8_t> p, size_t& protobuf_read_offset)
{
    unsigned int retval=0;
    unsigned int multiplier = 1;
    do
    {
        uint8_t next_byte = p[protobuf_read_offset];
        ++protobuf_read_offset;
        if( (next_byte&0x80) == 0 )
        {
            retval += next_byte * multiplier;
            break;
        }
        else
        {
            retval += (next_byte&0x7F) * multiplier;
            multiplier *= 128;
        }
    } while(true);
    return retval;
}

#if 0
"FenderId": ,
"FenderId": ,
"FenderId": "",
"FenderId": "DUBS_Excelsior",
"FenderId": "DUBS_LinearGain",
"FenderId": "DUBS_Or120",
"FenderId": "",
"FenderId": "DUBS_Silvertone",
"FenderId": ,
"FenderId": ,

// Names here are copied from brentmaxwell's C# work at
// https://github.com/brentmaxwell/LtAmp/blob/d62fd958cebe231723b160b1d53814754ffe9fbb/LtAmpDotNet/LtAmpDotNet.Lib/Model/Preset/Node.cs#L74
/*
        DBUS_LinearGain,  //SUPER CLEAN
        DUBS_Excelsior,   //EXCELSIOR
        DUBS_Silvertone,  //SMALLTONE
        DUBS_Or120,       //DOOM METAL
        DUBS_Plexi87,     //70S ROCK
        DUBS_SuperSonic,  //BURN
        DUBS_MetalRect2,  //ALT METAL
        DUBS_MetalEvh3,   //SUPER HEAVY


        DUBS_Champ57,     //CHAMP
        DUBS_Twin57,	  //50S TWIN
        DUBS_Bassman59,   //BASSMAN
        DUBS_Princeton65, //PRINCETON
        DUBS_Deluxe65,    //DELUXE CLN
        DUBS_Twin65,      //TWIN CLEAN
        DUBS_DR103,       //70S UK CLN
        DUBS_Ac30Tb,      //60S UK CLN
        DUBS_Jcm800,      //80S ROCK
        DUBS_Rect2,       //90S ROCK
        DUBS_Evh3,        //METAL 2000
*/
#endif

static const std::map<std::string, plug::amps> json_amp_names {
            {"DUBS_Deluxe57", plug::amps::FENDER_57_DELUXE},
            {"DUBS_Bassman59", plug::amps::FENDER_59_BASSMAN },
            {"DUBS_Champ57", plug::amps::FENDER_57_CHAMP},
            {"DUBS_Deluxe65", plug::amps::FENDER_65_DELUXE_REVERB },
            {"DUBS_Princeton65", plug::amps::FENDER_65_PRINCETON, },
            {"DUBS_Twin65", plug::amps::FENDER_65_TWIN_REVERB},
            {"DUBS_SuperSonic", plug::amps::FENDER_SUPER_SONIC},
            {"DUBS_Ac30Tb", plug::amps::BRITISH_60S},
            {"DUBS_DR103", plug::amps::BRITISH_70S},
            {"DUBS_Jcm800", plug::amps::BRITISH_80S},
            {"DUBS_Rect2", plug::amps::AMERICAN_90S},
            {"DUBS_Evh3",plug::amps::METAL_2000},
            {"DUBS_Twin57", plug::amps::FENDER_57_TWIN},
            // {amps::STUDIO_PREAMP, "Studio Preamp"},
            // {amps::FENDER_60_THRIFT, "Fender '60s Thrift"},
            // {amps::BRITISH_COLOUR, "British Colour"},
            // {amps::BRITISH_WATTS, "British Watts"}};
};

static const plug::amps* jsonNameToAmpId(std::string jsonName)
{
    auto pPair = json_amp_names.find(jsonName);
    if(pPair!=json_amp_names.cend())
    {
        return &(pPair->second);
    }
    else
    {
        return NULL;
    }
}
