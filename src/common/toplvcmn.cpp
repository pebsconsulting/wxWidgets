/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/toplvcmn.cpp
// Purpose:     common (for all platforms) wxTopLevelWindow functions
// Author:      Julian Smart, Vadim Zeitlin
// Created:     01/02/97
// Id:          $Id$
// Copyright:   (c) 1998 Robert Roebling and Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/toplevel.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
    #include "wx/app.h"
#endif // WX_PRECOMP

#include "wx/display.h"

// ----------------------------------------------------------------------------
// event table
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxTopLevelWindowBase, wxWindow)
    EVT_CLOSE(wxTopLevelWindowBase::OnCloseWindow)
    EVT_SIZE(wxTopLevelWindowBase::OnSize)
    EVT_WINDOW_DESTROY(wxTopLevelWindowBase::OnChildDestroy)
    WX_EVENT_TABLE_CONTROL_CONTAINER(wxTopLevelWindowBase)
END_EVENT_TABLE()

WX_DELEGATE_TO_CONTROL_CONTAINER(wxTopLevelWindowBase, wxWindow)

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_ABSTRACT_CLASS(wxTopLevelWindow, wxWindow)

// ----------------------------------------------------------------------------
// construction/destruction
// ----------------------------------------------------------------------------

wxTopLevelWindowBase::wxTopLevelWindowBase()
{
    // Unlike windows, top level windows are created hidden by default.
    m_isShown = false;
    m_winDefault =
    m_winTmpDefault = NULL;

    WX_INIT_CONTROL_CONTAINER();
}

wxTopLevelWindowBase::~wxTopLevelWindowBase()
{
    m_winDefault =
    m_winTmpDefault = NULL;

    // don't let wxTheApp keep any stale pointers to us
    if ( wxTheApp && wxTheApp->GetTopWindow() == this )
        wxTheApp->SetTopWindow(NULL);

    wxTopLevelWindows.DeleteObject(this);

    // delete any our top level children which are still pending for deletion
    // immediately: this could happen if a child (e.g. a temporary dialog
    // created with this window as parent) was Destroy()'d) while this window
    // was deleted directly (with delete, or maybe just because it was created
    // on the stack) immediately afterwards and before the child TLW was really
    // destroyed -- not destroying it now would leave it alive with a dangling
    // parent pointer and result in a crash later
    for ( wxObjectList::iterator i = wxPendingDelete.begin();
          i != wxPendingDelete.end();
          )
    {
        wxWindow * const win = wxDynamicCast(*i, wxWindow);
        if ( win && win->GetParent() == this )
        {
            wxPendingDelete.erase(i);

            delete win;

            // deleting it invalidated the list (and not only one node because
            // it could have resulted in deletion of other objects to)
            i = wxPendingDelete.begin();
        }
        else
        {
            ++i;
        }
    }

    if ( IsLastBeforeExit() )
    {
        // no other (important) windows left, quit the app
        wxTheApp->ExitMainLoop();
    }
}

bool wxTopLevelWindowBase::Destroy()
{
    // delayed destruction: the frame will be deleted during the next idle
    // loop iteration
    if ( !wxPendingDelete.Member(this) )
        wxPendingDelete.Append(this);

    // normally we want to hide the window immediately so that it doesn't get
    // stuck on the screen while it's being destroyed, however we shouldn't
    // hide the last visible window as then we might not get any idle events
    // any more as no events will be sent to the hidden window and without idle
    // events we won't prune wxPendingDelete list and the application won't
    // terminate
    for ( wxWindowList::const_iterator i = wxTopLevelWindows.begin(),
                                     end = wxTopLevelWindows.end();
          i != end;
          ++i )
    {
        wxTopLevelWindow * const win = wx_static_cast(wxTopLevelWindow *, *i);
        if ( win != this && win->IsShown() )
        {
            // there remains at least one other visible TLW, we can hide this
            // one
            Hide();

            break;
        }
    }

    return true;
}

bool wxTopLevelWindowBase::IsLastBeforeExit() const
{
    // first of all, automatically exiting the app on last window close can be
    // completely disabled at wxTheApp level
    if ( !wxTheApp || !wxTheApp->GetExitOnFrameDelete() )
        return false;

    wxWindowList::const_iterator i;
    const wxWindowList::const_iterator end = wxTopLevelWindows.end();

    // then decide whether we should exit at all
    for ( i = wxTopLevelWindows.begin(); i != end; ++i )
    {
        wxTopLevelWindow * const win = wx_static_cast(wxTopLevelWindow *, *i);
        if ( win->ShouldPreventAppExit() )
        {
            // there remains at least one important TLW, don't exit
            return false;
        }
    }

    // if yes, close all the other windows: this could still fail
    for ( i = wxTopLevelWindows.begin(); i != end; ++i )
    {
        // don't close twice the windows which are already marked for deletion
        wxTopLevelWindow * const win = wx_static_cast(wxTopLevelWindow *, *i);
        if ( !wxPendingDelete.Member(win) && !win->Close() )
        {
            // one of the windows refused to close, don't exit
            //
            // NB: of course, by now some other windows could have been already
            //     closed but there is really nothing we can do about it as we
            //     have no way just to ask the window if it can close without
            //     forcing it to do it
            return false;
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxTopLevelWindow geometry
// ----------------------------------------------------------------------------

void wxTopLevelWindowBase::SetMinSize(const wxSize& minSize)
{
    SetSizeHints(minSize, GetMaxSize());
}

void wxTopLevelWindowBase::SetMaxSize(const wxSize& maxSize)
{
    SetSizeHints(GetMinSize(), maxSize);
}

void wxTopLevelWindowBase::GetRectForTopLevelChildren(int *x, int *y, int *w, int *h)
{
    GetPosition(x,y);
    GetSize(w,h);
}

/* static */
wxSize wxTopLevelWindowBase::GetDefaultSize()
{
    wxSize size = wxGetClientDisplayRect().GetSize();

    // create proportionally bigger windows on small screens
    if ( size.x >= 1024 )
        size.x = 400;
    else if ( size.x >= 800 )
        size.x = 300;
    else if ( size.x >= 320 )
        size.x = 240;

    if ( size.y >= 768 )
        size.y = 250;
    else if ( size.y > 200 )
    {
        size.y *= 2;
        size.y /= 3;
    }

    return size;
}

void wxTopLevelWindowBase::DoCentre(int dir)
{
    // on some platforms centering top level windows is impossible
    // because they are always maximized by guidelines or limitations
    if(IsAlwaysMaximized())
        return;

    // we need the display rect anyhow so store it first: notice that we should
    // be centered on the same display as our parent window, the display of
    // this window itself is not really defined yet
    int nDisplay = wxDisplay::GetFromWindow(GetParent() ? GetParent() : this);
    wxDisplay dpy(nDisplay == wxNOT_FOUND ? 0 : nDisplay);
    const wxRect rectDisplay(dpy.GetClientArea());

    // what should we centre this window on?
    wxRect rectParent;
    if ( !(dir & wxCENTRE_ON_SCREEN) && GetParent() )
    {
        // centre on parent window: notice that we need screen coordinates for
        // positioning this TLW
        rectParent = GetParent()->GetScreenRect();

        // if the parent is entirely off screen (happens at least with MDI
        // parent frame under Mac but could happen elsewhere too if the frame
        // was hidden/moved away for some reason), don't use it as otherwise
        // this window wouldn't be visible at all
        if ( !rectDisplay.Contains(rectParent.GetTopLeft()) &&
                !rectParent.Contains(rectParent.GetBottomRight()) )
        {
            // this is enough to make IsEmpty() test below pass
            rectParent.width = 0;
        }
    }

    if ( rectParent.IsEmpty() )
    {
        // we were explicitly asked to centre this window on the entire screen
        // or if we have no parent anyhow and so can't centre on it
        rectParent = rectDisplay;
    }

    // centering maximized window on screen is no-op
    if((rectParent == rectDisplay) && IsMaximized())
        return;

    // the new window rect candidate
    wxRect rect = GetRect().CentreIn(rectParent, dir);

    // we don't want to place the window off screen if Centre() is called as
    // this is (almost?) never wanted and it would be very difficult to prevent
    // it from happening from the user code if we didn't check for it here
    if ( !rectDisplay.Contains(rect.GetTopLeft()) )
    {
        // move the window just enough to make the corner visible
        int dx = rectDisplay.GetLeft() - rect.GetLeft();
        int dy = rectDisplay.GetTop() - rect.GetTop();
        rect.Offset(dx > 0 ? dx : 0, dy > 0 ? dy : 0);
    }

    if ( !rectDisplay.Contains(rect.GetBottomRight()) )
    {
        // do the same for this corner too
        int dx = rectDisplay.GetRight() - rect.GetRight();
        int dy = rectDisplay.GetBottom() - rect.GetBottom();
        rect.Offset(dx < 0 ? dx : 0, dy < 0 ? dy : 0);
    }

    // the window top left and bottom right corner are both visible now and
    // although the window might still be not entirely on screen (with 2
    // staggered displays for example) we wouldn't be able to improve the
    // layout much in such case, so we stop here

    // -1 could be valid coordinate here if there are several displays
    SetSize(rect, wxSIZE_ALLOW_MINUS_ONE);
}

// ----------------------------------------------------------------------------
// wxTopLevelWindow size management: we exclude the areas taken by
// menu/status/toolbars from the client area, so the client area is what's
// really available for the frame contents
// ----------------------------------------------------------------------------

void wxTopLevelWindowBase::DoScreenToClient(int *x, int *y) const
{
    wxWindow::DoScreenToClient(x, y);

    // translate the wxWindow client coords to our client coords
    wxPoint pt(GetClientAreaOrigin());
    if ( x )
        *x -= pt.x;
    if ( y )
        *y -= pt.y;
}

void wxTopLevelWindowBase::DoClientToScreen(int *x, int *y) const
{
    // our client area origin (0, 0) may be really something like (0, 30) for
    // wxWindow if we have a toolbar, account for it before translating
    wxPoint pt(GetClientAreaOrigin());
    if ( x )
        *x += pt.x;
    if ( y )
        *y += pt.y;

    wxWindow::DoClientToScreen(x, y);
}

bool wxTopLevelWindowBase::IsAlwaysMaximized() const
{
#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------
// icons
// ----------------------------------------------------------------------------

wxIcon wxTopLevelWindowBase::GetIcon() const
{
    return m_icons.IsEmpty() ? wxIcon() : m_icons.GetIcon( -1 );
}

void wxTopLevelWindowBase::SetIcon(const wxIcon& icon)
{
    // passing wxNullIcon to SetIcon() is possible (it means that we shouldn't
    // have any icon), but adding an invalid icon to wxIconBundle is not
    wxIconBundle icons;
    if ( icon.Ok() )
        icons.AddIcon(icon);

    SetIcons(icons);
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

// default resizing behaviour - if only ONE subwindow, resize to fill the
// whole client area
void wxTopLevelWindowBase::DoLayout()
{
    // if we're using constraints or sizers - do use them
    if ( GetAutoLayout() )
    {
        Layout();
    }
    else
    {
        // do we have _exactly_ one child?
        wxWindow *child = (wxWindow *)NULL;
        for ( wxWindowList::compatibility_iterator node = GetChildren().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxWindow *win = node->GetData();

            // exclude top level and managed windows (status bar isn't
            // currently in the children list except under wxMac anyhow, but
            // it makes no harm to test for it)
            if ( !win->IsTopLevel() && !IsOneOfBars(win) )
            {
                if ( child )
                {
                    return;     // it's our second subwindow - nothing to do
                }

                child = win;
            }
        }

        // do we have any children at all?
        if ( child && child->IsShown() )
        {
            // exactly one child - set it's size to fill the whole frame
            int clientW, clientH;
            DoGetClientSize(&clientW, &clientH);

            child->SetSize(0, 0, clientW, clientH);
        }
    }
}

// The default implementation for the close window event.
void wxTopLevelWindowBase::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    Destroy();
}

void wxTopLevelWindowBase::OnChildDestroy(wxWindowDestroyEvent& event)
{
    event.Skip();

    wxWindow * const win = event.GetWindow();
    if ( win == m_winDefault )
        m_winDefault = NULL;
    if ( win == m_winTmpDefault )
        m_winTmpDefault = NULL;
}

bool wxTopLevelWindowBase::SendIconizeEvent(bool iconized)
{
    wxIconizeEvent event(GetId(), iconized);
    event.SetEventObject(this);

    return GetEventHandler()->ProcessEvent(event);
}

// do the window-specific processing after processing the update event
void wxTopLevelWindowBase::DoUpdateWindowUI(wxUpdateUIEvent& event)
{
    // call inherited, but skip the wxControl's version, and call directly the
    // wxWindow's one instead, because the only reason why we are overriding this
    // function is that we want to use SetTitle() instead of wxControl::SetLabel()
    wxWindowBase::DoUpdateWindowUI(event);

    // update title
    if ( event.GetSetText() )
    {
        if ( event.GetText() != GetTitle() )
            SetTitle(event.GetText());
    }
}

void wxTopLevelWindowBase::RequestUserAttention(int WXUNUSED(flags))
{
    // it's probably better than do nothing, isn't it?
    Raise();
}
