/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2024  offa
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

#include "effects_enum.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <map>

namespace plug::com::v3
{
    static const std::map<std::string, plug::amps> json_amp_names {
                {"DUBS_Deluxe57", plug::amps::FENDER_57_DELUXE},
                {"DUBS_Bassman59", plug::amps::FENDER_59_BASSMAN },
                {"DUBS_Champ57", plug::amps::FENDER_57_CHAMP},
                {"DUBS_Deluxe65", plug::amps::FENDER_65_DELUXE_REVERB },
                {"DUBS_Princeton65", plug::amps::FENDER_65_PRINCETON, },
                {"DUBS_Twin65", plug::amps::FENDER_65_TWIN_REVERB},
                {"DUBS_SuperSonic", plug::amps::FENDER_SUPER_SONIC},
                {"DUBS_Ac30Tb", plug::amps::BRITISH_60S},
                {"DUBS_Jcm800", plug::amps::BRITISH_80S},
                {"DUBS_Rect2", plug::amps::AMERICAN_90S},
                {"DUBS_MetalEvh3",plug::amps::METAL_2000},
                {"DUBS_Twin57", plug::amps::FENDER_57_TWIN},

                // Assignments below this point are presently guesses
                // based on information from
                // https://www.tdpri.com/threads/fender-mustang-gt-series-amps.723363/page-15
                // https://www.tdpri.com/threads/fender-fuse-software-discontinued.1015858/page-4
                // https://fender-mustang-amps-and-fuse.fandom.com/wiki/Amp_Models
                {"DUBS_LinearGain", plug::amps::STUDIO_PREAMP},
                {"DUBS_Plexi87", plug::amps::BRITISH_70S},
                {"DUBS_DR103", plug::amps::BRITISH_WATTS},
                {"DUBS_Silvertone", plug::amps::FENDER_60_THRIFT},
                {"DUBS_Or120", plug::amps::BRITISH_COLOUR},

                // Two new amps which were not supported in V1 or V2
                {"DUBS_Excelsior", plug::amps::V3_EXCELSIOR},
                {"DUBS_MetalRect2", plug::amps::V3_METAL_RECT_2},
    };
    static std::map<plug::amps, std::string> json_amp_names_reverse;

    static void populate_reverse_maps()
    {
        for(
            auto amp_map_iter = json_amp_names.begin();
            amp_map_iter != json_amp_names.end();
            ++amp_map_iter
        )
        {
            plug::amps ampId = amp_map_iter->second;
            json_amp_names_reverse.emplace(
                // std::make_pair<plug::amps, std::string>(
                    ampId, amp_map_iter->first
                //)
            );
        }
    }

    plug::amps jsonNameToAmpId(std::string jsonName)
    {
        auto pPair = json_amp_names.find(jsonName);
        if(pPair!=json_amp_names.cend())
        {
            return pPair->second;
        }
        else
        {
            return plug::amps::V3_NOT_RECOGNIZED;
        }
    }

    std::string ampIdToJsonName(plug::amps ampId)
    {
        auto pPair = json_amp_names_reverse.find(ampId);
        if(pPair!=json_amp_names_reverse.cend())
        {
            return pPair->second;
        }
        else
        {
            return "V3_NOT_RECOGNIZED";
        }
    }

#if 0
    constexpr amps lookupAmpById(std::uint8_t id)
    {
        switch (id)
        {
            case 0x67:
                return amps::FENDER_57_DELUXE;
            case 0x64:
                return amps::FENDER_59_BASSMAN;
            case 0x7c:
                return amps::FENDER_57_CHAMP;
            case 0x53:
                return amps::FENDER_65_DELUXE_REVERB;
            case 0x6a:
                return amps::FENDER_65_PRINCETON;
            case 0x75:
                return amps::FENDER_65_TWIN_REVERB;
            case 0x72:
                return amps::FENDER_SUPER_SONIC;
            case 0x61:
                return amps::BRITISH_60S;
            case 0x79:
                return amps::BRITISH_70S;
            case 0x5e:
                return amps::BRITISH_80S;
            case 0x5d:
                return amps::AMERICAN_90S;
            case 0x6d:
                return amps::METAL_2000;
            case 0xf1:
                return amps::STUDIO_PREAMP;
            case 0xf6:
                return amps::FENDER_57_TWIN;
            case 0xf9:
                return amps::FENDER_60_THRIFT;
            case 0xfc:
                return amps::BRITISH_COLOUR;
            case 0xff:
                return amps::BRITISH_WATTS;
            default:
                throw std::invalid_argument{"Invalid amp id: " + std::to_string(id)};
        }
    }


    constexpr effects lookupEffectById(std::uint8_t id)
    {
        switch (id)
        {
            case 0x00:
                return effects::EMPTY;
            case 0x3c:
                return effects::OVERDRIVE;
            case 0x49:
                return effects::WAH;
            case 0x4a:
                return effects::TOUCH_WAH;
            case 0x1a:
                return effects::FUZZ;
            case 0x1c:
                return effects::FUZZ_TOUCH_WAH;
            case 0x88:
                return effects::SIMPLE_COMP;
            case 0x07:
                return effects::COMPRESSOR;
            case 0x12:
                return effects::SINE_CHORUS;
            case 0x13:
                return effects::TRIANGLE_CHORUS;
            case 0x18:
                return effects::SINE_FLANGER;
            case 0x19:
                return effects::TRIANGLE_FLANGER;
            case 0x2d:
                return effects::VIBRATONE;
            case 0x40:
                return effects::VINTAGE_TREMOLO;
            case 0x41:
                return effects::SINE_TREMOLO;
            case 0x22:
                return effects::RING_MODULATOR;
            case 0x29:
                return effects::STEP_FILTER;
            case 0x4f:
                return effects::PHASER;
            case 0x1f:
                return effects::PITCH_SHIFTER;
            case 0x16:
                return effects::MONO_DELAY;
            case 0x43:
                return effects::MONO_ECHO_FILTER;
            case 0x48:
                return effects::STEREO_ECHO_FILTER;
            case 0x44:
                return effects::MULTITAP_DELAY;
            case 0x45:
                return effects::PING_PONG_DELAY;
            case 0x15:
                return effects::DUCKING_DELAY;
            case 0x46:
                return effects::REVERSE_DELAY;
            case 0x2b:
                return effects::TAPE_DELAY;
            case 0x2a:
                return effects::STEREO_TAPE_DELAY;
            case 0x24:
                return effects::SMALL_HALL_REVERB;
            case 0x3a:
                return effects::LARGE_HALL_REVERB;
            case 0x26:
                return effects::SMALL_ROOM_REVERB;
            case 0x3b:
                return effects::LARGE_ROOM_REVERB;
            case 0x4e:
                return effects::SMALL_PLATE_REVERB;
            case 0x4b:
                return effects::LARGE_PLATE_REVERB;
            case 0x4c:
                return effects::AMBIENT_REVERB;
            case 0x4d:
                return effects::ARENA_REVERB;
            case 0x21:
                return effects::FENDER_63_SPRING_REVERB;
            case 0x0b:
                return effects::FENDER_65_SPRING_REVERB;
            default:
                throw std::invalid_argument{"Invalid effect id: " + std::to_string(id)};
        }
    }


    constexpr cabinets lookupCabinetById(std::uint8_t id)
    {
        switch (id)
        {
            case 0x00:
                return cabinets::OFF;
            case 0x01:
                return cabinets::cab57DLX;
            case 0x02:
                return cabinets::cabBSSMN;
            case 0x03:
                return cabinets::cab65DLX;
            case 0x04:
                return cabinets::cab65PRN;
            case 0x05:
                return cabinets::cabCHAMP;
            case 0x06:
                return cabinets::cab4x12M;
            case 0x07:
                return cabinets::cab2x12C;
            case 0x08:
                return cabinets::cab4x12G;
            case 0x09:
                return cabinets::cab65TWN;
            case 0x0a:
                return cabinets::cab4x12V;
            case 0x0b:
                return cabinets::cabSS212;
            case 0x0c:
                return cabinets::cabSS112;
            default:
                throw std::invalid_argument{"Invalid cabinet id: " + std::to_string(id)};
        }
    }
#endif

}
