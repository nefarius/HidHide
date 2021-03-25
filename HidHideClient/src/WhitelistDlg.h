// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// WhitelistDlg.h
#pragma once

class CWhitelistDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CWhitelistDlg)

public:

    CWhitelistDlg(_In_opt_ CWnd* pParent = nullptr);
    virtual ~CWhitelistDlg();

private:

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_WHITELIST };
#endif

    void DoDataExchange(_In_ CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    // Post a request to refresh the list
    void Refresh();

    // User Message on CM Notification Callbacks
    LRESULT OnUserMessageRefresh(_In_ WPARAM wParam, _In_ LPARAM lParam);

    DECLARE_MESSAGE_MAP()

    // Controls
    CListBox m_Whitelist;
    CStatic  m_Guidance;
    CButton  m_Insert;
    CButton  m_Delete;

    // Events
    afx_msg void OnBnClickedButtonWhitelistInsert();
    afx_msg void OnBnClickedButtonWhitelistDelete();
    afx_msg void OnShowWindow(_In_ BOOL bShow, _In_ UINT nStatus);
};
