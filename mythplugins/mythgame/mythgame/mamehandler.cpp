#include <fstream>
#include <iostream>
#include "mamehandler.h"
#include "mamerominfo.h"
#include "mamesettingsdlg.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qdir.h>

#include <sys/types.h>
#include <unistd.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdialogs.h>

#include <string>
using namespace std;

struct Prefs general_prefs;

MameHandler::~MameHandler()
{
}

void MameHandler::processGames()
{
        FILE* xmame_info;
        FILE* xmame_vrfy;
        FILE* xmame_drv;
        char line[500];
        QString infocmd;
        QString vrfycmd;
        QString drvcmd;

        QString romname;
        QString gamename;
        QString year;
        QString manu;
        QString cloneof;
        QString romof;
        QString input;
        QString video;
        QString chip[16];
        QString driver_status;

        QString driver = NULL;
        QString status = NULL;

        char *p;
        char *keyword;
        char *value = NULL;
        char *verifyname;
        char *drivername;

        QString thequery;

        QStringList tmp_array;

        MameRomInfo *rom;

        int i = 0;
        int tmp_counter = 0;
        int done_roms = 0;
        float done;

        QSqlDatabase *db = QSqlDatabase::database();

        //remove all metadata entries from the tables, all correct values will be
        //added as they are found.  This is done so that entries that may no longer be
        //available or valid are removed each time the game list is remade.
        thequery = "DELETE FROM mamemetadata;";
        db->exec(thequery);
        thequery = "DELETE FROM gamemetadata WHERE system = \"Mame\";";
        db->exec(thequery);

        for (tmp_counter = 0; tmp_counter < 16; tmp_counter++) {
                chip[tmp_counter] = "";
        }

        VERBOSE(VB_ALL, "Checking xmame version");
        check_xmame_exe();
        if (!xmame_version_ok) {
                tmp_counter = 0;
                while ((chip[tmp_counter] != "")) {
                        chip[tmp_counter] = "";
                        tmp_counter++;
                }
                supported_games = 0;
                VERBOSE(VB_GENERAL, "This version of xmame is not supported");
                MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                          QObject::tr("Wrong xmame version."),
                                          QObject::tr("This version of xmame "
                                                      "is not supported."));
                return;
        }

        /* Get number of supported games */
        VERBOSE(VB_ALL, "Getting the number of supported games");

        makecmd_line("-list 2>/dev/null", &vrfycmd, NULL);
        xmame_info = popen(vrfycmd, "r");

        while (fgets(line, 500, xmame_info)) {
                p = line + 1;
                while (*p && (*p++ != ':'));

                value = p;
                i = 0;

                while (*p && (*p++ != '\n'))
                        i++;
                value[i] = 0;
        }
        pclose(xmame_info);

        if (value)
            supported_games = atoi(value);
        else
        {
            VERBOSE(VB_ALL, "Failed to get supported games");
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      QObject::tr("Failed to get supported "
                                                  "games."),
                                      QObject::tr("MythTV was unable to "
                                                  "retrieve the number of "
                                                  "supported games.\nIs your "
                                                  "path to xmame correct?"));
            return;
        }

        /* Generate the list */
        VERBOSE(VB_ALL, QString("This version of xmame supports %1 games... "
                                "Getting the list.").arg(supported_games));
        makecmd_line("-listinfo 2>/dev/null", &infocmd, NULL);
        xmame_info = popen(infocmd, "r");

        VERBOSE(VB_ALL, "Verifying the installed games.");
        makecmd_line("-verifyroms 2>/dev/null", &vrfycmd, NULL);
        xmame_vrfy = popen(vrfycmd, "r");

        VERBOSE(VB_ALL, "Getting the list of source files associated with "
                        "the various games.");
        makecmd_line("-listsourcefile 2>/dev/null", &drvcmd, NULL);
        xmame_drv = popen(drvcmd, "r");

        map<QString, QString> CatMap;
        if (!LoadCatfile(&CatMap))
        {
            MythPopupBox::showOkPopup(gContext->GetMainWindow(),
                                      QObject::tr("Failed to read catver.ini"),
                                      QObject::tr("MythTV was unable to read "
                                                  "catver.ini.\nPlease enter "
                                                  "the correct path and try "
                                                  "again."));
        }

        VERBOSE(VB_ALL, "Looking for games..."); 

        MythProgressDialog pdial(QObject::tr("Looking for Mame games..."), 
                                 supported_games);

        while (fgets(line, 500, xmame_info)) {
                if (!strncmp(line, "game (", 6)) {
                        romname = QObject::tr("Unknown");
                        gamename = QObject::tr("Unknown");
                        year = "-";
                        manu = QObject::tr("Unknown");
                        cloneof = "-";
                        romof = "-";
                        while (fgets(line, 500, xmame_info)) {
                                if (line[0] == ')')
                                        break;
                                p = line + 1;
                                keyword = p;
                                i = 0;

                                while (*p && (*p++ != ' '))
                                        i++;
                                keyword[i] = 0;


                                if (p[0] == '\"')
                                        *p = *p++;
                                value = p;

                                i = 0;

                                while (*p && (*p++ != '\n'))
                                        i++;

                                if (value[i - 1] == '\"')
                                        i--;
                                value[i] = '\0';

                                if (!strcmp(keyword, "name"))
                                        romname = value;
                                if (!strcmp(keyword, "description"))
                                        gamename = value;
                                if (!strcmp(keyword, "year"))
                                        year = value;
                                if (!strcmp(keyword, "manufacturer"))
                                        manu = value;
                                if (!strcmp(keyword, "cloneof"))
                                        cloneof = value;
                                if (!strcmp(keyword, "romof"))
                                        romof = value;
                                if (!strcmp(keyword, "chip")) {
                                        tmp_counter = 0;
                                        /* Put it in the next free chip[] slot */
                                        while ((chip[tmp_counter] != "")
                                               && tmp_counter < 16)
                                                tmp_counter++;
                                        if ((chip[tmp_counter] == ""))
                                                chip[tmp_counter] =
                                                    strdup(value);
                                }
                                if (!strcmp(keyword, "video"))
                                        video = value;
                                if (!strcmp(keyword, "input"))
                                        input = value;
                                if (!strcmp(keyword, "driver"))
                                        driver_status = value;

                        }
                        /* Get romstatus */
                        while (TRUE) {
                                if (!fgets(line, 500, xmame_vrfy))
                                {
                                        VERBOSE(VB_ALL, "breaking");
                                        break;
                                }
                                if (strstr(line, "romset")) {
                                        verifyname = line + 7;
                                        p = verifyname;
                                        i = 0;

                                        while (*p && (*p++ != ' '))
                                                i++;

                                        verifyname[i] = 0;
                                        i++;

                                        value = line + 7 + i;
                                        i++;

                                        while (*p && (*p++ != '\n'))
                                                i++;

                                        value[i] = '\0';
                                        status = value;
                                        break;
                                }
                        }
                        /* Get drivername */
                        while (TRUE) {
                                if (!fgets(line, 500, xmame_drv))
                                        break;

                                drivername = line + 1;
                                p = drivername;
                                i = 0;

                                while (*p && (*p++ != '/') && (*p != '\n'))
                                        i++;
                                /* If no '/' was found, this line is not of the
                                 * wanted kind, skip to next */
                                if (*p != '\n') {
                                        i++;
                                        while (*p && (*p++ != '/'))
                                                i++;
                                        i++;

                                        value = line + i + 1;
                                        i = 0;

                                        while (*p && (*p++ != '.')
                                               && (*p != '\n'))
                                                i++;

                                        value[i] = '\0';

                                        if (i > 0) {
                                                driver = value;
                                                break;
                                        }
                                }
                        }

                        rom = new MameRomInfo();
                        if (!rom)
                        {
                            VERBOSE(VB_ALL, "Out of memory while generating "
                                            "gamelist");
                        }

                        /* Setting som default values */
                        rom->setCpu1("-");
                        rom->setCpu2("-");
                        rom->setCpu3("-");
                        rom->setCpu4("-");
                        rom->setSound1("-");
                        rom->setSound2("-");
                        rom->setSound3("-");
                        rom->setSound4("-");
                        rom->setControl("-");
                        rom->setNum_buttons(0);
                        rom->setNum_players(0);
                        rom->setRomname(romname);
                        rom->setGamename(gamename);
                        rom->setYear(year.toInt());
                        rom->setManu(manu);
                        rom->setCloneof(cloneof);
                        rom->setRomof(romof);
                        rom->setDriver(driver);

                        tmp_counter = 0;
                        while ((chip[tmp_counter] != "")
                               && (tmp_counter < 16)) {
                                tmp_array =
                                    QStringList::split(" ", chip[tmp_counter]);
                                if (tmp_array[3] && !strcmp(tmp_array[3], "name")) {
                                        if (!strcmp(tmp_array[2], "cpu")) {
                                                if (!strcmp(rom->Cpu1(), "-"))
                                                        rom->setCpu1(tmp_array[4]);
                                                else if (!strcmp(rom->Cpu2(), "-"))
                                                        rom->setCpu2(tmp_array[4]);
                                                else if (!strcmp(rom->Cpu3(), "-"))
                                                        rom->setCpu3(tmp_array[4]);
                                                else if (!strcmp(rom->Cpu4(), "-"))
                                                        rom->setCpu4(tmp_array[4]);
                                        }
                                        if (!strcmp(tmp_array[2], "audio")) {
                                                if (!strcmp(rom->Sound1(), "-"))
                                                        rom->setSound1(tmp_array[4]);
                                                else if (!strcmp(rom->Sound2(),"-"))
                                                        rom->setSound2(tmp_array[4]);
                                                else if (!strcmp(rom->Sound3(),"-"))
                                                        rom->setSound3(tmp_array[4]);
                                                else if (!strcmp(rom->Sound4(),"-"))
                                                        rom->setSound4(tmp_array[4]);
                                        }
                                }
                                tmp_array.clear();
                                tmp_counter++;
                        }
                        tmp_counter = 0;
                        while ((chip[tmp_counter] != "")) {
                                chip[tmp_counter] = "";
                                tmp_counter++;
                        }

                        tmp_array = QStringList::split(" ", video);
                        if (tmp_array[2] && !strcmp(tmp_array[2], "vector")) {
                                rom->setVector(TRUE);
                        } else {
                                rom->setVector(FALSE);
                        }

                        tmp_array.clear();
                        tmp_array = QStringList::split(" ", input);
                        tmp_counter = 0;
                        while ((tmp_array[tmp_counter + 1] != NULL)) {
                                if (!strcmp
                                    (tmp_array[tmp_counter], "players"))
                                        rom->setNum_players(atoi(tmp_array[tmp_counter + 1]));
                                if (!strcmp
                                    (tmp_array[tmp_counter], "control"))
                                        rom->setControl(tmp_array[tmp_counter + 1]);
                                if (!strcmp
                                    (tmp_array[tmp_counter], "buttons"))
                                        rom->setNum_buttons(atoi(tmp_array[tmp_counter + 1]));
                                tmp_counter++;
                        }
                        tmp_array.clear();

                        tmp_array = QStringList::split(" ", driver_status);
                        if ((tmp_array[2])
                            && !strcmp(tmp_array[2], "good")) {
                                rom->setWorking(TRUE);
                        } else {
                                rom->setWorking(FALSE);
                        }
                        tmp_array.clear();
                        if (!strcmp(status, "correct\n"))
                        {
                            rom->setStatus(CORRECT);

                            map<QString, QString>::iterator i;;
                            if ((!CatMap.empty()) && ((i = CatMap.find(rom->Romname().latin1())) != CatMap.end()))
                            {
                                rom->setGenre((*i).second);
                                //cout << "Genre = " << (*i).second.latin1() << endl;
                            }
                            else
                            {
                                rom->setGenre(QObject::tr("Unknown"));
                            }
                           
                            thequery = QString("INSERT INTO gamemetadata "
                                               "(system,romname,gamename,genre,"
                                               "year) VALUES (\"Mame\",\"%1\","
                                               "\"%2\",\"%3\",%4);")
                                               .arg(rom->Romname().latin1())
                                               .arg(rom->Gamename().latin1())
                                               .arg(rom->Genre().latin1())
                                               .arg(rom->Year());
                            db->exec(thequery);

                            thequery.sprintf("INSERT INTO mamemetadata (romname,manu,cloneof,romof,"
                                              "driver,cpu1,cpu2,cpu3,cpu4,sound1,sound2,sound3,sound4,"
                                              "players,buttons) VALUES (\"%s\",\"%s\",\"%s\",\"%s\","
                                              "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\","
                                              "\"%s\",\"%s\",%d,%d);", rom->Romname().latin1(),
                                              rom->Manu().latin1(), rom->Cloneof().latin1(),
                                              rom->Romof().latin1(), rom->Driver().latin1(),
                                              rom->Cpu1().latin1(), rom->Cpu2().latin1(),
                                              rom->Cpu3().latin1(), rom->Cpu4().latin1(),
                                              rom->Sound1().latin1(), rom->Sound2().latin1(),
                                              rom->Sound3().latin1(), rom->Sound4().latin1(),
                                              rom->Num_players(), rom->Num_buttons());
                            db->exec(thequery);
                        }

                        delete rom;

                        done_roms++;
                        done = (float) ((float) (done_roms) /
                                         (float) (supported_games));

                        pdial.setProgress(done_roms);
                }

        }

        VERBOSE(VB_ALL, "Finished rom scan. Cleaning up");
        VERBOSE(VB_ALL, "If the rom scan didn't detect any roms make sure "
                        "your history and high score settings point to FILES "
                        "not directories."); 

        pdial.Close();

        pclose(xmame_info);
        pclose(xmame_vrfy);
        pclose(xmame_drv);
        tmp_counter = 0;
        while ((chip[tmp_counter] != "")) {
                chip[tmp_counter] = "";
                tmp_counter++;
        }
}

void MameHandler::start_game(RomInfo * romdata)
{
        QString exec;
        check_xmame_exe();
        makecmd_line(romdata->Romname(), &exec, static_cast<MameRomInfo*>(romdata));

        // Get count of roms used for total in progress bar
        int romcount = 0;

        QString romlistcmd;
        makecmd_line("-lr \"" + romdata->Romname() + "\" 2>/dev/null", &romlistcmd, NULL);

        FILE* romlist_info = popen(romlistcmd, "r");

        char line[500];
        while (fgets(line, 500 - 1, romlist_info))
            ++romcount;
        // Adjust for non rom lines.  I think it's always the same.
        romcount -= 6;

        pclose(romlist_info);

        FILE *command;
        command = popen(exec + " 2>&1", "r");

        MythProgressDialog pdial(QObject::tr("Loading game..."), romcount);
        int loadprogress = 0;

        while (fgets(line, 500 - 1, command))
        {
            if (strncmp(line, "loading rom", 11) == 0)
                pdial.setProgress(++loadprogress);
        }

        pclose(command);
}

void MameHandler::edit_settings(RomInfo * romdata)
{
    GameSettings game_settings;
    MameRomInfo *mamedata = dynamic_cast<MameRomInfo*>(romdata);
    SetGameSettings(game_settings, mamedata);

    check_xmame_exe();
    MameSettingsDlg settingsdlg(mamedata->Romname().latin1(), &general_prefs);
    settingsdlg.exec(QSqlDatabase::database());
}

void MameHandler::edit_system_settings(RomInfo * romdata)
{
    romdata = romdata;

    check_xmame_exe();
    MameSettingsDlg settingsDlg("default", &general_prefs);
    settingsDlg.exec(QSqlDatabase::database());

    SetDefaultSettings();
}

bool MameHandler::check_xmame_exe()
{
        FILE *command;
        bool changed = FALSE;
        QString exec = "";
        char line[255];
        QString xmame_version_string = "";
        char *tmp = NULL;
        int i, major, release, versionchk;
        char *minor, *versionchk_string = NULL;
        QStringList version_array;

        xmame_version_ok = FALSE;

        /* Check if the xmame executable has changed */
        /* TODO: Expand mame executable to full path if the user has simply entered xmame */
        /* just check if it has an absolute path, get the PATH variable, split it and go through each part */
        /* Maybe set the prefs to the absolute path after */
        //cout << "xmame exe = " << general_prefs.xmame_exe << endl;
        if (!fopen(general_prefs.xmame_exe, "r")) {

                /* Remove the version currently defined in prefs */
                if (general_prefs.xmame_major ||
                    general_prefs.xmame_minor.length() ||
                    general_prefs.xmame_release ||
                    general_prefs.xmame_display_target.length() ) {
                        changed = TRUE;
                }
                general_prefs.xmame_major = 0;
                if (general_prefs.xmame_minor.length())
                        general_prefs.xmame_minor = "";
                general_prefs.xmame_minor = "";
                general_prefs.xmame_release = 0;
                if (general_prefs.xmame_display_target.length())
                        general_prefs.xmame_display_target = "";
                general_prefs.xmame_display_target = "";
        } else {
                /* Get output of xmame -version */
                exec = general_prefs.xmame_exe;
                exec+= " -version 2>/dev/null";
                command = popen(exec, "r");

                /* Chech xmame display target */
                while (fgets(line, 254, command) && !xmame_version_ok) {
                        tmp = line + 1;
                        while ((*tmp && (*tmp++ != '('))
                               && (*tmp != '\n'));
                        if (*tmp != '\n') {
                                xmame_version_string = tmp;
                                i = 0;

                                while (*tmp && (*tmp++ != ')'))
                                        i++;
                                xmame_version_string[i] = 0;
                        }

                        /* Set the xmame display_target in the general_prefs struct */
                        if (xmame_version_string) {
                                general_prefs.xmame_display_target = xmame_version_string;

                        }

                        /* Check xmame version */
                        xmame_version_string = "";
                        i = 0;
                        if ((*tmp != '\n')) {
                                /* skip " version " */
                                tmp = tmp + 9;
                                xmame_version_string = tmp;
                                while (*tmp && (*tmp++ != '\n'))
                                        i++;
                                xmame_version_string[i] = 0;

                                version_array =
                                    QStringList::split(".",xmame_version_string);

                                major = version_array[0].toInt();
                                if (version_array[1] != "")
                                        minor = strdup(version_array[1]);
                                else
                                        minor = strdup("00");
                                if (version_array[2] != "")
                                        release = version_array[2].toInt();
                                else
                                        release = 0;

                                //int supported_games;

                                if (!general_prefs.xmame_minor ||
                                    general_prefs.xmame_major != major ||
                                    strcmp(general_prefs.xmame_minor,
                                           minor)
                                    || general_prefs.xmame_release !=
                                    release) {
                                        changed = TRUE;

                                        /* This is just for debuging */
                                        if (general_prefs.xmame_major !=
                                            major)  {}
                                        if (!general_prefs.xmame_minor
                                            || strcmp(general_prefs.xmame_minor, minor)) {}
                                        if (general_prefs.xmame_release !=
                                            release) {}

                                }
                                if (general_prefs.xmame_minor.length()) general_prefs.xmame_minor = "";
                                general_prefs.xmame_major = major;
                                general_prefs.xmame_minor = minor;
                                general_prefs.xmame_release = release;

                                /* See if xmame is new enough. */
                                versionchk_string = (char*)malloc(3);
                                snprintf(versionchk_string, 3, "%s",
                                           minor);
                                versionchk = atoi(versionchk_string);
                                /*0.37 and up is ok */
                                if (versionchk >= 37)
                                        xmame_version_ok = TRUE;
                                else {
                                        /* The following four versions are ok */
                                        if (!strcmp(minor, "36b16") ||
                                            !strcmp(minor, "36rc1") ||
                                            !strcmp(minor, "36"))
                                                xmame_version_ok = TRUE;
                                }
                                free(versionchk_string);

                        }
                        version_array.clear();
                }
                pclose(command);
                exec = "";

                /* There is a problem with the installed xmame */
                if (!xmame_version_ok) {
                        if (xmame_version_string.length()) {
                        } else {
                                /* If the version was not defined before, something has changed */
                                if (general_prefs.xmame_major ||
                                    general_prefs.xmame_minor.length() ||
                                    general_prefs.xmame_release ||
                                    general_prefs.xmame_display_target.length()) {
                                        changed = TRUE;
                                }
                                general_prefs.xmame_major = 0;
                                if (general_prefs.xmame_minor.length())
                                        general_prefs.xmame_minor = "";
                                general_prefs.xmame_minor = "";
                                general_prefs.xmame_release = 0;
                                if (general_prefs.xmame_display_target.length())
                                        general_prefs.xmame_display_target = "";
                                general_prefs.xmame_display_target = "";
                        }
                }
        }
        return (changed);
}

void MameHandler::makecmd_line(const char * game, QString *exec, MameRomInfo * rominfo)
{
        QStringList screenshot_dirs;
        QString volume;
        QString scale;
        QString vectoropts;
        QString beam;
        QString flicker;
        QString vectorres;
        QString joytype;
        QString fullscreen;
        QString windowed;
        QString winkeys;
        QString nowinkeys;
        QString grabmouse;
        QString nograbmouse;
        QString screenshotdir;

        GameSettings game_settings;
        SetGameSettings(game_settings, rominfo);

        QString tmp;

        /* Need to do these ugly hacks to keep it from returning 0.0 as the scale value
           * or 0.000 for the beam value when no games are in the list (i.e
           * when gRustibus is started for the first time. -scale 0.0 and -beam
           * 0.00 makes xmame complain and quit.
         */
        if (game_settings.scale != 0)
                scale.sprintf("%d", game_settings.scale);
        else
                scale = "1";

        if (game_settings.beam != 0)
                beam.sprintf("%f",game_settings.beam);
        else
                beam = "1.0";

        flicker.sprintf("%f",game_settings.flicker);

  if (rominfo!=NULL && rominfo->Vector())
  {
    vectoropts = " -beam " + beam + " -flicker " + flicker;

    switch (game_settings.vectorres) {
    case 0:
      vectorres = " ";
      break;
    case 1:
      vectorres = " -vectorres 640x480";
      break;
    case 2:
      vectorres = " -vectorres 800x600";
      break;
    case 3:
      vectorres = " -vectorres 1024x768";
      break;
    case 4:
      vectorres = " -vectorres 1280x1024";
      break;
    case 5:
      vectorres = " -vectorres 1600x1200";
      break;
    }

  } else {
    vectoropts = " ";
    vectorres = " ";
  }

        if (general_prefs.screenshot_dir) {
                screenshot_dirs = QStringList::split(":", general_prefs.screenshot_dir);
                screenshotdir = screenshot_dirs[0];
        } else {
                 screenshotdir = "  ";
        }

        volume.sprintf("%d", game_settings.volume);
        joytype.sprintf("%d", game_settings.joytype);

        if (!strcmp(general_prefs.xmame_display_target, "x11")) {
                fullscreen = (game_settings.fullscreen == 1) ? " -x11-mode 1" :
                             " -fullscreen";
                windowed = " -x11-mode 3";
                winkeys = " -winkeys";
                nowinkeys = " -nowinkeys";
                grabmouse = " -grabmouse";
                nograbmouse = " -nograbmouse";
        } else if (!strcmp(general_prefs.xmame_display_target, "xgl")) {
                /* Check for version. 0.37b4->uses -nocabview */
                /* FIXME: This is a very ugly hack. It will be replaced by a
                 * more generic aproach shortly */
                tmp = general_prefs.xmame_minor;

                if (!strcmp(tmp, "37 BETA 4")) {
                        fullscreen = " -nocabview";
                } else {
                        fullscreen = " -fullview";
                }
                if (!strcmp(tmp, "37 BETA 5")) {
                        fullscreen = " -nocabview";
                } else {
                        fullscreen = " -fullview";
                }
                windowed = " -cabview";
                winkeys = " -winkeys";
                nowinkeys = " -nowinkeys";
                grabmouse = " -grabmouse";
                nograbmouse = " -nograbmouse";
        } else if (!strcmp(general_prefs.xmame_display_target, "xfx")) {
                fullscreen = " ";
                windowed = " ";
                winkeys = " -winkeys";
                nowinkeys = " -nowinkeys";
                grabmouse = " -grabmouse";
                nograbmouse = " -nograbmouse";
        }
        else if (!strcmp(general_prefs.xmame_display_target, "SDL")) {
                fullscreen = " -fullscreen";
                windowed = " -nofullscreen";
                winkeys = " ";
                nowinkeys = " ";
                grabmouse = " ";
                nograbmouse = " ";
        } else if (!strcmp(general_prefs.xmame_display_target, "ggi")) {
                fullscreen = " ";
                windowed = " ";
                winkeys = " ";
                nowinkeys = " ";
                grabmouse = " ";
                nograbmouse = " ";
                if (game_settings.fullscreen && !getuid())
                        putenv((char *)"GGI_DISPLAY=DGA");
                else
                        putenv((char *)"GGI_DISPLAY=X");
        }

        if (!general_prefs.xmame_exe.isEmpty())
            *exec = general_prefs.xmame_exe;
        else
        {
            cerr << "XMameBinary not set in mythgame.settings.txt, using "
                 << "default.";
            *exec = "/usr/X11R6/bin/xmame";
        }
        *exec+= " -rompath ";
        if (!general_prefs.rom_dir.isEmpty())
        {
            *exec+= general_prefs.rom_dir;
        }
        else
        {
            cerr << "MameRomLocation not set in mythgame-settings.txt, using "
                 << "default.";
            *exec+= "/roms";
        }


        /* define the integer version of xmame_minor so we only have to evaluate it once */
        tmp = general_prefs.xmame_minor;
        int xmame_minor = atoi(tmp);

        if (!general_prefs.cheat_file.isEmpty())
        {
           /* The cheatfile argument name changed in 0.62 */
           if (xmame_minor >= 62) 
             *exec+= " -cheat_file ";
           else
             *exec+= " -cheatfile ";
           *exec+= general_prefs.cheat_file;
        }
        if (!general_prefs.game_history_file.isEmpty())
        {
           /* The historyfile argument name changed in 0.62 */
           if (xmame_minor >= 62)
             *exec+= " -history_file ";
           else
             *exec+= " -historyfile ";
           *exec+= general_prefs.game_history_file;
        }
        if (!screenshotdir.isEmpty())
        {
          /* The screenshot dir argument name changed in 0.62. */
          if (xmame_minor >= 62) {
            *exec+= " -snapshot_directory ";
          } else {
            *exec+= " -screenshotdir ";
          }
          *exec+= screenshotdir;
        }
        if (!general_prefs.highscore_dir.isEmpty())
        {
          if (xmame_minor < 62) {
            *exec+= " -spooldir ";
            *exec+= general_prefs.highscore_dir;
          }
        }

        // The .65 builds of mame finally allow you to turn off the disclaimer
        //  and the game info screen.  See if the user wants them off
        if (xmame_minor >= 65 ) {
            *exec+= general_prefs.show_disclaimer ? " -noskip_disclaimer" : " -skip_disclaimer";
            *exec+= general_prefs.show_gameinfo ?" -noskip_gameinfo" : " -skip_gameinfo";
        }
        /* the nocursor option doesn't apply to SDL builds of xmame */
        if ( strcmp(general_prefs.xmame_display_target, "SDL")) {
          *exec+= game_settings.fullscreen ? (" -nocursor" + fullscreen) : 
                  windowed;
        }
        else
          *exec+= game_settings.fullscreen ? fullscreen : windowed;

        if(!game_settings.default_options)
        {
            *exec+= game_settings.scanlines ? " -scanlines" : " -noscanlines";
            *exec+= game_settings.extra_artwork ? " -artwork" : " -noartwork";
            *exec+= game_settings.autoframeskip ? " -autoframeskip" : " -noautoframeskip";
            *exec+= game_settings.auto_colordepth ? " -bpp 0" : " ";
            *exec+= game_settings.rot_left ? " -rol" : "";
            *exec+= game_settings.rot_right ? " -ror" : "";
            *exec+= game_settings.flipx ? " -flipx" : "";
            *exec+= game_settings.flipy ? " -flipy" : "";
            *exec+= " -scale ";
            *exec+= scale;
            *exec+= game_settings.antialias ? " -antialias" : " -noantialias";
            *exec+= game_settings.translucency ? " -translucency" : " -notranslucency";
            *exec+= vectoropts;
            *exec+= vectorres;
            *exec+= game_settings.analog_joy ? " -analogstick" : " -noanalogstick";
            *exec+= game_settings.mouse ? " -mouse" : " -nomouse";
            *exec+= game_settings.winkeys ? winkeys : nowinkeys;
            *exec+= game_settings.grab_mouse ? grabmouse : nograbmouse;
            *exec+= " -joytype ";
            *exec+= joytype;
            *exec+= game_settings.sound ? " -sound" : " -nosound";
            *exec+= game_settings.samples ? " -samples" : " -nosamples";
            *exec+= game_settings.fake_sound ? " -fakesound" : "";
            *exec+= " -volume ";
            *exec+= volume;
            *exec+=  " ";
            *exec+= game_settings.cheat ? " -cheat " : " -nocheat ";
            //*exec+= game_settings.extra_options ? game_settings.extra_options : " ";
        }
        *exec+= " ";
        *exec+= game;
}

void MameHandler::SetGeneralPrefs()
{
    general_prefs.xmame_exe = gContext->GetSetting("XMameBinary");
    general_prefs.rom_dir = gContext->GetSetting("MameRomLocation");
    general_prefs.screenshot_dir = gContext->GetSetting("MameScreensLocation");
    general_prefs.highscore_dir = gContext->GetSetting("MameScoresLocation");
    general_prefs.flyer_dir = gContext->GetSetting("MameFlyersLocation");
    general_prefs.cabinet_dir = gContext->GetSetting("MameCabinetsLocation");
    general_prefs.game_history_file = gContext->GetSetting("MameHistoryLocation");
    general_prefs.cheat_file = gContext->GetSetting("MameCheatLocation");
    general_prefs.show_disclaimer = gContext->GetNumSetting("MameShowDisclaimer");
    general_prefs.show_gameinfo = gContext->GetNumSetting("MameShowGameInfo");
}

void MameHandler::SetGameSettings(GameSettings &game_settings, MameRomInfo *rominfo)
{
    game_settings = defaultSettings;
    if(rominfo)
    {
        QSqlDatabase *db = QSqlDatabase::database();
        QString thequery; 
        thequery = QString("SELECT * FROM mamesettings WHERE romname = \"%1\";")
                          .arg(rominfo->Romname().latin1());
        QSqlQuery query = db->exec(thequery);
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            if (!query.value(1).toBool())
            {
                game_settings.default_options = false;
                game_settings.fullscreen = query.value(2).toInt();
                game_settings.scanlines = query.value(3).toBool();
                game_settings.extra_artwork = query.value(4).toBool();
                game_settings.autoframeskip = query.value(5).toBool();
                game_settings.auto_colordepth = query.value(6).toBool();
                game_settings.rot_left = query.value(7).toBool();
                game_settings.rot_right = query.value(8).toBool();
                game_settings.flipx = query.value(9).toBool();
                game_settings.flipy = query.value(10).toBool();
                game_settings.scale = query.value(11).toInt();
                game_settings.antialias = query.value(12).toBool();
                game_settings.translucency = query.value(13).toBool();
                game_settings.beam = query.value(14).toDouble();
                game_settings.flicker = query.value(15).toDouble();
                game_settings.vectorres = query.value(16).toInt();
                game_settings.analog_joy = query.value(17).toBool();
                game_settings.mouse = query.value(18).toBool();
                game_settings.winkeys = query.value(19).toBool();
                game_settings.grab_mouse = query.value(20).toBool();
                game_settings.joytype = query.value(21).toInt();
                game_settings.sound = query.value(22).toBool();
                game_settings.samples = query.value(23).toBool();
                game_settings.fake_sound = query.value(24).toBool();
                game_settings.volume = query.value(25).toInt();
                game_settings.cheat = query.value(26).toBool();
                game_settings.extra_options = query.value(27).toString();
            }
            else
                game_settings.default_options = true;
        }
    }
}

void MameHandler::SetDefaultSettings()
{
    QSqlDatabase *db = QSqlDatabase::database();
    QSqlQuery query = db->exec("SELECT * FROM mamesettings WHERE romname = \"default\";");

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();
        defaultSettings.default_options = query.value(1).toBool();
        defaultSettings.fullscreen = query.value(2).toInt();
        defaultSettings.scanlines = query.value(3).toBool();
        defaultSettings.extra_artwork = query.value(4).toBool();
        defaultSettings.autoframeskip = query.value(5).toBool();
        defaultSettings.auto_colordepth = query.value(6).toBool();
        defaultSettings.rot_left = query.value(7).toBool();
        defaultSettings.rot_right = query.value(8).toBool();
        defaultSettings.flipx = query.value(9).toBool();
        defaultSettings.flipy = query.value(10).toBool();
        defaultSettings.scale = query.value(11).toInt();
        defaultSettings.antialias = query.value(12).toBool();
        defaultSettings.translucency = query.value(13).toBool();
        defaultSettings.beam = query.value(14).toDouble();
        defaultSettings.flicker = query.value(15).toDouble();
        defaultSettings.vectorres = query.value(16).toInt();
        defaultSettings.analog_joy = query.value(17).toBool();
        defaultSettings.mouse = query.value(18).toBool();
        defaultSettings.winkeys = query.value(19).toBool();
        defaultSettings.grab_mouse = query.value(20).toBool();
        defaultSettings.joytype = query.value(21).toInt();
        defaultSettings.sound = query.value(22).toBool();
        defaultSettings.samples = query.value(23).toBool();
        defaultSettings.fake_sound = query.value(24).toBool();
        defaultSettings.volume = query.value(25).toInt();
        defaultSettings.cheat = query.value(26).toBool();
        defaultSettings.extra_options = query.value(27).toString();
    }
}

MameHandler* MameHandler::getHandler(void)
{
    if(!pInstance)
    {
        pInstance = new MameHandler();
    }
    return pInstance;
}

RomInfo* MameHandler::create_rominfo(RomInfo *parent)
{
    return new MameRomInfo(*parent);
}

bool MameHandler::LoadCatfile(map<QString, QString>* pCatMap)
{
    QString CatFile = gContext->GetSetting("XMameCatFile");
    VERBOSE(VB_GENERAL, QString("Loading xmame catfile from %1...")
                               .arg(CatFile));
    fstream fin(CatFile.ascii(), ios::in);
    if (!fin.is_open())
        return false;

    string strLine;
    QString strKey;
    QString strVal;
    int nSplitPoint = 0;
    while(!fin.eof())
    {
        getline(fin,strLine);
        if((strLine[0] != ';') && (!strLine.empty()))
        {
            if(strLine.find("[VerAdded]") != string::npos)  //don't care about version right now. maybe in the future.
                break;
            nSplitPoint = strLine.find('=');
            if(nSplitPoint != -1)
            {
                strKey = strLine.substr(0, nSplitPoint).c_str();
                strVal = strLine.substr(nSplitPoint + 1, strLine.size()).c_str();
                (*pCatMap)[strKey] = strVal;
            }
        }
    }

    return true;
}   

MameHandler* MameHandler::pInstance = 0;

