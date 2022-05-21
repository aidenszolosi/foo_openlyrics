#pragma once

#include "stdafx.h"

namespace std
{
#ifdef UNICODE
    using tstring = wstring;
    using tstring_view = wstring_view;
#else
    using tstring = string;
    using tstring_view = string_view;
#endif
}

std::tstring to_tstring(std::string_view string);
std::tstring to_tstring(const std::string& string);
std::tstring to_tstring(const pfc::string8& string);

std::string from_tstring(std::tstring_view string);
std::string from_tstring(const std::tstring& string);

std::tstring normalise_utf8(std::tstring_view input);

std::optional<SIZE> GetTextExtents(HDC dc, std::tstring_view string); // GetTextExtentPoint32
BOOL DrawTextOut(HDC dc, int x, int y, std::tstring_view string); // TextOut
