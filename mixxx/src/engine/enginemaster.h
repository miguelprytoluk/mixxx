/***************************************************************************
                          enginemaster.h  -  description
                             -------------------
    begin                : Sun Apr 28 2002
    copyright            : (C) 2002 by
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ENGINEMASTER_H
#define ENGINEMASTER_H

#include "engine/enginestate.h"
#include "engine/callbackcontrolmanager.h"
#include "engine/engineobject.h"
#include "engine/enginechannel.h"
#include "soundmanagerutil.h"
#include "recording/recordingmanager.h"

class EngineWorkerScheduler;
class EngineBuffer;
class EngineChannel;
class EngineClipping;
class EngineFlanger;
#ifdef __LADSPA__
class EngineLADSPA;
#endif
class EngineVuMeter;
class EngineVinylSoundEmu;
class EngineSideChain;
class SyncWorker;

class EngineMaster : public EngineObject, public AudioSource {
    Q_OBJECT
  public:
    EngineMaster(ConfigObject<ConfigValue>* pConfig,
                 const char* pGroup,
                 bool bEnableSidechain);
    virtual ~EngineMaster();

    // Get access to the sample buffers. None of these are thread safe. Only to
    // be called by SoundManager.
    const CSAMPLE* buffer(AudioOutput output) const;

    void process(const CSAMPLE *, const CSAMPLE *pOut, const int iBufferSize);

    // Add an EngineChannel to the mixing engine. This is not thread safe --
    // only call it before the engine has started mixing.
    void addChannel(EngineChannel* pChannel);
    EngineChannel* getChannel(QString group);
    static inline double gainForOrientation(EngineChannel::ChannelOrientation orientation,
                                            double leftGain,
                                            double centerGain,
                                            double rightGain) {
        switch (orientation) {
            case EngineChannel::LEFT:
                return leftGain;
            case EngineChannel::RIGHT:
                return rightGain;
            case EngineChannel::CENTER:
            default:
                return centerGain;
        }
    }

    // These are really only exposed for tests to use.
    const CSAMPLE* getMasterBuffer() const;
    const CSAMPLE* getHeadphoneBuffer() const;
    const CSAMPLE* getDeckBuffer(unsigned int i) const;
    const CSAMPLE* getChannelBuffer(QString name) const;

    inline EngineState* getState() {
        return &m_state;
    }

    inline EngineSideChain* getSideChain() const {
        return m_pSideChain;
    }

  private:
    struct ChannelInfo {
        EngineChannel* m_pChannel;
        CSAMPLE* m_pBuffer;
        CallbackControl* m_pVolumeControl;
    };

    class GainCalculator {
      public:
        virtual double getGain(ChannelInfo* pChannelInfo) = 0;
    };
    class ConstantGainCalculator : public GainCalculator {
      public:
        inline double getGain(ChannelInfo* pChannelInfo) {
            Q_UNUSED(pChannelInfo);
            return m_dGain;
        }
        inline void setGain(double dGain) {
            m_dGain = dGain;
        }
      private:
        double m_dGain;
    };
    class OrientationVolumeGainCalculator : public GainCalculator {
      public:
        inline double getGain(ChannelInfo* pChannelInfo) {
            double channelVolume = pChannelInfo->m_pVolumeControl->get();
            double orientationGain = EngineMaster::gainForOrientation(
                pChannelInfo->m_pChannel->getOrientation(),
                m_dLeftGain, m_dCenterGain, m_dRightGain);
            return m_dVolume * channelVolume * orientationGain;
        }

        inline void setGains(double dVolume, double leftGain, double centerGain, double rightGain) {
            m_dVolume = dVolume;
            m_dLeftGain = leftGain;
            m_dCenterGain = centerGain;
            m_dRightGain = rightGain;
        }
      private:
        double m_dVolume, m_dLeftGain, m_dCenterGain, m_dRightGain;
    };

    void mixChannels(unsigned int channelBitvector, unsigned int maxChannels,
                     CSAMPLE* pOutput, unsigned int iBufferSize, GainCalculator* pGainCalculator);

    EngineState m_state;
    CallbackControlManager& m_callbackControlManager;
    CallbackTrackManager& m_callbackTrackManager;
    QList<ChannelInfo*> m_channels;

    CSAMPLE *m_pMaster, *m_pHead;

    EngineWorkerScheduler *m_pWorkerScheduler;

    CallbackControl* m_pMasterVolume;
    CallbackControl* m_pHeadVolume;
    CallbackControl* m_pMasterSampleRate;
    CallbackControl* m_pMasterLatency;
    CallbackControl* m_pMasterRate;
    CallbackControl* m_pMasterUnderflowCount;
    CallbackControl* m_pMasterAudioBufferSize;
    EngineClipping *clipping, *head_clipping;

#ifdef __LADSPA__
    EngineLADSPA *ladspa;
#endif
    EngineVuMeter *vumeter;
    EngineSideChain* m_pSideChain;

    CallbackControl* crossfader;
    CallbackControl* head_mix;
    CallbackControl* m_pBalance;
    CallbackControl* xFaderMode;
    CallbackControl* xFaderCurve;
    CallbackControl* xFaderCalibration;
    CallbackControl* xFaderReverse;

    ConstantGainCalculator m_headphoneGain;
    OrientationVolumeGainCalculator m_masterGain;
};

#endif
