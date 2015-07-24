#include "ytsong.h"
#include <iomanip>

size_t calc_hash(const char *s, size_t seed = 0)
{
    for (; *s != '\0'; ++s)
        boost::hash_combine(seed, *s);
    return seed;
}

std::string YTSong::TagsSeparator = " | ";

std::string YTSong::get(mpd_tag_type type, unsigned idx) const
{
    return "";
}

YTSong::YTSong(const Video &video)
{
    m_video = video;
    m_hash = calc_hash(video.id.c_str());
}

std::string YTSong::getURI(unsigned idx) const
{
    return "";
}

std::string YTSong::getName(unsigned idx) const
{
    return "";
}

std::string YTSong::getDirectory(unsigned idx) const
{
    return "";
}

std::string YTSong::getArtist(unsigned idx) const
{
    return "";
}

std::string YTSong::getTitle(unsigned idx) const
{
    if (idx > 0) return "";
    return m_video.title;
}

std::string YTSong::getAlbum(unsigned idx) const
{
    return "";
}

std::string YTSong::getAlbumArtist(unsigned idx) const
{
    return "";
}

std::string YTSong::getTrack(unsigned idx) const
{
    return "";
}

std::string YTSong::getTrackNumber(unsigned idx) const
{
    return "";
}

std::string YTSong::getDate(unsigned idx) const
{
    return "";
}

std::string YTSong::getGenre(unsigned idx) const
{
    return "";
}

std::string YTSong::getComposer(unsigned idx) const
{
    return "";
}

std::string YTSong::getPerformer(unsigned idx) const
{
    return "";
}

std::string YTSong::getDisc(unsigned idx) const
{
    return "";
}

std::string YTSong::getComment(unsigned idx) const
{
    return "";
}

std::string YTSong::getLength(unsigned idx) const
{
    if (idx > 0)
        return "";
    unsigned len = getDuration();
    if (len > 0)
    {
        return ShowTime(len);
    }
    else
    {
        return "-:--";
    }
}

std::string YTSong::getPriority(unsigned idx) const
{
    return "";
}

std::string YTSong::getTags(GetFunction f) const
{
    unsigned idx = 0;
    std::string result;
    for (std::string tag; !(tag = (this->*f)(idx)).empty(); ++idx)
    {
        if (!result.empty())
            result += TagsSeparator;
        result += tag;
    }
    return result;
}

unsigned YTSong::getDuration() const
{
    return std::get<0>(m_video.duration) * 3600 + std::get<1>(m_video.duration) * 60 + std::get<2>(m_video.duration);
}

unsigned YTSong::getPosition() const
{
    return 0;
}

unsigned YTSong::getID() const
{
    return 0;
}

unsigned YTSong::getPrio() const
{
    return 0;
}

time_t YTSong::getMTime() const
{
    return 0;
}

bool YTSong::isFromDatabase() const
{
    return false;
}

bool YTSong::isStream() const
{
    return false;
}

bool YTSong::empty() const
{
    return false;
}

std::string YTSong::ShowTime(unsigned length)
{
    int hours = length / 3600;
    length -= hours * 3600;
    int minutes = length / 60;
    length -= minutes * 60;
    int seconds = length;

    std::ostringstream result;
    if (hours > 0)
    {
        result << hours << ":" << std::setfill('0') << std::setw(2) << minutes << ":" << seconds;
    }
    else
    {
        result << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    }
    return result.str();
}
