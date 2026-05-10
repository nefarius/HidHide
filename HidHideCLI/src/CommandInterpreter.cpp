// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// CommandInterpreter.cpp
#include "stdafx.h"
#include "CommandInterpreter.h"
#include "CliParsing.h"
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
            auto const commands{ CliParsing::ExtractCommands(line) };
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
        return (CliParsing::ExtractKeyword(value));
    }

    _Use_decl_annotations_
    std::wstring CommandInterpreter::ExtractString(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        return (CliParsing::ExtractString(value));
    }

    _Use_decl_annotations_
    CommandInterpreter::Args CommandInterpreter::ExtractCommand(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        return (CliParsing::ExtractCommand(value));
    }

    _Use_decl_annotations_
    std::vector<CommandInterpreter::Args> CommandInterpreter::ExtractCommands(std::wstring& value)
    {
        TRACE_ALWAYS(L"");
        return (CliParsing::ExtractCommands(value));
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
