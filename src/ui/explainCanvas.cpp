//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
// RCS-ID:      $Id$
// Copyright (C) 2002 - 2005, The pgAdmin Development Team
// This software is released under the Artistic Licence
//
// explainCanvas.cpp - Explain Canvas
//
//////////////////////////////////////////////////////////////////////////


// wxWindows headers
#include <wx/wx.h>

// App headers
#include "pgAdmin3.h"

#include <explainCanvas.h>



ExplainCanvas::ExplainCanvas(wxWindow *parent)
: wxShapeCanvas(parent)
{
    SetDiagram(new wxDiagram);
    GetDiagram()->SetCanvas(this);
    SetBackgroundColour(*wxWHITE);
    lastShape=0;
    popup = new ExplainPopup(this);
}


ExplainCanvas::~ExplainCanvas()
{
}


void ExplainCanvas::Clear()
{
    GetDiagram()->DeleteAllShapes();
    lastShape=0;
}


void ExplainCanvas::SetExplainString(const wxString &str)
{
    Clear();

    ExplainShape *last=0;
    int maxLevel=0;

    wxStringTokenizer lines(str, wxT("\n"));

    while (lines.HasMoreTokens())
    {
        wxString tmp=lines.GetNextToken();
        wxString line=tmp.Strip(wxString::both);
        long level = (tmp.Length() - line.Length() +4) / 6;

        if (last)
        {
            if (level)
            {
                if (line.Left(4) == wxT("->  "))
                    line = line.Mid(4);
                else
                {
                    last->SetCondition(line);
                    continue;
                }
            }

            while (last && level <= last->GetLevel())
                last = last->GetUpper();
        }

        ExplainShape *s=ExplainShape::Create(level, last, line);
        if (!s)
            continue;
        s->SetCanvas(this);
        InsertShape(s);
        s->Show(true);

        if (level > maxLevel)
            maxLevel = level;
        
        if (!last)
            rootShape = s;
        last=s;
    }


#if EXPLAIN_VERTICAL
    int x0 = rootShape->GetWidth()*3/2;
    int y0 = rootShape->GetHeight()*3/2;
    int xoffs = rootShape->GetWidth()*3/2;
    int yoffs = rootShape->GetHeight()*3/2;
#else
    int x0 = rootShape->GetWidth()*3;
    int y0 = rootShape->GetHeight()*3/2;
    int xoffs = rootShape->GetWidth()*3;
    int yoffs = rootShape->GetHeight()*3/2;
#endif

    wxNode *current = GetDiagram()->GetShapeList()->GetFirst();
    while (current)
    {
        ExplainShape *s = (ExplainShape*)current->GetData();

        if (!s->totalShapes)
            s->totalShapes = 1;
        if (s->GetUpper())
            s->GetUpper()->totalShapes += s->totalShapes;
        current = current->GetNext();
    }

    current = GetDiagram()->GetShapeList()->GetLast();
    while (current)
    {
        ExplainShape *s = (ExplainShape*)current->GetData();

#if EXPLAIN_VERTICAL
        s->SetY(y0 + s->GetLevel() * yoffs);
#else
        s->SetX(y0 + s->GetLevel() * xoffs);
#endif
        ExplainShape *upper = s->GetUpper();

        if (upper)
        {
#if EXPLAIN_VERTICAL
            s->SetX(upper->GetX() - (upper->totalShapes-1)*xoffs/2 + upper->usedShapes * xoffs + (s->totalShapes-1) *xoffs/2);
#else
            s->SetY(upper->GetY() + upper->usedShapes * yoffs);
#endif
            upper->usedShapes += s->totalShapes;

            wxLineShape *l=new ExplainLine(s, upper);
            l->Show(true);
            AddShape(l);
        }
        else
        {
#if EXPLAIN_VERTICAL
            s->SetX(x0 + s->totalShapes*xoffs/2);
#else
            s->SetY(y0);
#endif
        }

        current = current->GetPrevious();
    }

#define PIXPERUNIT  20
#if EXPLAIN_VERTICAL
    int w=(rootShape->totalShapes * xoffs + x0*2 + PIXPERUNIT - 1) / PIXPERUNIT;
    int h=(maxLevel * yoffs + y0*2 + PIXPERUNIT - 1) / PIXPERUNIT;
#else
    int w=(maxLevel * xoffs + x0*2 + PIXPERUNIT - 1) / PIXPERUNIT;
    int h=(rootShape->totalShapes * yoffs + y0*2 + PIXPERUNIT - 1) / PIXPERUNIT;
#endif

    SetScrollbars(PIXPERUNIT, PIXPERUNIT, w, h);
}


void ExplainCanvas::ShowPopup(ExplainShape *s)
{
    int sx, sy;
    CalcScrolledPosition(s->GetX(), s->GetY(), &sx, &sy);

    popup->SetShape(s);

    if (sy > GetClientSize().y*2/3)
        sy -= popup->GetSize().y;
    sx -= popup->GetSize().x / 2;
    if (sx < 0) sx=0;

    popup->Move(ClientToScreen(wxPoint(sx, sy)));
    popup->Popup();
}


class ExplainText : public wxWindow
{
public:
    ExplainText(wxWindow *parent, ExplainShape *s);

private:
    ExplainShape *shape;
    void OnPaint(wxPaintEvent &ev);

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ExplainText, wxWindow)
    EVT_PAINT(ExplainText::OnPaint)
END_EVENT_TABLE()


ExplainText::ExplainText(wxWindow *parent, ExplainShape *s) : wxWindow(parent, -1)
{
    SetBackgroundColour(wxColour(255,255,224));

    shape=s;

    wxWindowDC dc(this);
    dc.SetFont(settings->GetSystemFont());
    int w1, w2, h;

    dc.GetTextExtent(shape->condition, &w1, &h);
    dc.GetTextExtent(shape->cost, &w2, &h);
    if (w1 < w2)    w1=w2;
    dc.GetTextExtent(shape->actual, &w2, &h);
    if (w1 < w2)    w1=w2;

    int n=1;
    if (!shape->condition.IsEmpty())
        n++;
    if (!shape->cost.IsEmpty())
        n++;
    if (!shape->actual.IsEmpty())
        n++;

    SetSize(GetCharHeight() + w1, GetCharHeight() + h +  h * 5 / 4 * n);
}

    
void ExplainText::OnPaint(wxPaintEvent &ev)
{
    wxPaintDC dc(this);

    wxFont stdFont = settings->GetSystemFont();
    wxFont boldFont = stdFont;
    boldFont.SetWeight(wxBOLD);

    int x = GetCharHeight() / 2;
    int y = GetCharHeight() / 2;
    int w, yoffs;
    dc.GetTextExtent(wxT("Dummy"), &w, &yoffs);
    yoffs *= 5;
    yoffs /= 4;

    dc.SetFont(boldFont);
    dc.DrawText(shape->label, x, y);

    dc.SetFont(stdFont);

    if (!shape->condition.IsEmpty())
    {
        y += yoffs;
        dc.DrawText(shape->condition, x, y);
    }
    if (!shape->cost.IsEmpty())
    {
        y += yoffs;
        dc.DrawText(shape->cost, x, y);
    }
    if (!shape->actual.IsEmpty())
    {
        y += yoffs;
        dc.DrawText(shape->actual, x, y);
    }
}


BEGIN_EVENT_TABLE(ExplainPopup, wxDialog)
EVT_MOTION(ExplainPopup::LeaveWindow)
END_EVENT_TABLE()

ExplainPopup::ExplainPopup(wxWindow *w) : wxDialog(w, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER)
{
    explainText=0;
}


void ExplainPopup::SetShape(ExplainShape *s)
{
    if (explainText)
        delete explainText;
    explainText = new ExplainText(this, s);
    SetSize(explainText->GetSize());
}

void ExplainPopup::Popup()
{
    Show();
    wxYield();

    CaptureMouse();
}


void ExplainPopup::LeaveWindow(wxMouseEvent &ev)
{
    ReleaseMouse();
    delete explainText;
    explainText=0;
    wxDialog::Hide();
}