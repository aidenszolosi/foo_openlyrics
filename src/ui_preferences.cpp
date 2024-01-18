#include "stdafx.h"

#pragma warning(push, 0)
#include "resource.h"
#include "foobar2000/helpers/atl-misc.h"
#pragma warning(pop)

#include "config/config_auto.h"
#include "logging.h"
#include "lyric_data.h"
#include "preferences.h"
#include "sources/lyric_source.h"
#include "win32_util.h"

extern const GUID GUID_PREFERENCES_PAGE_ROOT = { 0x29e96cfa, 0xab67, 0x4793, { 0xa1, 0xc3, 0xef, 0xc3, 0xa, 0xbc, 0x8b, 0x74 } };

static const GUID GUID_CFG_SEARCH_ACTIVE_SOURCES_GENERATION = { 0x9046aa4a, 0x352e, 0x4467, { 0xbc, 0xd2, 0xc4, 0x19, 0x47, 0xd2, 0xbf, 0x24 } };
static const GUID GUID_CFG_SEARCH_ACTIVE_SOURCES = { 0x7d3c9b2c, 0xb87b, 0x4250, { 0x99, 0x56, 0x8d, 0xf5, 0x80, 0xc9, 0x2f, 0x39 } };
static const GUID GUID_CFG_SEARCH_TAGS = { 0xb7332708, 0xe70b, 0x4a6e, { 0xa4, 0xd, 0x14, 0x6d, 0xe3, 0x74, 0x56, 0x65 } };
static const GUID GUID_CFG_SEARCH_EXCLUDE_TRAILING_BRACKETS = { 0x2cbdf6c3, 0xdb8c, 0x43d4, { 0xb5, 0x40, 0x76, 0xc0, 0x4a, 0x39, 0xa7, 0xc7 } };
static const GUID GUID_CFG_SEARCH_MUSIXMATCH_TOKEN = { 0xb88a82a7, 0x746d, 0x44f3, { 0xb8, 0x34, 0x9b, 0x9b, 0xe2, 0x6f, 0x8, 0x4c } };

// NOTE: These were copied from the relevant lyric-source source file.
//       It should not be a problem because these GUIDs must never change anyway (since it would
//       break everybody's config), but probably worth noting that the information is duplicated.
static const GUID localfiles_src_guid = { 0x76d90970, 0x1c98, 0x4fe2, { 0x94, 0x4e, 0xac, 0xe4, 0x93, 0xf3, 0x8e, 0x85 } };
static const GUID musixmatch_src_guid = { 0xf94ba31a, 0x7b33, 0x49e4, { 0x81, 0x9b, 0x0, 0xc, 0x36, 0x44, 0x29, 0xcd } };
static const GUID netease_src_guid = { 0xaac13215, 0xe32e, 0x4667, { 0xac, 0xd7, 0x1f, 0xd, 0xbd, 0x84, 0x27, 0xe4 } };
static const GUID qqmusic_src_guid = { 0x4b0b5722, 0x3a84, 0x4b8e, { 0x82, 0x7a, 0x26, 0xb9, 0xea, 0xb3, 0xb4, 0xe8 } };

static const GUID cfg_search_active_sources_default[] = {localfiles_src_guid, qqmusic_src_guid, netease_src_guid};

static cfg_int_t<uint64_t> cfg_search_active_sources_generation(GUID_CFG_SEARCH_ACTIVE_SOURCES_GENERATION, 0);
static cfg_objList<GUID>   cfg_search_active_sources(GUID_CFG_SEARCH_ACTIVE_SOURCES, cfg_search_active_sources_default);
static cfg_auto_string     cfg_search_tags(GUID_CFG_SEARCH_TAGS, IDC_SEARCH_TAGS, "LYRICS;SYNCEDLYRICS;UNSYNCEDLYRICS;UNSYNCED LYRICS");
static cfg_auto_bool       cfg_search_exclude_trailing_brackets(GUID_CFG_SEARCH_EXCLUDE_TRAILING_BRACKETS, IDC_SEARCH_EXCLUDE_BRACKETS, true);
static cfg_auto_string     cfg_search_musixmatch_token(GUID_CFG_SEARCH_MUSIXMATCH_TOKEN, IDC_SEARCH_MUSIXMATCH_TOKEN, "");

static cfg_auto_property* g_root_auto_properties[] =
{
    &cfg_search_tags,
    &cfg_search_exclude_trailing_brackets,
    &cfg_search_musixmatch_token,
};

uint64_t preferences::searching::source_config_generation()
{
    return cfg_search_active_sources_generation.get_value();
}

std::vector<GUID> preferences::searching::active_sources()
{
    GUID save_source_guid = preferences::saving::save_source();
    bool save_source_seen = false;

    size_t source_count = cfg_search_active_sources.get_size();
    std::vector<GUID> result;
    result.reserve(source_count+1);
    for(size_t i=0; i<source_count; i++)
    {
        save_source_seen |= (save_source_guid == cfg_search_active_sources[i]);
        result.push_back(cfg_search_active_sources[i]);
    }

    if(!save_source_seen && (save_source_guid != GUID{}))
    {
        result.push_back(save_source_guid);
    }

    return result;
}

std::vector<std::string> preferences::searching::tags()
{
    const std::string_view setting = {cfg_search_tags.get_ptr(), cfg_search_tags.get_length()};
    std::vector<std::string> result;

    size_t prev_index = 0;
    for(size_t i=0; i<setting.length(); i++) // Avoid infinite loops
    {
        size_t next_index = setting.find(';', prev_index);
        size_t len = next_index - prev_index;
        if(len > 0)
        {
            result.emplace_back(setting.substr(prev_index, len));
        }

        if((next_index == std::string_view::npos) || (next_index >= setting.length()))
        {
            break;
        }
        prev_index = next_index + 1;
    }
    return result;
}

bool preferences::searching::exclude_trailing_brackets()
{
    return cfg_search_exclude_trailing_brackets.get_value();
}

std::string preferences::searching::musixmatch_api_key()
{
    return std::string(cfg_search_musixmatch_token.get_ptr(), cfg_search_musixmatch_token.get_length());
}

const LRESULT MAX_SOURCE_NAME_LENGTH = 64;

// The UI for the root element (for OpenLyrics) in the preferences UI tree
class PreferencesRoot : public CDialogImpl<PreferencesRoot>, public auto_preferences_page_instance
{
public:
    // Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
    PreferencesRoot(preferences_page_callback::ptr callback) : auto_preferences_page_instance(callback, g_root_auto_properties) {}

    // Dialog resource ID - Required by WTL/Create()
    enum {IDD = IDD_PREFERENCES_ROOT};

    void apply() override;
    void reset() override;
    bool has_changed() override;

    BEGIN_MSG_MAP_EX(PreferencesRoot)
        MSG_WM_INITDIALOG(OnInitDialog)
        COMMAND_HANDLER_EX(IDC_SEARCH_TAGS, EN_CHANGE, OnUIChange)
        COMMAND_HANDLER_EX(IDC_SEARCH_MUSIXMATCH_TOKEN, EN_CHANGE, OnUIChange)
        COMMAND_HANDLER_EX(IDC_SEARCH_EXCLUDE_BRACKETS, BN_CLICKED, OnUIChange)
        COMMAND_HANDLER_EX(IDC_SOURCE_MOVE_UP_BTN, BN_CLICKED, OnMoveUp)
        COMMAND_HANDLER_EX(IDC_SOURCE_MOVE_DOWN_BTN, BN_CLICKED, OnMoveDown)
        COMMAND_HANDLER_EX(IDC_SOURCE_ACTIVATE_BTN, BN_CLICKED, OnSourceActivate)
        COMMAND_HANDLER_EX(IDC_SOURCE_DEACTIVATE_BTN, BN_CLICKED, OnSourceDeactivate)
        COMMAND_HANDLER_EX(IDC_ACTIVE_SOURCE_LIST, LBN_SELCHANGE, OnActiveSourceSelect)
        COMMAND_HANDLER_EX(IDC_INACTIVE_SOURCE_LIST, LBN_SELCHANGE, OnInactiveSourceSelect)
        COMMAND_HANDLER_EX(IDC_SEARCH_MUSIXMATCH_HELP, BN_CLICKED, OnMusixmatchHelp)
        COMMAND_HANDLER_EX(IDC_SEARCH_MUSIXMATCH_SHOW, BN_CLICKED, OnMusixmatchShow)
    END_MSG_MAP()

private:
    BOOL OnInitDialog(CWindow, LPARAM);
    void OnUIChange(UINT, int, CWindow);
    void OnMoveUp(UINT, int, CWindow);
    void OnMoveDown(UINT, int, CWindow);
    void OnSourceActivate(UINT, int, CWindow);
    void OnSourceDeactivate(UINT, int, CWindow);
    void OnActiveSourceSelect(UINT, int, CWindow);
    void OnInactiveSourceSelect(UINT, int, CWindow);
    void OnMusixmatchHelp(UINT, int, CWindow);
    void OnMusixmatchShow(UINT, int, CWindow);

    void SourceListInitialise();
    void SourceListResetFromSaved();
    void SourceListResetToDefault();
    void SourceListApply();
    bool SourceListHasChanged();

    DWORD m_default_password_char;
};

BOOL PreferencesRoot::OnInitDialog(CWindow, LPARAM)
{
    SourceListInitialise();
    init_auto_preferences();

    CWindow token = GetDlgItem(IDC_SEARCH_MUSIXMATCH_TOKEN);
    m_default_password_char = SendDlgItemMessage(IDC_SEARCH_MUSIXMATCH_TOKEN, EM_GETPASSWORDCHAR, 0, 0);
    return FALSE;
}

void PreferencesRoot::OnUIChange(UINT, int, CWindow)
{
    on_ui_interaction();
}

void PreferencesRoot::OnMoveUp(UINT, int, CWindow)
{
    LRESULT select_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCURSEL, 0, 0);
    if(select_index == LB_ERR)
    {
        return; // No selection
    }
    if(select_index == 0)
    {
        return; // Can't move the top item upwards
    }

    TCHAR buffer[MAX_SOURCE_NAME_LENGTH];
    LRESULT select_strlen = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXTLEN, select_index, 0);
    assert(select_strlen+1 <= MAX_SOURCE_NAME_LENGTH);

    LRESULT strcopy_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXT, select_index, (LPARAM)buffer);
    LRESULT select_data = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETITEMDATA, select_index, 0);
    LRESULT delete_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_DELETESTRING, select_index, 0);
    assert(strcopy_result != LB_ERR);
    assert(select_data != LB_ERR);
    assert(delete_result != LB_ERR);

    LRESULT new_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_INSERTSTRING, select_index-1, (LPARAM)buffer);
    LRESULT set_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, select_data);
    LRESULT select_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETCURSEL, new_index, 0);
    assert((new_index != LB_ERR) && (new_index != LB_ERRSPACE));
    assert(set_result != LB_ERR);
    assert(select_result != LB_ERR);

    OnActiveSourceSelect(0, 0, {}); // Update the button enabled state (e.g if we moved an item to the bottom we should disable the "down" button)

    on_ui_interaction();
}

void PreferencesRoot::OnMoveDown(UINT, int, CWindow)
{
    LRESULT item_count = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(item_count != LB_ERR);

    LRESULT select_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCURSEL, 0, 0);
    if(select_index == LB_ERR)
    {
        return; // No selection
    }
    if(select_index+1 == item_count)
    {
        return; // Can't move the bottom item downwards
    }

    TCHAR buffer[MAX_SOURCE_NAME_LENGTH];
    LRESULT select_strlen = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXTLEN, select_index, 0);
    assert(select_strlen+1 <= MAX_SOURCE_NAME_LENGTH);

    LRESULT strcopy_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXT, select_index, (LPARAM)buffer);
    LRESULT select_data = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETITEMDATA, select_index, 0);
    LRESULT delete_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_DELETESTRING, select_index, 0);
    assert(strcopy_result != LB_ERR);
    assert(select_data != LB_ERR);
    assert(delete_result != LB_ERR);

    LRESULT new_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_INSERTSTRING, select_index+1, (LPARAM)buffer);
    LRESULT set_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, select_data);
    LRESULT select_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETCURSEL, new_index, 0);
    assert((new_index != LB_ERR) && (new_index != LB_ERRSPACE));
    assert(set_result != LB_ERR);
    assert(select_result != LB_ERR);

    OnActiveSourceSelect(0, 0, {}); // Update the button enabled state (e.g if we moved an item to the bottom we should disable the "down" button)

    on_ui_interaction();
}

void PreferencesRoot::OnSourceActivate(UINT, int, CWindow)
{
    LRESULT select_index = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_GETCURSEL, 0, 0);
    if(select_index == LB_ERR)
    {
        return; // No selection
    }

    TCHAR buffer[MAX_SOURCE_NAME_LENGTH];
    LRESULT select_strlen = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXTLEN, select_index, 0);
    assert(select_strlen+1 <= MAX_SOURCE_NAME_LENGTH);

    LRESULT strcopy_result = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_GETTEXT, select_index, (LPARAM)buffer);
    LRESULT select_data = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_GETITEMDATA, select_index, 0);
    LRESULT delete_result = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_DELETESTRING, select_index, 0);
    assert(strcopy_result != LB_ERR);
    assert(select_data != LB_ERR);
    assert(delete_result != LB_ERR);

    LRESULT new_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)buffer);
    LRESULT set_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, select_data);
    assert((new_index != LB_ERR) && (new_index != LB_ERRSPACE));
    assert(set_result != LB_ERR);

    // Select the appropriate adjacent item in the active list so that you can spam (de)activate.
    LRESULT item_count = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(item_count != LB_ERR);
    if(item_count > 0)
    {
        LRESULT new_inactive_index;
        if(select_index < item_count)
        {
            new_inactive_index = select_index;
        }
        else
        {
            if(select_index == 0)
            {
                new_inactive_index = 0;
            }
            else
            {
                new_inactive_index = select_index-1;
            }
        }
        SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_SETCURSEL, new_inactive_index, 0);
    }

    on_ui_interaction();
}

void PreferencesRoot::OnSourceDeactivate(UINT, int, CWindow)
{
    LRESULT select_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCURSEL, 0, 0);
    if(select_index == LB_ERR)
    {
        return; // No selection
    }

    TCHAR buffer[MAX_SOURCE_NAME_LENGTH];
    LRESULT select_strlen = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXTLEN, select_index, 0);
    assert(select_strlen+1 <= MAX_SOURCE_NAME_LENGTH);

    LRESULT strcopy_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETTEXT, select_index, (LPARAM)buffer);
    LRESULT select_data = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETITEMDATA, select_index, 0);
    LRESULT delete_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_DELETESTRING, select_index, 0);
    assert(strcopy_result != LB_ERR);
    assert(select_data != LB_ERR);
    assert(delete_result != LB_ERR);

    LRESULT new_index = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)buffer);
    LRESULT set_result = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, select_data);
    assert((new_index != LB_ERR) && (new_index != LB_ERRSPACE));
    assert(set_result != LB_ERR);

    // Select the appropriate adjacent item in the active list so that you can spam (de)activate.
    LRESULT item_count = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(item_count != LB_ERR);
    if(item_count > 0)
    {
        LRESULT new_active_index;
        if(select_index < item_count)
        {
            new_active_index = select_index;
        }
        else
        {
            if(select_index == 0)
            {
                new_active_index = 0;
            }
            else
            {
                new_active_index = select_index-1;
            }
        }
        SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETCURSEL, new_active_index, 0);
    }

    on_ui_interaction();
}

void PreferencesRoot::OnActiveSourceSelect(UINT, int, CWindow)
{
    LRESULT select_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCURSEL, 0, 0);
    LRESULT item_count = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(item_count != LB_ERR);

    SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_SETCURSEL, -1);

    CWindow activate_btn = GetDlgItem(IDC_SOURCE_ACTIVATE_BTN);
    CWindow deactivate_btn = GetDlgItem(IDC_SOURCE_DEACTIVATE_BTN);
    assert(activate_btn != nullptr);
    assert(deactivate_btn != nullptr);
    activate_btn.EnableWindow(FALSE);
    deactivate_btn.EnableWindow(TRUE);

    CWindow move_up_btn = GetDlgItem(IDC_SOURCE_MOVE_UP_BTN);
    assert(move_up_btn != nullptr);
    move_up_btn.EnableWindow((select_index != LB_ERR) && (select_index != 0));

    CWindow move_down_btn = GetDlgItem(IDC_SOURCE_MOVE_DOWN_BTN);
    assert(move_down_btn != nullptr);
    move_down_btn.EnableWindow((select_index != LB_ERR) && (select_index+1 != item_count));
}

void PreferencesRoot::OnInactiveSourceSelect(UINT, int, CWindow)
{
    SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETCURSEL, -1);

    CWindow activate_btn = GetDlgItem(IDC_SOURCE_ACTIVATE_BTN);
    CWindow deactivate_btn = GetDlgItem(IDC_SOURCE_DEACTIVATE_BTN);
    assert(activate_btn != nullptr);
    assert(deactivate_btn != nullptr);
    activate_btn.EnableWindow(TRUE);
    deactivate_btn.EnableWindow(FALSE);

    CWindow move_up_btn = GetDlgItem(IDC_SOURCE_MOVE_UP_BTN);
    CWindow move_down_btn = GetDlgItem(IDC_SOURCE_MOVE_DOWN_BTN);
    assert(move_up_btn != nullptr);
    assert(move_down_btn != nullptr);
    move_up_btn.EnableWindow(FALSE);
    move_down_btn.EnableWindow(FALSE);
}

void PreferencesRoot::OnMusixmatchHelp(UINT, int, CWindow)
{
    popup_message_v3::query_t query = {};
    query.title = "Musixmatch Help";
    query.msg = "The Musixmatch source requires an authentication token to work. Without one it will not find any lyrics.\r\n\r\nAn authentication token is roughly like a randomly-generated password that musixmatch uses to differentiate between different users.\r\n\r\nWould you like OpenLyrics to attempt to get a token automatically for you now?";
    query.buttons = popup_message_v3::buttonYes | popup_message_v3::buttonNo;
    query.defButton = popup_message_v3::buttonNo;
    query.icon = popup_message_v3::iconInformation;
    uint32_t popup_result = popup_message_v3::get()->show_query_modal(query);
    if(popup_result == popup_message_v3::buttonYes)
    {
        std::string output_token;
        const auto async_search = [&output_token](threaded_process_status& /*status*/, abort_callback& abort)
        {
            output_token = musixmatch_get_token(abort);
        };
        bool success = threaded_process::g_run_modal(threaded_process_callback_lambda::create(async_search),
                                                     threaded_process::flag_show_abort,
                                                     m_hWnd,
                                                     "Attempting to get Musixmatch token...");

        if(!success || output_token.empty())
        {
            popup_message_v3::query_t failed_query = {};
            failed_query.title = "Musixmatch Help";
            failed_query.msg = "Failed to automatically get a Musixmatch token.\r\n\r\nYou could try to get a token manually, using the instructions found here:\r\nhttps://github.com/khanhas/genius-spicetify#musicxmatch";
            failed_query.buttons = popup_message_v3::buttonOK;
            failed_query.defButton = popup_message_v3::buttonOK;
            failed_query.icon = popup_message_v3::iconWarning;
            popup_message_v3::get()->show_query_modal(failed_query);
        }
        else
        {
            std::tstring ui_token = to_tstring(output_token);
            SetDlgItemText(IDC_SEARCH_MUSIXMATCH_TOKEN, ui_token.c_str());
        }
    }
}

void PreferencesRoot::OnMusixmatchShow(UINT, int, CWindow)
{
    CWindow token = GetDlgItem(IDC_SEARCH_MUSIXMATCH_TOKEN);
    LRESULT password_char = token.SendMessage(EM_GETPASSWORDCHAR, 0, 0);
    if(password_char == m_default_password_char)
    {
        token.SendMessage(EM_SETPASSWORDCHAR, 0, 0);
    }
    else
    {
        token.SendMessage(EM_SETPASSWORDCHAR, m_default_password_char, 0);
    }
    token.Invalidate(); // Force it to redraw with the new character
}

void PreferencesRoot::reset()
{
    SourceListResetToDefault();
    auto_preferences_page_instance::reset();
}

void PreferencesRoot::apply()
{
    SourceListApply();
    auto_preferences_page_instance::apply();

    bool has_musixmatch_token = (cfg_search_musixmatch_token.get_length() > 0);
    bool musixmatch_enabled = false;

    size_t source_count = cfg_search_active_sources.get_size();
    for(size_t i=0; i<source_count; i++)
    {
        if(cfg_search_active_sources[i] == musixmatch_src_guid)
        {
            musixmatch_enabled = true;
            break;
        }
    }

    if(musixmatch_enabled && !has_musixmatch_token)
    {
        popup_message_v3::query_t query = {};
        query.title = "Musixmatch Warning";
        query.msg = "You have enabled the 'Musixmatch' source for the OpenLyrics component, but have not provided a token. Without a token the Musixmatch source will not work.\r\n\r\nYou can click on the '?' button for more information on how to get a token.";
        query.buttons = popup_message_v3::buttonOK;
        query.defButton = popup_message_v3::buttonOK;
        query.icon = popup_message_v3::iconWarning;
        popup_message_v3::get()->show_query_modal(query);
    }
}

bool PreferencesRoot::has_changed()
{
    if(SourceListHasChanged()) return true;
    return auto_preferences_page_instance::has_changed();
}

void PreferencesRoot::SourceListInitialise()
{
    SourceListResetFromSaved();
}

void PreferencesRoot::SourceListResetFromSaved()
{
    SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_RESETCONTENT, 0, 0);

    std::vector<GUID> all_src_ids = LyricSourceBase::get_all_ids();
    size_t total_source_count = all_src_ids.size();
    std::vector<bool> sources_active(total_source_count);

    size_t active_source_count = cfg_search_active_sources.get_count();
    for(size_t active_source_index=0; active_source_index<active_source_count; active_source_index++)
    {
        GUID src_guid = cfg_search_active_sources[active_source_index];
        LyricSourceBase* src = LyricSourceBase::get(src_guid);
        assert(src != nullptr);

        bool found = false;
        for(size_t i=0; i<total_source_count; i++)
        {
            if(all_src_ids[i] == src_guid)
            {
                sources_active[i] = true;
                found = true;
                break;
            }
        }
        assert(found);

        LRESULT new_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)src->friendly_name().data());
        LRESULT set_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, (LPARAM)&src->id());
        assert(new_index != LB_ERR);
        assert(set_result != LB_ERR);
    }

    for(size_t entry_index=0; entry_index<total_source_count; entry_index++)
    {
        if(sources_active[entry_index]) continue;

        LyricSourceBase* src = LyricSourceBase::get(all_src_ids[entry_index]);
        assert(src != nullptr);

        LRESULT new_index = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)src->friendly_name().data());
        LRESULT set_result = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, (LPARAM)&src->id());
        assert(new_index != LB_ERR);
        assert(set_result != LB_ERR);
    }
}

void PreferencesRoot::SourceListResetToDefault()
{
    SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_RESETCONTENT, 0, 0);
    SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_RESETCONTENT, 0, 0);

    std::vector<GUID> all_src_ids = LyricSourceBase::get_all_ids();
    size_t total_source_count = all_src_ids.size();
    std::vector<bool> sources_active(total_source_count);

    for(GUID src_guid : cfg_search_active_sources_default)
    {
        LyricSourceBase* src = LyricSourceBase::get(src_guid);
        assert(src != nullptr);

        bool found = false;
        for(size_t i=0; i<total_source_count; i++)
        {
            if(all_src_ids[i] == src_guid)
            {
                sources_active[i] = true;
                found = true;
                break;
            }
        }
        assert(found);

        LRESULT new_index = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)src->friendly_name().data());
        LRESULT set_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, (LPARAM)&src->id());
        assert(new_index != LB_ERR);
        assert(set_result != LB_ERR);
    }

    for(size_t entry_index=0; entry_index<total_source_count; entry_index++)
    {
        if(sources_active[entry_index]) continue;

        LyricSourceBase* src = LyricSourceBase::get(all_src_ids[entry_index]);
        assert(src != nullptr);

        LRESULT new_index = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_ADDSTRING, 0, (LPARAM)src->friendly_name().data());
        LRESULT set_result = SendDlgItemMessage(IDC_INACTIVE_SOURCE_LIST, LB_SETITEMDATA, new_index, (LPARAM)&src->id());
        assert(new_index != LB_ERR);
        assert(set_result != LB_ERR);
    }
}

void PreferencesRoot::SourceListApply()
{
    cfg_search_active_sources.remove_all();

    LRESULT item_count = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(item_count != LB_ERR);

    for(LRESULT item_index=0; item_index<item_count; item_index++)
    {
        LRESULT item_data = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETITEMDATA, item_index, 0);
        assert(item_data != LB_ERR);
        const GUID* ui_item_id = (GUID*)item_data;
        assert(ui_item_id != nullptr);

        cfg_search_active_sources.add_item(*ui_item_id);
    }
}

bool PreferencesRoot::SourceListHasChanged()
{
    size_t saved_item_count = cfg_search_active_sources.get_count();
    LRESULT ui_item_count_result = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETCOUNT, 0, 0);
    assert(ui_item_count_result != LB_ERR);
    assert(ui_item_count_result >= 0);
    size_t ui_item_count = static_cast<size_t>(ui_item_count_result);

    if(saved_item_count != ui_item_count)
    {
        return true;
    }
    assert(saved_item_count == ui_item_count);

    for(size_t item_index=0; item_index<saved_item_count; item_index++)
    {
        LRESULT ui_item = SendDlgItemMessage(IDC_ACTIVE_SOURCE_LIST, LB_GETITEMDATA, item_index, 0);
        assert(ui_item != LB_ERR);

        const GUID* ui_item_id = (const GUID*)ui_item;
        assert(ui_item_id != nullptr);

        GUID saved_item_id = cfg_search_active_sources[item_index];

        if(saved_item_id != *ui_item_id)
        {
            return true;
        }
    }

    return false;
}

class PreferencesRootImpl : public preferences_page_impl<PreferencesRoot>
{
public:
    const char* get_name() { return "OpenLyrics"; }
    GUID get_guid() { return GUID_PREFERENCES_PAGE_ROOT; }
    GUID get_parent_guid() { return guid_tools; }
};

static preferences_page_factory_t<PreferencesRootImpl> g_preferences_page_root_factory;
