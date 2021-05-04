// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// CommandInterpreter.cpp
#include "stdafx.h"
#include "CommandInterpreter.h"
#include "Utils.h"
#include "Logging.h"

namespace HidHide
{
    CommandInterpreter::~CommandInterpreter()
    {
        TRACE_ALWAYS(L"");
    }

    _Use_decl_annotations_
    void CommandInterpreter::Start(std::wstring const& commandLine)
    {
        TRACE_ALWAYS(L"");
        
        // Display the console welcome
        if (m_InteractiveMode) std::wcout << std::endl << HidHide::StringTable(IDS_DIALOG_APPLICATION) << std::endl << HidHide::StringTable(IDS_CONSOLE_GUIDANCE) << std::endl << std::endl;

        // Keep processing commands till ctrl-z is entered
        auto line{ commandLine };
        do
        {
            auto const commands{ ExtractCommands(line) };
            auto const errorMessage{ line.empty() ? ExecuteCommands(commands) : HidHide::StringTable(IDS_SYNTAX_ERROR) };
            if (!errorMessage.empty())
            {
                std::wcerr << errorMessage << std::endl;
                if (!m_InteractiveMode) return;
            }

            // Bail out on cancellation
            if (m_Cancel) return;

            // Display the command prompt
            if (m_InteractiveMode) std::wcout << L"$ ";

        } while ((m_InteractiveMode || m_ScriptMode) && (std::getline(std::wcin, line)));

        // Apply configuration changes
        m_FilterDriverProxy.ApplyConfigurationChanges();
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ExecuteCommands(std::vector<Args> const& commands) const
    {
        TRACE_ALWAYS(L"");

        for (auto const& command : commands)
        {
            auto const commandInfo{ m_RegisteredCommands.find(command.at(0)) };
            if (std::end(m_RegisteredCommands) == commandInfo) return (HidHide::StringTable(IDS_COMMAND_NOT_RECOGNIZED));
            if (auto const result{ commandInfo->second.validate(command) }; !result.empty()) return (result);
            commandInfo->second.execute(command);
        }

        return (std::wstring{});
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ExtractKeyword(std::wstring& value)
    {
        TRACE_ALWAYS(L"");

        // Find the next white space
        if (auto const it{ std::find_if(std::begin(value), std::end(value), [](wchar_t character) { return (std::isspace(character)); }) }; (std::end(value) != it))
        {
            auto const result{ std::wstring(std::begin(value), it) };
            value.erase(std::begin(value), it + 1);
            return (result);
        }

        // When no white spaces are found then the whole value is the file name
        auto const result{ value };
        value.clear();
        return (result);
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ExtractString(std::wstring& value)
    {
        TRACE_ALWAYS(L"");

        // When the string is starting with a double-quote then look for the next double-quote
        // There has to be a double-quote followed by either a whitespace or an end-of-line or we have a syntax error
        if (value.empty()) return (std::wstring{});
        if (L'"' == value.at(0))
        {
            auto const index{ value.find(L'"', 1) };
            auto const indexAtEnd{ index == (value.size() - 1) };
            if (!((std::wstring::npos != index) && (indexAtEnd || std::isspace(value.at(index + 1))))) return (std::wstring{});
            auto const result{ std::wstring(std::begin(value) + 1, std::begin(value) + index) };
            value.erase(std::begin(value), std::begin(value) + index + (indexAtEnd ? 1 : 2));
            return (result);
        }

        // When the string isn't starting with a double-quote then the file name delimiter is a whitespace
        return (ExtractKeyword(value));
    }

    _Use_decl_annotations_
    CommandInterpreter::Args CommandInterpreter::ExtractCommand(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        Args result{};

        // Remove leading indicator (if any)
        if (L"--" == value.substr(0, 2)) value.erase(std::begin(value), std::begin(value) + 2);

        // Any command starts with a keyword
        auto const keyword{ ExtractKeyword(value) };
        result.emplace_back(keyword);

        // Read all arguments till the start of new command or the string end but bail out on a syntax error
        while ((!Trim(value).empty()) && (L"--" != value.substr(0, 2)))
        {
            auto const arg{ ExtractString(value) };
            if ((arg.empty()) && (!value.empty())) return (Args{});
            result.emplace_back(arg);
        }

        return (result);
    }

    _Use_decl_annotations_
    std::vector<CommandInterpreter::Args> CommandInterpreter::ExtractCommands(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        std::vector<Args> result{};

        // Process the whole command line but bail out on a syntax error
        while (!Trim(value).empty())
        {
            auto const command{ ExtractCommand(value) };
            if (command.empty() && (!value.empty())) return (std::vector<Args>());
            result.emplace_back(command);
        }

        return (result);
    }

    _Use_decl_annotations_
    size_t CommandInterpreter::CommandSize(RegisteredCommands::value_type const& registeredCommand) noexcept
    {
        TRACE_ALWAYS(L"");
        return (registeredCommand.first.size());
    }

    _Use_decl_annotations_
    size_t CommandInterpreter::SyntaxSize(RegisteredCommands::value_type const& registeredCommand) noexcept
    {
        TRACE_ALWAYS(L"");
        return (registeredCommand.second.syntax.size());
    }

    size_t CommandInterpreter::MaxCommandSize() const
    {
        TRACE_ALWAYS(L"");
        return (std::max_element(std::begin(m_RegisteredCommands), std::end(m_RegisteredCommands), [](RegisteredCommands::value_type const& lhs, RegisteredCommands::value_type const& rhs) { return (lhs.first.size() < rhs.first.size()); })->first.size());
    }

    size_t CommandInterpreter::MaxSyntaxSize() const
    {
        TRACE_ALWAYS(L"");
        return (std::max_element(std::begin(m_RegisteredCommands), std::end(m_RegisteredCommands), [](RegisteredCommands::value_type const& lhs, RegisteredCommands::value_type const& rhs) { return (lhs.second.syntax.size() < rhs.second.syntax.size()); })->second.syntax.size());
    }
}
