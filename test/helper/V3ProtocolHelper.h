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

#include "com/V3MessageProtobuf.h"
#include <cctype>
#include <cassert>
#include <vector>

static std::vector<std::uint8_t> readPresetJson(std::string presetFilePath)
{
    std::ifstream presetJson(presetFilePath.c_str());
    std::vector<std::uint8_t> jsonPayload;
    assert(presetJson.good());

    while(true)
    {
        uint8_t nextByte;
        presetJson >> std::noskipws >> nextByte;
        if(presetJson.good())
        {
            jsonPayload.push_back(nextByte);
        }
        else
        {
            assert(presetJson.eof());
            break;
        }
    }
    presetJson.close();

    return jsonPayload;

}

static  std::vector<std::uint8_t> buildProtobufPresetPayload(std::string presetFilePath, uint8_t presetIndex)
{
    // The protobuf type of the outer message structure is 'LEN'
    // which represents a sequence of bytes, preceded by the length
    // of the sequence encoded as a single varint
    const unsigned int PB_MESSAGE_TYPE_LEN = 2;

    std::vector<std::uint8_t> pbBeforeJson{
        0x08, 0x02, // magic=protobuf, protobuf_version=2
    };
    std::vector<std::uint8_t> pbAfterJson;

    if (presetIndex==0)
    {
        // valid stored preset indices are in the range 1-n where n reflects DeviceModel::numberOfPresets()
        // We use the value 0 as an indicator for requesting the current active preset, which is a different
        // function
        const unsigned int MSG_ID_ACTIVE_PRESET_REQUEST = 32;
        unsigned int pb_message_tag = (MSG_ID_ACTIVE_PRESET_REQUEST<<3) + PB_MESSAGE_TYPE_LEN;
        assert(pb_message_tag==258);
        plug::com::v3::protobuf_append_varint(pb_message_tag, pbBeforeJson);
        assert(pbBeforeJson[02]==0x82);
        assert(pbBeforeJson[03]==0x02);

        pbAfterJson = std::vector<uint8_t>{
            0x10, // element tag for second field, element type 0=single byte, element id 2=field 2
            0x01, // element value for second field, (active preset is factory state?)
            0x18, // element tag for third field, element type 0=single byte , element id 3=field 3
            0x00,  // element value, active preset is not dirty
        };
    }
    else
    {
        const unsigned int MSG_ID_STORED_PRESET_REQUEST = 31;
        unsigned int pb_message_tag = (MSG_ID_STORED_PRESET_REQUEST<<3) + PB_MESSAGE_TYPE_LEN;
        assert(pb_message_tag==250);
        plug::com::v3::protobuf_append_varint(pb_message_tag, pbBeforeJson);
        assert(pbBeforeJson[2]==0xfa);
        assert(pbBeforeJson[3]==0x01);

        pbAfterJson = std::vector<uint8_t>{
            0x10, // element tag for second field, element type 0=single byte, element id 2=field 2
            presetIndex, // element value for second field
        };
    }

    // V3 returns a large protobuf-wrapped JSON document containing the current preset
    // We need to read the JSON stream so that and determine its length before we
    // can add the last required bytes to pbBeforeJson
    std::vector<uint8_t> jsonPayload = readPresetJson(presetFilePath);

    unsigned int jsonFieldLength = jsonPayload.size();

    unsigned int jsonLengthLength;
    if (jsonFieldLength<0x80)
    {
        // length of JSON field will be encoded as a
        // single-byte varint (unlikely, but we handle it)
        jsonLengthLength = 1;
    }
    else
    {
        // No support for messages long enough to require
        // a 3-byte varint length
        assert(jsonFieldLength<0x4000);
        jsonLengthLength = 2;
    }

    unsigned int pbMessageLength = (
        1 + // field tag for the JSON field
        jsonLengthLength + // length of the varint encoding the JSON field length
        jsonFieldLength + // length of the JSON
        pbAfterJson.size()
    );

    // finally we can close out the values in pbBeforeJson
    plug::com::v3::protobuf_append_varint(pbMessageLength, pbBeforeJson);
    pbBeforeJson.push_back(0x0a); // field tag for the JSON
    plug::com::v3::protobuf_append_varint(jsonFieldLength,pbBeforeJson);

    // Finally we put the three parts together
    std::vector<std::uint8_t> pbPayload;
    std::copy(
        pbBeforeJson.cbegin(),
        pbBeforeJson.cend(),
        std::back_inserter(pbPayload)
    );
    std::copy(
        jsonPayload.cbegin(),
        jsonPayload.cend(),
        std::back_inserter(pbPayload)
    );
    std::copy(
        pbAfterJson.cbegin(),
        pbAfterJson.cend(),
        std::back_inserter(pbPayload)
    );
    return pbPayload;
}

static std::vector<std::vector<uint8_t>> payloadToHIDPackets(std::vector<uint8_t> pbPayload)
{
    // The protobuf payload needs to be divided up into 64 byte packets
    // with a 3 byte header and up to 61 bytes of payload in each packet
    std::vector<std::vector<std::uint8_t>> packets;
    size_t payload_offset=0;
    while (payload_offset<pbPayload.size())
    {
        const uint8_t DEFAULT_BYTES_TO_INCLUDE =  0x3d; // 61 bytes for all packets except the last
        std::vector<std::uint8_t>nextPacket;
        nextPacket.push_back(0x00); // HID packet type = 'unsolicited'
        uint8_t bytes_to_include =  DEFAULT_BYTES_TO_INCLUDE;
        if( (pbPayload.size()-payload_offset) <= static_cast<size_t>(bytes_to_include) )
        {
            nextPacket.push_back(0x35); // last packet (may or may not also be the first)
            bytes_to_include = pbPayload.size()-payload_offset;
        }
        else if (payload_offset==0)
        {
            nextPacket.push_back(0x33); // first packet of 2 or more
        }
        else
        {
            nextPacket.push_back(0x34); // middle packet, neither first nor last
        }
        nextPacket.push_back(bytes_to_include);

        std::copy(
            pbPayload.cbegin()+payload_offset,
            pbPayload.cbegin()+ payload_offset+bytes_to_include,
            std::back_inserter(nextPacket)
        );
        if(bytes_to_include != DEFAULT_BYTES_TO_INCLUDE)
        {
            nextPacket.resize(3+bytes_to_include, 0);
        }
        packets.push_back(nextPacket);
        payload_offset += bytes_to_include;
    }

    return packets;
}

static std::vector<std::vector<uint8_t>> presetJsonFileToHIDPackets(std::string presetFilePath, uint8_t storedPresetIndex)
{
    std::vector<std::uint8_t> payloadBytes = buildProtobufPresetPayload(presetFilePath, storedPresetIndex);
    return payloadToHIDPackets(payloadBytes);

}


