// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// BlacklistDlg.h
#pragma once
#include "HidHideApi.h"

class CBlacklistDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CBlacklistDlg)

public:

    CBlacklistDlg() noexcept = delete;
    CBlacklistDlg(_In_ CBlacklistDlg const& rhs) = delete;
    CBlacklistDlg(_In_ CBlacklistDlg && rhs) noexcept = delete;
    CBlacklistDlg& operator=(_In_ CBlacklistDlg const& rhs) = delete;
    CBlacklistDlg& operator=(_In_ CBlacklistDlg && rhs) = delete;

    explicit CBlacklistDlg(_In_opt_ CWnd* pParent);
    virtual ~CBlacklistDlg();

    // Notification handler called when a PnP event of the specified type occurs
    // Be sure to handle Plug and Play device events as quickly as possible
    DWORD OnCmNotificationCallback(_In_ HCMNOTIFICATION cmNotification, _In_ CM_NOTIFY_ACTION cmNotifyAction, _In_reads_bytes_(cmNotifyEventDataSize) PCM_NOTIFY_EVENT_DATA cmNotifyEventData, _In_ DWORD cmNotifyEventDataSize);

private:

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_BLACKLIST };
#endif

    void DoDataExchange(_In_ CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    // Post a request to refresh the list
    void Refresh();

    // User Message on CM Notification Callbacks
    LRESULT OnUserMessageRefresh(_In_ WPARAM wParam, _In_ LPARAM lParam);

    DECLARE_MESSAGE_MAP()

    // The item data for the black-list
    HidHide::DescriptionToHidDeviceInstancePathsWithModelInfo m_BlacklistItemData;

    // Controls
    CTreeCtrl       m_Blacklist;
    HCMNOTIFICATION m_CmNotificationHandle;
    HICON           m_LockOn;
    HICON           m_LockOff;
    HICON           m_LockBlank;
    CImageList      m_ImageList;
    CButton         m_Filter;
    CButton         m_Gaming;
    CButton         m_Enable;
    CStatic         m_Guidance;

    // Events
    afx_msg void OnTvnItemChangedTreeBlacklist(_In_ NMHDR* pNMHDR, _Out_ LRESULT* pResult);
    afx_msg void OnBnClickedCheckFilter();
    afx_msg void OnBnClickedCheckGaming();
    afx_msg void OnBnClickedCheckEnable();
    afx_msg void OnShowWindow(_In_ BOOL bShow, _In_ UINT nStatus);
};
