// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// CommandInterpreter.h
#pragma once
#include "FilterDriverProxy.h"

namespace HidHide
{
    class CommandInterpreter
    {
    public:

        CommandInterpreter() noexcept = delete;
        CommandInterpreter(_In_ CommandInterpreter const& rhs) = delete;
        CommandInterpreter(_In_ CommandInterpreter&& rhs) noexcept = delete;
        CommandInterpreter& operator=(_In_ CommandInterpreter const& rhs) = delete;
        CommandInterpreter& operator=(_In_ CommandInterpreter&& rhs) = delete;

        // Exclusively lock the device driver and ensure the module file name is on the whitelist
        explicit CommandInterpreter(_In_ bool writeThrough);
        ~CommandInterpreter();

        // Start the command interpreter using stdin, stdout, and stderr
        // - Script mode (standard input redirected and/or command line not empty)
        // - Interactive mode (standard input not redirected + empty command line)
        void Start(_In_ std::wstring const& commandLine);

    private:

        typedef std::vector<std::wstring> Args;
        typedef std::function<void(_In_ Args const& args)> ExecuteFunction;
        typedef std::function<std::wstring(_In_ Args const& args)> ValidateFunction;

        struct RegisteredCommandInfo
        {
            std::wstring     syntax;      // Help text explaining the command syntax
            std::wstring     description; // Help text describing what the command does
            ExecuteFunction  execute;     // Command handler
            ValidateFunction validate;    // Argument validation prior to execution
        };

        typedef std::map<std::wstring, RegisteredCommandInfo> RegisteredCommands;

        // Execute a sequence of commands
        std::wstring ExecuteCommands(_In_ std::vector<Args> const& commands) const;

        // Extract a keyword (no double-quote allowed)
        // Returns empty an on empty string
        static std::wstring ExtractKeyword(_Inout_ std::wstring& value);

        // Extract a string (double-quotes allowed)
        // Returns empty on syntax error
        static std::wstring ExtractString(_Inout_ std::wstring& value);

        // Extract the next command and its arguments
        // Returns empty on syntax error
        static Args ExtractCommand(_Inout_ std::wstring& value);

        // Extract all commands from a command line
        // Returns empty on syntax error
        static std::vector<Args> ExtractCommands(_Inout_ std::wstring& value);

        // Get the size of the command
        static size_t CommandSize(_In_ RegisteredCommands::value_type const& registeredCommand) noexcept;

        // Get the size of the syntax
        static size_t SyntaxSize(_In_ RegisteredCommands::value_type const& registeredCommand) noexcept;

        // Get the size of the largest command
        size_t MaxCommandSize() const;

        // Get the size of the largest syntax
        size_t MaxSyntaxSize() const;

        // Validate that there are no additional arguments; returns the parsing error message
        std::wstring ValNoArguments(_In_ Args const& args) const;

        // Validate that there is exactly one argument pointing to a valid device instance path; returns the parsing error message
        std::wstring ValOneDeviceInstancePath(_In_ Args const& args) const;

        // Validate that there is exactly one argument pointing to an executable file on a storage volume; returns the parsing error message
        // Note that this doesn't imply that the file actually exists; application registration may go in advance of its actual installation
        std::wstring ValOneFullyQualifiedExecutablePath(_In_ Args const& args) const;

        // Summarizes the commands supported
        void Help(_In_ Args const& args) const;

        // Displays the current version label
        void Version(_In_ Args const& args) const;

        // Exit without saving configuration changes
        void Cancel(_In_ Args const& args);

        // Grants ability to see hidden devices
        void AppReg(_In_ Args const& args);

        // Revokes ability to see hidden devices
        void AppUnreg(_In_ Args const& args);

        // Remove absent registered applications
        void AppClean(Args const&);

        // Lists the registered applications
        void AppList(_In_ Args const& args) const;

        // Hide the device specified
        void DevHide(_In_ Args const& args);

        // Unhide the device specified
        void DevUnhinde(_In_ Args const& args);

        // Lists the hidden devices
        void DevList(_In_ Args const& args) const;

        // Lists all HID devices used for gaming
        void DevGaming(_In_ Args const& args) const;

        // Lists all HID devices
        void DevAll(_In_ Args const& args) const;

        // Turns device hiding on
        void CloakOn(_In_ Args const& args);

        // Turns device hiding off
        void CloakOff(_In_ Args const& args);

        // Toggles current device hiding state
        void CloakToggle(_In_ Args const& args);

        // Reports the current cloaking state
        void CloakState(_In_ Args const& args) const;

        // Turns whitelist inverse on
        void InvOn(_In_ Args const& args);

        // Turns whitelist inverse off
        void InvOff(_In_ Args const& args);

        // Reports the current whitelist inverse
        void InvState(_In_ Args const& args) const;

        bool const               m_ScriptMode;         // Active when standard input is redirected
        bool const               m_InteractiveMode;    // Active without command line arguments or input redirection
        RegisteredCommands const m_RegisteredCommands; // Self reflection on all commands supported
        FilterDriverProxy        m_FilterDriverProxy;  // Filter driver access and cache layer
        bool                     m_Cancel;             // Flag set when the configuration changes shouldn't be applied
    };
}
