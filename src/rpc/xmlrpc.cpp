// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmlrpc.h"

#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

namespace tremotesf::xmlrpc {
    namespace {
        void appendEscaped(QByteArray& out, const QString& value) {
            const QByteArray utf8 = value.toUtf8();
            for (const char c : utf8) {
                switch (c) {
                case '&':
                    out.append("&amp;");
                    break;
                case '<':
                    out.append("&lt;");
                    break;
                case '>':
                    out.append("&gt;");
                    break;
                case '\r':
                    out.append("&#13;");
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n') {
                        out.append("&#");
                        out.append(QByteArray::number(static_cast<unsigned char>(c)));
                        out.append(';');
                    } else {
                        out.append(c);
                    }
                }
            }
        }

        void appendInteger(QByteArray& out, const QVariant& value) {
            out.append("<i8>");
            out.append(value.toString().toUtf8());
            out.append("</i8>");
        }

        void appendValue(QByteArray& out, const QVariant& value) {
            out.append("<value>");

            switch (value.typeId()) {
            case QMetaType::Bool:
                out.append(value.toBool() ? "<boolean>1</boolean>" : "<boolean>0</boolean>");
                break;
            case QMetaType::Int:
            case QMetaType::UInt:
            case QMetaType::Long:
            case QMetaType::ULong:
            case QMetaType::LongLong:
            case QMetaType::ULongLong:
            case QMetaType::Short:
            case QMetaType::UShort:
            case QMetaType::Char:
            case QMetaType::SChar:
            case QMetaType::UChar:
                appendInteger(out, value);
                break;
            case QMetaType::Double:
            case QMetaType::Float:
                out.append("<double>");
                out.append(QByteArray::number(value.toDouble(), 'g', 17));
                out.append("</double>");
                break;
            case QMetaType::QByteArray:
                out.append("<base64>");
                out.append(value.toByteArray().toBase64());
                out.append("</base64>");
                break;
            case QMetaType::QStringList:
                out.append("<array><data>");
                for (const auto& item : value.toStringList()) {
                    appendValue(out, item);
                }
                out.append("</data></array>");
                break;
            case QMetaType::QVariantList:
                out.append("<array><data>");
                for (const auto& item : value.toList()) {
                    appendValue(out, item);
                }
                out.append("</data></array>");
                break;
            case QMetaType::QVariantMap:
                out.append("<struct>");
                for (auto [key, item] : value.toMap().asKeyValueRange()) {
                    out.append("<member><name>");
                    appendEscaped(out, key);
                    out.append("</name>");
                    appendValue(out, item);
                    out.append("</member>");
                }
                out.append("</struct>");
                break;
            case QMetaType::QVariantHash:
                out.append("<struct>");
                for (auto [key, item] : value.toHash().asKeyValueRange()) {
                    out.append("<member><name>");
                    appendEscaped(out, key);
                    out.append("</name>");
                    appendValue(out, item);
                    out.append("</member>");
                }
                out.append("</struct>");
                break;
            default:
                out.append("<string>");
                appendEscaped(out, value.toString());
                out.append("</string>");
                break;
            }

            out.append("</value>");
        }

        QVariant parseValue(QXmlStreamReader& xml);

        QVariant parseArray(QXmlStreamReader& xml) {
            QVariantList list{};
            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == "data"_L1) {
                    break;
                }
                if (xml.isEndElement() && xml.name() == "array"_L1) {
                    return list;
                }
            }
            while (!xml.atEnd()) {
                const auto token = xml.readNext();
                if (token == QXmlStreamReader::StartElement) {
                    if (xml.name() == "value"_L1) {
                        list.append(parseValue(xml));
                    } else {
                        xml.skipCurrentElement();
                    }
                } else if (token == QXmlStreamReader::EndElement && xml.name() == "data"_L1) {
                    break;
                }
            }
            while (!xml.atEnd()) {
                if (xml.readNext() == QXmlStreamReader::EndElement && xml.name() == "array"_L1) {
                    break;
                }
            }
            return list;
        }

        QVariant parseStruct(QXmlStreamReader& xml) {
            QVariantMap map{};
            while (!xml.atEnd()) {
                const auto token = xml.readNext();
                if (token == QXmlStreamReader::EndElement && xml.name() == "struct"_L1) {
                    break;
                }
                if (token != QXmlStreamReader::StartElement || xml.name() != "member"_L1) {
                    continue;
                }
                QString name{};
                QVariant value{};
                while (!xml.atEnd()) {
                    const auto memberToken = xml.readNext();
                    if (memberToken == QXmlStreamReader::EndElement && xml.name() == "member"_L1) {
                        break;
                    }
                    if (memberToken != QXmlStreamReader::StartElement) {
                        continue;
                    }
                    if (xml.name() == "name"_L1) {
                        name = xml.readElementText();
                    } else if (xml.name() == "value"_L1) {
                        value = parseValue(xml);
                    } else {
                        xml.skipCurrentElement();
                    }
                }
                map.insert(name, value);
            }
            return map;
        }

        QVariant parseTypedValue(QXmlStreamReader& xml) {
            const auto name = xml.name();
            if (name == "string"_L1 || name == "dateTime.iso8601"_L1) {
                return xml.readElementText();
            }
            if (name == "i4"_L1 || name == "int"_L1 || name == "i8"_L1) {
                bool ok{};
                const auto value = xml.readElementText().trimmed().toLongLong(&ok);
                return ok ? QVariant(value) : QVariant();
            }
            if (name == "boolean"_L1) {
                return xml.readElementText().trimmed() == "1"_L1;
            }
            if (name == "double"_L1) {
                bool ok{};
                const auto value = xml.readElementText().trimmed().toDouble(&ok);
                return ok ? QVariant(value) : QVariant();
            }
            if (name == "base64"_L1) {
                return QByteArray::fromBase64(xml.readElementText().toLatin1());
            }
            if (name == "nil"_L1) {
                xml.skipCurrentElement();
                return {};
            }
            if (name == "array"_L1) {
                return parseArray(xml);
            }
            if (name == "struct"_L1) {
                return parseStruct(xml);
            }
            xml.skipCurrentElement();
            return {};
        }

        QVariant parseValue(QXmlStreamReader& xml) {
            QVariant value{};
            while (!xml.atEnd()) {
                const auto token = xml.readNext();
                if (token == QXmlStreamReader::StartElement) {
                    value = parseTypedValue(xml);
                } else if (token == QXmlStreamReader::EndElement && xml.name() == "value"_L1) {
                    break;
                } else if (token == QXmlStreamReader::Characters && !xml.isWhitespace()) {
                    value = xml.text().toString();
                }
            }
            return value;
        }
    }

    QByteArray makeMethodCall(const QString& methodName, const QVariantList& parameters) {
        QByteArray data{"<?xml version=\"1.0\" encoding=\"UTF-8\"?><methodCall><methodName>"};
        appendEscaped(data, methodName);
        data.append("</methodName><params>");
        for (const auto& parameter : parameters) {
            data.append("<param>");
            appendValue(data, parameter);
            data.append("</param>");
        }
        data.append("</params></methodCall>");
        return data;
    }

    Response parseMethodResponse(const QByteArray& data) {
        QXmlStreamReader xml(data);
        Response response{};

        while (!xml.atEnd() && !response.fault && !response.value.isValid()) {
            const auto token = xml.readNext();
            if (token != QXmlStreamReader::StartElement) {
                continue;
            }
            if (xml.name() == "fault"_L1) {
                while (!xml.atEnd()) {
                    if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == "value"_L1) {
                        const auto fault = parseValue(xml).toMap();
                        response.fault = true;
                        response.faultCode = fault.value("faultCode"_L1).toInt();
                        response.faultString = fault.value("faultString"_L1).toString();
                        break;
                    }
                }
            } else if (xml.name() == "params"_L1) {
                while (!xml.atEnd()) {
                    if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == "value"_L1) {
                        response.value = parseValue(xml);
                        break;
                    }
                }
            }
        }

        if (xml.hasError()) {
            response.parseError = xml.errorString();
        } else if (!response.fault && !response.value.isValid()) {
            response.parseError = "Response does not contain a value"_L1;
        }
        return response;
    }
}
