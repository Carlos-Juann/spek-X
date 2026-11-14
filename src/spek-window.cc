#include <wx/aboutdlg.h>
#include <wx/artprov.h>
#include <wx/dnd.h>
#include <wx/filename.h>
#include <wx/protocol/http.h>
#include <wx/sstream.h>
#include <wx/wrapsizer.h>

// WX on WIN doesn't like it when pthread.h is included first.
#include <pthread.h>

#include <spek-utils.h>

#include "spek-artwork.h"
#include "spek-preferences-dialog.h"
#include "spek-preferences.h"
#include "spek-spectrogram.h"

#include "spek-window.h"

DECLARE_EVENT_TYPE(SPEK_NOTIFY_EVENT, -1)
DEFINE_EVENT_TYPE(SPEK_NOTIFY_EVENT)

BEGIN_EVENT_TABLE(SpekWindow, wxFrame)
    EVT_MENU(wxID_OPEN, SpekWindow::on_open)
    EVT_MENU(wxID_SAVE, SpekWindow::on_save)
    EVT_MENU(wxID_EXIT, SpekWindow::on_exit)
    EVT_MENU(wxID_PREFERENCES, SpekWindow::on_preferences)
    EVT_MENU(wxID_HELP, SpekWindow::on_help)
    EVT_MENU(wxID_ABOUT, SpekWindow::on_about)
    EVT_COMMAND(-1, SPEK_NOTIFY_EVENT, SpekWindow::on_notify)
END_EVENT_TABLE()

#ifdef SPEK_CHECK_VERSION
// Forward declarations.
static void * check_version(void *);
#endif

class SpekDropTarget : public wxFileDropTarget
{
public:
    SpekDropTarget(SpekWindow *window) : wxFileDropTarget(), window(window) {}

protected:
    virtual bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames){
        if (filenames.GetCount() == 1) {
            window->open(filenames[0]);
            return true;
        }
        return false;
    }

private:
    SpekWindow *window;
};

SpekWindow::SpekWindow(int width, int height, const wxString& path, const wxString& pngpath) :
    wxFrame(NULL, -1, wxEmptyString, wxDefaultPosition, wxSize(width, height)), path(path), pngpath(pngpath)
{
    this->description = _("Spek - Acoustic Spectrum Analyser");
    SetTitle(this->description);

#ifndef OS_OSX
    SetIcons(wxArtProvider::GetIconBundle(ART_SPEK, wxART_FRAME_ICON));
#endif

    wxMenuBar *menu = new wxMenuBar();

    wxMenu *menu_file = new wxMenu();
    wxMenuItem *menu_file_open = new wxMenuItem(menu_file, wxID_OPEN);
    menu_file->Append(menu_file_open);
    wxMenuItem *menu_file_save = new wxMenuItem(menu_file, wxID_SAVE);
    menu_file->Append(menu_file_save);
    menu_file->AppendSeparator();
    menu_file->Append(wxID_EXIT);
    menu->Append(menu_file, _("&File"));

    wxMenu *menu_edit = new wxMenu();
    wxMenuItem *menu_edit_prefs = new wxMenuItem(menu_edit, wxID_PREFERENCES);
    menu_edit_prefs->SetItemLabel(menu_edit_prefs->GetItemLabelText() + "\tCtrl-E");
    menu_edit->Append(menu_edit_prefs);
    menu->Append(menu_edit, _("&Edit"));

    wxMenu *menu_help = new wxMenu();
    wxMenuItem *menu_help_contents = new wxMenuItem(
        menu_help, wxID_HELP, wxString(_("&Help")) + "\tF1");
    menu_help->Append(menu_help_contents);
    wxMenuItem *menu_help_about = new wxMenuItem(menu_help, wxID_ABOUT);
    menu_help_about->SetItemLabel(menu_help_about->GetItemLabelText() + "\tShift-F1");
    menu_help->Append(menu_help_about);
    menu->Append(menu_help, _("&Help"));

    SetMenuBar(menu);

    wxToolBar *toolbar = CreateToolBar();
    toolbar->AddTool(
        wxID_OPEN,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_OPEN, wxART_TOOLBAR),
        menu_file_open->GetItemLabelText()
    );
    toolbar->AddTool(
        wxID_SAVE,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_SAVE, wxART_TOOLBAR),
        menu_file_save->GetItemLabelText()
    );
    toolbar->AddStretchableSpace();
    toolbar->AddTool(
        wxID_HELP,
        wxEmptyString,
        wxArtProvider::GetBitmap(ART_HELP, wxART_TOOLBAR),
        _("Help")
    );
    toolbar->Realize();

    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    // wxInfoBar is too limited, construct a custom one.
    wxPanel *info_bar = new wxPanel(this);
    info_bar->Hide();
    info_bar->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
    info_bar->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
    wxSizer *info_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *label = new wxStaticText(
        info_bar, -1, _("A new version of Spek is available, click to download."));
    label->SetCursor(*new wxCursor(wxCURSOR_HAND));
    label->Connect(wxEVT_LEFT_DOWN, wxCommandEventHandler(SpekWindow::on_visit));
    // This second Connect() handles clicks on the border
    info_bar->Connect(wxEVT_LEFT_DOWN, wxCommandEventHandler(SpekWindow::on_visit));
    info_sizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
    wxBitmapButton *button = new wxBitmapButton(
        info_bar, -1, wxArtProvider::GetBitmap(ART_CLOSE, wxART_BUTTON),
        wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SpekWindow::on_close));
    info_sizer->Add(button, 0, wxALIGN_CENTER_VERTICAL);
    info_bar->SetSizer(info_sizer);
    sizer->Add(info_bar, 0, wxEXPAND);

    // Buttons that do the same as the keys handled by the spectrogram.
    for (int i = 0; i < 2; ++i) {
        this->channel_buttons[i] = NULL;
        this->stream_buttons[i] = NULL;
    }
    wxPanel *controls = new wxPanel(this);
    wxSizer *controls_sizer = new wxWrapSizer(wxHORIZONTAL);
    this->add_control(
        controls, controls_sizer, _("Channel"),
        "<", _("Previous channel") + " (Shift+C)", 'C',
        ">", _("Next channel") + " (C)", 'c',
        this->channel_buttons);
    this->add_control(
        controls, controls_sizer, _("Stream"),
        "<", _("Previous stream") + " (Shift+S)", 'S',
        ">", _("Next stream") + " (S)", 's',
        this->stream_buttons);
    this->add_control(
        controls, controls_sizer, _("Window"),
        "<", _("Previous window function") + " (Shift+F)", 'F',
        ">", _("Next window function") + " (F)", 'f',
        NULL);
    this->add_control(
        controls, controls_sizer, _("Palette"),
        "<", _("Previous palette") + " (Shift+P)", 'P',
        ">", _("Next palette") + " (P)", 'p',
        NULL);
    this->add_control(
        controls, controls_sizer, _("FFT size"),
        "-", _("Smaller FFT window") + " (Shift+W)", 'W',
        "+", _("Larger FFT window") + " (W)", 'w',
        NULL);
    this->add_control(
        controls, controls_sizer, _("Upper dB"),
        "-", _("Lower the upper limit of the range") + " (Shift+U)", 'U',
        "+", _("Raise the upper limit of the range") + " (U)", 'u',
        NULL);
    this->add_control(
        controls, controls_sizer, _("Lower dB"),
        "-", _("Lower the lower limit of the range") + " (Shift+L)", 'L',
        "+", _("Raise the lower limit of the range") + " (L)", 'l',
        NULL);
    controls->SetSizer(controls_sizer);
    sizer->Add(controls, 0, wxEXPAND);

    this->spectrogram = new SpekSpectrogram(this);
    sizer->Add(this->spectrogram, 1, wxEXPAND);
    this->update_controls();

    this->cur_dir = wxGetHomeDir();

    if (!path.IsEmpty()) {
        open(path);
    }

    SetDropTarget(new SpekDropTarget(this));

    SetSizer(sizer);

#ifdef SPEK_CHECK_VERSION
    pthread_t thread;
    pthread_create(&thread, NULL, &check_version, this);
#endif
}

void SpekWindow::open(const wxString& path)
{
    wxFileName file_name(path);
    if (file_name.FileExists()) {
        this->path = path;
        wxString full_name = file_name.GetFullName();
        // TRANSLATORS: window title, %s is replaced with the file name
        wxString title = wxString::Format(_("Spek - %s"), full_name.c_str());
        SetTitle(title);

        this->spectrogram->open(path, this->pngpath);
        this->update_controls();
    }
}

// Adds "label [back] [forward]" to `sizer`. Pressing a button is the same as
// pressing the key, see SpekSpectrogram::handle_key(). If `buttons` is not NULL,
// the two buttons are stored there so they can be enabled/disabled later.
void SpekWindow::add_control(
    wxWindow *parent, wxSizer *sizer, const wxString& label,
    const wxString& back_text, const wxString& back_tip, int back_key,
    const wxString& fwd_text, const wxString& fwd_tip, int fwd_key,
    wxButton **buttons)
{
    wxSizer *group = new wxBoxSizer(wxHORIZONTAL);
    group->Add(
        new wxStaticText(parent, wxID_ANY, label),
        0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    const wxString texts[2] = {back_text, fwd_text};
    const wxString tips[2] = {back_tip, fwd_tip};
    const int keys[2] = {back_key, fwd_key};
    for (int i = 0; i < 2; ++i) {
        wxButton *button = new wxButton(
            parent, wxID_ANY, texts[i], wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        button->SetToolTip(tips[i]);
        int key = keys[i];
        button->Bind(wxEVT_BUTTON, [this, key](wxCommandEvent&) {
            this->spectrogram->handle_key(key);
            this->update_controls();
            // Give the focus back so the keyboard keeps working.
            this->spectrogram->SetFocus();
        });
        group->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        if (buttons) {
            buttons[i] = button;
        }
    }

    sizer->Add(group, 0, wxALL, 4);
}

// Channel and stream buttons do nothing unless the file has several of them.
void SpekWindow::update_controls()
{
    bool has_channels = this->spectrogram->get_channels() > 1;
    bool has_streams = this->spectrogram->get_streams() > 1;
    for (int i = 0; i < 2; ++i) {
        if (this->channel_buttons[i]) {
            this->channel_buttons[i]->Enable(has_channels);
        }
        if (this->stream_buttons[i]) {
            this->stream_buttons[i]->Enable(has_streams);
        }
    }
}

// TODO: s/audio/media/
static const char *audio_extensions[] = {
    "3gp",
    "aac",
    "ac3",
    "aif",
    "aifc",
    "aiff",
    "amr",
    "awb",
    "ape",
    "au",
    "dts",
    "flac",
    "flv",
    "gsm",
    "m4a",
    "m4p",
    "m4r",
    "mka",
    "mp3",
    "mp4",
    "mp+",
    "mpc",
    "mpp",
    "oga",
    "ogg",
    "opus",
    "ra",
    "ram",
    "snd",
    "wav",
    "wma",
    "wv",
    NULL
};

void SpekWindow::on_open(wxCommandEvent&)
{
    static wxString filters = wxEmptyString;
    static int filter_index = 1;

    if (filters.IsEmpty()) {
        filters.Alloc(1024);
        filters += _("All files");
        filters += "|*.*|";
        filters += _("Audio files");
        filters += "|";
        for (int i = 0; audio_extensions[i]; ++i) {
            if (i) {
                filters += ";";
            }
            filters += "*.";
            filters += wxString::FromAscii(audio_extensions[i]);
        }
        filters.Shrink();
    }

    wxFileDialog *dlg = new wxFileDialog(
        this,
        _("Open File"),
        this->cur_dir,
        wxEmptyString,
        filters,
        wxFD_OPEN
    );
    dlg->SetFilterIndex(filter_index);

    if (dlg->ShowModal() == wxID_OK) {
        this->cur_dir = dlg->GetDirectory();
        filter_index = dlg->GetFilterIndex();
        open(dlg->GetPath());
    }

    dlg->Destroy();
}

void SpekWindow::on_save(wxCommandEvent&)
{
    static wxString filters = wxEmptyString;

    if (filters.IsEmpty()) {
        filters = _("PNG images");
        filters += "|*.png";
    }

    wxFileDialog *dlg = new wxFileDialog(
        this,
        _("Save Spectrogram"),
        this->cur_dir,
        wxEmptyString,
        filters,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT
    );

    // Suggested name is <file_name>.png
    wxString name = _("Untitled");
    if (!this->path.IsEmpty()) {
        wxFileName file_name(this->path);
        name = file_name.GetFullName();
    }
    name += ".png";
    dlg->SetFilename(name);

    if (dlg->ShowModal() == wxID_OK) {
        this->cur_dir = dlg->GetDirectory();
        this->spectrogram->save(dlg->GetPath());
    }

    dlg->Destroy();
}

void SpekWindow::on_exit(wxCommandEvent&)
{
    Close(true);
}

void SpekWindow::on_preferences(wxCommandEvent&)
{
    SpekPreferencesDialog dlg(this);
    dlg.ShowModal();
}

void SpekWindow::on_help(wxCommandEvent&)
{
    wxLaunchDefaultBrowser(
        wxString::Format("https://github.com/MikeWang000000/spek-X/blob/v%s/MANUAL.md", PACKAGE_VERSION)
    );
}

void SpekWindow::on_about(wxCommandEvent&)
{
    wxAboutDialogInfo info;
    info.AddDeveloper("Alexander Kojevnikov");
    info.AddDeveloper("Andreas Cadhalpun");
    info.AddDeveloper("Colin Watson");
    info.AddDeveloper("Daniel Hams");
    info.AddDeveloper("Elias Ojala");
    info.AddDeveloper("Fabian Deutsch");
    info.AddDeveloper("Guillaume Fourrier");
    info.AddDeveloper("Jakov Smolic");
    info.AddDeveloper("Jonathan Gonzalez V");
    info.AddDeveloper("Matteo Bini");
    info.AddDeveloper("Mike Wang");
    info.AddDeveloper("Simon Ruderich");
    info.AddDeveloper("Stefan Kost");
    info.AddDeveloper("Thibault North");
    info.AddDeveloper("Wyatt J. Brown");
    info.AddArtist("Olga Vasylevska");
    // TRANSLATORS: Add your name here
    wxString translator = _("translator-credits");
    if (translator != "translator-credits") {
        info.AddTranslator(translator);
    }
    info.SetName("Spek-X");
    info.SetVersion(PACKAGE_VERSION);
    info.SetCopyright(_("Copyright (c) 2010-2013 Alexander Kojevnikov and contributors"));
    info.SetDescription(this->description);
#ifdef OS_UNIX
    info.SetWebSite(
        wxString::Format("https://github.com/MikeWang000000/spek-X/blob/v%s/MANUAL.md", PACKAGE_VERSION),
        _("Spek Website")
    );
    info.SetIcon(wxArtProvider::GetIcon("spek", wxART_OTHER, wxSize(128, 128)));
#endif
    wxAboutBox(info);
}

void SpekWindow::on_notify(wxCommandEvent&)
{
    this->GetSizer()->Show((size_t)0);
    this->Layout();
}

void SpekWindow::on_visit(wxCommandEvent&)
{
    wxLaunchDefaultBrowser("https://github.com/MikeWang000000/spek-X/releases");
}

void SpekWindow::on_close(wxCommandEvent& event)
{
    wxWindow *self = ((wxWindow *)event.GetEventObject())->GetGrandParent();
    self->GetSizer()->Hide((size_t)0);
    self->Layout();
}

#ifdef SPEK_CHECK_VERSION
static void * check_version(void *p)
{
    // Does the user want to check for updates?
    SpekPreferences& prefs = SpekPreferences::get();
    if (!prefs.get_check_update()) {
        return NULL;
    }

    // Calculate the number of days since 0001-01-01, borrowed from GLib.
    wxDateTime now = wxDateTime::Now();
    int year = now.GetYear() - 1;
    int days = year * 365;
    days += (year >>= 2); // divide by 4 and add
    days -= (year /= 25); // divides original # years by 100
    days += year >> 2; // divides by 4, which divides original by 400
    days += now.GetDayOfYear();

    // When was the last update?
    int diff = days - prefs.get_last_update();
    if (diff < 7) {
        return NULL;
    }

    // Get the version number.
    wxString version;
    wxHTTP http;
    if (http.Connect("morten.with.de")) {
        wxInputStream *stream = http.GetInputStream("/spek");
        if (stream) {
            wxStringOutputStream out(&version);
            stream->Read(out);
            version.Trim();
            delete stream;
        }
    }

    if (version.IsEmpty()) {
        return NULL;
    }

    if (1 == spek_vercmp(version.mb_str(wxConvLibc), PACKAGE_VERSION)) {
        SpekWindow *self = (SpekWindow *)p;
        wxCommandEvent event(SPEK_NOTIFY_EVENT, -1);
        event.SetEventObject(self);
        wxPostEvent(self, event);
    }

    // Update the preferences.
    prefs.set_check_update(true);
    prefs.set_last_update(days);
    return NULL;
}
#endif
