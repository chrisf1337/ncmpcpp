/*
 * ytsearcher.cpp
 *
 *  Created on: Jul 10, 2015
 *      Author: chrisf
 */

#include <Python.h>

#include <array>
#include <boost/range/detail/any_iterator.hpp>
#include <iomanip>

#include "display.h"
#include "global.h"
#include "helpers.h"
#include "menu_impl.h"
#include "playlist.h"
#include "ytsearcher.h"
#include "settings.h"
#include "status.h"
#include "statusbar.h"
#include "helpers/song_iterator_maker.h"
#include "utility/comparators.h"
#include "title.h"
#include "screen_switcher.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <tuple>
#include <cwchar>
#include <cstdio>
#undef NDEBUG
#include <cassert>
#include <cstdlib>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>

#include <rapidjson/document.h>

using namespace rapidjson;

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

std::string queryToString(const char *originalString)
{
    std::string arg(originalString);
    std::vector<std::string> elems;
    split(arg, ' ', elems);
    std::string str;
    for (auto it = elems.begin(); it != elems.end() - 1; it++)
    {
        str += *it + "+";
    }
    str += elems.back();
    return str;
}

std::string queryToString(const std::string &originalString)
{
    std::string arg = originalString;
    std::vector<std::string> elems;
    split(arg, ' ', elems);
    std::string str;
    for (auto it = elems.begin(); it != elems.end() - 1; it++)
    {
        str += *it + "+";
    }
    str += elems.back();
    return str;
}

std::string videoIdsToString(const std::vector<std::string> &videoIds)
{
    std::string str;
    for (auto it = videoIds.begin(); it != videoIds.end() - 1; it++)
    {
        str += *it + ",";
    }
    str += videoIds.back();
    return curlpp::escape(str);
}

void checkError(PyObject *obj)
{
    if (obj == nullptr)
    {
        PyErr_Print();
    }
}

// test comment

using Global::MainHeight;
using Global::MainStartY;

namespace ph = std::placeholders;

YTSearcher *myYTSearcher;

namespace
{
std::string YTItemToString(const YTItem &ei);
bool YTItemEntryMatcher(const Regex::Regex &rx, const NC::Menu<YTItem>::Item &item, bool filter);

template <bool Const>
struct SongExtractor
{
    typedef SongExtractor type;

    typedef typename NC::Menu<YTItem>::Item MenuItem;
    typedef typename std::conditional<Const, const MenuItem, MenuItem>::type Item;
    typedef typename std::conditional<Const, const MPD::Song, MPD::Song>::type Song;

    Song *operator()(Item &item) const
    {
        Song *ptr = nullptr;
        if (!item.isSeparator() && item.value().isSong())
            ptr = &item.value().song();
        return ptr;
    }
};

}

SongIterator YTSearcherWindow::currentS()
{
    return makeSongIterator_<YTItem>(current(), SongExtractor<false>());
}

ConstSongIterator YTSearcherWindow::currentS() const
{
    return makeConstSongIterator_<YTItem>(current(), SongExtractor<true>());
}

SongIterator YTSearcherWindow::beginS()
{
    return makeSongIterator_<YTItem>(begin(), SongExtractor<false>());
}

ConstSongIterator YTSearcherWindow::beginS() const
{
    return makeConstSongIterator_<YTItem>(begin(), SongExtractor<true>());
}

SongIterator YTSearcherWindow::endS()
{
    return makeSongIterator_<YTItem>(end(), SongExtractor<false>());
}

ConstSongIterator YTSearcherWindow::endS() const
{
    return makeConstSongIterator_<YTItem>(end(), SongExtractor<true>());
}

std::vector<MPD::Song> YTSearcherWindow::getSelectedSongs()
{
    return {};
}

const char *YTSearcher::ConstraintsNames[] =
{
    "Search",
    0
};

// const char *YTSearcher::SearchModes[] =
// {
//     "Match if tag contains searched phrase (no regexes)",
//     "Match if tag contains searched phrase (regexes supported)",
//     "Match only if both values are the same",
//     0
// };

// size_t YTSearcher::StaticOptions = 10;
// size_t YTSearcher::ResetButton = 6;
// size_t YTSearcher::SearchButton = 5;

size_t YTSearcher::StaticOptions = 7;
size_t YTSearcher::ResetButton = 3;
size_t YTSearcher::SearchButton = 2;

YTSearcher::YTSearcher()
: Screen(NC::Menu<YTItem>(0, MainStartY, COLS, MainHeight, "", Config.main_color, NC::Border()))
{
    // w.setHighlightColor(Config.main_highlight_color);
    w.setHighlightColor(NC::Color::Green);
    w.cyclicScrolling(Config.use_cyclic_scrolling);
    w.centeredCursor(Config.centered_cursor);
    w.setItemDisplayer(std::bind(Display::YTItems, ph::_1, std::cref(w)));
    w.setSelectedPrefix(Config.selected_item_prefix);
    w.setSelectedSuffix(Config.selected_item_suffix);
    // SearchMode = &SearchModes[Config.search_engine_default_search_mode];

    // initialize pafy
    PyObject *pName;
    Py_Initialize();

    const wchar_t *wArgv = L"./pafy_test";
    wchar_t *wArgv_copy = wcsdup(wArgv);
    PySys_SetArgv(1, &wArgv_copy);

    pName = PyUnicode_DecodeFSDefault("pafy");

    pafy = PyImport_Import(pName);
    Py_DECREF(pName);
    checkError(pafy);
    pafy_fNew = PyObject_GetAttrString(pafy, "new");
    checkError(pafy_fNew);
}

void YTSearcher::resize()
{
    size_t x_offset, width;
    getWindowResizeParams(x_offset, width);
    w.resize(width, MainHeight);
    w.moveTo(x_offset, MainStartY);
    switch (Config.search_engine_display_mode)
    {
        case DisplayMode::Columns:
            if (Config.titles_visibility)
            {
                w.setTitle(Display::Columns(w.getWidth()));
                break;
            }
        case DisplayMode::Classic:
            w.setTitle("");
    }
    hasToBeResized = 0;
}

void YTSearcher::switchTo()
{
    SwitchTo::execute(this);
    if (w.empty())
        Prepare();
    markSongsInPlaylist(w);
    drawHeader();
}

std::wstring YTSearcher::title()
{
    return L"YouTube searcher";
}

void YTSearcher::enterPressed()
{
    size_t option = w.choice();
    if (option > ConstraintsNumber && option < SearchButton)
    {
        w.current()->value().buffer().clear();
    }

    if (option < ConstraintsNumber)
    {
        Statusbar::ScopedLock slock;
        std::string constraint = ConstraintsNames[option];
        Statusbar::put() << NC::Format::Bold << constraint << NC::Format::NoBold << ": ";
        itsConstraints[option] = Global::wFooter->prompt(itsConstraints[option]);
        w.current()->value().buffer().clear();
        constraint.resize(13, ' ');
        w.current()->value().buffer() << NC::Format::Bold << constraint << NC::Format::NoBold << ": ";
        ShowTag(w.current()->value().buffer(), itsConstraints[option]);
    }
    // else if (option == ConstraintsNumber+1)
    // {
    //     Config.search_in_db = !Config.search_in_db;
    //     w.current()->value().buffer() << NC::Format::Bold << "Search in:" << NC::Format::NoBold << ' ' << (Config.search_in_db ? "Database" : "Current playlist");
    // }
    // else if (option == ConstraintsNumber+2)
    // {
    //     if (!*++SearchMode)
    //         SearchMode = &SearchModes[0];
    //     w.current()->value().buffer() << NC::Format::Bold << "Search mode:" << NC::Format::NoBold << ' ' << *SearchMode;
    // }
    else if (option == SearchButton)
    {
        Statusbar::print("Searching...");
        if (w.size() > StaticOptions)
            Prepare();
        Search();
        if (w.rbegin()->value().isSong())
        {
            if (Config.search_engine_display_mode == DisplayMode::Columns)
                w.setTitle(Config.titles_visibility ? Display::Columns(w.getWidth()) : "");
            size_t found = w.size() - YTSearcher::StaticOptions;
            found += 3; // don't count options inserted below
            w.insertSeparator(ResetButton+1);
            w.insertItem(ResetButton+2, YTItem(), NC::List::Properties::Bold | NC::List::Properties::Inactive);
            w.at(ResetButton+2).value().mkBuffer() << Config.color1 << "Search results: " << Config.color2 << "Found " << found << (found > 1 ? " songs" : " song") << NC::Color::Default;
            w.insertSeparator(ResetButton+3);
            markSongsInPlaylist(w);
            Statusbar::print("Searching finished");
            if (Config.block_search_constraints_change)
                for (size_t i = 0; i < StaticOptions-4; ++i)
                    w.at(i).setInactive(true);
            w.scroll(NC::Scroll::Down);
            w.scroll(NC::Scroll::Down);
        }
        else
            Statusbar::print("No results found");
    }
    else if (option == ResetButton)
    {
        reset();
    }
    else
    {
        addSongToPlaylist(w.current()->value().song(), true);
    }
}

void YTSearcher::mouseButtonPressed(MEVENT me)
{
    if (w.empty() || !w.hasCoords(me.x, me.y) || size_t(me.y) >= w.size())
        return;
    if (me.bstate & (BUTTON1_PRESSED | BUTTON3_PRESSED))
    {
        if (!w.Goto(me.y))
            return;
        w.refresh();
        if ((me.bstate & BUTTON3_PRESSED || w.choice() > ConstraintsNumber) && w.choice() < StaticOptions)
            enterPressed();
        else if (w.choice() >= StaticOptions)
        {
            if (me.bstate & BUTTON1_PRESSED)
                addItemToPlaylist();
            else
                enterPressed();
        }
    }
    else
        Screen<WindowType>::mouseButtonPressed(me);
}

/***********************************************************************/

bool YTSearcher::addItemToPlaylist()
{
    bool result = false;
    if (!w.empty() && w.current()->value().isSong())
        result = addSongToPlaylist(w.current()->value().song(), false);
    return result;
}

std::vector<MPD::Song> YTSearcher::getSelectedSongs()
{
    std::vector<MPD::Song> result;
    for (auto it = w.begin(); it != w.end(); ++it)
    {
        if (it->isSelected())
        {
            assert(it->value().isSong());
            result.push_back(it->value().song());
        }
    }
    // if no item is selected, add current one
    if (result.empty() && !w.empty())
    {
        assert(w.current()->value().isSong());
        result.push_back(w.current()->value().song());
    }
    return result;
}

/***********************************************************************/

void YTSearcher::Prepare()
{
    w.setTitle("");
    w.clear();
    w.resizeList(StaticOptions-3);
    for (auto &item : w)
        item.setSelectable(false);

    w.at(ConstraintsNumber).setSeparator(true);
    // w.at(SearchButton-1).setSeparator(true);

    for (size_t i = 0; i < ConstraintsNumber; ++i)
    {
        std::string constraint = ConstraintsNames[i];
        constraint.resize(13, ' ');
        w[i].value().mkBuffer() << NC::Format::Bold << constraint << NC::Format::NoBold << ": ";
        ShowTag(w[i].value().buffer(), itsConstraints[i]);
    }

    // w.at(ConstraintsNumber+1).value().mkBuffer() << NC::Format::Bold << "Search in:" << NC::Format::NoBold << ' ' << (Config.search_in_db ? "Database" : "Current playlist");
    // w.at(ConstraintsNumber+2).value().mkBuffer() << NC::Format::Bold << "Search mode:" << NC::Format::NoBold << ' ' << *SearchMode;

    w.at(SearchButton).value().mkBuffer() << "Search";
    w.at(ResetButton).value().mkBuffer() << "Reset";
}

void YTSearcher::reset()
{
    for (size_t i = 0; i < ConstraintsNumber; ++i)
        itsConstraints[i].clear();
    w.reset();
    Prepare();
    Statusbar::print("Search state reset");
}

void YTSearcher::Search()
{
    bool constraints_empty = 1;
    for (size_t i = 0; i < ConstraintsNumber; ++i)
    {
        if (!itsConstraints[i].empty())
        {
            constraints_empty = 0;
            break;
        }
    }
    if (constraints_empty)
        return;

    std::string q = queryToString(itsConstraints[0]);
    std::clog << q << std::endl;

    curlpp::Easy request;
    std::vector<std::string> videoIds;
    std::vector<Video> videos;

    using namespace curlpp::Options;
    std::ostringstream os;
    os << "https://www.googleapis.com/youtube/v3/search?part=snippet&q=" << q << "&maxResults=10&key="
          "AIzaSyDoz41CwhuVkfFFqmfUFzkfur2sOZr5SwE";
    request.setOpt(Url(os.str()));

    os.str("");
    os << request;

    Document document;
    document.Parse(os.str().c_str());
    assert(document.IsObject());
    assert(document.HasMember("items"));

    Video video;
    for (Value::ConstValueIterator itr = document["items"].Begin(); itr != document["items"].End(); itr++)
    {
        assert(itr->HasMember("snippet"));
        assert(itr->HasMember("id"));
        assert((*itr)["snippet"].HasMember("title"));
        // assert((*itr)["id"].HasMember("videoId"));

        // playlists currently unsupported
        if ((*itr)["id"].HasMember("videoId"))
        {
            std::string videoTitle = (*itr)["snippet"]["title"].GetString();
            video.title = videoTitle;
            std::string videoId = (*itr)["id"]["videoId"].GetString();
            video.id = videoId;
            videoIds.push_back(videoId);
            std::clog << videoId << ": " << videoTitle << std::endl;
            video.duration = std::make_tuple(0, 0, 0);
            videos.push_back(video);
        }
    }

    std::string videoIdsString = videoIdsToString(videoIds);
    std::clog << videoIdsString << std::endl;

    os.str("");
    os << "https://www.googleapis.com/youtube/v3/videos?part=snippet%2CcontentDetails&id=" << videoIdsString
       << "&key=AIzaSyDoz41CwhuVkfFFqmfUFzkfur2sOZr5SwE";
    request.setOpt(Url(os.str()));

    os.str("");
    os << request;
    std::clog << os.str() << std::endl;

    document.Parse(os.str().c_str());
    assert(document.IsObject());
    assert(document.HasMember("items"));

    std::regex re(R"(PT((\d+)H)?((\d+)M)?((\d+)S)?)");
    std::smatch match;

    int videosIndex = 0;
    for (Value::ConstValueIterator itr = document["items"].Begin(); itr != document["items"].End(); itr++)
    {
        assert(itr->HasMember("contentDetails"));
        assert((*itr)["contentDetails"].HasMember("duration"));
        std::string duration = (*itr)["contentDetails"]["duration"].GetString();
        std::clog << duration << std::endl;
        std::regex_search(duration, match, re);
        assert(match.size() == 3 || match.size() == 5 || match.size() == 7);
        std::clog << "matches for '" << duration << "'" << std::endl;
        for (size_t i = 0; i < match.size(); i++)
        {
            std::clog << i << ": " << match[i] << std::endl;
        }
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
        if (match[2].matched)
        {
            hours = std::stoi(match[2]);
        }
        if (match[4].matched)
        {
            minutes = std::stoi(match[4]);
        }
        if (match[6].matched)
        {
            seconds = std::stoi(match[6]);
        }
        videos[videosIndex].duration = std::make_tuple(hours, minutes, seconds);
        videosIndex++;
    }

    for (auto it = videos.begin(); it != videos.end(); ++it)
    {
        std::clog << it->title << " (" << std::get<0>(it->duration) << ":" << std::setfill('0') << std::setw(2)
                  << std::get<1>(it->duration) << ":" << std::get<2>(it->duration) << ")" << std::endl;
        YTItem ytItem(*it);
        w.addItem(std::move(ytItem));
    }



    // PyObject *url = PyUnicode_FromString(videos[0].id.c_str());
    // PyObject *pafyVideo = PyObject_CallFunctionObjArgs(fNew, url, NULL);
    // checkError(pafyVideo);
    // PyObject *title = PyObject_GetAttrString(pafyVideo, "title");
    // checkError(title);
    // PyObject *fGetBestAudio = PyObject_GetAttrString(pafyVideo, "getbestaudio");
    // checkError(fGetBestAudio);
    // PyObject *bestAudio = PyObject_CallFunctionObjArgs(fGetBestAudio, NULL);
    // checkError(bestAudio);
    // PyObject *bestAudioUrl = PyObject_GetAttrString(bestAudio, "url");
    // checkError(bestAudioUrl);
    // std::string bestAudioUrlString(PyUnicode_AsUTF8(bestAudioUrl));
    // std::clog << bestAudioUrlString << std::endl;
    return;
}

namespace {

std::string YTItemToString(const YTItem &ei)
{
    std::string result;
    if (ei.isSong())
    {
        switch (Config.search_engine_display_mode)
        {
            case DisplayMode::Classic:
                result = Format::stringify<char>(Config.song_list_format, &ei.song());
                break;
            case DisplayMode::Columns:
                result = Format::stringify<char>(Config.song_columns_mode_format, &ei.song());
                break;
        }
    }
    else
        result = ei.buffer().str();
    return result;
}

bool YTItemEntryMatcher(const Regex::Regex &rx, const NC::Menu<YTItem>::Item &item, bool filter)
{
    if (item.isSeparator() || !item.value().isSong())
        return filter;
    return Regex::search(YTItemToString(item.value()), rx);
}

}
