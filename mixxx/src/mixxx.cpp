/***************************************************************************
                          mixxx.cpp  -  description
                             -------------------
    begin                : Mon Feb 18 09:48:17 CET 2002
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
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

#include <QtDebug>
#include <QtCore>
#include <QtGui>

#include "widget/wknob.h"
#include "widget/wslider.h"
#include "widget/wpushbutton.h"
#include "widget/woverview.h"
#include "mixxx.h"
#include "controlnull.h"
#include "controlpotmeter.h"
#include "controlobjectthreadmain.h"
#include "engine/enginebuffer.h"
#include "engine/enginemaster.h"
#include "engine/enginechannel.h"
#include "engine/enginevumeter.h"
#include "trackinfoobject.h"
#include "dlgabout.h"
#include "waveform/waveformrenderer.h"
#include "soundsourceproxy.h"

#include "analyserqueue.h"
#include "player.h"
#include "playermanager.h"
#include "wtracktableview.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/libraryscanner.h"
#include "library/legacylibraryimporter.h"

#include "soundmanager.h"
#include "defs_urls.h"
#include "recording/defs_recording.h"

#include "midi/mididevicemanager.h"
#include "defs_version.h"
#include "upgrade.h"

#include "build.h" //#defines of details of the build set up (flags, repo number, etc). This isn't a real file, SConscript generates it and it probably gets placed in $PLATFORM_build/. By including this file here and only here we make sure that updating src or changing the build flags doesn't force a rebuild of everything

#ifdef __IPOD__
#include "wtracktableview.h"
#include "gpod/itdb.h"
#endif

#ifdef __C_METRICS__
#include <cmetrics.h>
#include "defs_mixxxcmetrics.h"
#endif

#include "lights/lightcontroller.h"

extern "C" void crashDlg()
{
    QMessageBox::critical(0, "Mixxx", "Mixxx has encountered a serious error and needs to close.");
}


MixxxApp::MixxxApp(QApplication * a, struct CmdlineArgs args)
{
    app = a;

    QString buildRevision, buildFlags;
    #ifdef BUILD_REV
      buildRevision = BUILD_REV;
    #endif

    #ifdef BUILD_FLAGS
      buildFlags = BUILD_FLAGS;
    #endif

    if (buildRevision.trimmed().length() > 0) {
        if (buildFlags.trimmed().length() > 0)
            buildRevision = "(bzr r" + buildRevision + "; built on: " + __DATE__ + " @ " + __TIME__ + "; flags: " + buildFlags.trimmed() + ") ";
        else
            buildRevision = "(bzr r" + buildRevision + "; built on: " + __DATE__ + " @ " + __TIME__ + ") ";
    }

    qDebug() << "Mixxx" << VERSION << buildRevision << "is starting...";
    QCoreApplication::setApplicationName("Mixxx");
    QCoreApplication::setApplicationVersion(VERSION);
#if defined(AMD64) || defined(EM64T) || defined(x86_64)
    setWindowTitle(tr("Mixxx " VERSION " x64"));
#elif defined(IA64)
    setWindowTitle(tr("Mixxx " VERSION " Itanium"));
#else
    setWindowTitle(tr("Mixxx " VERSION));
#endif
    setWindowIcon(QIcon(":/images/ic_mixxx_window.png"));

    //Reset pointer to players
    soundmanager = 0;
    prefDlg = 0;
    m_pMidiDeviceManager = 0;

    // Check to see if this is the first time this version of Mixxx is run after an upgrade and make any needed changes.
    Upgrade upgrader;
    config = upgrader.versionUpgrade();
    bool bFirstRun = upgrader.isFirstRun();
    bool bUpgraded = upgrader.isUpgraded();
    QString qConfigPath = config->getConfigPath();

#ifdef __C_METRICS__
    // Initialize Case Metrics if User is OK with that
    QString metricsAgree = config->getValueString(ConfigKey("[User Experience]","AgreedToUserExperienceProgram"));

    if (metricsAgree.isEmpty() || (metricsAgree != "yes" && metricsAgree != "no")) {
      metricsAgree = "no";
      int dlg = -1;
      while (dlg != 0 && dlg != 1) {
         dlg = QMessageBox::question(this, "Mixxx", "Mixxx's development is driven by community feedback.  At your discretion, Mixxx can automatically send data on your user experience back to the developers. Would you like to help us make Mixxx better by enabling this feature?", "Yes", "No", "Privacy Policy", 0, -1);
       switch (dlg) {
           case 0: metricsAgree = "yes";
         case 1: break;
           default: //show privacy policy
                QMessageBox::information(this, "Mixxx: Privacy Policy", "Mixxx's development is driven by community feedback.  In order to help improve future versions Mixxx will with your permission collect information on your hardware and usage of Mixxx.  This information will primarily be used to fix bugs, improve features, and determine the system requirements of later versions.  Additionally this information may be used in aggregate for statistical purposes.\n\nThe hardware information will include:\n\t- CPU model and features\n\t- Total/Available Amount of RAM\n\t- Available disk space\n\t- OS version\n\nYour usage information will include:\n\t- Settings/Preferences\n\t- Internal errors\n\t- Internal debugging messages\n\t- Performance statistics (average latency, CPU usage)\n\nThis information will not be used to personally identify you, contact you, advertise to you, or otherwise bother you in any way.\n");
                    break;
       }
      }
    }
    config->set(ConfigKey("[User Experience]","AgreedToUserExperienceProgram"), ConfigValue(metricsAgree));

    // If the user agrees...
    if(metricsAgree == "yes") {
       // attempt to load the user ID from the config file
       if ( config->getValueString(ConfigKey("[User Experience]", "UID")) == ""){
         QString pUID = cm_generate_userid();
         if(!pUID.isEmpty()) config->set(ConfigKey("[User Experience]", "UID"), ConfigValue(pUID));
       }
    }
    // Initialize cmetrics
    cm_init(100,20, metricsAgree == "yes", MIXXCMETRICS_RELEASE_ID, config->getValueString(ConfigKey("[User Experience]", "UID")).ascii());
    cm_set_crash_dlg(crashDlg);
    cm_writemsg_ascii(MIXXXCMETRICS_VERSION, VERSION);
#endif

    // Store the path in the config database
    config->set(ConfigKey("[Config]","Path"), ConfigValue(qConfigPath));

    // Instantiate a ControlObject, and set static parent widget
    control = new ControlNull();

    // Read keyboard configuration and set kdbConfig object in WWidget
    // Check first in user's Mixxx directory
    QString userKeyboard = QDir::homePath().append("/").append(SETTINGS_PATH).append("Custom.kbd.cfg");
    if (QFile::exists(userKeyboard)) {
        qDebug() << "Found and will use custom keyboard preset" << userKeyboard;
        kbdconfig = new ConfigObject<ConfigValueKbd>(userKeyboard);
    }
    else
        // Otherwise use the default
        kbdconfig = new ConfigObject<ConfigValueKbd>(QString(qConfigPath).append("keyboard/").append("Standard.kbd.cfg"));
    WWidget::setKeyboardConfig(kbdconfig);

    // Starting the master (mixing of the channels and effects):
    m_pEngine = new EngineMaster(config, "[Master]");

    QString lightMappings = QDir::homePath().append("/").append(SETTINGS_PATH).append("lightmappings.xml");

    if (QFile::exists(lightMappings)) {
        qDebug() << "Trying to open" << lightMappings;
        QDomDocument lightsDocument;
        QFile lightMappingsFile(lightMappings);
        lightMappingsFile.open(QIODevice::ReadOnly);
        lightsDocument.setContent(&lightMappingsFile);
        QDomElement root = lightsDocument.documentElement();
        qDebug() << "ROOT" << root.nodeName();
        LightController* pController = LightController::fromXml(root);
        m_pEngine->setLightController(pController);
    } else {
        m_pEngine->setLightController(new LightController());
    }

    // Initialize player device
    // while this is created here, setupDevices needs to be called sometime
    // after the players are added to the engine (as is done currently) -- bkgood
    soundmanager = new SoundManager(config, m_pEngine);

    // Find path of skin
    QString qSkinPath = getSkinPath();

    // Get Music dir
    QDir dir(config->getValueString(ConfigKey("[Playlist]","Directory")));
    if ((config->getValueString(ConfigKey("[Playlist]","Directory")).length()<1) || (!dir.exists()))
    {
        QString fd = QFileDialog::getExistingDirectory(this, "Choose music library directory");
        if (fd != "")
        {
            config->set(ConfigKey("[Playlist]","Directory"), fd);
            config->Save();
        }
    }
    // Needed for Search class and Simple skin
    new ControlPotmeter(ConfigKey("[Channel1]","virtualplayposition"),0.,1.);

    // Use frame as container for view, needed for fullscreen display
    frame = new QFrame;
    setCentralWidget(frame);

    m_pLibrary = new Library(this, config, bFirstRun || bUpgraded);
    qRegisterMetaType<TrackPointer>("TrackPointer");

    // Create the player manager.
    m_pPlayerManager = new PlayerManager(config, m_pEngine, m_pLibrary);
    m_pPlayerManager->addPlayer();
    m_pPlayerManager->addPlayer();

    view=new MixxxView(frame, kbdconfig, qSkinPath, config,
                       m_pPlayerManager,
                       m_pLibrary);

    //Scan the library directory.
    m_pLibraryScanner = new LibraryScanner(m_pLibrary->getTrackCollection());

    //Refresh the library models when the library (re)scan is finished.
    connect(m_pLibraryScanner, SIGNAL(scanFinished()),
            m_pLibrary, SLOT(slotRefreshLibraryModels()));

    //Scan the library for new files and directories.
    m_pLibraryScanner->scan(config->getValueString(ConfigKey("[Playlist]","Directory")));


    // Call inits to invoke all other construction parts

    // TODO rryan : Move this to WaveformViewerFactory or something.
    /*
    if (bVisualsWaveform && !view->activeWaveform())
    {
        config->set(ConfigKey("[Controls]","Visuals"), ConfigValue(1));
        QMessageBox * mb = new QMessageBox(this);
        mb->setWindowTitle(QString("Wavform displays"));
        mb->setIcon(QMessageBox::Information);
        mb->setText("OpenGL cannot be initialized, which means that\nthe waveform displays won't work. A simple\nmode will be used instead where you can still\nuse the mouse to change speed.");
        mb->show();
    }
    */

    // Verify path for xml track file.
    QFile trackfile(config->getValueString(ConfigKey("[Playlist]","Listfile")));
    if ((config->getValueString(ConfigKey("[Playlist]","Listfile")).length()<1) || (!trackfile.exists()))
    {
        config->set(ConfigKey("[Playlist]","Listfile"), QDir::homePath().append("/").append(SETTINGS_PATH).append(TRACK_FILE));
        config->Save();
    }

    // Intialize default BPM system values
    if(config->getValueString(ConfigKey("[BPM]","BPMRangeStart")).length()<1)
    {
        config->set(ConfigKey("[BPM]","BPMRangeStart"),ConfigValue(65));
    }

    if(config->getValueString(ConfigKey("[BPM]","BPMRangeEnd")).length()<1)
    {
        config->set(ConfigKey("[BPM]","BPMRangeEnd"),ConfigValue(135));
    }

    if(config->getValueString(ConfigKey("[BPM]","AnalyzeEntireSong")).length()<1)
    {
        config->set(ConfigKey("[BPM]","AnalyzeEntireSong"),ConfigValue(1));
    }

    // Initialise midi
    m_pMidiDeviceManager = new MidiDeviceManager(config);
    //TODO: Try to open MIDI devices?
    m_pMidiDeviceManager->queryDevices();
    m_pMidiDeviceManager->setupDevices();


    // Initialize preference dialog
    prefDlg = new DlgPreferences(this, view, soundmanager,
                                 m_pMidiDeviceManager, config);
    prefDlg->setHidden(true);

    // Try open player device If that fails, the preference panel is opened.
    int setupDevices = soundmanager->setupDevices();
    unsigned int numDevices = soundmanager->getConfig().getOutputs().count();
    // test for at least one out device, if none, display another dlg that
    // says "mixxx will barely work with no outs"
    while (setupDevices != OK || numDevices == 0)
    {

#ifdef __C_METRICS__
        cm_writemsg_ascii(MIXXXCMETRICS_FAILED_TO_OPEN_SNDDEVICE_AT_STARTUP,
                          "Mixxx failed to open audio device(s) on startup.");
#endif

        // Exit when we press the Exit button in the noSoundDlg dialog
        // only call it if setupDevices != OK
        if (setupDevices != OK) {
            if (noSoundDlg() != 0) {
                exit(0);
            }
        } else if (numDevices == 0) {
            bool continueClicked = false;
            int noOutput = noOutputDlg(&continueClicked);
            if (continueClicked) break;
            if (noOutput != 0) {
                exit(0);
            }
        }
        setupDevices = soundmanager->setupDevices();
        numDevices = soundmanager->getConfig().getOutputs().count();
    }

    //setFocusPolicy(QWidget::StrongFocus);
    //grabKeyboard();

    // Load tracks in args.qlMusicFiles (command line arguments) into player 1 and 2:
    for (int i = 0; i < m_pPlayerManager->numPlayers() && i < args.qlMusicFiles.count(); ++i) {
        m_pPlayerManager->slotLoadToPlayer(args.qlMusicFiles.at(i), i+1);
    }

    //Automatically load specially marked promotional tracks on first run
    if (bFirstRun || bUpgraded) {
        QList<TrackPointer> tracksToAutoLoad = m_pLibrary->getTracksToAutoLoad();
        for (int i = 0; i < m_pPlayerManager->numPlayers() && i < tracksToAutoLoad.count(); i++) {
            m_pPlayerManager->slotLoadTrackToPlayer(tracksToAutoLoad.at(i), i+1);
        }
    }

#ifdef __SCRIPT__
    scriptEng = new ScriptEngine(this, m_pTrack);
#endif

    initActions();
    initMenuBar();

    // Check direct rendering and warn user if they don't have it
    view->checkDirectRendering();

    //Install an event filter to catch certain QT events, such as tooltips.
    //This allows us to turn off tooltips.
    app->installEventFilter(this); //The eventfilter is located in this Mixxx class as a callback.

    //If we were told to start in fullscreen mode on the command-line, then turn on fullscreen mode.
    if (args.bStartInFullscreen)
        slotOptionsFullScreen(true);
#ifdef __C_METRICS__
    cm_writemsg_ascii(MIXXXCMETRICS_MIXXX_CONSTRUCTOR_COMPLETE, "Mixxx constructor complete.");
#endif

    // Refresh the GUI (workaround for Qt 4.6 display bug)
    QString QtVersion = qVersion();
    if (QtVersion>="4.6.0") {
        qDebug() << "Qt v4.6.0 or higher detected. Using rebootMixxxView() workaround."
                 << "\n    (See bug https://bugs.launchpad.net/mixxx/+bug/521509)";
        rebootMixxxView();
    }
}

MixxxApp::~MixxxApp()
{
    QTime qTime;
    qTime.start();

    qDebug() << "Destroying MixxxApp";

// Moved this up to insulate macros you've worked hard on from being lost in
// a segfault that happens sometimes somewhere below here
#ifdef __SCRIPT__
    scriptEng->saveMacros();
    delete scriptEng;
#endif

#ifdef __IPOD__
    if (m_pTrack->m_qIPodPlaylist.getSongNum()) {
      qDebug() << "Dispose of iPod track collection";
      m_pTrack->m_qIPodPlaylist.clear();
    }
#endif
    qDebug() << "save config, " << qTime.elapsed();
    config->Save();

    qDebug() << "close soundmanager" << qTime.elapsed();
    soundmanager->closeDevices();
    qDebug() << "soundmanager->close() done";

    qDebug() << "delete MidiDeviceManager";
    delete m_pMidiDeviceManager;

    qDebug() << "delete soundmanager, " << qTime.elapsed();
    delete soundmanager;

    qDebug() << "delete playerManager" << qTime.elapsed();
    delete m_pPlayerManager;

    qDebug() << "delete m_pEngine, " << qTime.elapsed();
    delete m_pEngine;

//    qDebug() << "delete prefDlg";
//    delete m_pControlEngine;

    qDebug() << "delete view, " << qTime.elapsed();
    delete view;

    qDebug() << "delete library scanner" <<  qTime.elapsed();
    delete m_pLibraryScanner;

    //Delete the library after the view so there are no dangling pointers to the data models.
    qDebug() << "delete library" << qTime.elapsed();
    delete m_pLibrary;

    //HACK: Save config again. We saved it once before doing some dangerous stuff. We only really want to
    //      save it here, but the first one was just a precaution. The earlier one can be removed when
    //      stuff is more stable at exit.
    config->Save();

    delete prefDlg;

    delete frame;

#ifdef __C_METRICS__ // cmetrics will cause this whole method to segfault on Linux/i386 if it is called after config is deleted. Obviously, it depends on config somehow.
    qDebug() << "cmetrics to report:" << "Mixxx deconstructor complete.";
    cm_writemsg_ascii(MIXXXCMETRICS_MIXXX_DESTRUCTOR_COMPLETE, "Mixxx deconstructor complete.");
    cm_close(10);
#endif

    qDebug() << "delete config, " << qTime.elapsed();
    delete config;
}

int MixxxApp::noSoundDlg(void)
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("Sound Device Busy");
    msgBox.setText( "<html>Mixxx was unable to open all the configured sound devices. "
                    "Another application is using a sound device Mixxx is configured to use "
                    "or a device is not plugged in."
                    "<ul>"
                        "<li>"
                            "<b>Retry</b> after closing the other application "
                            "or reconnecting a sound device"
                        "</li>"
                        "<li>"
                            "<b>Reconfigure</b> Mixxx's sound device settings."
                        "</li>"
                        "<li>"
                            "Get <b>Help</b> from the Mixxx Wiki."
                        "</li>"
                        "<li>"
                            "<b>Exit</b> Mixxx."
                        "</li>"
                    "</ul></html>"
    );

    QPushButton *retryButton = msgBox.addButton(tr("Retry"), QMessageBox::ActionRole);
    QPushButton *reconfigureButton = msgBox.addButton(tr("Reconfigure"), QMessageBox::ActionRole);
    QPushButton *wikiButton = msgBox.addButton(tr("Help"), QMessageBox::ActionRole);
    QPushButton *exitButton = msgBox.addButton(tr("Exit"), QMessageBox::ActionRole);

    while(1)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == retryButton) {
            soundmanager->queryDevices();
            return 0;
        } else if (msgBox.clickedButton() == wikiButton) {
            QDesktopServices::openUrl(QUrl("http://mixxx.org/wiki/doku.php/troubleshooting#no_or_too_few_sound_cards_appear_in_the_preferences_dialog"));
            wikiButton->setEnabled(false);
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();
            soundmanager->queryDevices();

            // This way of opening the dialog allows us to use it synchronously
            prefDlg->setWindowModality(Qt::ApplicationModal);
            prefDlg->exec();
            if ( prefDlg->result() == QDialog::Accepted) {
                soundmanager->queryDevices();
                return 0;
            }

            msgBox.show();

        } else if (msgBox.clickedButton() == exitButton) {
            return 1;
        }
    }
}

int MixxxApp::noOutputDlg(bool *continueClicked)
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("No Output Devices");
    msgBox.setText( "<html>Mixxx was configured without any output sound devices. "
                    "Audio processing will be disabled without a configured output device."
                    "<ul>"
                        "<li>"
                            "<b>Continue</b> without any outputs."
                        "</li>"
                        "<li>"
                            "<b>Reconfigure</b> Mixxx's sound device settings."
                        "</li>"
                        "<li>"
                            "<b>Exit</b> Mixxx."
                        "</li>"
                    "</ul></html>"
    );

    QPushButton *continueButton = msgBox.addButton(tr("Continue"), QMessageBox::ActionRole);
    QPushButton *reconfigureButton = msgBox.addButton(tr("Reconfigure"), QMessageBox::ActionRole);
    QPushButton *exitButton = msgBox.addButton(tr("Exit"), QMessageBox::ActionRole);

    while(1)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == continueButton) {
            *continueClicked = true;
            return 0;
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();
            soundmanager->queryDevices();

            // This way of opening the dialog allows us to use it synchronously
            prefDlg->setWindowModality(Qt::ApplicationModal);
            prefDlg->exec();
            if ( prefDlg->result() == QDialog::Accepted) {
                soundmanager->queryDevices();
                return 0;
            }

            msgBox.show();

        } else if (msgBox.clickedButton() == exitButton) {
            return 1;
        }
    }
}

/** initializes all QActions of the application */
void MixxxApp::initActions()
{
    fileLoadSongPlayer1 = new QAction(tr("&Load Song (Player 1)..."), this);
    fileLoadSongPlayer1->setShortcut(tr("Ctrl+O"));
    fileLoadSongPlayer1->setShortcutContext(Qt::ApplicationShortcut);

    fileLoadSongPlayer2 = new QAction(tr("&Load Song (Player 2)..."), this);
    fileLoadSongPlayer2->setShortcut(tr("Ctrl+Shift+O"));
    fileLoadSongPlayer2->setShortcutContext(Qt::ApplicationShortcut);

    fileQuit = new QAction(tr("E&xit"), this);
    fileQuit->setShortcut(tr("Ctrl+Q"));
    fileQuit->setShortcutContext(Qt::ApplicationShortcut);

    libraryRescan = new QAction(tr("&Rescan Library"), this);

    playlistsNew = new QAction(tr("Add &new playlist"), this);
    playlistsNew->setShortcut(tr("Ctrl+N"));
    playlistsNew->setShortcutContext(Qt::ApplicationShortcut);

    playlistsImport = new QAction(tr("&Import playlist"), this);
    playlistsImport->setShortcut(tr("Ctrl+I"));
    playlistsImport->setShortcutContext(Qt::ApplicationShortcut);

#ifdef __IPOD__
    iPodToggle = new QAction(tr("iPod &Active"), this);
    iPodToggle->setShortcut(tr("Ctrl+A"));
    iPodToggle->setShortcutContext(Qt::ApplicationShortcut);
    iPodToggle->setCheckable(true);
    connect(iPodToggle, SIGNAL(toggled(bool)), this, SLOT(slotiPodToggle(bool)));
#endif

    optionsBeatMark = new QAction(tr("&Audio Beat Marks"), this);

    optionsFullScreen = new QAction(tr("&Full Screen"), this);
    optionsFullScreen->setShortcut(tr("F11"));
    optionsFullScreen->setShortcutContext(Qt::ApplicationShortcut);
    //QShortcut * shortcut = new QShortcut(QKeySequence(tr("Esc")),  this);
    //    connect(shortcut, SIGNAL(activated()), this, SLOT(slotQuitFullScreen()));

    optionsPreferences = new QAction(tr("&Preferences"), this);
    optionsPreferences->setShortcut(tr("Ctrl+P"));
    optionsPreferences->setShortcutContext(Qt::ApplicationShortcut);

    helpAboutApp = new QAction(tr("&About..."), this);
    helpSupport = new QAction(tr("&Community Support..."), this);
#ifdef __VINYLCONTROL__
    optionsVinylControl = new QAction(tr("Enable &Vinyl Control"), this);
    optionsVinylControl->setShortcut(tr("Ctrl+Y"));
    optionsVinylControl->setShortcutContext(Qt::ApplicationShortcut);
#endif

    optionsRecord = new QAction(tr("&Record Mix"), this);
    //optionsRecord->setShortcut(tr("Ctrl+R"));
    optionsRecord->setShortcutContext(Qt::ApplicationShortcut);

#ifdef __SCRIPT__
    macroStudio = new QAction(tr("Show Studio"), this);
#endif

    fileLoadSongPlayer1->setStatusTip(tr("Opens a song in player 1"));
    fileLoadSongPlayer1->setWhatsThis(tr("Open\n\nOpens a song in player 1"));
    connect(fileLoadSongPlayer1, SIGNAL(activated()), this, SLOT(slotFileLoadSongPlayer1()));

    fileLoadSongPlayer2->setStatusTip(tr("Opens a song in player 2"));
    fileLoadSongPlayer2->setWhatsThis(tr("Open\n\nOpens a song in player 2"));
    connect(fileLoadSongPlayer2, SIGNAL(activated()), this, SLOT(slotFileLoadSongPlayer2()));

    fileQuit->setStatusTip(tr("Quits the application"));
    fileQuit->setWhatsThis(tr("Exit\n\nQuits the application"));
    connect(fileQuit, SIGNAL(activated()), this, SLOT(slotFileQuit()));

    libraryRescan->setStatusTip(tr("Rescans the song library"));
    libraryRescan->setWhatsThis(tr("Rescan library\n\nRescans the song library"));
    libraryRescan->setCheckable(false);
    connect(libraryRescan, SIGNAL(activated()), this, SLOT(slotScanLibrary()));
    connect(m_pLibraryScanner, SIGNAL(scanFinished()), this, SLOT(slotEnableRescanLibraryAction()));

    playlistsNew->setStatusTip(tr("Create a new playlist"));
    playlistsNew->setWhatsThis(tr("New playlist\n\nCreate a new playlist"));
    connect(playlistsNew, SIGNAL(activated()), m_pLibrary, SLOT(slotCreatePlaylist()));

    playlistsImport->setStatusTip(tr("Import playlist"));
    playlistsImport->setWhatsThis(tr("Import playlist"));
    //connect(playlistsImport, SIGNAL(activated()), m_pTrack, SLOT(slotImportPlaylist()));
    //FIXME: Disabled due to library rework

    optionsBeatMark->setCheckable(false);
    optionsBeatMark->setChecked(false);
    optionsBeatMark->setStatusTip(tr("Audio Beat Marks"));
    optionsBeatMark->setWhatsThis(tr("Audio Beat Marks\nMark beats by audio clicks"));
    connect(optionsBeatMark, SIGNAL(toggled(bool)), this, SLOT(slotOptionsBeatMark(bool)));

#ifdef __VINYLCONTROL__
    //Either check or uncheck the vinyl control menu item depending on what it was saved as.
    optionsVinylControl->setCheckable(true);
    if ((bool)config->getValueString(ConfigKey("[VinylControl]","Enabled")).toInt() == true)
        optionsVinylControl->setChecked(true);
    else
        optionsVinylControl->setChecked(false);
    optionsVinylControl->setStatusTip(tr("Activate Vinyl Control"));
    optionsVinylControl->setWhatsThis(tr("Use timecoded vinyls on external turntables to control Mixxx"));
    connect(optionsVinylControl, SIGNAL(toggled(bool)), this, SLOT(slotOptionsVinylControl(bool)));
#endif

    optionsRecord->setCheckable(true);
    optionsRecord->setStatusTip(tr("Start Recording your Mix"));
    optionsRecord->setWhatsThis(tr("Record your mix to a file"));
    connect(optionsRecord, SIGNAL(toggled(bool)), this, SLOT(slotOptionsRecord(bool)));

    optionsFullScreen->setCheckable(true);
    optionsFullScreen->setChecked(false);
    optionsFullScreen->setStatusTip(tr("Full Screen"));
    optionsFullScreen->setWhatsThis(tr("Display Mixxx using the full screen"));
    connect(optionsFullScreen, SIGNAL(toggled(bool)), this, SLOT(slotOptionsFullScreen(bool)));

    optionsPreferences->setStatusTip(tr("Preferences"));
    optionsPreferences->setWhatsThis(tr("Preferences\nPlayback and MIDI preferences"));
    connect(optionsPreferences, SIGNAL(activated()), this, SLOT(slotOptionsPreferences()));

    helpSupport->setStatusTip(tr("Support..."));
    helpSupport->setWhatsThis(tr("Support\n\nGet help with Mixxx"));
    connect(helpSupport, SIGNAL(activated()), this, SLOT(slotHelpSupport()));

    helpAboutApp->setStatusTip(tr("About the application"));
    helpAboutApp->setWhatsThis(tr("About\n\nAbout the application"));
    connect(helpAboutApp, SIGNAL(activated()), this, SLOT(slotHelpAbout()));

#ifdef __SCRIPT__
    macroStudio->setStatusTip(tr("Shows the macro studio window"));
    macroStudio->setWhatsThis(tr("Show Studio\n\nMakes the macro studio visible"));
     connect(macroStudio, SIGNAL(activated()), scriptEng->getStudio(), SLOT(showStudio()));
#endif
}

void MixxxApp::initMenuBar()
{
    // MENUBAR
    fileMenu=new QMenu("&File");
    optionsMenu=new QMenu("&Options");
    libraryMenu=new QMenu("&Library");
    viewMenu=new QMenu("&View");
    helpMenu=new QMenu("&Help");
#ifdef __SCRIPT__
    macroMenu=new QMenu("&Macro");
#endif

    // menuBar entry fileMenu
    fileMenu->addAction(fileLoadSongPlayer1);
    fileMenu->addAction(fileLoadSongPlayer2);
    fileMenu->addSeparator();
    fileMenu->addAction(fileQuit);

    // menuBar entry optionsMenu
    //optionsMenu->setCheckable(true);
    //  optionsBeatMark->addTo(optionsMenu);
#ifdef __VINYLCONTROL__
    optionsMenu->addAction(optionsVinylControl);
#endif
    optionsMenu->addAction(optionsRecord);
    optionsMenu->addAction(optionsFullScreen);
    optionsMenu->addSeparator();
    optionsMenu->addAction(optionsPreferences);

    //    libraryMenu->setCheckable(true);
    libraryMenu->addAction(libraryRescan);
    libraryMenu->addSeparator();
    libraryMenu->addAction(playlistsNew);
    //libraryMenu->addAction(playlistsImport);

#ifdef __IPOD__
    libraryMenu->addSeparator();
    libraryMenu->addAction(iPodToggle);
    connect(libraryMenu, SIGNAL(aboutToShow()), this, SLOT(slotlibraryMenuAboutToShow()));
#endif

    // menuBar entry viewMenu
    //viewMenu->setCheckable(true);

    // menuBar entry helpMenu
    helpMenu->addAction(helpSupport);
    helpMenu->addSeparator();
    helpMenu->addAction(helpAboutApp);


#ifdef __SCRIPT__
    macroMenu->addAction(macroStudio);
#endif

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(libraryMenu);
    menuBar()->addMenu(optionsMenu);

    //    menuBar()->addMenu(viewMenu);
#ifdef __SCRIPT__
    menuBar()->addMenu(macroMenu);
#endif
    menuBar()->addSeparator();
    menuBar()->addMenu(helpMenu);

}


void MixxxApp::slotiPodToggle(bool toggle) {
#ifdef __IPOD__
// iPod stuff
  QString iPodMountPoint = config->getValueString(ConfigKey("[iPod]","MountPoint"));
  bool iPodAvailable = !iPodMountPoint.isEmpty() &&
                       QDir( iPodMountPoint + "/iPod_Control").exists();
  bool iPodActivated = iPodAvailable && toggle;

  iPodToggle->setEnabled(iPodAvailable);

  if (iPodAvailable && iPodActivated && view->m_pComboBox->findData(TABLE_MODE_IPOD) == -1 ) {
    view->m_pComboBox->addItem( "iPod", TABLE_MODE_IPOD );
    // Activate IPod model

    Itdb_iTunesDB *itdb;
    itdb = itdb_parse (iPodMountPoint, NULL);
    if (itdb == NULL) {
      qDebug() << "Error reading iPod database\n";
      return;
    }
    GList *it;
    int count = 0;
    m_pTrack->m_qIPodPlaylist.clear();

    for (it = itdb->tracks; it != NULL; it = it->next) {
       count++;
       Itdb_Track *song;
       song = (Itdb_Track *)it->data;

//     DON'T USE QFileInfo, it does a disk i/o stat on every file introducing a VERY long delay in loading from the iPod
//       QFileInfo file(iPodMountPoint + QString(song->ipod_path).replace(':','/'));

       QString fullFilePath = iPodMountPoint + QString(song->ipod_path).mid(1).replace(':','/');
       QString filePath = fullFilePath.left(fullFilePath.lastIndexOf('/'));
       QString fileName = fullFilePath.mid(fullFilePath.lastIndexOf('/')+1);
       QString fileSuffix = fullFilePath.mid(fullFilePath.lastIndexOf('.')+1);

       if (song->movie_flag) { qDebug() << "Movies/Videos not supported." << song->title << fullFilePath; continue; }
       if (song->unk220 && fileSuffix == "m4p") { qDebug() << "Protected media not supported." << song->title << fullFilePath; continue; }
#ifndef __FFMPEGFILE__
       if (fileSuffix == "m4a") { qDebug() << "m4a media support (via FFMPEG) is not compiled into this build of Mixxx. :( " << song->title << fullFilePath; continue; }
#endif // __FFMPEGFILE__


//       qDebug() << "iPod file" << filePath << "--"<< fileName << "--" << fileSuffix;

       TrackInfoObject* pTrack = new TrackInfoObject(filePath, fileName);
       pTrack->setBpm(song->BPM);
       pTrack->setBpmConfirm(song->BPM != 0);  //    void setBeatFirst(float); ??
//       pTrack->setHeaderParsed(true);
       pTrack->setComment(song->comment);
//       pTrack->setType(file.suffix());
       pTrack->setType(fileSuffix);
       pTrack->setBitrate(song->bitrate);
       pTrack->setSampleRate(song->samplerate);
       pTrack->setDuration(song->tracklen/1000);
       pTrack->setTitle(song->title);
       pTrack->setArtist(song->artist);
       // song->rating // user rating
       // song->volume and song->soundcheck -- track level normalization / gain info as determined by iTunes
       m_pTrack->m_qIPodPlaylist.addTrack(pTrack);
    }
    itdb_free (itdb);

    //qDebug() << "iPod playlist has" << m_pTrack->m_qIPodPlaylist.getSongNum() << "of"<< count <<"songs on the iPod.";

    view->m_pComboBox->setCurrentIndex( view->m_pComboBox->findData(TABLE_MODE_IPOD) );
    //m_pTrack->slotActivatePlaylist( view->m_pComboBox->findData(TABLE_MODE_IPOD) );
    //m_pTrack->resizeColumnsForLibraryMode();

    //FIXME: Commented out above due to library rework.

  } else if (view->m_pComboBox->findData(TABLE_MODE_IPOD) != -1 ) {
    view->m_pComboBox->setCurrentIndex( view->m_pComboBox->findData(TABLE_MODE_LIBRARY) );
    //m_pTrack->slotActivatePlaylist( view->m_pComboBox->findData(TABLE_MODE_LIBRARY) );
    //FIXME: library reworking

    view->m_pComboBox->removeItem( view->m_pComboBox->findData(TABLE_MODE_IPOD) );
    // Empty iPod model m_qIPodPlaylist
    //m_pTrack->m_qIPodPlaylist.clear();

  }
#endif
}


void MixxxApp::slotlibraryMenuAboutToShow(){

#ifdef __IPOD__
  QString iPodMountPoint = config->getValueString(ConfigKey("[iPod]","MountPoint"));
  bool iPodAvailable = !iPodMountPoint.isEmpty() &&
                       QDir( iPodMountPoint + "/iPod_Control").exists();
  iPodToggle->setEnabled(iPodAvailable);

#endif
}

bool MixxxApp::queryExit()
{
    int exit=QMessageBox::information(this, tr("Quit..."),
                                      tr("Do your really want to quit?"),
                                      QMessageBox::Ok, QMessageBox::Cancel);

    if (exit==1)
    {
    }
    else
    {
    };

    return (exit==1);
}

void MixxxApp::slotFileLoadSongPlayer1()
{
    ControlObject* play = ControlObject::getControl(ConfigKey("[Channel1]", "play"));

    if (play->get() == 1.)
    {
        int ret = QMessageBox::warning(this, tr("Mixxx"),
                                        tr("Player 1 is currently playing a song.\n"
                                          "Are you sure you want to load a new song?"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No);

        if (ret != QMessageBox::Yes)
            return;
    }

    QString s =
            QFileDialog::getOpenFileName(
                this,
                tr("Load Song into Player 1"),
                config->getValueString(ConfigKey("[Playlist]","Directory")),
                QString("Audio (%1)").arg(SoundSourceProxy::supportedFileExtensionsString()));

    if (s != QString::null) {
        m_pPlayerManager->slotLoadToPlayer(s, 1);
    }
}

void MixxxApp::slotFileLoadSongPlayer2()
{
    ControlObject* play = ControlObject::getControl(ConfigKey("[Channel2]", "play"));

    if (play->get() == 1.)
    {
        int ret = QMessageBox::warning(this, tr("Mixxx"),
                                        tr("Player 2 is currently playing a song.\n"
                                          "Are you sure you want to load a new song?"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No);

        if (ret != QMessageBox::Yes)
            return;
    }

    QString s =
            QFileDialog::getOpenFileName(
                this,
                tr("Load Song into Player 2"),
                config->getValueString(ConfigKey("[Playlist]","Directory")),
                QString("Audio (%1)").arg(SoundSourceProxy::supportedFileExtensionsString()));

    if (s != QString::null) {
        m_pPlayerManager->slotLoadToPlayer(s, 2);
    }
}

void MixxxApp::slotFileQuit()
{
    qApp->quit();
}

void MixxxApp::slotOptionsBeatMark(bool)
{
// BEAT MARK STUFF
}

void MixxxApp::slotOptionsFullScreen(bool toggle)
{
    if (optionsFullScreen)
        optionsFullScreen->setChecked(toggle);

    // Making a fullscreen window on linux and windows is harder than you could possibly imagine...
    if (toggle)
    {
#ifdef __LINUX__
         winpos = pos();
         // Can't set max to -1,-1 or 0,0 for unbounded?
         setMaximumSize(32767,32767);
#endif

        showFullScreen();
        //menuBar()->hide();
        // FWI: Begin of fullscreen patch
#ifdef __LINUX__
        // Crazy X window managers break this so I'm told by Qt docs
        //         int deskw = app->desktop()->width();
        //         int deskh = app->desktop()->height();

        //support for xinerama
        int deskw = app->desktop()->screenGeometry(frame).width();
        int deskh = app->desktop()->screenGeometry(frame).height();
#else
        int deskw = width();
        int deskh = height();
#endif
        view->move((deskw - view->width())/2, (deskh - view->height())/2);
        // FWI: End of fullscreen patch
    }
    else
    {
        // FWI: Begin of fullscreen patch
        view->move(0,0);
        menuBar()->show();
        showNormal();

#ifdef __LINUX__
        if (size().width() != view->width() ||
            size().height() != view->height() + menuBar()->height()) {
          setFixedSize(view->width(), view->height() + menuBar()->height());
        }
        move(winpos);
#endif

        // FWI: End of fullscreen patch
    }
}

void MixxxApp::slotOptionsPreferences()
{
    prefDlg->setHidden(false);
}

//Note: Can't #ifdef this because MOC doesn't catch it.
void MixxxApp::slotOptionsVinylControl(bool toggle)
{
#ifdef __VINYLCONTROL__
    //qDebug() << "slotOptionsVinylControl: toggle is " << (int)toggle;

    QMultiHash<QString, AudioInput> inputs = soundmanager->getConfig().getInputs();
    unsigned int countVCIns = 0;
    foreach (AudioInput in, inputs.values()) {
        if (in.getType() == AudioInput::VINYLCONTROL) {
            ++countVCIns;
        }
    }

    if (countVCIns == 0 && toggle)
    {
        QMessageBox::warning(this, tr("Mixxx"),
                                   tr("No input device(s) select.\n"
                                      "Please select your soundcard(s) in the sound hardware preferences."),
                                   QMessageBox::Ok,
                                   QMessageBox::Ok);
        prefDlg->show();
        prefDlg->showSoundHardwarePage();
        optionsVinylControl->setChecked(false);
    }
    else
    {
        config->set(ConfigKey("[VinylControl]","Enabled"), ConfigValue((int)toggle));
        ControlObject::getControl(ConfigKey("[VinylControl]", "Enabled"))->set((int)toggle);
    }
#endif
}

//Also can't ifdef this (MOC again)
void MixxxApp::slotOptionsRecord(bool toggle)
{
    ControlObjectThreadMain *recordingControl = new ControlObjectThreadMain(ControlObject::getControl(ConfigKey("[Master]", "Record")));
    QString recordPath = config->getValueString(ConfigKey("[Recording]","Path"));
    QString encodingType = config->getValueString(ConfigKey("[Recording]","Encoding"));
    QString encodingFileFilter = QString("Audio (*.%1)").arg(encodingType);
    bool proceedWithRecording = true;

    if (toggle == true)
    {
        //If there was no recording path set,
        if (recordPath == "")
        {
            QString selectedFile = QFileDialog::getSaveFileName(NULL, tr("Save Recording As..."),
                                                                recordPath,
                                                                encodingFileFilter);
            if (selectedFile.toLower() != "")
            {
                if(!selectedFile.toLower().endsWith("." + encodingType.toLower()))
                {
                    selectedFile.append("." + encodingType.toLower());
                }
                //Update the saved Path
                config->set(ConfigKey(RECORDING_PREF_KEY, "Path"), selectedFile);
            }
            else
                proceedWithRecording = false; //Empty filename, so don't record
        }
        else //If there was already a recording path set
        {
            //... and the file already exists, ask the user if they want to overwrite it.
            int result;
            if(QFile::exists(recordPath))
            {
                QFileInfo fi(recordPath);
                result = QMessageBox::question(this, tr("Mixxx Recording"), tr("The file %1 already exists. Would you like to overwrite it?\nSelecting \"No\" will abort the recording.").arg(fi.fileName()), QMessageBox::Yes | QMessageBox::No);
                if (result == QMessageBox::Yes) //If the user selected, "yes, overwrite the recording"...
                    proceedWithRecording = true;
                else
                    proceedWithRecording = false;
            }
        }

        if (proceedWithRecording == true)
        {
            qDebug() << "Setting record status: READY";
            recordingControl->slotSet(RECORD_READY);
        }
        else
        {
            optionsRecord->setChecked(false);
        }

    }
    else
    {
        qDebug() << "Setting record status: OFF";
        recordingControl->slotSet(RECORD_OFF);
    }

    delete recordingControl;
}

void MixxxApp::slotHelpAbout()
{

    DlgAbout *about = new DlgAbout(this);
#if defined(AMD64) || defined(EM64T) || defined(x86_64)
    about->version_label->setText(VERSION " x64");
#elif defined(IA64)
    about->version_label->setText(VERSION " IA64");
#else
    about->version_label->setText(VERSION);
#endif
    QString credits =
    QString("<p align=\"center\"><b>Mixxx %1 Development Team</b></p>"
"<p align=\"center\">"
"Adam Davison<br>"
"Albert Santoni<br>"
"RJ Ryan<br>"
"Garth Dahlstrom<br>"
"Sean Pappalardo<br>"
"Phillip Whelan<br>"
"Tobias Rafreider<br>"
"S. Brandt<br>"

"</p>"
"<p align=\"center\"><b>With contributions from:</b></p>"
"<p align=\"center\">"
"Mark Hills<br>"
"Andre Roth<br>"
"Robin Sheat<br>"
"Mark Glines<br>"
"Mathieu Rene<br>"
"Miko Kiiski<br>"
"Brian Jackson<br>"
"Owen Williams<br>"
"Andreas Pflug<br>"
"Bas van Schaik<br>"
"J&aacute;n Jockusch<br>"
"Oliver St&ouml;neberg<br>"
"Jan Jockusch<br>"
"C. Stewart<br>"
"Bill Egert<br>"
"Zach Shutters<br>"
"Owen Bullock<br>"
"Bill Good<br>"
"Graeme Mathieson<br>"
"Sebastian Actist<br>"
"Jussi Sainio<br>"
"David Gnedt<br>"
"Antonio Passamani<br>"
"Guy Martin<br>"

"</p>"
"<p align=\"center\"><b>And special thanks to:</b></p>"
"<p align=\"center\">"
"Stanton<br>"
"Hercules<br>"
"EKS<br>"
"Echo Digital Audio<br>"
"Adam Bellinson<br>"
"Alexandre Bancel<br>"
"Melanie Thielker<br>"
"Julien Rosener<br>"
"Pau Arum&iacute;<br>"
"David Garcia<br>"
"Seb Ruiz<br>"
"Joseph Mattiello<br>"
"</p>"

"<p align=\"center\"><b>Past Developers</b></p>"
"<p align=\"center\">"
"Tue Haste Andersen<br>"
"Ken Haste Andersen<br>"
"Cedric Gestes<br>"
"John Sully<br>"
"Torben Hohn<br>"
"Peter Chang<br>"
"Micah Lee<br>"
"Ben Wheeler<br>"
"Wesley Stessens<br>"
"Nathan Prado<br>"
"Zach Elko<br>"
"Tom Care<br>"
"Pawel Bartkiewicz<br>"
"Nick Guenther<br>"
"</p>"

"<p align=\"center\"><b>Past Contributors</b></p>"
"<p align=\"center\">"
"Ludek Hor&#225;cek<br>"
"Svein Magne Bang<br>"
"Kristoffer Jensen<br>"
"Ingo Kossyk<br>"
"Mads Holm<br>"
"Lukas Zapletal<br>"
"Jeremie Zimmermann<br>"
"Gianluca Romanin<br>"
"Tim Jackson<br>"
"Stefan Langhammer<br>"
"Frank Willascheck<br>"
"Jeff Nelson<br>"
"Kevin Schaper<br>"
"Alex Markley<br>"
"Oriol Puigb&oacute;<br>"
"Ulrich Heske<br>"
"James Hagerman<br>"
"quil0m80<br>"
"Martin Sakm&#225;r<br>"
"Ilian Persson<br>"
"Alex Barker<br>"
"Dave Jarvis<br>"
"Thomas Baag<br>"
"Karlis Kalnins<br>"
"Amias Channer<br>"
"Sacha Berger<br>"
"James Evans<br>"
"Martin Sakmar<br>"
"Navaho Gunleg<br>"
"Gavin Pryke<br>"
"Michael Pujos<br>"
"Claudio Bantaloukas<br>"
"Pavol Rusnak<br>"

#if defined(AMD64) || defined(EM64T) || defined(x86_64)
    "</p>").arg(VERSION " x64");
#elif defined(IA64)
    "</p>").arg(VERSION " IA64");
#else
    "</p>").arg(VERSION);
#endif



    about->textBrowser->setHtml(credits);
    about->show();

}

void MixxxApp::slotHelpSupport()
{
    QUrl qSupportURL;
    qSupportURL.setUrl(MIXXX_SUPPORT_URL);
    QDesktopServices::openUrl(qSupportURL);
}

void MixxxApp::rebootMixxxView() {
    // Ok, so wierdly if you call setFixedSize with the same value twice, Qt breaks
    // So we check and if the size hasn't changed we don't make the call
    int oldh = view->height();
    int oldw = view->width();
    qDebug() << "Now in Rebootmixxview...";

    // Workaround for changing skins while fullscreen, just go out of fullscreen
    // mode. If you change skins while in fullscreen (on Linux, at least) the
    // window returns to 0,0 but and the backdrop disappears so it looks as if
    // it is not fullscreen, but acts as if it is.
    slotOptionsFullScreen(false);

    QString qSkinPath = getSkinPath();

    view->rebootGUI(frame, config, qSkinPath);

    qDebug() << "rebootgui DONE";

    if (oldw != view->width() || oldh != view->height() + menuBar()->height()) {
      setFixedSize(view->width(), view->height() + menuBar()->height());
    }
}

QString MixxxApp::getSkinPath() {
    QString qConfigPath = config->getConfigPath();

    QString qSkinPath(qConfigPath);
    qSkinPath.append("skins/");
    if (QDir(qSkinPath).exists())
    {
        // Is the skin listed in the config database there? If not, use default (outlineSmall) skin
        if ((config->getValueString(ConfigKey("[Config]","Skin")).length()>0 && QDir(QString(qSkinPath).append(config->getValueString(ConfigKey("[Config]","Skin")))).exists()))
            qSkinPath.append(config->getValueString(ConfigKey("[Config]","Skin")));
        else
        {
            config->set(ConfigKey("[Config]","Skin"), ConfigValue("outlineNetbook"));
            config->Save();
            qSkinPath.append(config->getValueString(ConfigKey("[Config]","Skin")));
        }
    }
    else
        qCritical() << "Skin directory does not exist:" << qSkinPath;

    return qSkinPath;
}

/** Event filter to block certain events. For example, this function is used
  * to disable tooltips if the user specifies in the preferences that they
  * want them off. This is a callback function.
  */
bool MixxxApp::eventFilter(QObject *obj, QEvent *event)
{
    static int tooltips = config->getValueString(ConfigKey("[Controls]","Tooltips")).toInt();

    if (event->type() == QEvent::ToolTip) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (tooltips == 1)
            return false;
        else
            return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }

}

void MixxxApp::slotScanLibrary()
{
    libraryRescan->setEnabled(false);
    m_pLibraryScanner->scan(config->getValueString(ConfigKey("[Playlist]","Directory")));
}

void MixxxApp::slotEnableRescanLibraryAction()
{
    libraryRescan->setEnabled(true);
}
