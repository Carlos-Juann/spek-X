#pragma once

#include <functional>
#include <memory>

#include <wx/wx.h>

#include "spek-palette.h"
#include "spek-pipeline.h"

class Audio;
class FFT;
class SpekHaveSampleEvent;
struct spek_pipeline;

class SpekSpectrogram : public wxWindow
{
public:
    SpekSpectrogram(wxFrame *parent);
    ~SpekSpectrogram();
    void open(const wxString& path, const wxString& pngpath);
    void save(const wxString& path);
    bool handle_key(int key_code);
    int get_streams() const { return streams; }
    int get_stream() const { return stream; }
    int get_channels() const { return channels; }
    int get_channel() const { return channel; }
    enum window_function get_window_function() const { return window_function; }
    enum palette get_palette() const { return palette; }
    int get_fft_bits() const { return fft_bits; }
    int get_urange() const { return urange; }
    int get_lrange() const { return lrange; }
    // Called after a key (or a button) changes any of the settings above.
    void set_on_change(std::function<void()> callback) { on_change = callback; }

private:
    void on_char(wxKeyEvent& evt);
    void on_paint(wxPaintEvent& evt);
    void on_size(wxSizeEvent& evt);
    void on_have_sample(wxEvent& evt);
    void render(wxDC& dc);

    void start();
    void stop();

    void create_palette();

    static const int MIN_RANGE;
    static const int MAX_RANGE;
    static const int URANGE;
    static const int LRANGE;
    static const int FFT_BITS;
    static const int MIN_FFT_BITS;
    static const int MAX_FFT_BITS;
    static const int LPAD;
    static const int TPAD;
    static const int RPAD;
    static const int BPAD;
    static const int GAP;
    static const int RULER;

    std::unique_ptr<Audio> audio;
    std::unique_ptr<FFT> fft;
    spek_pipeline *pipeline;
    int streams;
    int stream;
    int channels;
    int channel;
    enum window_function window_function;
    wxString path;
    wxString pngpath;
    wxString desc;
    double duration;
    int sample_rate;
    enum palette palette;
    wxImage palette_image;
    wxImage image;
    int prev_width;
    int fft_bits;
    int urange;
    int lrange;
    std::function<void()> on_change;

    DECLARE_EVENT_TABLE()
};
