// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideClient.h
#pragma once

class CHidHideClientApp : public CWinApp
{
public:

    CHidHideClientApp() noexcept;
    CHidHideClientApp(_In_ CHidHideClientApp const& rhs) = delete;
    CHidHideClientApp(_In_ CHidHideClientApp&& rhs) noexcept = delete;
    CHidHideClientApp& operator=(_In_ CHidHideClientApp const& rhs) = delete;
    CHidHideClientApp& operator=(_In_ CHidHideClientApp&& rhs) = delete;
    virtual ~CHidHideClientApp();

private:

    BOOL InitInstance() override;

    DECLARE_MESSAGE_MAP()
};
