#include "stdafx.h"
#include <cctype>

#include "libxml/HTMLparser.h"
#include "libxml/tree.h"
#include "libxml/xpath.h"

#include "logging.h"
#include "lyric_source.h"
#include "tag_util.h"

static const GUID src_guid = { 0x5901c128, 0xc67f, 0x4eec, { 0x8f, 0x10, 0x47, 0x5d, 0x12, 0x52, 0x89, 0xe9 } };

class DarkLyricsSource : public LyricSourceRemote
{
    const GUID& id() const final { return src_guid; }
    std::tstring_view friendly_name() const final { return _T("DarkLyrics.com"); }

    void add_all_text_to_string(std::string& output, xmlNodePtr node) const;
    std::vector<LyricDataRaw> search(std::string_view artist, std::string_view album, std::string_view title, abort_callback& abort) final;
    bool lookup(LyricDataRaw& data, abort_callback& abort) final;
};
static const LyricSourceFactory<DarkLyricsSource> src_factory;

static std::string remove_chars_for_url(const std::string_view input)
{
    std::string transliterated = transliterate_to_ascii(input);

    std::string output;
    output.reserve(transliterated.length());

    for(char c : transliterated)
    {
        if(pfc::char_is_ascii_alphanumeric(c))
        {
            output += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    return output;
}

void DarkLyricsSource::add_all_text_to_string(std::string& output, xmlNodePtr node) const
{
    if(node == nullptr)
    {
        return;
    }

    xmlNode* current = node;
    while(current != nullptr)
    {
        if(current->type == XML_TEXT_NODE)
        {
            // NOTE: libxml2 stores strings as UTF8 internally, so we don't need to do any conversion here
            std::string_view node_text = trim_surrounding_whitespace((char*)current->content);
            output += node_text;
            output += "\r\n";
        }
        else if(current->type == XML_ELEMENT_NODE)
        {
            const std::string_view current_name = (const char*)(current->name);
            if((current_name == "h3") || (current_name == "div"))
            {
                break;
            }

            add_all_text_to_string(output, current->children);
        }

        current = current->next;
    }
}

std::vector<LyricDataRaw> DarkLyricsSource::search(std::string_view artist, std::string_view album, std::string_view title, abort_callback& abort)
{
    http_request::ptr request = http_client::get()->create_request("GET");

    std::string url_artist = remove_chars_for_url(artist);
    std::string url_album = remove_chars_for_url(album);
    std::string url_title = remove_chars_for_url(title);
    std::string url = "http://darklyrics.com/lyrics/" + url_artist + "/" + url_album + ".html";;
    LOG_INFO("Querying for lyrics from %s...", url.c_str());

    pfc::string8 content;
    try
    {
        file_ptr response_file = request->run(url.c_str(), abort);
        response_file->read_string_raw(content, abort);
        // NOTE: We're assuming here that the response is encoded in UTF-8 
    }
    catch(const std::exception& e)
    {
        LOG_WARN("Failed to download darklyrics.com page %s: %s", url.c_str(), e.what());
        return {};
    }

    htmlDocPtr doc = htmlReadMemory(content.c_str(), content.length(), url.c_str(), nullptr, 0);
    if(doc != nullptr)
    {
        xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
        if(xpath_ctx == nullptr)
        {
            xmlFreeDoc(doc);
            throw std::runtime_error("Failed to create xpath context");
        }

        std::string lyric_text;

        char xpath_query[64] = {};
        snprintf(xpath_query, sizeof(xpath_query), "//div[@class='lyrics']/h3/a[@name]");
        xmlXPathObjectPtr xpath_obj = xmlXPathEvalExpression(BAD_CAST xpath_query, xpath_ctx);
        if(xpath_obj != nullptr)
        {
            if((xpath_obj->nodesetval != nullptr) && (xpath_obj->nodesetval->nodeNr > 0))
            {
                for(int i=0; i<xpath_obj->nodesetval->nodeNr; i++)
                {
                    xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
                    if(node->type != XML_ELEMENT_NODE) continue;
                    if(node->children == nullptr) continue;
                    if(node->children->type != XML_TEXT_NODE) continue;

                    std::string_view title_text = (const char*)node->children->content;
                    size_t title_dot_index = title_text.find('.');

                    if(title_dot_index == std::string_view::npos) continue;

                    title_text.remove_prefix(title_dot_index + 1); // +1 to include the '.' that we found
                    title_text = trim_surrounding_whitespace(title_text);
                    if(!tag_values_match(title_text, title))
                    {
                        continue;
                    }

                    if(node->parent != nullptr)
                    {
                        add_all_text_to_string(lyric_text, node->parent->next);
                        break;
                    }
                }
            }

            xmlXPathFreeObject(xpath_obj);
        }

        xmlXPathFreeContext(xpath_ctx);
        xmlFreeDoc(doc);

        if(lyric_text.empty())
        {
            throw new std::runtime_error("Failed to parse lyrics, the page format may have changed");
        }
        else
        {
            LOG_INFO("Successfully retrieved lyrics from %s", url.c_str());

            LyricDataRaw result = {};
            result.source_id = id();
            result.persistent_storage_path = url;
            result.artist = artist;
            result.album = album;
            result.title = title;
            result.text = trim_surrounding_whitespace(lyric_text);
            return {std::move(result)};
        }
    }

    return {};
}

bool DarkLyricsSource::lookup(LyricDataRaw& /*data*/, abort_callback& /*abort*/)
{
    LOG_ERROR("We should never need to do a lookup of the %s source", friendly_name().data());
    assert(false);
    return false;
}
