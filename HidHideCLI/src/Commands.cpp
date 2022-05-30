// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// Commands.cpp
#include "stdafx.h"
#include "Commands.h"
#include "HID.h"
#include "Utils.h"
#include "Volume.h"
#include "Logging.h"
#include <sstream>
#include <iomanip>

namespace
{
    // https://stackoverflow.com/a/33799784
    std::wstring escape_json(const std::wstring &s) {
        std::wostringstream o;
        for (const wchar_t c : s)
        {
            switch (c) {
            case L'"': o << L"\\\""; break;
            case L'\\': o << L"\\\\"; break;
            case L'\b': o << L"\\b"; break;
            case L'\f': o << L"\\f"; break;
            case L'\n': o << L"\\n"; break;
            case L'\r': o << L"\\r"; break;
            case L'\t': o << L"\\t"; break;
            default:
                if (L'\x00' <= c && c <= L'\x1f') {
                    o << "\\u"
                      << std::hex << std::setw(4) << std::setfill(L'0') << static_cast<int>(c);
                } else {
                    o << c;
                }
            }
        }
        return o.str();
    }
    
    // Serialize the HidDeviceInformation in a JSON format
    std::wostream& operator<<(_Inout_ std::wostream& os, _In_ HidHide::HidDeviceInformation const& hidDeviceInformation)
    {
        os  << L"{ " \
            << L"\"present\" : " << std::boolalpha << hidDeviceInformation.present << L" ," << std::endl \
            << L"\"gamingDevice\" : " << std::boolalpha << hidDeviceInformation.gamingDevice << L" ," << std::endl \
            << L"\"symbolicLink\" : \"" << escape_json(hidDeviceInformation.symbolicLink.wstring()) << L"\" ," << std::endl \
            << L"\"vendor\" : \"" << escape_json(hidDeviceInformation.vendor) << L"\" ," << std::endl \
            << L"\"product\" : \"" << escape_json(hidDeviceInformation.product) << L"\" ," << std::endl \
            << L"\"serialNumber\" : \"" << escape_json(hidDeviceInformation.serialNumber) << L"\" ," << std::endl \
            << L"\"usage\" : \"" << escape_json(hidDeviceInformation.usage) << L"\" ," << std::endl \
            << L"\"description\" : \"" << escape_json(hidDeviceInformation.description) << L"\" ," << std::endl \
            << L"\"deviceInstancePath\" : \"" << escape_json(hidDeviceInformation.deviceInstancePath) << L"\" ," << std::endl \
            << L"\"baseContainerDeviceInstancePath\" : \"" << escape_json(hidDeviceInformation.baseContainerDeviceInstancePath) << L"\" ," << std::endl \
            << L"\"baseContainerClassGuid\" : \"" << escape_json(HidHide::GuidToString(hidDeviceInformation.baseContainerClassGuid)) << L"\" ," << std::endl \
            << L"\"baseContainerDeviceCount\" : " << hidDeviceInformation.baseContainerDeviceCount << L" }";
        return (os);
    }

    // Serialize a list of HidDeviceInformation in a JSON format
    std::wostream& operator<<(_Inout_ std::wostream& os, _In_ std::vector<HidHide::HidDeviceInformation> const& hidDevices)
    {
        os << L" [" << std::endl;
        auto first{ true };
        for (auto const& hidDevice : hidDevices)
        {
            if (first) first = false; else os << L"," << std::endl;
            os << hidDevice << L" ";
        }
        os << L"] ";
        return (os);
    }

    // Serialize a multimap of the HidDeviceInformation in a JSON format
    std::wostream& operator<<(_Inout_ std::wostream& os, _In_ HidHide::FriendlyNamesAndHidDeviceInformation const& hidDevices)
    {
        os << L" [ ";
        auto first{ true };
        for (auto const& hidContainer : hidDevices)
        {
            if (first) first = false; else os << L"," << std::endl;
            os << L"{ \"friendlyName\" : \"" << escape_json(hidContainer.first) << L"\" , \"devices\" :" << hidContainer.second << L"} ";
        }
        os << L"] " << std::endl;
        return (os);
    }
}

namespace HidHide
{
    _Use_decl_annotations_
    CommandInterpreter::CommandInterpreter(bool writeThrough)
        : m_ScriptMode{ StandardInputRedirected() }
        , m_InteractiveMode{ (!m_ScriptMode) && (HidHide::CommandLineArguments().empty()) }
        , m_RegisteredCommands
          {
            { L"app-list",     { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_APP_LIST),     std::bind(&CommandInterpreter::AppList,     this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"app-reg",      { StringTable(IDS_CLI_SYNTAX_APP_PATH),      StringTable(IDS_CLI_APP_REG),      std::bind(&CommandInterpreter::AppReg,      this, std::placeholders::_1), std::bind(&CommandInterpreter::ValOneFullyQualifiedExecutablePath, this, std::placeholders::_1) } },
            { L"app-unreg",    { StringTable(IDS_CLI_SYNTAX_APP_PATH),      StringTable(IDS_CLI_APP_UNREG),    std::bind(&CommandInterpreter::AppUnreg,    this, std::placeholders::_1), std::bind(&CommandInterpreter::ValOneFullyQualifiedExecutablePath, this, std::placeholders::_1) } },
            { L"app-clean",    { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_APP_CLEAN),    std::bind(&CommandInterpreter::AppClean,    this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"inv-off",      { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_INV_OFF),      std::bind(&CommandInterpreter::InvOff,      this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"inv-on",       { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_INV_ON),       std::bind(&CommandInterpreter::InvOn,       this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"inv-state",    { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_INV_STATE),    std::bind(&CommandInterpreter::InvState,    this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"cancel",       { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_CANCEL),       std::bind(&CommandInterpreter::Cancel,      this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"cloak-off",    { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_CLOAK_OFF),    std::bind(&CommandInterpreter::CloakOff,    this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"cloak-on",     { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_CLOAK_ON),     std::bind(&CommandInterpreter::CloakOn,     this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"cloak-state",  { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_CLOAK_STATE),  std::bind(&CommandInterpreter::CloakState,  this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"cloak-toggle", { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_CLOAK_TOGGLE), std::bind(&CommandInterpreter::CloakToggle, this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"dev-all",      { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_DEV_ALL),      std::bind(&CommandInterpreter::DevAll,      this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"dev-gaming",   { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_DEV_GAMING),   std::bind(&CommandInterpreter::DevGaming,   this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"dev-hide",     { StringTable(IDS_CLI_SYNTAX_DEV_INST_PATH), StringTable(IDS_CLI_DEV_HIDE),     std::bind(&CommandInterpreter::DevHide,     this, std::placeholders::_1), std::bind(&CommandInterpreter::ValOneDeviceInstancePath, this, std::placeholders::_1) } },
            { L"dev-list",     { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_DEV_LIST),     std::bind(&CommandInterpreter::DevList,     this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"dev-unhide",   { StringTable(IDS_CLI_SYNTAX_DEV_INST_PATH), StringTable(IDS_CLI_DEV_UNHIDE),   std::bind(&CommandInterpreter::DevUnhinde,  this, std::placeholders::_1), std::bind(&CommandInterpreter::ValOneDeviceInstancePath, this, std::placeholders::_1) } },
            { L"help",         { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_HELP),         std::bind(&CommandInterpreter::Help,        this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } },
            { L"version",      { StringTable(IDS_CLI_SYNTAX_NO_ARGUMENTS),  StringTable(IDS_CLI_VERSION),      std::bind(&CommandInterpreter::Version,     this, std::placeholders::_1), std::bind(&CommandInterpreter::ValNoArguments, this, std::placeholders::_1) } }
          }
        , m_FilterDriverProxy(writeThrough)
        , m_Cancel{}
    {
        TRACE_ALWAYS(L"");
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ValNoArguments(Args const& args) const
    {
        TRACE_ALWAYS(L"");
        return ((1 == args.size()) ? std::wstring{} : HidHide::StringTable(IDS_WRONG_NUMBER_OF_ARGUMENTS));
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ValOneDeviceInstancePath(Args const& args) const
    {
        TRACE_ALWAYS(L"");
        if (2 != args.size()) return (HidHide::StringTable(IDS_WRONG_NUMBER_OF_ARGUMENTS));
        if (args.at(1).size() > MAX_DEVICE_ID_LEN) return (HidHide::StringTable(IDS_DEV_INST_PATH_TOO_LONG));
        return (std::wstring{});
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ValOneFullyQualifiedExecutablePath(Args const& args) const
    {
        TRACE_ALWAYS(L"");
        if (2 != args.size()) return (HidHide::StringTable(IDS_WRONG_NUMBER_OF_ARGUMENTS));
        auto const fullyQualifiedFileName{ std::filesystem::path(args.at(1)) };
        if (fullyQualifiedFileName.is_relative()) return (HidHide::StringTable(IDS_NOT_A_FULLY_QUALIFIED_PATH));
        if (!HidHide::FileIsAnApplication(fullyQualifiedFileName)) return (HidHide::StringTable(IDS_NOT_AN_EXECUTABLE));
        auto const fullImageName{ HidHide::FileNameToFullImageName(fullyQualifiedFileName) };
        if (fullImageName.empty()) return (HidHide::StringTable(IDS_NOT_ON_A_VOLUME));
        return (std::wstring{});
    }

    _Use_decl_annotations_
    void CommandInterpreter::Help(Args const&) const
    {
        TRACE_ALWAYS(L"");

        // The help text has three columns so determine the column widths
        auto const maxCommandSize{ MaxCommandSize() };
        auto const maxSyntaxSize{ MaxSyntaxSize() };

        // Help header
        std::wcout << std::endl;

        // Iterate all commands supported
        for (auto const& registeredCommand : m_RegisteredCommands)
        {
            std::wcout \
                << L"--" << registeredCommand.first << std::setfill(L' ') << std::setw(1 + maxCommandSize - CommandSize(registeredCommand)) << L" " \
                << registeredCommand.second.syntax << std::setfill(L' ') << std::setw(1 + maxSyntaxSize - SyntaxSize(registeredCommand)) << L" " \
                << registeredCommand.second.description \
                << std::endl;
        }
        
        // Help footer
        std::wcout << std::endl << StringTable(IDS_HELP_GUIDANCE) << std::endl << std::endl;
    }

    _Use_decl_annotations_
    void CommandInterpreter::Version(Args const&) const
    {
        TRACE_ALWAYS(L"");
        std::wcout << _L(BldProductVersion) << std::endl;
    }

    _Use_decl_annotations_
    void CommandInterpreter::Cancel(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_Cancel = true;
    }

    void CommandInterpreter::AppClean(Args const&)
    {
        TRACE_ALWAYS(L"");
        for (auto const& fullImageName : m_FilterDriverProxy.GetWhitelist())
        {
            if (!std::filesystem::exists(HidHide::FullImageNameToFileName(fullImageName)))
            {
                m_FilterDriverProxy.WhitelistDelEntry(fullImageName);
            }
        }
    }

    _Use_decl_annotations_
    void CommandInterpreter::AppReg(Args const& args)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.WhitelistAddEntry(HidHide::FileNameToFullImageName(args.at(1)));
    }

    _Use_decl_annotations_
    void CommandInterpreter::AppUnreg(Args const& args)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.WhitelistDelEntry(HidHide::FileNameToFullImageName(args.at(1)));
    }

    _Use_decl_annotations_
    void CommandInterpreter::AppList(Args const&) const
    {
        TRACE_ALWAYS(L"");
        for (auto const& fullImageName : m_FilterDriverProxy.GetWhitelist())
        {
            std::wcout << L"--app-reg \"" << HidHide::FullImageNameToFileName(fullImageName).native() << L"\"" << std::endl;
        }
    }

    _Use_decl_annotations_
    void CommandInterpreter::DevHide(Args const& args)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.BlacklistAddEntry(args.at(1));
    }

    _Use_decl_annotations_
    void CommandInterpreter::DevUnhinde(Args const& args)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.BlacklistDelEntry(args.at(1));
    }

    _Use_decl_annotations_
    void CommandInterpreter::DevList(Args const&) const
    {
        TRACE_ALWAYS(L"");
        for (auto const& device : m_FilterDriverProxy.GetBlacklist())
        {
            std::wcout << L"--dev-hide \"" << device << L"\"" << std::endl;
        }
    }

    _Use_decl_annotations_
    void CommandInterpreter::DevGaming(Args const&) const
    {
        TRACE_ALWAYS(L"");
        std::wcout << HidHide::HidDevices(true) << std::endl;
    }

    _Use_decl_annotations_
    void CommandInterpreter::DevAll(Args const&) const
    {
        TRACE_ALWAYS(L"");
        std::wcout << HidHide::HidDevices(false) << std::endl;
    }

    _Use_decl_annotations_
    void CommandInterpreter::CloakOn(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.SetActive(true);
    }

    _Use_decl_annotations_
    void CommandInterpreter::CloakOff(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.SetActive(false);
    }

    _Use_decl_annotations_
    void CommandInterpreter::CloakToggle(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.SetActive(!m_FilterDriverProxy.GetActive());
    }

    _Use_decl_annotations_
    void CommandInterpreter::CloakState(Args const&) const
    {
        TRACE_ALWAYS(L"");
        std::wcout << (m_FilterDriverProxy.GetActive() ? L"--cloak-on" : L"--cloak-off") << std::endl;
    }

    _Use_decl_annotations_
        void CommandInterpreter::InvOn(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.SetInverse(true);
    }

    _Use_decl_annotations_
        void CommandInterpreter::InvOff(Args const&)
    {
        TRACE_ALWAYS(L"");
        m_FilterDriverProxy.SetInverse(false);
    }

    _Use_decl_annotations_
        void CommandInterpreter::InvState(Args const&) const
    {
        TRACE_ALWAYS(L"");
        std::wcout << (m_FilterDriverProxy.GetInverse() ? L"--inv-on" : L"--inv-off") << std::endl;
    }
}
