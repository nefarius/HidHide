// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// WhitelistDlg.h
#pragma once
#include "IDropTarget.h"
#include "FilterDriverProxy.h"

class CHidHideClientDlg;

class CWhitelistDlg : public CDialogEx, public HidHide::IDropTarget
{
    DECLARE_DYNAMIC(CWhitelistDlg)

public:

    CWhitelistDlg(_In_ CHidHideClientDlg& hidHideClientDlg, _In_opt_ CWnd* pParent);
    virtual ~CWhitelistDlg();

    // Called when the cursor first enters the window
    DROPEFFECT OnDragEnter(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point) override;

    // Called repeatedly when the cursor is dragged over the window
    DROPEFFECT OnDragOver(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point) override;

    // Called when data is dropped into the window, initial handler
    DROPEFFECT OnDropEx(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DROPEFFECT dropDefault, _In_ DROPEFFECT dropList, _In_ CPoint point) override;

private:

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_WHITELIST };
#endif

    void DoDataExchange(_In_ CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    // Post a request to refresh the list
    void Refresh();

    // Is the mouse pointer in the whitelist control ?
    bool MousePointerAtWhitelist(_In_ CPoint point);

    // User Message on CM Notification Callbacks
    LRESULT OnUserMessageRefresh(_In_ WPARAM wParam, _In_ LPARAM lParam);

    // The shared filter driver proxy
    HidHide::FilterDriverProxy& FilterDriverProxy() noexcept;

    DECLARE_MESSAGE_MAP()

    // The parent
    CHidHideClientDlg& m_HidHideClientDlg;

    // Attributes
    HidHide::FullImageNames m_DropTargetFullImageNames;

    // Controls
    CListBox m_Whitelist;
    CStatic  m_Guidance;
    CButton  m_Insert;
    CButton  m_Delete;
    CButton  m_Inverse;

    // Events
    afx_msg void OnBnClickedButtonWhitelistInsert();
    afx_msg void OnBnClickedButtonWhitelistDelete();
    afx_msg void OnBnClickedCheckInverse();
    afx_msg void OnShowWindow(_In_ BOOL bShow, _In_ UINT nStatus);
};
