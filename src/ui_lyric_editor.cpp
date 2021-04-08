#include "stdafx.h"

#pragma warning(push, 0)
#include "resource.h"
#include "foobar2000/helpers/atl-misc.h"
#pragma warning(pop)

#include "logging.h"
#include "parsers.h"
#include "sources/localfiles.h"
#include "ui_lyric_editor.h"
#include "winstr_util.h"

// TODO: Add a "seek to the time of the timestamp on the selected line" button

class LyricEditor : public CDialogImpl<LyricEditor>, private play_callback_impl_base
{
public:
    // Dialog resource ID
    enum { IDD = IDD_LYRIC_EDIT };

    LyricEditor(const std::string& text, metadb_handle_ptr track_handle);

    BEGIN_MSG_MAP_EX(LyricEditor)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)
        COMMAND_HANDLER_EX(IDC_LYRIC_TEXT, EN_CHANGE, OnEditChange)
        COMMAND_HANDLER_EX(IDC_LYRIC_EDIT_BACK5, BN_CLICKED, OnBackwardSeek)
        COMMAND_HANDLER_EX(IDC_LYRIC_EDIT_FWD5, BN_CLICKED, OnForwardSeek)
        COMMAND_HANDLER_EX(IDC_LYRIC_EDIT_PLAY, BN_CLICKED, OnPlaybackToggle)
        COMMAND_HANDLER_EX(IDC_LYRIC_EDIT_SYNC, BN_CLICKED, OnLineSync)
        COMMAND_HANDLER_EX(IDC_LYRIC_EDIT_RESET, BN_CLICKED, OnEditReset)
        COMMAND_HANDLER_EX(ID_LYRIC_EDIT_CANCEL, BN_CLICKED, OnCancel)
        COMMAND_HANDLER_EX(ID_LYRIC_EDIT_APPLY, BN_CLICKED, OnApply)
        COMMAND_HANDLER_EX(ID_LYRIC_EDIT_OK, BN_CLICKED, OnOK)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow parent, LPARAM clientData);
    void OnClose();
    void OnEditChange(UINT, int, CWindow);
    void OnBackwardSeek(UINT btn_id, int notify_code, CWindow btn);
    void OnForwardSeek(UINT btn_id, int notify_code, CWindow btn);
    void OnPlaybackToggle(UINT btn_id, int notify_code, CWindow btn);
    void OnLineSync(UINT btn_id, int notify_code, CWindow btn);
    void OnEditReset(UINT btn_id, int notify_code, CWindow btn);
    void OnCancel(UINT btn_id, int notify_code, CWindow btn);
    void OnApply(UINT btn_id, int notify_code, CWindow btn);
    void OnOK(UINT btn_id, int notify_code, CWindow btn);

    void on_playback_new_track(metadb_handle_ptr track) override;
    void on_playback_stop(play_control::t_stop_reason reason) override;
    void on_playback_pause(bool state) override;
    void on_playback_seek(double new_time) override;
    void on_playback_time(double new_time) override;

    void update_play_button();
    void update_time_text(double time);
    bool HasContentChanged(size_t* new_length);
    void SaveLyricEdits();

    metadb_handle_ptr m_track_handle;
    TCHAR* m_input_text;
    size_t m_input_text_length;
};

LyricEditor::LyricEditor(const std::string& text, metadb_handle_ptr track_handle) :
    m_track_handle(track_handle),
    m_input_text(nullptr),
    m_input_text_length(0)
{
    m_input_text_length = string_to_tchar(text, m_input_text) - 1; // -1 to not count the null-terminator
}

LyricEditor::~LyricEditor()
{
    if(m_input_text != nullptr)
    {
        delete[] m_input_text;
    }
}

BOOL LyricEditor::OnInitDialog(CWindow /*parent*/, LPARAM /*clientData*/)
{
    TRACK_CALL_TEXT("Editor window initialisation");
    if(m_input_text)
    {
        SetDlgItemText(IDC_LYRIC_TEXT, m_input_text);

        int select_start = 0;
        int select_end = 0;
        TCHAR* line_buffer = nullptr;
        int line_buffer_len = 0;
        int line_count = (int)SendDlgItemMessage(IDC_LYRIC_TEXT, EM_GETLINECOUNT, 0, 0);
        for(int i=0; i<line_count; i++)
        {
            LRESULT line_start = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINEINDEX, i, 0);
            LRESULT line_length = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINELENGTH, line_start, 0);
            if(line_length <= 0) continue;

            if(line_buffer_len < line_length)
            {
                line_buffer = (TCHAR*)realloc(line_buffer, (line_length+1)*sizeof(TCHAR));
                line_buffer_len = line_length;
            }

            line_buffer[0] = line_buffer_len; // EM_GETLINE reads the first word as the number of characters in the buffer
            LRESULT chars_copied = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_GETLINE, i, (LPARAM)line_buffer);
            std::string linestr = tchar_to_string(line_buffer, chars_copied);
            if(!parsers::lrc::is_tag_line(linestr))
            {
                select_start = line_start;
                select_end = line_start + line_length;
                break;
            }
        }
        free(line_buffer);

        SendDlgItemMessage(IDC_LYRIC_TEXT, EM_SETSEL, select_start, select_end);
        SendDlgItemMessage(IDC_LYRIC_TEXT, EM_SCROLLCARET , 0, 0);
    }

    service_ptr_t<playback_control> playback = playback_control::get();
    update_time_text(playback->playback_get_position());
    update_play_button();

    ShowWindow(SW_SHOW);
    return TRUE;
}

void LyricEditor::OnClose()
{
    DestroyWindow();
}

void LyricEditor::OnEditChange(UINT, int, CWindow)
{
    size_t new_length = 0;
    bool changed = HasContentChanged(&new_length);
    bool is_empty = (new_length == 0);

    CWindow apply_btn = GetDlgItem(ID_LYRIC_EDIT_APPLY);
    assert(apply_btn != nullptr);
    apply_btn.EnableWindow(changed && !is_empty);
}

void LyricEditor::OnBackwardSeek(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    service_ptr_t<playback_control> playback = playback_control::get();
    playback->playback_seek_delta(-5.0);
}

void LyricEditor::OnForwardSeek(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    service_ptr_t<playback_control> playback = playback_control::get();
    playback->playback_seek_delta(5.0);
}

void LyricEditor::OnPlaybackToggle(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    service_ptr_t<playback_control> playback = playback_control::get();
    playback->play_or_pause();
}

void LyricEditor::OnLineSync(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    TRACK_CALL_TEXT("Editor line synchronisation");
    // NOTE: Passing -1 will give the line index of the line containing the start of the current selection
    LRESULT curr_line_index = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINEFROMCHAR, -1, 0);
    LRESULT curr_line_start = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINEINDEX, curr_line_index, 0);
    LRESULT curr_line_length = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINELENGTH, curr_line_start, 0);

    assert((curr_line_length >= 0) && (curr_line_length <= INT_MAX));
    int replace_start = (int)curr_line_start;
    int replace_end = replace_start;

    assert(curr_line_length >= 0);
    const int line_buffer_len = 32;
    TCHAR line_buffer[line_buffer_len+1] = {}; // +1 for the null-terminator
    line_buffer[0] = line_buffer_len; // EM_GETLINE reads the first word as the number of characters in the buffer
    LRESULT chars_copied = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_GETLINE, curr_line_index, (LPARAM)line_buffer);
    assert(chars_copied <= line_buffer_len+1);
    if(line_buffer[0] == _T('['))
    {
        for(int i=0; i<line_buffer_len; i++)
        {
            if(line_buffer[i] == _T('\0')) break;
            if(line_buffer[i] == _T(']'))
            {
                double time = 0;
                std::string tag = tchar_to_string(&line_buffer[0], i+1); // +1 so that the length covers this current (']') character
                bool success6 = parsers::lrc::try_parse_6digit_timestamp(tag, time);
                bool success7 = parsers::lrc::try_parse_7digit_timestamp(tag, time);
                if(success6 || success7)
                {
                    replace_end = replace_start + i + 1;
                }
                break;
            }
        }
    }

    service_ptr_t<playback_control> playback = playback_control::get();
    double timestamp = playback->playback_get_position();
    char timestamp_str[11] = {};
    parsers::lrc::print_6digit_timestamp(timestamp, timestamp_str);

    TCHAR* new_buffer = nullptr;
    string_to_tchar(timestamp_str, 0, sizeof(timestamp_str), new_buffer);
    SendDlgItemMessage(IDC_LYRIC_TEXT, EM_SETSEL, replace_start, replace_end);
    SendDlgItemMessage(IDC_LYRIC_TEXT, EM_REPLACESEL, TRUE, (LPARAM)new_buffer);
    delete[] new_buffer;

    LRESULT next_line_start = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINEINDEX, curr_line_index+1, 0); 
    LRESULT next_line_length = SendDlgItemMessage(IDC_LYRIC_TEXT, EM_LINELENGTH, next_line_start, 0);
    LRESULT next_line_end = next_line_start + next_line_length;

    SendDlgItemMessage(IDC_LYRIC_TEXT, EM_SETSEL, next_line_start, next_line_end);
    SendDlgItemMessage(IDC_LYRIC_TEXT, EM_SCROLLCARET , 0, 0); // Scroll the selected line into view

    // We've (almost) certainly changed the contents so enable the "Apply" button
    CWindow apply_btn = GetDlgItem(ID_LYRIC_EDIT_APPLY);
    assert(apply_btn != nullptr);
    apply_btn.EnableWindow(true);
}

void LyricEditor::OnEditReset(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    if(m_input_text)
    {
        SetDlgItemText(IDC_LYRIC_TEXT, m_input_text);
    }
    else
    {
        SetDlgItemText(IDC_LYRIC_TEXT, _T(""));
    }
}

void LyricEditor::OnCancel(UINT /*btn_id*/, int /*notification_type*/, CWindow /*btn*/)
{
    DestroyWindow();
}

void LyricEditor::OnApply(UINT /*btn_id*/, int /*notify_code*/, CWindow /*btn*/)
{
    assert(HasContentChanged(nullptr));
    SaveLyricEdits();
}

void LyricEditor::OnOK(UINT /*btn_id*/, int /*notify_code*/, CWindow /*btn*/)
{
    if(HasContentChanged(nullptr))
    {
        SaveLyricEdits();
    }
    DestroyWindow();
}

void LyricEditor::on_playback_new_track(metadb_handle_ptr /*track*/)
{
    update_play_button();
    update_time_text(0);
}

void LyricEditor::on_playback_stop(play_control::t_stop_reason /*reason*/)
{
    update_play_button();
    update_time_text(0);
}

void LyricEditor::on_playback_pause(bool state)
{
    update_play_button();
}

void LyricEditor::on_playback_seek(double new_time)
{
    update_time_text(new_time);
}

void LyricEditor::on_playback_time(double new_time)
{
    update_time_text(new_time);
}

void LyricEditor::update_time_text(double new_time)
{
    // NOTE: The time text doesn't exist when this class first gets created, so we should handle
    //       that possibility in general (because we could create it just before a seek/time event).
    CWindow time_text = GetDlgItem(IDC_LYRIC_EDIT_TIME);
    if(time_text == nullptr) return;

    int total_sec = static_cast<int>(new_time);
    int time_min = total_sec / 60;
    int time_sec = total_sec % 60;

    TCHAR buffer[64];
    constexpr size_t buffer_len = sizeof(buffer)/sizeof(buffer[0]);
    _sntprintf_s(buffer, buffer_len, _T("Playback time: %02d:%02d"), time_min, time_sec);

    time_text.SetWindowText(buffer);
}

void LyricEditor::update_play_button()
{
    CWindow btn = GetDlgItem(IDC_LYRIC_EDIT_PLAY);
    if(btn == nullptr) return;

    service_ptr_t<playback_control> playback = playback_control::get();
    const TCHAR* newText = playback->is_playing() && playback->is_paused() ? TEXT("Pause") : TEXT("Play");
    btn.SetWindowText(newText);
}

bool LyricEditor::HasContentChanged(size_t* new_length)
{
    LRESULT lyric_length_result = SendDlgItemMessage(IDC_LYRIC_TEXT, WM_GETTEXTLENGTH, 0, 0);
    assert(lyric_length_result >= 0);

    size_t lyric_length = static_cast<size_t>(lyric_length_result);
    if(new_length)
    {
        *new_length = lyric_length;
    }

    if(lyric_length != m_input_text_length)
    {
        return true;
    }

    if(lyric_length > 0)
    {
        TCHAR* lyric_buffer = new TCHAR[lyric_length+1]; // +1 for the null-terminator
        UINT chars_copied = GetDlgItemText(IDC_LYRIC_TEXT, lyric_buffer, lyric_length+1);
        if(chars_copied != lyric_length)
        {
            LOG_WARN("Dialog character count mismatch. Expected %u, got %u", lyric_length, chars_copied);
        }

        bool changed = (_tcscmp(lyric_buffer, m_input_text) != 0);
        delete[] lyric_buffer;

        return changed;
    }
    else
    {
        return false; // The new length is zero and it matches the old length
    }
}

void LyricEditor::SaveLyricEdits()
{
    TRACK_CALL_TEXT("Editor save");
    LRESULT lyric_length = SendDlgItemMessage(IDC_LYRIC_TEXT, WM_GETTEXTLENGTH, 0, 0);
    if(lyric_length <= 0) return;

    TCHAR* lyric_buffer = new TCHAR[lyric_length+1]; // +1 for the null-terminator
    UINT chars_copied = GetDlgItemText(IDC_LYRIC_TEXT, lyric_buffer, lyric_length+1);
    if(chars_copied != lyric_length)
    {
        LOG_WARN("Dialog character count mismatch while saving. Expected %u, got %u", lyric_length, chars_copied);
    }

    // Update m_input_text (and length) so that HasContentChanged() will return the correct value after the same
    if(m_input_text != nullptr)
    {
        delete[] m_input_text;
    }
    m_input_text = lyric_buffer;
    m_input_text_length = chars_copied;

    std::string lyrics = tchar_to_string(lyric_buffer, chars_copied);
    LyricDataRaw data_raw = {{}, lyrics}; // TODO: Should we store/fill in the original source of the lyrics?

    LyricData data = parsers::lrc::parse(data_raw);
    if(data.IsTimestamped())
    {
        lyrics = parsers::lrc::shrink_text(data);

        abort_callback_dummy noAbort;
        sources::localfiles::SaveLyrics(m_track_handle, true, lyrics, noAbort);
    }
    else
    {
        abort_callback_dummy noAbort;
        sources::localfiles::SaveLyrics(m_track_handle, false, lyrics, noAbort);
    }

    // We know that if we ran HasContentChanged() now, it would return false.
    // So short-circuit it and just disable the apply button directly
    CWindow apply_btn = GetDlgItem(ID_LYRIC_EDIT_APPLY);
    assert(apply_btn != nullptr);
    apply_btn.EnableWindow(FALSE);
}

void SpawnLyricEditor(const std::string& lyric_text, metadb_handle_ptr lyric_to_edit_track)
{
    TRACK_CALL_TEXT("Editor spawn");
    try
    {
        new CWindowAutoLifetime<ImplementModelessTracking<LyricEditor>>(core_api::get_main_window(), lyric_text, lyric_to_edit_track);
    }
    catch(const std::exception& e)
    {
        popup_message::g_complain("Failed to create lyric editor dialog", e);
    }
}

