/***************************************************************************
                          midiobjectwin.h  -  description
                             -------------------
    begin                : Thu Jul 4 2002
    copyright            : (C) 2002 by Tue & Ken Haste Andersen
    email                : haste@diku.dk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MIDIOBJECTWIN_H
#define MIDIOBJECTWIN_H

#include "midiobject.h"
#include "windows.h"
#include <QtCore>

/**
  *@author Tue & Ken Haste Andersen
  */

class MidiObjectWin : public MidiObject  {
public:
    MidiObjectWin();
    ~MidiObjectWin();
    void devOpen(QString device);
    void devClose(QString device);
    void handleMidi(char channel, char midicontrol, char midivalue, QString device);
    void sendShortMsg(unsigned int word);
    virtual void updateDeviceList();

    QMap<QString, HMIDIIN> handles;

protected:
    void run();
    void stop();

    HMIDIOUT outhandle;
};

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

#endif
