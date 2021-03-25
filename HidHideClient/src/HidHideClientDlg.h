// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideClientDlg.h
#pragma once
#include "BlacklistDlg.h"
#include "WhitelistDlg.h"

class CHidHideClientDlg : public CDialogEx
{
public:

    CHidHideClientDlg() noexcept = delete;
    CHidHideClientDlg(_In_ CHidHideClientDlg const& rhs) = delete;
    CHidHideClientDlg(_In_ CHidHideClientDlg&& rhs) noexcept = delete;
    CHidHideClientDlg& operator=(_In_ CHidHideClientDlg const& rhs) = delete;
    CHidHideClientDlg& operator=(_In_ CHidHideClientDlg&& rhs) = delete;

    explicit CHidHideClientDlg(_In_opt_ CWnd* pParent);
    virtual ~CHidHideClientDlg() = default;

private:

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_APPLICATION };
#endif

    // Update visibility of tab dialogs based on the currently selected tab
    void ResyncTabDialogVisibilityState();

    void DoDataExchange(_In_ CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

    // Controls
    HICON         m_hIcon;
    CTabCtrl      m_TabApplication;
    CBlacklistDlg m_BlacklistDlg;
    CWhitelistDlg m_WhitelistDlg;

    // Events
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTcnSelchangeTabApplication(_In_ NMHDR* pNMHDR, _Out_ LRESULT* pResult);
    afx_msg void OnShowWindow(_In_ BOOL bShow, _In_ UINT nStatus);
};
