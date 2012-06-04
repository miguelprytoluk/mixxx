// engineworker.h
// Created 6/2/2010 by RJ Ryan (rryan@mit.edu)

#ifndef ENGINEWORKER_H
#define ENGINEWORKER_H

#include <QObject>
#include <QRunnable>

// EngineWorker is an interface for running background processing work when the
// audio callback is not active. While the audio callback is active, an
// EngineWorker can emit its workReady signal, and an EngineWorkerManager will
// schedule it for running after the audio callback has completed.

class EngineWorker : public QObject, public QRunnable {
    Q_OBJECT
  public:
    EngineWorker();
    virtual ~EngineWorker();

    virtual void run();

  signals:
    void workReady(EngineWorker* worker);
    void workStarting(EngineWorker* worker);
    void workDone(EngineWorker* worker);
};

#endif /* ENGINEWORKER_H */
