// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// HidHideClientDlg.h
#pragma once
#include "HidHideApi.h"
#include "BlacklistDlg.h"
#include "WhitelistDlg.h"

class CHidHideClientDlg : public CDialogEx, public HidHide::IDropTarget
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

    // Handler for drop target events
    class CDropTarget : public COleDropTarget
    {
    public:

        CDropTarget(_In_ CDropTarget const& rhs) = delete;
        CDropTarget(_In_ CDropTarget&& rhs) noexcept = delete;
        CDropTarget& operator=(_In_ CDropTarget const& rhs) = delete;
        CDropTarget& operator=(_In_ CDropTarget&& rhs) = delete;

        CDropTarget() noexcept : m_IDropTarget{} {};
        virtual ~CDropTarget() {};

        // Called when the cursor first enters the window
        DROPEFFECT OnDragEnter(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point) override
        {
            return ((nullptr == m_IDropTarget) ? DROPEFFECT_NONE : m_IDropTarget->OnDragEnter(pWnd, pDataObject, dwKeyState, point));
        }

        // Called repeatedly when the cursor is dragged over the window
        DROPEFFECT OnDragOver(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point) override
        {
            return ((nullptr == m_IDropTarget) ? DROPEFFECT_NONE : m_IDropTarget->OnDragOver(pWnd, pDataObject, dwKeyState, point));
        }

        // Called when data is dropped into the window, initial handler
        DROPEFFECT OnDropEx(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DROPEFFECT dropDefault, _In_ DROPEFFECT dropList, _In_ CPoint point) override
        {
            return ((nullptr == m_IDropTarget) ? DROPEFFECT_NONE : m_IDropTarget->OnDropEx(pWnd, pDataObject, dropDefault, dropList, point));
        }

        // Define redirection
        void SetRedirectionTarget(IDropTarget& iDropTarget)
        {
            m_IDropTarget = &iDropTarget;
        }

    private:
        IDropTarget* m_IDropTarget;
    };

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_APPLICATION };
#endif

    // Update visibility of tab dialogs based on the currently selected tab
    void ResyncTabDialogVisibilityState();

    void DoDataExchange(_In_ CDataExchange* pDX) override;
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

    // Drop file support
    CDropTarget m_DropTarget;

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
