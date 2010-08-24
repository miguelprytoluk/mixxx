#include <QtDebug>

#include "lights/lightbrick.h"

LightBrick::LightBrick(QObject* pParent, QString ip_address, QString port)
        : Light(pParent) {
    m_osc_destination = lo_address_new(ip_address.toAscii().data(),
                                       port.toAscii().data());
}

LightBrick::~LightBrick() {

}

void LightBrick::sync() {
    const char* location_format = "/light/color/set";
    QColor color = getColor();
    float r = color.redF();
    float g = color.greenF();
    float b = color.blueF();

    //qDebug() << "LIGHTBRICK Sending" << r << g << b;

    if (lo_send(m_osc_destination, location_format,
                "fff", r, g, b) < 0) {
        qDebug() << "OSC error " << lo_address_errno(m_osc_destination)
                 << ": " << lo_address_errstr(m_osc_destination);
    }
}
