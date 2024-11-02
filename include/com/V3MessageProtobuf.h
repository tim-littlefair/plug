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

#include <cassert>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

namespace plug::com::v3
{
    static std::vector<uint8_t> array64_to_vector(std::array<uint8_t,64> a)
    {
        std::vector<uint8_t> v;
        std::copy(a.cbegin(),a.cend(),std::back_inserter(v));
        return v;
    }

    static unsigned int protobuf_read_varint(std::vector<uint8_t> p, size_t& protobuf_read_offset)
    {
        unsigned int retval=0;
        unsigned int multiplier_shift=0;
        do
        {
            uint8_t next_byte = p[protobuf_read_offset];
            ++protobuf_read_offset;
            assert(protobuf_read_offset<p.size());
            if (next_byte<0x80)
            {
                retval += next_byte<<multiplier_shift;
                break;
            }
            else
            {
                retval += (next_byte&0x7F);
                multiplier_shift += 7;
            }
        } while(true);
        return retval;
    }

    // Note that we can read a varint starting from anywhere in the vector, but we don't know
    // how many bytes the varint contains until we calculate it so the only place we can
    // insert it is at the end.
    // Therefore we have _read_ and _append_ methods, not _read_ and _write_
    static std::vector<uint8_t>& protobuf_append_varint(unsigned int value, std::vector<uint8_t>& target)
    {
        unsigned int remaining_value = value;
        do
        {
            if (remaining_value < 0x80)
            {
                target.push_back(static_cast<uint32_t>(remaining_value));
                break;
            }
            else
            {
                uint8_t next_byte = 0x80 + (remaining_value & 0x7F);
                remaining_value >>= 7;
                target.push_back(next_byte);
            }
        } while(true);

        return target;
    }

    /*
    * This function returns a vector of vectors of bytes by deserializing the protobuf message id and then unpacking according to what will make further processing easiest
    * element 0 is the whole protobuf std::vector<std::vector<uint8_t>> extractResponsePayload_V3_USB(std::vector<plug::com::PacketRawType> packets)
    * For messages containing a variable length payload (i.e. preset JSON), that payload is element 1, element 2 is the rest of the message (fixed size, easy to unpack)
    * For messages which do not contain a variable length payload, only element 0 is returned (fixed size, easy to unpack)
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
                fender_message_type = (fender_message_tag) >> 3;
            }
            assert(fender_message_type!=-1);

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

        switch (fender_message_type)
        {
            // Refer to:
            // https://github.com/brentmaxwell/LtAmp/blob/main/Schema/protobuf/FenderMessageLT.proto
            // for the constants for different message types

            // Messages in this group start with a JSON document, followed by one or more
            // fixed format parameters
            // The JSON document will be returned in retval[1], retval[2] will contain
            // all other parameters
            case 31: // presetJSONMessage
            case 32: // currentPresetStatus
                {
                    unsigned int preset_json_length = protobuf_read_varint(retval[0],protobuf_read_offset);
                    std::vector<uint8_t> preset_json_bytes;
                    std::copy(
                        retval[0].cbegin() + protobuf_read_offset,
                        retval[0].cbegin() + protobuf_read_offset + preset_json_length,
                        std::back_inserter(preset_json_bytes)
                    );
                    retval.push_back(preset_json_bytes);

                    protobuf_read_offset += preset_json_length;
                    std::vector<uint8_t> rest_of_message_bytes;
                    std::copy(
                        retval[0].cbegin() + protobuf_read_offset,
                        retval[0].cend(),
                        std::back_inserter(rest_of_message_bytes)
                    );
                    retval.push_back(rest_of_message_bytes);
                }
                break;

            default:
                // For any other message type, for now, we don't need to unpack the protobuf
                // so we return from here
                break;
        }
        return retval;
    }

#pragma GCC diagnostic pop

}
