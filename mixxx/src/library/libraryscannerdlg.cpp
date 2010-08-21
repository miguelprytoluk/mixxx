/***************************************************************************
                          LibraryScannerDlg.cpp  -  shows library scanning
                                                       progress
                             -------------------
    begin                : 11/27/2007
    copyright            : (C) 2007 Albert Santoni and Adam Davison
    email                : gamegod \a\t users.sf.net
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <QtCore>
#include <QtDebug>
#include <QtGui>
#include "libraryscannerdlg.h"

LibraryScannerDlg::LibraryScannerDlg()
{
    m_bCancelled = false;

    m_progress = new QWidget();
    m_layout = new QVBoxLayout();
    m_label = new QLabel(tr("It's taking Mixxx a minute to scan your music library, please wait..."));
    m_layout->addWidget(m_label);
    m_cancel = new QPushButton(tr("Cancel"), m_progress);
    m_layout->addWidget(m_cancel);
    m_current = new QLabel();
    m_current->setMaximumWidth(600);
    m_current->setWordWrap(true);
    m_layout->addWidget(m_current);
    m_progress->setLayout(m_layout);

    connect(m_cancel, SIGNAL(clicked()), this, SLOT(slotCancel()));

    m_timer.start();
}

LibraryScannerDlg::~LibraryScannerDlg()
{
    delete m_current;
    delete m_cancel;
    delete m_progress;
    delete m_layout;
    delete m_label;
}

void LibraryScannerDlg::slotUpdate(QString path) {

    //qDebug() << "LibraryScannerDlg slotUpdate" << m_timer.elapsed();
    if (!m_bCancelled && m_timer.elapsed() > 2000) {
        m_progress->setVisible(true);
    }

    if (m_progress->isVisible()) {
        m_current->setText("Scanning: " + path);
        //m_current->repaint();
    }
}

void LibraryScannerDlg::slotCancel()
{
    qDebug() << "Cancelling library scan...";
    m_bCancelled = true;

    emit(scanCancelled());

    // Need to use close() or else if you close the Mixxx window and then hit
    // Cancel, Mixxx will not shutdown.

    //m_progress->setVisible(false);
    m_progress->close();
}

void LibraryScannerDlg::slotScanFinished()
{
    m_bCancelled = true; //Raise this flag to prevent any
                         //latent slotUpdates() from showing the dialog again.

    // Need to use close() or else if you close the Mixxx window and then hit
    // Cancel, Mixxx will not shutdown.
    //m_progress->setVisible(false);
    m_progress->close();
}
