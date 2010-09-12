

#ifndef TRACKDAO_H
#define TRACKDAO_H

#include <QFileInfo>
#include <QObject>
#include <QSet>
#include <QHash>
#include <QSqlDatabase>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QCache>

#include "trackinfoobject.h"
#include "library/dao/cuedao.h"
#include "library/dao/dao.h"

const QString LIBRARYTABLE_ID = "id";
const QString LIBRARYTABLE_ARTIST = "artist";
const QString LIBRARYTABLE_TITLE = "title";
const QString LIBRARYTABLE_ALBUM = "album";
const QString LIBRARYTABLE_YEAR = "year";
const QString LIBRARYTABLE_GENRE = "genre";
const QString LIBRARYTABLE_TRACKNUMBER = "tracknumber";
const QString LIBRARYTABLE_FILETYPE = "filetype";
const QString LIBRARYTABLE_LOCATION = "location";
const QString LIBRARYTABLE_COMMENT = "comment";
const QString LIBRARYTABLE_DURATION = "duration";
const QString LIBRARYTABLE_BITRATE = "bitrate";
const QString LIBRARYTABLE_BPM = "bpm";
const QString LIBRARYTABLE_CUEPOINT = "cuepoint";
const QString LIBRARYTABLE_URL = "url";
const QString LIBRARYTABLE_SAMPLERATE = "samplerate";
const QString LIBRARYTABLE_WAVESUMMARYHEX = "wavesummaryhex";
const QString LIBRARYTABLE_CHANNELS = "channels";
const QString LIBRARYTABLE_MIXXXDELETED = "mixxx_deleted";
const QString LIBRARYTABLE_DATETIMEADDED = "datetime_added";
const QString LIBRARYTABLE_HEADERPARSED = "header_parsed";

class TrackDAO : public QObject { //// public DAO {
Q_OBJECT
  public:
    //TrackDAO() {};
    TrackDAO(QSqlDatabase& database, CueDAO& cueDao);
    virtual ~TrackDAO();
    void setDatabase(QSqlDatabase& database) { m_database = database; };

    void initialize();
    int getTrackId(QString absoluteFilePath);
    bool trackExistsInDatabase(QString absoluteFilePath);
    QString getTrackLocation(int id);
    int addTrack(QString absoluteFilePath);
    int addTrack(QFileInfo& fileInfo);
    void removeTrack(int id);
    void unremoveTrack(int trackId);
    TrackPointer getTrack(int id) const;
    bool isDirty(int trackId);

    // Scanning related calls. Should be elsewhere or private somehow.
    void markTrackLocationAsVerified(QString location);
    void markTracksInDirectoryAsVerified(QString directory);
    void invalidateTrackLocationsInLibrary(QString libraryPath);
    void markUnverifiedTracksAsDeleted();
    void markTrackLocationsAsDeleted(QString directory);
    void detectMovedFiles();

  signals:
    void trackDirty(int trackId);
    void trackClean(int trackId);
    void trackChanged(int trackId);

  public slots:
    // The public interface to the TrackDAO requires a TrackPointer so that we
    // have a guarantee that the track will not be deleted while we are working
    // on it. However, private parts of TrackDAO can use the raw saveTrack(TIO*)
    // call.
    void saveTrack(TrackPointer pTrack);

    // TrackDAO provides a cache of TrackInfoObject's that have been requested
    // via getTrack(). saveDirtyTracks() saves all cached tracks marked dirty
    // to the database.
    void saveDirtyTracks();

    // Clears the cached TrackInfoObjects, which can be useful when the 
    // underlying database tables change (eg. during a library rescan,
    // we might detect that a track has been moved and modify the update
    // the tables directly.)
    void clearCache();

  private slots:
    void slotTrackDirty();
    void slotTrackChanged();
    void slotTrackSave();

  private:
    void saveTrack(TrackInfoObject* pTrack);
    void updateTrack(TrackInfoObject* pTrack);
    void addTrack(TrackInfoObject* pTrack);
    TrackPointer getTrackFromDB(QSqlQuery &query) const;
    QString absoluteFilePath(QString location);

    // Called when the TIO reference count drops to 0
    static void deleteTrack(TrackInfoObject* pTrack);

    // Prevents evil copy constructors! (auto-generated ones by the compiler
    // that don't compile)
    TrackDAO(TrackDAO&);
    bool operator=(TrackDAO&);
    /**
       NOTE: If you get a compile error complaining about these, it means you're
             copying a track DAO, which is probably not what you meant to
             do. Did you declare:
               TrackDAO m_trackDAO;
             instead of:
               TrackDAO &m_trackDAO;
             Go back and check your code...
       -- Albert Nov 1/2009
     */

    QSqlDatabase &m_database;
    CueDAO &m_cueDao;
    mutable QHash<int, TrackWeakPointer> m_tracks;
    mutable QSet<int> m_dirtyTracks;
    mutable QCache<int,TrackPointer> m_trackCache;
};

#endif //TRACKDAO_H
