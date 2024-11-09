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
#include "com/V3FenderIdLookup.h"
#include "com/V3MessageProtobuf.h"
#include "com/V3PresetJson.h"

#include "com/Mustang.h"

#include <algorithm>

#include <iostream>
#include <iomanip>
#include <fstream>

#include <cassert>

// Forward declarations of helper functions
// definitions of these are at the end of the file, after the namespace closes
static void debug_dump_hex(std::vector<uint8_t> retval, const std::string& label);

// Stores for the data most recently loaded back from the Mustang device
static std::vector<std::string> storedPresetNames;
static std::vector<plug::amp_settings> storedAmpSettings;
static std::vector<std::vector<plug::fx_pedal_settings>> storedEffects;


static int debug_verbosity = 0;
void set_v3_protocol_debug_verbosity(int v)
{
    ::debug_verbosity = v;
}

int get_v3_protocol_debug_verbosity()
{
    return ::debug_verbosity;
}

namespace plug::com
{

    MustangProtocolV3::MustangProtocolV3(DeviceModel model):
    MustangProtocolBase(model)
    {
        storedPresetNames.resize(model.numberOfPresets()+1);
        storedAmpSettings.resize(model.numberOfPresets()+1);
        storedEffects.resize(model.numberOfPresets()+1);
    };

    std::array<Packet<EmptyPayload>,2> MustangProtocolV3::serializeInitCommand()
    {
        std::array<Packet<EmptyPayload>,2> retval;

        Header header0{};
        std::string hexBytes0("350708008a0704080010");
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

    InitialData MustangProtocolV3::loadData(const std::shared_ptr<Connection> conn)
    {
        std::string currentPresetName;
        amp_settings currentAmpSettings;
        std::vector<plug::fx_pedal_settings> currentEffects;

        v3::populate_reverse_maps();

        m_ppConn = &conn;

        int response_type_received;
        std::vector<std::vector<uint8_t>> current_preset_response_bytes = sendCommandAndReceiveResponse("current_preset","35070800c206020801", response_type_received);
        debug_dump_hex(current_preset_response_bytes[0],"current_preset");
        for(int i=1; i<=4; ++i)
        {
            fx_pedal_settings ps{FxSlot{0}, effects::EMPTY, 0, 0, 0, 0, 0, 0};
            currentEffects.push_back(ps);
        }
        plug::com::v3::parse_preset_json(current_preset_response_bytes[1], "current_preset", currentPresetName, currentAmpSettings, currentEffects);
        assert(current_preset_response_bytes.size()==3);
        assert(current_preset_response_bytes[2].size()==4);
        assert(current_preset_response_bytes[2][0]==0x10);
        uint8_t current_preset_index = current_preset_response_bytes[2][1];
        static_cast<void>(current_preset_index);

        for(int i=1; i<=60; ++i)
        {
            std::ostringstream presetFilenameStr;
            presetFilenameStr << "preset_" << std::setfill('0') << std::setw(2) << i << std::ends;
            std::string presetFilename = presetFilenameStr.str();

            std::ostringstream storedPresetRequestStr;
            storedPresetRequestStr << "35070800ca060208" << std::setfill('0') << std::setw(2) << std::hex << i << "0110" << std::ends;
            std::string storedPresetRequest = storedPresetRequestStr.str();

            std::vector<std::vector<uint8_t>> stored_preset_response_bytes = sendCommandAndReceiveResponse(
                presetFilename.c_str(),
                storedPresetRequest.c_str(),
                response_type_received
            );
            plug::com::v3::parse_preset_json(
                stored_preset_response_bytes[1],
                presetFilename.c_str(),
                storedPresetNames[i], storedAmpSettings[i], storedEffects[i]
            );

            debug_dump_hex(current_preset_response_bytes[0],presetFilename.c_str());
        }


        m_ppConn = NULL;

        return InitialData{SignalChain{currentPresetName, currentAmpSettings, currentEffects},storedPresetNames};
    }

    SignalChain MustangProtocolV3::load_memory_bank(const std::shared_ptr<Connection> conn, uint8_t slot)
    {
        m_ppConn = &conn;

        std::ostringstream switchToPresetFilenameStr;
        switchToPresetFilenameStr << "switch_to_preset_" << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(slot) << std::ends;
        std::string switchToPresetFilename = switchToPresetFilenameStr.str();

        std::ostringstream switchToPresetRequestStr;
        switchToPresetRequestStr << "350708008a020208" << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(slot) << std::ends;
        std::string switchToPresetRequest = switchToPresetRequestStr.str();

        int response_type_received;
        std::vector<std::vector<uint8_t>> switch_preset_response1_bytes = sendCommandAndReceiveResponse(
            switchToPresetFilename, switchToPresetRequest, response_type_received
        );

        // We expect to receive two messages back.
        // The first will have beem returned by sendCommandAndReceiveResponse
        assert(response_type_received==38);

        // We do an additional receive for the second message
        const auto receivedData = receiveResponse((*m_ppConn), true);
        auto response_fields = plug::com::v3::extractResponsePayload_V3_USB(receivedData, response_type_received);
        assert(response_type_received==37);

        m_ppConn = NULL;

        return SignalChain{storedPresetNames[slot], storedAmpSettings[slot], storedEffects[slot]};
    }


    std::vector<std::vector<uint8_t>> MustangProtocolV3::sendCommandAndReceiveResponse(
        std::string command_description,
        std::string command_hex_bytes,
        int& response_message_type
    )
    {
#ifdef NDEBUG
        std::cout << "Sending " << command_description << ":" << command_hex_bytes << std::endl;
#endif
        auto command = serializeCommand(command_hex_bytes);

        auto recieved = (*m_ppConn)->send(command.getBytes());

        if(recieved==0)
        {
            char exception_message[100];
            snprintf(
                exception_message,sizeof(exception_message),
                "Empty response to %s request",
                command_description.c_str()
            );
#ifdef NDEBUG
            std::cout << exception_message << std::endl;
#endif
            // throw CommunicationException(std::string(exception_message));
        }

        const auto receivedData = receiveResponse((*m_ppConn), true);

        auto response_fields = plug::com::v3::extractResponsePayload_V3_USB(receivedData, response_message_type);

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

    Packet<EmptyPayload> MustangProtocolV3::serializePresetSwitchCommand(int presetIndex)
    {
        Packet<EmptyPayload> retval;
        Header header2{};
        std::string hexBytes2("350708008a020208");
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



