#ifndef NCMPCPP_YTSONG_H
#define NCMPCPP_YTSONG_H

#include "interfaces.h"
#include "mpdpp.h"
#include "regex_filter.h"
#include "screen.h"
#include "song_list.h"

struct Video
{
    std::string title;
    std::string id;
    std::tuple<int, int, int> duration;
};

struct YTSong : MPD::Song
{
    struct Hash
    {
        size_t operator()(const YTSong &song) const {return song.m_hash;}
    };

    typedef std::string (MPD::Song::*GetFunction)(unsigned) const;

    YTSong() : m_hash(0) {}
    virtual ~YTSong() {}

    YTSong(const Video &video);
    // YTSong(YTSong &&rhs) : m_video(std::move(rhs.m_video)), m_hash(rhs.m_hash) { }
    YTSong &operator=(YTSong rhs)
    {
        m_video = std::move(rhs.m_video);
        m_hash = rhs.m_hash;
        return *this;
    }

    std::string get(mpd_tag_type type, unsigned idx = 0) const;

    virtual std::string getURI(unsigned idx = 0) const override;
    virtual std::string getName(unsigned idx = 0) const override;
    virtual std::string getDirectory(unsigned idx = 0) const override;
    virtual std::string getArtist(unsigned idx = 0) const override;
    virtual std::string getTitle(unsigned idx = 0) const override;
    virtual std::string getAlbum(unsigned idx = 0) const override;
    virtual std::string getAlbumArtist(unsigned idx = 0) const override;
    virtual std::string getTrack(unsigned idx = 0) const override;
    virtual std::string getTrackNumber(unsigned idx = 0) const override;
    virtual std::string getDate(unsigned idx = 0) const override;
    virtual std::string getGenre(unsigned idx = 0) const override;
    virtual std::string getComposer(unsigned idx = 0) const override;
    virtual std::string getPerformer(unsigned idx = 0) const override;
    virtual std::string getDisc(unsigned idx = 0) const override;
    virtual std::string getComment(unsigned idx = 0) const override;
    virtual std::string getLength(unsigned idx = 0) const override;
    virtual std::string getPriority(unsigned idx = 0) const override;

    virtual std::string getTags(GetFunction f) const override;

    virtual unsigned getDuration() const override;
    virtual unsigned getPosition() const override;
    virtual unsigned getID() const override;
    virtual unsigned getPrio() const override;
    virtual time_t getMTime() const override;

    virtual bool isFromDatabase() const override;
    virtual bool isStream() const override;

    virtual bool empty() const override;

    bool operator==(const YTSong &rhs) const
    {
        return m_hash == rhs.m_hash;
    }

    bool operator!=(const YTSong &rhs) const
    {
        return !(operator==(rhs));
    }

    static std::string ShowTime(unsigned length);

    static std::string TagsSeparator;

    Video m_video;
    size_t m_hash;
};

#endif
