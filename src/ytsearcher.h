/*
 * ytsearcher.h
 *
 *  Created on: Jul 10, 2015
 *      Author: chrisf
 */

#ifndef NCMPCPP_YTSEARCHER_H
#define NCMPCPP_YTSEARCHER_H

#include <Python.h>

#include <cassert>

#include "interfaces.h"
#include "mpdpp.h"
#include "regex_filter.h"
#include "screen.h"
#include "song_list.h"

#include <curlpp/cURLpp.hpp>

#include "ytsong.h"

struct YTItem
{
    YTItem() : m_is_song(false), m_buffer(0) { }
    YTItem(NC::Buffer *buf) : m_is_song(false), m_buffer(buf) { }
    YTItem(const MPD::Song &s) : m_is_song(true), m_song(s) { }
    YTItem(const YTSong &song) : m_is_song(true), m_ytSong(song) {}
    YTItem(const YTItem &ei) { *this = ei; }
    ~YTItem() {
        if (!m_is_song)
           delete m_buffer;
    }

    NC::Buffer &mkBuffer() {
        assert(!m_is_song);
        delete m_buffer;
        m_buffer = new NC::Buffer();
        return *m_buffer;
    }

    bool isSong() const { return m_is_song; }

    NC::Buffer &buffer() { assert(!m_is_song && m_buffer); return *m_buffer; }
    MPD::Song &song() { assert(m_is_song); return m_song; }

    const NC::Buffer &buffer() const { assert(!m_is_song && m_buffer); return *m_buffer; }
    // const MPD::Song &song() const { assert(m_is_song); return m_song; }
    const YTSong &song() const {assert(m_is_song); return m_ytSong;}

    YTItem &operator=(const YTItem &se) {
        if (this == &se)
            return *this;
        m_is_song = se.m_is_song;
        if (se.m_is_song)
        {
            m_song = se.m_song;
            m_ytSong = se.m_ytSong;
        }
        else if (se.m_buffer)
            m_buffer = new NC::Buffer(*se.m_buffer);
        else
            m_buffer = 0;
        return *this;
    }

private:
    bool m_is_song;

    NC::Buffer *m_buffer;
    MPD::Song m_song;
    YTSong m_ytSong;
};

struct YTSearcherWindow: NC::Menu<YTItem>, SongList
{
    YTSearcherWindow() { }
    YTSearcherWindow(NC::Menu<YTItem> &&base)
    : NC::Menu<YTItem>(std::move(base)) { }

    virtual SongIterator currentS() OVERRIDE;
    virtual ConstSongIterator currentS() const OVERRIDE;
    virtual SongIterator beginS() OVERRIDE;
    virtual ConstSongIterator beginS() const OVERRIDE;
    virtual SongIterator endS() OVERRIDE;
    virtual ConstSongIterator endS() const OVERRIDE;

    virtual std::vector<MPD::Song> getSelectedSongs() OVERRIDE;
};

struct YTSearcher: Screen<YTSearcherWindow>, HasSongs, Tabbable
{
    YTSearcher();

    // Screen<YTSearcherWindow> implementation
    virtual void resize() OVERRIDE;
    virtual void switchTo() OVERRIDE;

    virtual std::wstring title() OVERRIDE;
    virtual ScreenType type() OVERRIDE { return ScreenType::YTSearcher; }

    virtual void update() OVERRIDE { }

    virtual void enterPressed() OVERRIDE;
    virtual void mouseButtonPressed(MEVENT me) OVERRIDE;

    virtual bool isLockable() OVERRIDE { return true; }
    virtual bool isMergable() OVERRIDE { return true; }

    // HasSongs implementation
    virtual bool addItemToPlaylist() OVERRIDE;
    virtual std::vector<MPD::Song> getSelectedSongs() OVERRIDE;

    // private members
    void reset();

    static size_t StaticOptions;
    static size_t SearchButton;
    static size_t ResetButton;

private:
    void Prepare();
    void Search();

    Regex::ItemFilter<YTItem> m_search_predicate;

    // const char **SearchMode;

    // static const char *SearchModes[];

    static const size_t ConstraintsNumber = 1;
    static const char *ConstraintsNames[];
    std::string itsConstraints[ConstraintsNumber];

    static bool MatchToPattern;

    static curlpp::Cleanup cleaner;

    PyObject *pafy;
    PyObject *pafy_fNew;
};

extern YTSearcher *myYTSearcher;

#endif /* NCMPCPP_YTSEARCHER_H */
