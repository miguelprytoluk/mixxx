#ifndef LIGHTCONTROLLER_H
#define LIGHTCONTROLLER_H

#include <QColor>
#include <QString>
#include <QObject>
#include <QList>
#include <QMutex>

#include <aubio/aubio.h>
#include <lo/lo.h>

#include "defs.h"

#include "lights/featurestate.h"

class Light;
class ControlGroup;
class ColorGenerator;
class LightManager;
class DMXLightManager;
class LightBrickManager;

class LightController : public QObject {
    Q_OBJECT
  public:
    LightController();
    virtual ~LightController();

    void process(SAMPLE* pBuffer, int iFramesPerBuffer);

    void setColor(QColor color);

    ControlGroup* getControlGroup(int i);
    int numControlGroups();

    Light* getLight(int i);
    int numLights();

    ColorGenerator* getColorGenerator(int i);
    int numColorGenerators();
    void addColorGenerator(ColorGenerator* pGenerator);

  signals:
    void stateUpdated();

  private:
    void process_buffer();

    // OSC address
    QString m_base_location;
    lo_address m_osc_destination;

    QMutex m_mutex;

    // aubio structures
    aubio_fft_t* m_aubio_fft;
    aubio_onset_t* m_aubio_onset;
    aubio_tempo_t* m_aubio_tempo;
    aubio_pitch_t* m_aubio_pitch;

    int m_iCurInput;
    fvec_t* m_input_buf;
    fvec_t* m_tempo_output;
    fvec_t* m_onset_output;
    fvec_t* m_pitch_output;
    cvec_t* m_fft_output;

    FeatureState m_features;

    DMXLightManager* m_pDMXManager;
    LightBrickManager* m_pLightBrickManager;

    QList<LightManager*> m_lightManagers;
    QList<Light*> m_lights;
    QList<ControlGroup*> m_controlGroups;
    QList<ColorGenerator*> m_colorGenerators;

  public:
    // Dirty dirty
    static LightController* sInstance;
};

#endif /* LIGHTCONTROLLER_H */
