/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2026  offa
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

#include "effects_enum.h"
#include <string>
#include <map>
#include <algorithm>

namespace plug
{
    namespace
    {
        const std::map<amps, std::string> ampNames{
            {amps::FENDER_57_DELUXE, "Fender '57 Deluxe"},
            {amps::FENDER_59_BASSMAN, "Fender '59 Bassman"},
            {amps::FENDER_57_CHAMP, "Fender '57 Champ"},
            {amps::FENDER_65_DELUXE_REVERB, "Fender '65 Deluxe Reverb"},
            {amps::FENDER_65_PRINCETON, "Fender '65 Princeton"},
            {amps::FENDER_65_TWIN_REVERB, "Fender '65 Twin Reverb"},
            {amps::FENDER_SUPER_SONIC, "Fender Super-Sonic"},
            {amps::BRITISH_60S, "British 60's"},
            {amps::BRITISH_70S, "British 70's"},
            {amps::BRITISH_80S, "British 80's"},
            {amps::AMERICAN_90S, "American 90's"},
            {amps::METAL_2000, "Metal 2000"},
            {amps::STUDIO_PREAMP, "Studio Preamp"},
            {amps::FENDER_57_TWIN, "Fender '57 Twin"},
            {amps::FENDER_60_THRIFT, "Fender '60s Thrift"},
            {amps::BRITISH_COLOUR, "British Colour"},
            {amps::BRITISH_WATTS, "British Watts"}};
    }

    std::string getAmpName(int ampNumber) 
    {
        amps ampNumberAsEnum = static_cast<amps>(ampNumber);
        std::map<amps, std::string>::const_iterator iter = ampNames.find(ampNumberAsEnum);
        if(iter==ampNames.end()) 
        {
            return std::string("UNKNOWN AMPLIFIER");
        } 
        else
        {
            return iter->second;
        }
    }

    void populate_amp_name_list(DeviceModel *pDeviceModel, std::list<std::string> &amp_name_list) 
    {
        std::for_each(ampNames.cbegin(), ampNames.cend(), [pDeviceModel, &amp_name_list](const auto& item)
        {
            if (!isV2Amp(item.first) || (isV2Amp(item.first) && pDeviceModel->category() == DeviceModel::Category::MustangV2)){
                std::string amp_name(item.second);
                amp_name_list.push_back(amp_name);
            }
        });
    }

    std::string amp_name_for_plug_id(amps plug_id) 
    {
        return ampNames.at(plug_id);
    }
}
