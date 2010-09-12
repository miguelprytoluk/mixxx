
#ifndef WGLWAVEFORMVIEWER_H
#define WGLWAVEFORMVIEWER_H

#include <qgl.h>
#include <QList>
#include <QEvent>
#include <QDateTime>
#include <QMutex>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTimerEvent>

#include "wwidget.h"
#include "defs.h"

class EngineBuffer;
class WaveformRenderer;

class WGLWaveformViewer : public QGLWidget
{
    Q_OBJECT
  public:
    WGLWaveformViewer(const char *group, WaveformRenderer* pWaveformRenderer,
                      QWidget *pParent=0, const QGLWidget *pShareWidget = 0,
                      Qt::WFlags f = 0);
    virtual ~WGLWaveformViewer();

    bool directRendering();
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void setup(QDomNode node);
    bool eventFilter(QObject *o, QEvent *e);

  public slots:
    void setValue(double);
    void refresh();

  signals:
    void valueChangedLeftDown(double);
    void valueChangedRightDown(double);
    void trackDropped(QString filename, QString group);

  protected:
    void paintEvent(QPaintEvent* event);

  private:
    /** Used in mouse event handler */
    int m_iMouseStart;

    /** Waveform Renderer does all the work for us */
    WaveformRenderer *m_pWaveformRenderer;

    bool m_painting;
    QMutex m_paintMutex;

    const char *m_pGroup;

};

#endif
