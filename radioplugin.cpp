#include <typeinfo>
#include <iostream>

#include <QLabel>
#include <QtGui/QHBoxLayout>
#include <QtCore/QDebug>

#include "radioplugin.h"

#include "../../controlpanel/plugin.h"

#include "../../controlpanel/musiccontrol.h"

namespace Msg
{
    ChumbyRadio::ChumbyRadio()
    {
        p_crad = 0;
        qDebug() << "creating ChumbyRadio";
        settings = new QSettings("/mnt/usb/radio.conf", QSettings::NativeFormat);

        qDebug() << "refreshing p_crad ...";
        int ret = crad_refresh(p_crad, CRAD_DEFAULT_DEVICE_PATH);
        qDebug() << "done";

        if ( CRAD_FAILED(ret) )
        {
            qDebug() << "Refreshing radio handlers failed!";

            crad_info_t crad_info = { 0 };

            int ret = crad_create(&crad_info, &p_crad);

            if ( CRAD_FAILED(ret) )
            {
                qDebug() << "Initializing radio failed!";
                return;
            }
        }

        qDebug() << "loading setting";

        if ( settings->contains("current") )
        {
            qDebug() << "Station found in config";
            this->setStation( settings->value("current").toDouble() );
        }

        qDebug() << "setting up rds timer";

        timer = new QTimer();
        timer->setInterval(5000);
        connect( timer, SIGNAL( timeout() ), this, SLOT( rdsUpdate() ) );
    }

    void ChumbyRadio::refreshCrad()
    {
        int ret = crad_refresh(p_crad, CRAD_DEFAULT_DEVICE_PATH);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Refreshing radio handlers failed (" << ret << ")!";
        }
    }

    void ChumbyRadio::play()
    {
        qDebug() << "Play!";
        refreshCrad();
        MusicControl* mc = &MusicControl::getInstance();
        //crad_set_power(p_crad, 1);
        mc->alsa_select_input(INPUT_LINE1);
        snd_pcm_t* capture;
        mc->alsa_open(&capture, SND_PCM_STREAM_CAPTURE);
        mc->play(capture);
        crad_set_rds(p_crad, 1);
        rds = false;
        timer->start();
    }

    void ChumbyRadio::pause()
    {
        refreshCrad();
        timer->stop();
        MusicControl* mc = &MusicControl::getInstance();
        //crad_set_power(p_crad, 0);
        mc->stop();
        mc->alsa_close();
        //crad_set_rds(p_crad, 0);
        qDebug() << "Radio stopped!";
    }

    void ChumbyRadio::stepUp()
    {
        qDebug() << "stepUp";
        refreshCrad();

        int ret = crad_tune_radio(p_crad, p_crad->frequency/100 + 0.05);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Tuning radio to" << (p_crad->frequency/100 + 0.05) << "failed:" << CRAD_RETURN_CODE_LOOKUP[ret];
            return;
        }

        settings->setValue("current", this->getStation());
        settings->sync();
        emit stationChanged(this->getStation());
    }

    void ChumbyRadio::stepDown()
    {
        qDebug() << "stepDown";
        refreshCrad();

        int ret = crad_tune_radio(p_crad, p_crad->frequency/100 - 0.05);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Tuning radio to" << (p_crad->frequency/100 - 0.05) << "failed:" << CRAD_RETURN_CODE_LOOKUP[ret];
            return;
        }

        settings->setValue("current", this->getStation());
        settings->sync();
        emit stationChanged(this->getStation());
    }

    void ChumbyRadio::seekDown()
    {
        emit stationChanged("Searching...");
        refreshCrad();

        unsigned int seek_strength = CRAD_DEFAULT_SEEK_STRENGTH;

        int ret = crad_seek_radio(p_crad, CRAD_SEEK_DIR_DOWN, seek_strength);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Seeking failed: " << CRAD_RETURN_CODE_LOOKUP[ret];
            return;
        }

        settings->setValue("current", this->getStation());
        settings->sync();
        emit stationChanged(this->getStation());
    }

    void ChumbyRadio::seekUp()
    {
        emit stationChanged("Searching...");
        refreshCrad();

        unsigned int seek_strength = CRAD_DEFAULT_SEEK_STRENGTH;

        int ret = crad_seek_radio(p_crad, CRAD_SEEK_DIR_UP, seek_strength);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Seeking failed: " << CRAD_RETURN_CODE_LOOKUP[ret];
            return;
        }

        settings->setValue("current", this->getStation());
        settings->sync();
        emit stationChanged(this->getStation());
    }

    void ChumbyRadio::setStation( double station )
    {
        refreshCrad();

        int ret = crad_tune_radio(p_crad, station);

        if ( CRAD_FAILED( ret ) )
        {
            qDebug() << "Tuning radio failed: " << CRAD_RETURN_CODE_LOOKUP[ret];
            return;
        }

        settings->setValue("current", this->getStation());
        settings->sync();
        emit stationChanged(this->getStation());
    }

    void ChumbyRadio::rdsUpdate()
    {
        refreshCrad();
        pthread_mutex_lock(&p_crad->rds_mutex);
        QString station = QString((char*)p_crad->rds_data.program_service_name);
        if ( ( !station.isEmpty() ) && rds )
            emit stationChanged(station);
        else
            emit stationChanged(this->getStation());
        emit radioText1Changed(QString((char*)p_crad->rds_data.radiotext[0]));
        emit radioText2Changed(QString((char*)p_crad->rds_data.radiotext[1]));
        qDebug() << "RDS:"
                 << QString((char*)p_crad->rds_data.name)
                 << QString((char*)p_crad->rds_data.radiotext[0])
                 << QString((char*)p_crad->rds_data.program_service_name)
                 << QString((char*)p_crad->rds_data.radiotext_filled[0]);
        pthread_mutex_unlock(&p_crad->rds_mutex);
        rds = !rds;
        timer->start();
    }

    QString ChumbyRadio::getStation()
    {
        double station = p_crad->frequency;

        return QString::number(station/100, 'f', 2);
    }

    void ChumbyRadio::getPreset()
    {
        qDebug() << "foobar";
        PresetButton* button = qobject_cast<PresetButton*>(sender());
        double freq = button->getPreset();
        qDebug() << "Got Preset" << button->getKey() << ":" << freq;
        if ( freq > 0 )
            this->setStation(freq);
    }

    void ChumbyRadio::setPreset()
    {
        qDebug() << "foobar2";
        PresetButton* button = qobject_cast<PresetButton*>(sender());
        qDebug() << "Setting Preset" << button->getKey() << "to" << p_crad->frequency/100;
        button->setPreset(p_crad->frequency/100);
    }

    QSettings* ChumbyRadio::getSettings()
    {
        return settings;
    }

    LongPressButton::LongPressButton()
    {
        mTimer = new QTimer();
        mTimer->setInterval(1000);
        clickable = true;

        connect( this, SIGNAL( pressed() ), this, SLOT( press() ) );
        connect( this, SIGNAL( released() ), this, SLOT( shortPress() ) );
        connect( mTimer, SIGNAL( timeout() ), this, SLOT( longPress() ) );
    }

    void LongPressButton::press()
    {
        qDebug() << "button pressed...";
        mTimer->start();
    }

    void LongPressButton::longPress()
    {
        qDebug() << "longpress";
        mTimer->stop();
        clickable = false;
        emit this->longPressed();
        mTimer->setInterval(500);
        mTimer->start();
    }

    void LongPressButton::shortPress()
    {
        qDebug() << "shortpress";
        mTimer->stop();
        if ( clickable )
            emit this->shortPressed();
    }

    PresetButton::PresetButton(QString preset, QSettings *settings)
        : LongPressButton()
    {
        mSettings = settings;
        mKey = preset;
        qDebug() << "Preset" << *(preset.end()-1);
        if ( settings->contains(mKey) )
        {
            mPreset = settings->value(mKey).toDouble();
            this->setText(QString(*(preset.end()-1)) + ": " + QString::number(mPreset, 'f', 2));
        }
        else
        {
            mPreset = 0.0;
            this->setText(*(preset.end()-1));
        }
    }

    QString PresetButton::getKey()
    {
        return mKey;
    }

    void PresetButton::setPreset(double freq)
    {
        mPreset = freq;
        mSettings->setValue(mKey, QString::number(mPreset, 'f', 2));
        this->setText(QString(*(mKey.end()-1)) + ": " + QString::number(mPreset, 'f', 2));
    }

    double PresetButton::getPreset()
    {
        return mPreset;
    }

    // Class PlugInFun inherits from PlugIn
    // and shows the world when one is created/destroyed
    // and supplies a Show() method that announces its name

    class RadioPlugin : public Plugin
    {
     public:
            RadioPlugin()
            {
                    std::cout << "PlugInFun created" << std::endl;
            }

            virtual ~RadioPlugin()
            {
                    std::cout << "PlugInFun destroyed" << std::endl;
            }

            virtual std::string getName()
            {
                    //std::cout << "I am a Fun PlugIn" << std::endl;
                return "Radio";
            }

            virtual void init()
            {
                radio = new ChumbyRadio();
                qDebug() << "ChumbyRadio created";
            }

            virtual QWidget* getWidget()
            {
                QWidget* widget = new QWidget();
                QVBoxLayout* layout = new QVBoxLayout();
                QHBoxLayout* freq_layout = new QHBoxLayout();
                QVBoxLayout* radiotext_layout = new QVBoxLayout();
                QHBoxLayout* ctrl_layout = new QHBoxLayout();
                QGridLayout* preset_layout = new QGridLayout();

                layout->addLayout(freq_layout);
                layout->addLayout(radiotext_layout);
                layout->addLayout(ctrl_layout);
                layout->addLayout(preset_layout);

                QPushButton* playButton = new QPushButton();
                playButton->setText("Play");
                QPushButton* pauseButton = new QPushButton();
                pauseButton->setText("Pause");
                LongPressButton* seekUpButton = new LongPressButton();
                seekUpButton->setText(">");
                LongPressButton* seekDownButton = new LongPressButton();
                seekDownButton->setText("<");

                QLabel* station = new QLabel();
                station->setText(radio->getStation());

                QLabel* radiotext1 = new QLabel();
                QLabel* radiotext2 = new QLabel();

                PresetButton* preset1 = new PresetButton("preset1", radio->getSettings());
                PresetButton* preset2 = new PresetButton("preset2", radio->getSettings());
                PresetButton* preset3 = new PresetButton("preset3", radio->getSettings());
                PresetButton* preset4 = new PresetButton("preset4", radio->getSettings());
                PresetButton* preset5 = new PresetButton("preset5", radio->getSettings());
                PresetButton* preset6 = new PresetButton("preset6", radio->getSettings());

                freq_layout->addWidget(seekDownButton);
                freq_layout->addWidget(station);
                freq_layout->addWidget(seekUpButton);
                radiotext_layout->addWidget(radiotext1);
                radiotext_layout->addWidget(radiotext2);
                ctrl_layout->addWidget(playButton);
                ctrl_layout->addWidget(pauseButton);
                preset_layout->addWidget(preset1, 0, 0);
                preset_layout->addWidget(preset2, 0, 1);
                preset_layout->addWidget(preset3, 0, 2);
                preset_layout->addWidget(preset4, 1, 0);
                preset_layout->addWidget(preset5, 1, 1);
                preset_layout->addWidget(preset6, 1, 2);

                widget->setLayout(layout);

                connect( playButton, SIGNAL( clicked() ), radio, SLOT( play() ) );
                connect( pauseButton, SIGNAL( clicked() ), radio, SLOT( pause() ) );
                connect( seekUpButton, SIGNAL( shortPressed() ), radio, SLOT( seekUp() ) );
                connect( seekDownButton, SIGNAL( shortPressed() ), radio, SLOT( seekDown() ) );
                connect( seekUpButton, SIGNAL( longPressed() ), radio, SLOT( stepUp() ) );
                connect( seekDownButton, SIGNAL( longPressed() ), radio, SLOT( stepDown() ) );
                connect( radio, SIGNAL( stationChanged(QString) ), station, SLOT( setText(QString) ) );
                connect( radio, SIGNAL( radioText1Changed(QString) ), radiotext1, SLOT( setText(QString) ) );
                connect( radio, SIGNAL( radioText2Changed(QString) ), radiotext2, SLOT( setText(QString) ) );

                connect( preset1, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset2, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset3, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset4, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset5, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset6, SIGNAL( shortPressed() ), radio, SLOT( getPreset() ) );
                connect( preset1, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );
                connect( preset2, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );
                connect( preset3, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );
                connect( preset4, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );
                connect( preset5, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );
                connect( preset6, SIGNAL( longPressed() ), radio, SLOT( setPreset() ) );

                return widget;
            }

    private:
            ChumbyRadio *radio;
    };

    //
    // The PlugInFunFactory class inherits from PlugInFactory
    // and creates a PlugInFun object when requested.
    //


    class RadioPluginFactory : public PluginFactory
    {
     public:
            RadioPluginFactory()
            {
            }

            ~RadioPluginFactory()
            {
            }

            virtual Plugin * CreatePlugin()
            {
                    return new RadioPlugin;
            }

    };

    //
    // The "C" linkage factory0() function creates the PlugInFunFactory
    // class for this library
    //

    extern "C" void * factory0( void )
    {
            return new RadioPluginFactory;
    }
}
