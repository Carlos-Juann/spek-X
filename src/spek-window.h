#pragma once

#include <wx/wx.h>

class SpekSpectrogram;

class SpekWindow : public wxFrame
{
public:
    SpekWindow(int width, int height, const wxString& path, const wxString& pngpath);
    void open(const wxString& path);

private:
    void on_open(wxCommandEvent& event);
    void on_save(wxCommandEvent& event);
    void on_exit(wxCommandEvent& event);
    void on_preferences(wxCommandEvent& event);
    void on_help(wxCommandEvent& event);
    void on_about(wxCommandEvent& event);
    void on_notify(wxCommandEvent& event);
    void on_visit(wxCommandEvent& event);
    void on_close(wxCommandEvent& event);
    void add_control(
        wxWindow *parent, wxSizer *sizer, const wxString& label,
        const wxString& back_text, const wxString& back_tip, int back_key,
        const wxString& fwd_text, const wxString& fwd_tip, int fwd_key,
        wxButton **buttons);
    void update_controls();

    SpekSpectrogram *spectrogram;
    wxButton *channel_buttons[2];
    wxButton *stream_buttons[2];
    wxString path;
    wxString pngpath;
    wxString cur_dir;
    wxString description;

    DECLARE_EVENT_TABLE()
};
