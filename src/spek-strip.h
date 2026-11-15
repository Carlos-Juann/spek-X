#pragma once

#include <functional>

#include <wx/wx.h>

// Controls for the Winamp-like strip at the top of the window.
//
// The strip background is a vertical gradient, so every control is as tall as
// the strip and paints its own slice of it. This way they line up without any
// visible gap. None of the controls takes the keyboard focus, so the keys
// handled by the spectrogram keep working after a click.

// The dark metal background of the strip.
class SpekStripPanel : public wxPanel
{
public:
    explicit SpekStripPanel(wxWindow *parent);

private:
    void on_paint(wxPaintEvent& event);
};

// A raised metal button with an icon.
class SpekMetalButton : public wxControl
{
public:
    SpekMetalButton(
        wxWindow *parent, const wxBitmap& icon, const wxString& tip,
        std::function<void()> on_click);
    bool AcceptsFocus() const override { return false; }

protected:
    wxSize DoGetBestSize() const override;

private:
    void on_paint(wxPaintEvent& event);
    void on_left_down(wxMouseEvent& event);
    void on_left_up(wxMouseEvent& event);
    void on_enter_leave(wxMouseEvent& event);
    void on_capture_lost(wxMouseCaptureLostEvent& event);

    wxBitmap icon;
    std::function<void()> on_click;
    bool pressed;
    bool hover;
};

// A green LCD readout followed by two tiny buttons, up and down.
class SpekLcdSpin : public wxControl
{
public:
    // `widest` is the longest text the readout will ever show, it sets the
    // width of the control. `on_click` receives true for the up button.
    SpekLcdSpin(
        wxWindow *parent, const wxString& label, const wxString& widest,
        const wxString& up_tip, const wxString& down_tip,
        std::function<void(bool up)> on_click);
    // `tip` is shown over the readout, `label` is used if it is empty.
    void set_value(const wxString& value, const wxString& tip = wxString());
    bool Enable(bool enable = true) override;
    bool AcceptsFocus() const override { return false; }

protected:
    wxSize DoGetBestSize() const override;

private:
    enum Part { PART_NONE, PART_UP, PART_DOWN };

    wxRect cell_rect() const;
    wxRect up_rect() const;
    wxRect down_rect() const;
    Part hit_test(const wxPoint& point) const;
    void update_tip();
    void on_paint(wxPaintEvent& event);
    void on_left_down(wxMouseEvent& event);
    void on_left_up(wxMouseEvent& event);
    void on_motion(wxMouseEvent& event);
    void on_leave(wxMouseEvent& event);
    void on_capture_lost(wxMouseCaptureLostEvent& event);

    wxString label;
    wxString widest;
    wxString value;
    wxString value_tip;
    wxString up_tip;
    wxString down_tip;
    wxString shown_tip;
    std::function<void(bool up)> on_click;
    Part pressed;
    Part hover;
};
