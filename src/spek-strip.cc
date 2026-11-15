#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/fontenum.h>

#include "spek-strip.h"

namespace
{

// Sizes are in DIPs, see wxWindow::FromDIP().
const int STRIP_HEIGHT = 36;
const int BUTTON_WIDTH = 28;
const int BUTTON_HEIGHT = 28;
const int CELL_HEIGHT = 22;
const int CELL_PADDING = 5;
const int TINY_WIDTH = 12;

wxColour metal_light() { return wxColour(0xd4, 0xd4, 0xe4); }
wxColour metal_dark() { return wxColour(0x1a, 0x1a, 0x24); }

void fill_gradient(wxDC& dc, const wxRect& rect, const wxColour& top, const wxColour& bottom)
{
    if (rect.width > 0 && rect.height > 0) {
        dc.GradientFillLinear(rect, top, bottom, wxSOUTH);
    }
}

void paint_background(wxDC& dc, const wxSize& size)
{
    const int w = size.x;
    const int h = size.y;
    const int y1 = h * 14 / 100;
    const int y2 = h * 60 / 100;
    const wxColour c0(0x5e, 0x5e, 0x74);
    const wxColour c1(0x3c, 0x3c, 0x4c);
    const wxColour c2(0x2b, 0x2b, 0x38);
    const wxColour c3(0x1e, 0x1e, 0x28);
    fill_gradient(dc, wxRect(0, 0, w, y1), c0, c1);
    fill_gradient(dc, wxRect(0, y1, w, y2 - y1), c1, c2);
    fill_gradient(dc, wxRect(0, y2, w, h - y2), c2, c3);
    dc.SetPen(wxPen(wxColour(0x8a, 0x8a, 0xa2)));
    dc.DrawLine(0, 0, w, 0);
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawLine(0, h - 1, w, h - 1);
}

// One pixel wide frame, `tl` on the top and left sides, `br` on the others.
void draw_bevel(wxDC& dc, const wxRect& r, const wxColour& tl, const wxColour& br)
{
    const int x2 = r.x + r.width - 1;
    const int y2 = r.y + r.height - 1;
    dc.SetPen(wxPen(tl));
    dc.DrawLine(r.x, r.y, x2, r.y);
    dc.DrawLine(r.x, r.y, r.x, y2);
    dc.SetPen(wxPen(br));
    dc.DrawLine(x2, r.y, x2, y2 + 1);
    dc.DrawLine(r.x, y2, x2 + 1, y2);
}

void draw_metal(wxDC& dc, const wxRect& r, bool pressed, bool hover, bool enabled)
{
    wxColour top(0xc6, 0xc6, 0xd6);
    wxColour bottom(0x8a, 0x8a, 0x9e);
    if (!enabled) {
        top = wxColour(0x6a, 0x6a, 0x7c);
        bottom = wxColour(0x4a, 0x4a, 0x5a);
    } else if (pressed) {
        top = wxColour(0x8a, 0x8a, 0x9e);
        bottom = wxColour(0x6a, 0x6a, 0x7e);
    } else if (hover) {
        top = wxColour(0xdc, 0xdc, 0xea);
        bottom = wxColour(0x9c, 0x9c, 0xb0);
    }
    fill_gradient(dc, wxRect(r.x + 1, r.y + 1, r.width - 2, r.height - 2), top, bottom);
    if (pressed) {
        draw_bevel(dc, r, metal_dark(), metal_light());
    } else {
        draw_bevel(dc, r, metal_light(), metal_dark());
    }
}

void draw_triangle(wxDC& dc, const wxRect& r, bool up, const wxColour& colour)
{
    int base = (r.width / 2) | 1;
    int height = (base + 1) / 2;
    int cx = r.x + r.width / 2;
    int cy = r.y + r.height / 2;
    wxPoint points[3];
    if (up) {
        points[0] = wxPoint(cx - base / 2, cy + height / 2);
        points[1] = wxPoint(cx + base / 2, cy + height / 2);
        points[2] = wxPoint(cx, cy - height / 2);
    } else {
        points[0] = wxPoint(cx - base / 2, cy - height / 2);
        points[1] = wxPoint(cx + base / 2, cy - height / 2);
        points[2] = wxPoint(cx, cy + height / 2);
    }
    dc.SetPen(wxPen(colour));
    dc.SetBrush(wxBrush(colour));
    dc.DrawPolygon(3, points);
}

} // namespace

// SpekStripPanel

SpekStripPanel::SpekStripPanel(wxWindow *parent) : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &SpekStripPanel::on_paint, this);
}

void SpekStripPanel::on_paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    paint_background(dc, GetClientSize());
}

// SpekMetalButton

SpekMetalButton::SpekMetalButton(
    wxWindow *parent, const wxBitmap& icon, const wxString& tip,
    std::function<void()> on_click) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
    icon(icon),
    on_click(on_click),
    pressed(false),
    hover(false)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetToolTip(tip);
    Bind(wxEVT_PAINT, &SpekMetalButton::on_paint, this);
    Bind(wxEVT_LEFT_DOWN, &SpekMetalButton::on_left_down, this);
    Bind(wxEVT_LEFT_DCLICK, &SpekMetalButton::on_left_down, this);
    Bind(wxEVT_LEFT_UP, &SpekMetalButton::on_left_up, this);
    Bind(wxEVT_ENTER_WINDOW, &SpekMetalButton::on_enter_leave, this);
    Bind(wxEVT_LEAVE_WINDOW, &SpekMetalButton::on_enter_leave, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &SpekMetalButton::on_capture_lost, this);
}

wxSize SpekMetalButton::DoGetBestSize() const
{
    return wxSize(FromDIP(BUTTON_WIDTH + 2), FromDIP(STRIP_HEIGHT));
}

void SpekMetalButton::on_paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    paint_background(dc, size);
    const int w = size.x - 2 * FromDIP(1);
    const int h = FromDIP(BUTTON_HEIGHT);
    wxRect r(FromDIP(1), (size.y - h) / 2, w, h);
    draw_metal(dc, r, this->pressed, this->hover, IsEnabled());
    if (this->icon.IsOk()) {
        int offset = this->pressed ? 1 : 0;
        dc.DrawBitmap(
            this->icon,
            r.x + (r.width - this->icon.GetWidth()) / 2 + offset,
            r.y + (r.height - this->icon.GetHeight()) / 2 + offset,
            true);
    }
}

void SpekMetalButton::on_left_down(wxMouseEvent&)
{
    if (!HasCapture()) {
        CaptureMouse();
    }
    this->pressed = true;
    Refresh();
}

void SpekMetalButton::on_left_up(wxMouseEvent& event)
{
    if (HasCapture()) {
        ReleaseMouse();
    }
    bool click = this->pressed && GetClientRect().Contains(event.GetPosition());
    this->pressed = false;
    Refresh();
    if (click && this->on_click) {
        this->on_click();
    }
}

void SpekMetalButton::on_enter_leave(wxMouseEvent& event)
{
    this->hover = event.Entering();
    Refresh();
    event.Skip();
}

void SpekMetalButton::on_capture_lost(wxMouseCaptureLostEvent&)
{
    this->pressed = false;
    Refresh();
}

// SpekLcdSpin

SpekLcdSpin::SpekLcdSpin(
    wxWindow *parent, const wxString& label, const wxString& widest,
    const wxString& up_tip, const wxString& down_tip,
    std::function<void(bool up)> on_click) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
    label(label),
    widest(widest),
    up_tip(up_tip),
    down_tip(down_tip),
    on_click(on_click),
    pressed(PART_NONE),
    hover(PART_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    wxFont font(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE).Bold());
#ifdef OS_WIN
    // Consolas is narrower and easier to read than the default Courier New.
    if (wxFontEnumerator::IsValidFacename("Consolas")) {
        font = wxFont(wxFontInfo(9).FaceName("Consolas").Bold());
    }
#endif
    SetFont(font);
    Bind(wxEVT_PAINT, &SpekLcdSpin::on_paint, this);
    Bind(wxEVT_LEFT_DOWN, &SpekLcdSpin::on_left_down, this);
    Bind(wxEVT_LEFT_DCLICK, &SpekLcdSpin::on_left_down, this);
    Bind(wxEVT_LEFT_UP, &SpekLcdSpin::on_left_up, this);
    Bind(wxEVT_MOTION, &SpekLcdSpin::on_motion, this);
    Bind(wxEVT_LEAVE_WINDOW, &SpekLcdSpin::on_leave, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &SpekLcdSpin::on_capture_lost, this);
    update_tip();
}

void SpekLcdSpin::set_value(const wxString& value, const wxString& tip)
{
    if (value != this->value || tip != this->value_tip) {
        this->value = value;
        this->value_tip = tip;
        update_tip();
        Refresh();
    }
}

bool SpekLcdSpin::Enable(bool enable)
{
    bool result = wxControl::Enable(enable);
    Refresh();
    return result;
}

wxSize SpekLcdSpin::DoGetBestSize() const
{
    const wxSize text = GetTextExtent(this->widest);
    const int width =
        text.x + 2 * FromDIP(CELL_PADDING) + FromDIP(1) + FromDIP(TINY_WIDTH);
    return wxSize(width, FromDIP(STRIP_HEIGHT));
}

wxRect SpekLcdSpin::cell_rect() const
{
    const wxSize text = GetTextExtent(this->widest);
    const int h = FromDIP(CELL_HEIGHT);
    const int w = text.x + 2 * FromDIP(CELL_PADDING);
    return wxRect(0, (GetClientSize().y - h) / 2, w, h);
}

wxRect SpekLcdSpin::up_rect() const
{
    const wxRect cell = cell_rect();
    return wxRect(
        cell.GetRight() + 1 + FromDIP(1), cell.y, FromDIP(TINY_WIDTH), cell.height / 2);
}

wxRect SpekLcdSpin::down_rect() const
{
    const wxRect cell = cell_rect();
    const int top_height = cell.height / 2;
    return wxRect(
        cell.GetRight() + 1 + FromDIP(1), cell.y + top_height, FromDIP(TINY_WIDTH),
        cell.height - top_height);
}

SpekLcdSpin::Part SpekLcdSpin::hit_test(const wxPoint& point) const
{
    if (up_rect().Contains(point)) {
        return PART_UP;
    }
    if (down_rect().Contains(point)) {
        return PART_DOWN;
    }
    return PART_NONE;
}

void SpekLcdSpin::update_tip()
{
    wxString tip;
    if (this->hover == PART_UP) {
        tip = this->up_tip;
    } else if (this->hover == PART_DOWN) {
        tip = this->down_tip;
    } else {
        tip = this->value_tip.IsEmpty() ? this->label : this->value_tip;
    }
    if (tip != this->shown_tip) {
        this->shown_tip = tip;
        SetToolTip(tip);
    }
}

void SpekLcdSpin::on_paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    paint_background(dc, GetClientSize());
    const bool enabled = IsEnabled();

    // The readout.
    const wxRect cell = cell_rect();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(cell);
    draw_bevel(dc, cell, wxColour(0x08, 0x08, 0x0c), wxColour(0x6a, 0x6a, 0x82));
    dc.SetFont(GetFont());
    dc.SetTextForeground(enabled ? wxColour(0x18, 0xf0, 0x18) : wxColour(0x2f, 0x7a, 0x2f));
    const wxSize text = dc.GetTextExtent(this->value);
    dc.DrawText(this->value, cell.x + FromDIP(CELL_PADDING), cell.y + (cell.height - text.y) / 2);

    // The up and down buttons.
    const wxColour glyph = enabled ? metal_dark() : wxColour(0x30, 0x30, 0x40);
    const wxRect up = up_rect();
    const wxRect down = down_rect();
    draw_metal(dc, up, this->pressed == PART_UP, this->hover == PART_UP, enabled);
    draw_triangle(dc, up, true, glyph);
    draw_metal(dc, down, this->pressed == PART_DOWN, this->hover == PART_DOWN, enabled);
    draw_triangle(dc, down, false, glyph);
}

void SpekLcdSpin::on_left_down(wxMouseEvent& event)
{
    Part part = hit_test(event.GetPosition());
    if (part == PART_NONE) {
        return;
    }
    if (!HasCapture()) {
        CaptureMouse();
    }
    this->pressed = part;
    Refresh();
}

void SpekLcdSpin::on_left_up(wxMouseEvent& event)
{
    if (HasCapture()) {
        ReleaseMouse();
    }
    Part pressed = this->pressed;
    this->pressed = PART_NONE;
    Refresh();
    if (pressed != PART_NONE && pressed == hit_test(event.GetPosition()) && this->on_click) {
        this->on_click(pressed == PART_UP);
    }
}

void SpekLcdSpin::on_motion(wxMouseEvent& event)
{
    Part part = hit_test(event.GetPosition());
    if (part != this->hover) {
        this->hover = part;
        update_tip();
        Refresh();
    }
    event.Skip();
}

void SpekLcdSpin::on_leave(wxMouseEvent& event)
{
    this->hover = PART_NONE;
    update_tip();
    Refresh();
    event.Skip();
}

void SpekLcdSpin::on_capture_lost(wxMouseCaptureLostEvent&)
{
    this->pressed = PART_NONE;
    Refresh();
}
