#ifndef RADIOPLUGIN_H
#define RADIOPLUGIN_H

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <QtGui/QPushButton>

#include "crad/crad_interface.h"
#include "crad/qndriver.h"

namespace Msg
{
    class ChumbyRadio : public QObject
    {
        Q_OBJECT
    public:
        ChumbyRadio();
        QString getStation();
        QSettings* getSettings();

    public slots:
        void play();
        void pause();
        void seekUp();
        void seekDown();
        void stepUp();
        void stepDown();
        void setStation(double station);
        void rdsUpdate();
        void getPreset();
        void setPreset();

    signals:
        void stationChanged(QString);
        void radioText1Changed(QString);
        void radioText2Changed(QString);

    private:
        void refreshCrad();
        crad_t *p_crad;
        QSettings* settings;
        QTimer* timer;
        bool rds;
    };

    class LongPressButton : public QPushButton
    {
        Q_OBJECT

    public:
        LongPressButton();

    signals:
        void longPressed();
        void shortPressed();

    private slots:
        void press();
        void longPress();
        void shortPress();

    private:
        QTimer* mTimer;
        bool clickable;
    };

    class PresetButton : public LongPressButton
    {
        Q_OBJECT

    public:
        PresetButton(QString preset, QSettings* settings);
        double getPreset();
        QString getKey();
        void setPreset(double freq);

    private:
        double mPreset;
        QSettings* mSettings;
        QString mKey;
    };
}
#endif // RADIOPLUGIN_H
