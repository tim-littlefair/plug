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

static  std::vector<std::uint8_t> buildProtobufPresetPayload(std::string presetFilePath)
{
    // V3 returns a large protobuf-wrapped JSON document containing the current preset
    std::ifstream emptyPresetJson(presetFilePath.c_str());
    std::vector<std::uint8_t> pbPayload;
    std::vector<std::uint8_t> pbBeforeJson{
        0x08, 0x02, // magic=protobuf, protobuf_version=2
        0xfa, 0x01, // message tag as varint: vi_value=(0xfa&0x7f)+(0x01<<7)=0x7a+0x80=0xfa pbtype=(0xfa&0x07)=0x2 msgid=(0xfa&0xf8)>>3=32
        0x98, 0x0f, // because pbtype=2 message length as varint: vi_value=(0x98&0x7f)+(0x0f<<7)=0x18+0x780=0x798 = 1944 decimal
        0x0a, // element tag for first field, pbtype=0x02, fieldid=0x01
        0x93, 0x0f, // because pbtype=2, field length as varint: vi_value=(0x93&0x7f)+(0x0f<<7)=0x13+0x780=0x793 = 1939 decimal
        // packet continues 0x7b, 0x22, 0x63, 0x6f ... but we will get these from empty_preset.json
    };
    std::copy(
        pbBeforeJson.cbegin(),
        pbBeforeJson.cend(),
        std::back_inserter(pbPayload)
    );
    while(emptyPresetJson)
    {
        uint8_t nextNonWsByte;
        emptyPresetJson >> std::skipws >> nextNonWsByte;
        pbPayload.push_back(nextNonWsByte);
    }
    //assert(emptyPresetJson.eof());
    emptyPresetJson.close();
    // If the payload didn't end with a '}', something went wrong
    assert( pbPayload.at((pbPayload.size()-1)) == static_cast<uint8_t>(0x7d) );
    std::vector<std::uint8_t> pbAfterJson{
        0x10, // element tag for second field, element type 0=single byte, element id 2=field 2
        0x01, // element value for second field
        0x18, // element tag for third field, element type 0=single byte , element id 3=field 3
        0x00,  // element value, preset is not dirty
    };
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

static std::vector<std::vector<uint8_t>> presetJsonFileToHIDPackets(std::string presetFilePath)
{
    std::vector<std::uint8_t> payloadBytes = buildProtobufPresetPayload(presetFilePath);
    return payloadToHIDPackets(payloadBytes);

}


