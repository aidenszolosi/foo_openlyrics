#pragma once

#include "stdafx.h"

#include "lyric_data.h"
#include "tag_util.h"

class LyricUpdateHandle;

namespace io
{
    void search_for_lyrics(LyricUpdateHandle& handle, bool local_only);
    void search_for_all_lyrics(LyricUpdateHandle& handle, std::string artist, std::string album, std::string title);

    std::optional<LyricData> process_available_lyric_update(LyricUpdateHandle& update);

    // Updates the lyric data with the ID of the source used for saving, as well as the persistence path that it reports.
    // Returns a success flag
    bool save_lyrics(metadb_handle_ptr track, LyricData& lyrics, bool allow_overwrite, abort_callback& abort);

    bool delete_saved_lyrics(metadb_handle_ptr track, const LyricData& lyrics);
}


class LyricUpdateHandle
{
public:
    enum class Type
    {
        Unknown,
        AutoSearch,
        ManualSearch,
        Edit,
    };

    LyricUpdateHandle(Type type, metadb_handle_ptr track, metadb_v2_rec_t track_info, abort_callback& abort);
    LyricUpdateHandle(const LyricUpdateHandle& other) = delete;
    LyricUpdateHandle(LyricUpdateHandle&& other);
    ~LyricUpdateHandle();

    Type get_type();
    std::string get_progress();
    bool wait_for_complete(uint32_t timeout_ms);
    bool is_complete();
    bool has_result();
    bool has_searched_remote_sources(); // True if this update handle has searched any remote sources
    LyricData get_result();

    abort_callback& get_checked_abort(); // Checks the abort flag (so it might throw) and returns it
    metadb_handle_ptr get_track();
    const metadb_v2_rec_t& get_track_info();

    void set_started();
    void set_progress(std::string_view value);
    void set_remote_source_searched();
    void set_result(LyricData&& data, bool final_result);
    void set_complete();

private:
    enum class Status
    {
        Unknown,
        Created,
        Running,
        Complete,
        Closed
    };

    const metadb_handle_ptr m_track;
    const metadb_v2_rec_t m_track_info;
    const Type m_type;

    CRITICAL_SECTION m_mutex;
    std::vector<LyricData> m_lyrics;
    abort_callback& m_abort;
    HANDLE m_complete;
    Status m_status;
    std::string m_progress;
    bool m_searched_remote_sources;
};

