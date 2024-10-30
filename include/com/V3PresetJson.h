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

namespace plug::com::v3
{

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
            auto whichNode = node.value(QStringLiteral("nodeId")).toString();
            auto nodeFenderId = node.value(QStringLiteral("FenderId")).toString();
            if (whichNode==QStringLiteral("amp"))
            {
                auto ampId = plug::com::v3::jsonNameToAmpId(std::string(qPrintable(nodeFenderId)));
    #ifndef NDEBUG
                std::cout << "Getting settings for amp with FenderId " << qPrintable(nodeFenderId) << " type " << (0 + value(ampId)) << std::endl;
    #endif
                presetAmpSettings.amp_num = ampId;
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
            else
            {
    #ifndef NDEBUG
                std::cout << "Ignoring node of type " << qPrintable(whichNode) << " with FenderId " << qPrintable(nodeFenderId) << std::endl;
    #endif
            }
        }
        assert(presetEffects.size()>=1);
    }
}

