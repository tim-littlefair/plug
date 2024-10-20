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

        virtual std::vector<Packet<EmptyPayload>> serializeInitCommand() = 0;

    };

    class MustangProtocolV1V2: public MustangProtocolBase
    {

        public:

        MustangProtocolV1V2(DeviceModel model):
        MustangProtocolBase(model)
        {

        };

        std::vector<Packet<EmptyPayload>> serializeInitCommand()
        {
            std::vector<Packet<EmptyPayload>> retval;

            Header header0{};
            header0.setStage(Stage::init0);
            header0.setType(Type::init0);
            header0.setDSP(DSP::none);
            retval.push_back(Packet<EmptyPayload>{header0, EmptyPayload{}});

            Header header1{};
            header1.setStage(Stage::init1);
            header1.setType(Type::init1);
            header1.setDSP(DSP::none);
            retval.push_back(Packet<EmptyPayload>{header1, EmptyPayload{}});

            return retval;
        }
    };

    class MustangProtocolV3: public MustangProtocolBase {

        public:

        MustangProtocolV3(DeviceModel model):
        MustangProtocolBase(model)
        {

        };

        std::vector<Packet<EmptyPayload>> serializeInitCommand()
        {
            std::vector<Packet<EmptyPayload>> retval;

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
            retval.push_back(Packet<EmptyPayload>{header0, EmptyPayload{}});

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
            retval.push_back(Packet<EmptyPayload>{header1, EmptyPayload{}});

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
            retval.push_back(Packet<EmptyPayload>{header2, EmptyPayload{}});

            return retval;
        }
    };

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
