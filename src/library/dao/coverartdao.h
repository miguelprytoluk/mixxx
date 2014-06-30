#ifndef COVERARTDAO_H
#define COVERARTDAO_H

#include <QSqlDatabase>

#include "configobject.h"
#include "library/dao/dao.h"

const QString COVERART_TABLE = "cover_art";
const QString COVERARTTABLE_ID = "id";
const QString COVERARTTABLE_LOCATION = "location";

class CoverArtDAO : public DAO {
  public:
    CoverArtDAO(QSqlDatabase& database);
    virtual ~CoverArtDAO();
    void setDatabase(QSqlDatabase& database) { m_database = database; }

    void initialize();

    void deleteUnusedCoverArts();
    int getCoverArtId(QString coverLocation);
    int saveCoverLocation(QString coverLocation);

    struct CoverArtInfo {
        int trackId;
        QString coverLocation;
        QString album;
        QString trackBaseName;
        QString trackDirectory;
        QString trackLocation;
    };

    CoverArtInfo getCoverArtInfo(int trackId);

  private:
    QSqlDatabase& m_database;
    ConfigObject<ConfigValue>* m_pConfig;
};

#endif // COVERARTDAO_H