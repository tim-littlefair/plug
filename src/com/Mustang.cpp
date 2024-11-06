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

#include "com/Mustang.h"
#include "com/PacketSerializer.h"
#include "com/CommunicationException.h"
#include "com/Packet.h"

#include "com/MustangProtocolBase.h"
#include "com/V3SupportException.h"

#include <algorithm>

namespace plug::com
{

    static void v3UnsupportedFunctionCheck(DeviceModel model, std::string functionName)
    {
        switch (model.category())
        {
            case DeviceModel::Category::MustangV1:
            case DeviceModel::Category::MustangV2:
                break;

            default:
                std::string msg = (
                    "function " +
                    functionName +
                    " only supported for Mustang V1 and V2 models"
                );
                throw V3SupportException(msg);
        }
    }

    SignalChain decode_data(const std::array<PacketRawType, 7>& data)
    {
        const auto name = decodeNameFromData(fromRawData<NamePayload>(data[0]));
        const auto amp = decodeAmpFromData(fromRawData<AmpPayload>(data[1]), fromRawData<AmpPayload>(data[6]));
        const auto effects = decodeEffectsFromData({{fromRawData<EffectPayload>(data[2]), fromRawData<EffectPayload>(data[3]),
                                                     fromRawData<EffectPayload>(data[4]), fromRawData<EffectPayload>(data[5])}});

        return SignalChain{name, amp, effects};
    }

    std::vector<std::uint8_t> receivePacket(Connection& conn)
    {
        return conn.receive(packetRawTypeSize);
    }


    void sendCommand(Connection& conn, const PacketRawType& packet)
    {
        conn.send(packet);
        receivePacket(conn);
    }

    void sendApplyCommand(Connection& conn)
    {
        sendCommand(conn, serializeApplyCommand().getBytes());
    }

    std::array<PacketRawType, 7> loadBankData(Connection& conn, std::uint8_t slot)
    {
        std::array<PacketRawType, 7> data{{}};

        const auto loadCommand = serializeLoadSlotCommand(slot);
        auto n = conn.send(loadCommand.getBytes());

        for (std::size_t i = 0; n != 0; ++i)
        {
            const auto recvData = receivePacket(conn);
            n = recvData.size();

            if (i < 7)
            {
                std::copy(recvData.cbegin(), recvData.cend(), data[i].begin());
            }
        }
        return data;
    }


    Mustang::Mustang(DeviceModel deviceModel, std::shared_ptr<Connection> connection)
        : model(deviceModel), conn(connection), pProtocol(NULL)
    {
        pProtocol = MustangProtocolBase::factory(deviceModel);
    }

    InitialData Mustang::start_amp()
    {
        if (conn->isOpen() == false)
        {
            throw CommunicationException{"Device not connected"};
        }

        initializeAmp();

        return loadData();
    }

    void Mustang::stop_amp()
    {
        conn->close();
    }

    void Mustang::set_effect(fx_pedal_settings value)
    {
        v3UnsupportedFunctionCheck(model, std::string("set_effect"));

        const auto clearEffectPacket = serializeClearEffectSettings(value);
        sendCommand(*conn, clearEffectPacket.getBytes());
        sendApplyCommand(*conn);

        if ((value.enabled == true) && (value.effect_num != effects::EMPTY))
        {
            const auto settingsPacket = serializeEffectSettings(value);
            sendCommand(*conn, settingsPacket.getBytes());
            sendApplyCommand(*conn);
        }
    }

    void Mustang::set_amplifier(amp_settings value)
    {
        v3UnsupportedFunctionCheck(model, std::string("set_amplifier"));

        const auto settingsPacket = serializeAmpSettings(value);
        sendCommand(*conn, settingsPacket.getBytes());
        sendApplyCommand(*conn);

        const auto settingsGainPacket = serializeAmpSettingsUsbGain(value);
        sendCommand(*conn, settingsGainPacket.getBytes());
        sendApplyCommand(*conn);
    }

    void Mustang::save_on_amp(std::string_view name, std::uint8_t slot)
    {
        v3UnsupportedFunctionCheck(model, std::string("save_on_amp"));

        const auto data = serializeName(slot, name).getBytes();
        sendCommand(*conn, data);
        loadBankData(*conn, slot);
    }

    SignalChain Mustang::load_memory_bank(std::uint8_t slot)
    {
        return decode_data(loadBankData(*conn, slot));
    }

    void Mustang::save_effects(std::uint8_t slot, std::string_view name, const std::vector<fx_pedal_settings>& effects)
    {
        v3UnsupportedFunctionCheck(model, std::string("save_effects"));

        const auto saveNamePacket = serializeSaveEffectName(slot, name, effects);
        sendCommand(*conn, saveNamePacket.getBytes());

        const auto packets = serializeSaveEffectPacket(slot, effects);
        std::for_each(packets.cbegin(), packets.cend(), [this](const auto& p)
                      { sendCommand(*conn, p.getBytes()); });

        sendCommand(*conn, serializeApplyCommand(effects[0]).getBytes());
    }

    DeviceModel Mustang::getDeviceModel() const
    {
        return model;
    }


    InitialData Mustang::loadData()
    {
        return pProtocol->loadData(conn);
    }

    void Mustang::initializeAmp()
    {
        if (pProtocol==NULL)
        {
            throw CommunicationException{"No protocol has been selected"};
        }
        const auto packets = pProtocol ->serializeInitCommand();
        std::for_each(packets.cbegin(), packets.cend(), [this](const auto& p)
                      { sendCommand(*conn, p.getBytes()); });
    }
}
