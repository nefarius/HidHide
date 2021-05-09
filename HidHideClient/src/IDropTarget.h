// (c) Eric Korff de Gidts
// SPDX-License-Identifier: MIT
// IDropTarget.h
#pragma once

namespace HidHide
{
    // Interface for forwarding drop target events
    class IDropTarget
    {
    public:

        // Called when the cursor first enters the window
        virtual DROPEFFECT OnDragEnter(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dwKeyState);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }

        // Called repeatedly when the cursor is dragged over the window
        virtual DROPEFFECT OnDragOver(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DWORD dwKeyState, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dwKeyState);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }

        // Called when data is dropped into the window, initial handler
        virtual DROPEFFECT OnDropEx(_In_ CWnd* pWnd, _In_ COleDataObject* pDataObject, _In_ DROPEFFECT dropDefault, _In_ DROPEFFECT dropList, _In_ CPoint point)
        {
            UNREFERENCED_PARAMETER(pWnd);
            UNREFERENCED_PARAMETER(pDataObject);
            UNREFERENCED_PARAMETER(dropDefault);
            UNREFERENCED_PARAMETER(dropList);
            UNREFERENCED_PARAMETER(point);
            return (DROPEFFECT_NONE);
        }
    };
}
