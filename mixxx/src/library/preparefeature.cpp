// preparefeature.cpp
// Created 8/23/2009 by RJ Ryan (rryan@mit.edu)
// Forked 11/11/2009 by Albert Santoni (alberts@mixxx.org)

#include <QtDebug>

#include "library/preparefeature.h"
#include "library/librarytablemodel.h"
#include "library/trackcollection.h"
#include "dlgprepare.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "mixxxkeyboard.h"

const QString PrepareFeature::m_sPrepareViewName = QString("Prepare");

PrepareFeature::PrepareFeature(QObject* parent,
                               ConfigObject<ConfigValue>* pConfig,
                               TrackCollection* pTrackCollection)
        : LibraryFeature(parent),
          m_pConfig(pConfig),
          m_pTrackCollection(pTrackCollection) {
}

PrepareFeature::~PrepareFeature() {
    // TODO(XXX) delete these
    //delete m_pLibraryTableModel;
}

QVariant PrepareFeature::title() {
    return tr("Analyze");
}

QIcon PrepareFeature::getIcon() {
    return QIcon(":/images/library/ic_library_prepare.png");
}

void PrepareFeature::bindWidget(WLibrarySidebar* sidebarWidget,
                                WLibrary* libraryWidget,
                                MixxxKeyboard* keyboard) {
    DlgPrepare* pPrepareView = new DlgPrepare(libraryWidget,
                                              m_pConfig,
                                              m_pTrackCollection);
    connect(pPrepareView, SIGNAL(loadTrack(TrackPointer)),
            this, SIGNAL(loadTrack(TrackPointer)));
    connect(pPrepareView, SIGNAL(loadTrackToPlayer(TrackPointer, int)),
            this, SIGNAL(loadTrackToPlayer(TrackPointer, int)));
    pPrepareView->installEventFilter(keyboard);

    libraryWidget->registerView(m_sPrepareViewName, pPrepareView);
}

QAbstractItemModel* PrepareFeature::getChildModel() {
    return &m_childModel;
}

void PrepareFeature::activate() {
    //qDebug() << "PrepareFeature::activate()";
    emit(switchToView(m_sPrepareViewName));
}

void PrepareFeature::activateChild(const QModelIndex& index) {
}

void PrepareFeature::onRightClick(const QPoint& globalPos) {
}

void PrepareFeature::onRightClickChild(const QPoint& globalPos,
                                            QModelIndex index) {
}

bool PrepareFeature::dropAccept(QUrl url) {
    return false;
}

bool PrepareFeature::dropAcceptChild(const QModelIndex& index, QUrl url) {
    return false;
}

bool PrepareFeature::dragMoveAccept(QUrl url) {
    return false;
}

bool PrepareFeature::dragMoveAcceptChild(const QModelIndex& index,
                                              QUrl url) {
    return false;
}
