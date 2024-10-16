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

#include <algorithm>
#include <fstream>

#define INSTANTIATE_PROTOCOL_FACTORY_HERE
#include "com/MustangProtocols.h"
#undef INSTANTIATE_PROTOCOL_FACTORY_HERE

namespace plug::com
{
    static MustangProtocolBase *activeProtocol = NULL;

    SignalChain decode_data(const std::array<PacketRawType, 7>& data, DeviceModel model)
    {
        switch (model.category())
        {
            case DeviceModel::Category::MustangV1:
            case DeviceModel::Category::MustangV2:
            {
                const auto name = decodeNameFromData(fromRawData<NamePayload>(data[0]));
                const auto amp = decodeAmpFromData(fromRawData<AmpPayload>(data[1]), fromRawData<AmpPayload>(data[6]));
                const auto effects = decodeEffectsFromData({{fromRawData<EffectPayload>(data[2]), fromRawData<EffectPayload>(data[3]),
                                                             fromRawData<EffectPayload>(data[4]), fromRawData<EffectPayload>(data[5])}});

                return SignalChain{name, amp, effects};
            }

            case DeviceModel::Category::MustangV3_USB:
            {

                const auto name = decodeNameFromData(fromRawData<NamePayload>(data[0]));
                const amp_settings amp{};
                const std::vector<fx_pedal_settings> effects;

                return SignalChain{name, amp, effects};
            }

            case DeviceModel::Category::MustangV3_BT:
            default:
                throw new CommunicationException("Amplifier does not belong to a supported category");
        }
    }

    std::vector<std::uint8_t> receivePacket(Connection& conn)
    {
        std::vector<std::uint8_t> retval = conn.receive(packetRawTypeSize);
        return retval;
    }


    std::vector<std::uint8_t>  sendCommand(Connection& conn, const PacketRawType& packet)
    {
        conn.send(packet);
        return receivePacket(conn);
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
        : model(deviceModel), conn(connection)
    {
        activeProtocol = MustangProtocolBase::factory(deviceModel);
        if (activeProtocol == NULL)
        {
            throw CommunicationException{"Failed to select protocol version"};
        }
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
        const auto settingsPacket = serializeAmpSettings(value);
        sendCommand(*conn, settingsPacket.getBytes());
        sendApplyCommand(*conn);

        const auto settingsGainPacket = serializeAmpSettingsUsbGain(value);
        sendCommand(*conn, settingsGainPacket.getBytes());
        sendApplyCommand(*conn);
    }

    void Mustang::save_on_amp(std::string_view name, std::uint8_t slot)
    {
        const auto data = serializeName(slot, name).getBytes();
        sendCommand(*conn, data);
        loadBankData(*conn, slot);
    }

    SignalChain Mustang::load_memory_bank(std::uint8_t slot)
    {
        return decode_data(loadBankData(*conn, slot), model);
    }

    void Mustang::save_effects(std::uint8_t slot, std::string_view name, const std::vector<fx_pedal_settings>& effects)
    {
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
        std::vector<std::array<std::uint8_t, 64>> recieved_data;

        if (model.category() == DeviceModel::Category::MustangV3_USB)
        {
            // For V3_USB devices, the responses to the init command contain
            // the start of the first JSON bundle received, so we defer sending
            // this command until recieved_data exists to store the responses
            const auto packets = activeProtocol->serializeInitCommand();
            for (auto pPacket = packets.cbegin(); pPacket != packets.cend(); ++pPacket)
            {
                const auto recvData = sendCommand(*conn, pPacket->getBytes());
                PacketRawType p{};
                std::copy(recvData.cbegin(), recvData.cend(), p.begin());
                recieved_data.push_back(p);
            }
        }


        const auto loadCommand = serializeLoadCommand();
        auto recieved = conn->send(loadCommand.getBytes());

        while (recieved != 0)
        {
            const auto recvData = receivePacket(*conn);
            recieved = recvData.size();
            PacketRawType p{};
            std::copy(recvData.cbegin(), recvData.cend(), p.begin());
            recieved_data.push_back(p);
        }

        switch (model.category())
        {
            case DeviceModel::Category::MustangV1:
            case DeviceModel::Category::MustangV2:
            {
                const std::size_t numPresetPackets = model.numberOfPresets() > 0 ? (model.numberOfPresets() * 2) : (recieved_data.size() > 143 ? 200 : 48);
                std::vector<Packet<NamePayload>> presetListData;
                presetListData.reserve(numPresetPackets);
                std::transform(recieved_data.cbegin(), std::next(recieved_data.cbegin(), numPresetPackets), std::back_inserter(presetListData), [](const auto& p)
                            {
                    Packet<NamePayload> packet{};
                    packet.fromBytes(p);
                    return packet; });
                auto presetNames = decodePresetListFromData(presetListData);

                std::array<PacketRawType, 7> presetData{{}};
                std::copy(std::next(recieved_data.cbegin(), numPresetPackets), std::next(recieved_data.cbegin(), numPresetPackets + 7), presetData.begin());
            }
            break;

            case DeviceModel::Category::MustangV3_USB:
            {
                std::vector<uint8_t> jsonData = Mustang::extractResponsePayload_V3_USB(recieved_data, std::string("response1"));

                std::vector<Packet<NamePayload>> presetListData;
                presetListData.reserve(0);
                auto presetNames = decodePresetListFromData(presetListData);
                std::array<PacketRawType, 7> presetData{{}};

                return {decode_data(presetData, model), presetNames};
            }
            break;

            default:
                std::vector<Packet<NamePayload>> presetListData;
                presetListData.reserve(0);
                auto presetNames = decodePresetListFromData(presetListData);
                std::array<PacketRawType, 7> presetData{{}};

                return {decode_data(presetData, model), presetNames};
        }

        std::vector<Packet<NamePayload>> presetListData;
        presetListData.reserve(0);
        auto presetNames = decodePresetListFromData(presetListData);
        std::array<PacketRawType, 7> presetData{{}};

        return {decode_data(presetData, model), presetNames};
    }

    void Mustang::initializeAmp()
    {
        if (model.category() == DeviceModel::Category::MustangV3_USB)
        {
            // defer the init command until loadData is running because
            // loadData needs to see the responses
        }
        else
        {
            const auto packets = activeProtocol->serializeInitCommand();
            std::for_each(packets.cbegin(), packets.cend(), [this](const auto& p)
                          {sendCommand(*conn, p.getBytes()); });
        }
    }

    std::vector<uint8_t> Mustang::extractResponsePayload_V3_USB(std::vector<PacketRawType> packets, const std::string label) {
        std::vector<uint8_t> retval = std::vector<uint8_t>();
        for (size_t i=1; i<packets.size(); ++i)
        {
            PacketRawType p = packets.at(i);
            int json_offset;
            switch (p[1])
            {
                case 0x35:
                case 0x33:
                    json_offset = 12;
                    break;
                case 0x34:
                    json_offset = 3;
                    break;
                default:
                    json_offset=64;
                    continue;
            }
            std::copy(
                p.cbegin() + json_offset,
                p.cend(),
                std::back_inserter(retval)
            );
        }
#ifndef NDEBUG
        std::string json_dump_fname = label;
        json_dump_fname.append(".json");
        std::ofstream json_dump_stream(json_dump_fname);
        json_dump_stream.write(reinterpret_cast<const char*>(&(retval.at(0))), retval.size());
        json_dump_stream.flush();
        json_dump_stream.close();
#endif

        return retval;
    }

}
