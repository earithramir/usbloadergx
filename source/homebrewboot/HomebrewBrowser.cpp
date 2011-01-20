/****************************************************************************
 * Copyright (C) 2010
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "HomebrewBrowser.hpp"
#include "themes/CTheme.h"
#include "prompts/PromptWindows.h"
#include "language/gettext.h"
#include "network/networkops.h"
#include "utils/minizip/miniunz.h"
#include "usbloader/utils.h"
#include "prompts/TitleBrowser.h"
#include "homebrewboot/BootHomebrew.h"
#include "prompts/ProgressWindow.h"
#include "wstring.hpp"
#include "HomebrewXML.h"

extern u32 infilesize;
extern u32 uncfilesize;
extern char wiiloadVersion[2];
extern int connection;

HomebrewBrowser::HomebrewBrowser()
    : FlyingButtonsMenu(tr( "Homebrew Launcher" ))
{
    HomebrewList = new HomebrewFiles(Settings.homebrewapps_path);

    if (IsNetworkInit())
        ResumeNetworkWait();

    wifiNotSet = true;
    wifiImgData = Resources::GetImageData("Wifi_btn.png");
    wifiToolTip = new GuiTooltip(" ");
    wifiImg = new GuiImage(wifiImgData);
    wifiBtn = new GuiButton(wifiImgData->GetWidth(), wifiImgData->GetHeight());
    wifiBtn->SetImage(wifiImg);
    wifiBtn->SetPosition(300, 400);
    wifiBtn->SetSoundOver(btnSoundOver);
    wifiBtn->SetSoundClick(btnSoundClick);
    wifiBtn->SetEffectGrow();
    wifiBtn->SetAlpha(80);
    wifiBtn->SetTrigger(trigA);
    Append(wifiBtn);

    channelImgData = Resources::GetImageData("Channel_btn.png");
    channelBtnImg = new GuiImage(channelImgData);
    channelBtnImg->SetWidescreen(Settings.widescreen);
    channelBtn = new GuiButton(channelBtnImg->GetWidth(), channelBtnImg->GetHeight());
    channelBtn->SetPosition(240, 400);
    channelBtn->SetImage(channelBtnImg);
    channelBtn->SetSoundOver(btnSoundOver);
    channelBtn->SetSoundClick(btnSoundClick2);
    channelBtn->SetEffectGrow();
    channelBtn->SetTrigger(trigA);
    if (Settings.godmode || !(Settings.ParentalBlocks & BLOCK_TITLE_LAUNCHER_MENU))
        Append(channelBtn);

    MainButtonDesc.resize(HomebrewList->GetFilecount());
    MainButtonDescOver.resize(HomebrewList->GetFilecount());

    for(u32 i = 0; i < 4; ++i)
    {
        IconImgData[i] = NULL;
        IconImg[i] = NULL;
    }

    for(int i = 0; i < HomebrewList->GetFilecount(); ++i)
    {
        MainButtonDesc[i] = new GuiText((char *) NULL, 18, (GXColor) {0, 0, 0, 255});
        MainButtonDesc[i]->SetMaxWidth(MainButtonImgData->GetWidth() - 150, DOTTED);
        MainButtonDesc[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
        MainButtonDesc[i]->SetPosition(148, 15);

        MainButtonDescOver[i] = new GuiText((char *) NULL, 18, (GXColor) {0, 0, 0, 255});
        MainButtonDescOver[i]->SetMaxWidth(MainButtonImgData->GetWidth() - 150, SCROLL_HORIZONTAL);
        MainButtonDescOver[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
        MainButtonDescOver[i]->SetPosition(148, 15);
    }

    SetupMainButtons();
}

HomebrewBrowser::~HomebrewBrowser()
{
    HaltGui();
    delete HomebrewList;

    Remove(wifiBtn);
    delete wifiImgData;
    delete wifiImg;
    delete wifiToolTip;
    delete wifiBtn;

    Remove(channelBtn);
    delete channelImgData;
    delete channelBtnImg;
    delete channelBtn;

    for(u32 i = 0; i < MainButtonDesc.size(); ++i)
    {
        delete MainButtonDesc[i];
        delete MainButtonDescOver[i];
        MainButton[i]->SetLabel(NULL, 1);
        MainButton[i]->SetLabelOver(NULL, 1);
    }

    if (IsNetworkInit())
        HaltNetworkThread();
}

void HomebrewBrowser::AddMainButtons()
{
    HaltGui();

    for(u32 i = 0; i < 4; ++i)
    {
        if(IconImgData[i])
            delete IconImgData[i];
        if(IconImg[i])
            delete IconImg[i];
        IconImgData[i] = NULL;
        IconImg[i] = NULL;
    }

    for(u32 i = 0; i < MainButton.size(); ++i)
        MainButton[i]->SetIcon(NULL);

    char iconpath[200];
    int FirstItem = currentPage*DISPLAY_BUTTONS;

    for(int i = FirstItem, n = 0; i < (int) MainButton.size() && i < FirstItem+DISPLAY_BUTTONS; ++i, ++n)
    {
        snprintf(iconpath, sizeof(iconpath), "%sicon.png", HomebrewList->GetFilepath(i));
        IconImgData[n] = new GuiImageData(iconpath);
        IconImg[n] = new GuiImage(IconImgData[n]);
        IconImg[n]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
        IconImg[n]->SetPosition(12, 0);
        IconImg[n]->SetScale(0.95);
        MainButton[i]->SetIcon(IconImg[n]);
    }

    FlyingButtonsMenu::AddMainButtons();
}

void HomebrewBrowser::SetupMainButtons()
{
    HomebrewXML MetaXML;
    char metapath[200];

    for(int i = 0; i < HomebrewList->GetFilecount(); ++i)
    {
        const char * HomebrewName = NULL;
        snprintf(metapath, sizeof(metapath), "%smeta.xml", HomebrewList->GetFilepath(i));

        if (MetaXML.LoadHomebrewXMLData(metapath) > 0)
        {
            HomebrewName = MetaXML.GetName();
            MainButtonDesc[i]->SetText(MetaXML.GetShortDescription());
            MainButtonDescOver[i]->SetText(MetaXML.GetShortDescription());
        }
        else
        {
            const char * shortpath = strrchr(HomebrewList->GetFilename(i), '/');
            if(shortpath)
            {
                snprintf(metapath, sizeof(metapath), "%s/%s", shortpath, HomebrewList->GetFilename(i));
                HomebrewName = metapath;
            }
            else
                HomebrewName = HomebrewList->GetFilename(i);
            MainButtonDesc[i]->SetText(" ");
            MainButtonDescOver[i]->SetText(" ");
        }

        SetMainButton(i, HomebrewName, MainButtonImgData, MainButtonImgOverData);

        MainButtonTxt[i]->SetFontSize(18);
        MainButtonTxt[i]->SetMaxWidth(MainButtonImgData->GetWidth() - 150, DOTTED);
        MainButtonTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
        MainButtonTxt[i]->SetPosition(148, -12);
        MainButton[i]->SetLabel(MainButtonDesc[i], 1);
        MainButton[i]->SetLabelOver(MainButtonDescOver[i], 1);
    }
}

int HomebrewBrowser::MainLoop()
{
    if (IsNetworkInit() && wifiNotSet)
    {
        wifiToolTip->SetText(GetNetworkIP());
        wifiBtn->SetAlpha(255);
        wifiBtn->SetToolTip(wifiToolTip, 0, -50, 0, 5);
        wifiNotSet = false;
    }

    if(wifiBtn->GetState() == STATE_CLICKED)
    {
        ResumeNetworkWait();
        wifiBtn->ResetState();
    }
    else if(channelBtn->GetState() == STATE_CLICKED)
    {
        SetState(STATE_DISABLED);
        TitleBrowser();
        SetState(STATE_DEFAULT);
        channelBtn->ResetState();
    }
    else if (infilesize > 0)
    {
        int menu = ReceiveFile();
        if(menu != MENU_NONE)
            return menu;
        CloseConnection();
        ResumeNetworkWait();
    }

    return FlyingButtonsMenu::MainLoop();
}

//! Callback for MainButton clicked
void HomebrewBrowser::MainButtonClicked(int button)
{
    HomebrewXML MetaXML;
    char metapath[200];
    snprintf(metapath, sizeof(metapath), "%smeta.xml", HomebrewList->GetFilepath(button));
    MetaXML.LoadHomebrewXMLData(metapath);

    u64 filesize = HomebrewList->GetFilesize(button);

    wString HomebrewName(MainButtonTxt[button]->GetText());

    int choice = HBCWindowPrompt(HomebrewName.toUTF8().c_str(), MetaXML.GetCoder(), MetaXML.GetVersion(),
                            MetaXML.GetReleasedate(), MetaXML.GetLongDescription(), IconImgData[button % 4], filesize);

    if (choice == 1)
    {
        char homebrewpath[200];
        snprintf(homebrewpath, sizeof(homebrewpath), "%s%s", HomebrewList->GetFilepath(button), HomebrewList->GetFilename(button));
        BootHomebrew(homebrewpath);
    }
}

int HomebrewBrowser::ReceiveFile()
{
    char filesizetxt[50];
    char temp[50];
    u32 filesize = 0;

    if (infilesize < MB_SIZE)
        snprintf(filesizetxt, sizeof(filesizetxt), tr( "Incoming file %0.2fKB" ), infilesize / KB_SIZE);
    else snprintf(filesizetxt, sizeof(filesizetxt), tr( "Incoming file %0.2fMB" ), infilesize / MB_SIZE);

    snprintf(temp, sizeof(temp), tr( "Load file from: %s ?" ), GetIncommingIP());

    int choice = WindowPrompt(filesizetxt, temp, tr( "OK" ), tr( "Cancel" ));

    if (choice == 0)
        return MENU_NONE;

    u32 read = 0;
    int len = NETWORKBLOCKSIZE;
    filesize = infilesize;
    u8 * buffer = (u8 *) malloc(infilesize);
    if(!buffer)
    {
        WindowPrompt(tr( "Not enough memory." ), 0, tr( "OK" ));
        return MENU_NONE;
    }

    bool error = false;
    while (read < infilesize)
    {
        ShowProgress(tr( "Receiving file from:" ), GetIncommingIP(), NULL, read, infilesize, true);

        if (infilesize - read < (u32) len)
            len = infilesize - read;
        else len = NETWORKBLOCKSIZE;

        int result = network_read(connection, buffer+read, len);

        if (result < 0)
        {
            WindowPrompt(tr( "Error while transfering data." ), 0, tr( "OK" ));
            free(buffer);
            return MENU_NONE;
        }
        if (!result)
        {
            break;
        }

        read += result;
    }

    char filename[101];
    network_read(connection, (u8*) &filename, 100);

    // Do we need to unzip this thing?
    if (wiiloadVersion[0] > 0 || wiiloadVersion[1] > 4)
    {
        // We need to unzip...
        if (buffer[0] == 'P' && buffer[1] == 'K' && buffer[2] == 0x03 && buffer[3] == 0x04)
        {
            // It's a zip file, unzip to the apps directory
            // Zip archive, ask for permission to install the zip
            char zippath[255];
            sprintf(zippath, "%s%s", Settings.homebrewapps_path, filename);

            FILE *fp = fopen(zippath, "wb");
            if (!fp)
            {
                WindowPrompt(tr( "Error writing the data." ), 0, tr( "OK" ));
                return MENU_NONE;
            }

            fwrite(buffer, 1, infilesize, fp);
            fclose(fp);

            free(buffer);
            buffer = NULL;

            // Now unzip the zip file...
            unzFile uf = unzOpen(zippath);
            if (uf == NULL)
            {
                WindowPrompt(tr( "Error while opening the zip." ), 0, tr( "OK" ));
                return MENU_NONE;
            }

            extractZip(uf, 0, 1, 0, Settings.homebrewapps_path);
            unzCloseCurrentFile(uf);

            remove(zippath);

            WindowPrompt(tr( "Success:" ),
                    tr( "Uploaded ZIP file installed to homebrew directory." ), tr( "OK" ));

            // Reload this menu here...
            return MENU_HOMEBREWBROWSE;
        }
        else if (uncfilesize != 0) // if uncfilesize == 0, it's not compressed
        {
            // It's compressed, uncompress
            u8 *unc = (u8 *) malloc(uncfilesize);
            uLongf f = uncfilesize;
            error = uncompress(unc, &f, buffer, infilesize) != Z_OK;
            uncfilesize = f;
            filesize = uncfilesize;

            free(buffer);
            buffer = unc;
        }
    }

    CopyHomebrewMemory(buffer, 0, filesize);
    free(buffer);

    ProgressStop();

    if (error || read != infilesize || strcasestr(filename, ".dol") || strcasestr(filename, ".elf"))
    {
        WindowPrompt(tr( "Error:" ), tr( "No data could be read." ), tr( "OK" ));
        FreeHomebrewBuffer();
        return MENU_NONE;
    }

    CloseConnection();

    AddBootArgument(filename);

    return BootHomebrewFromMem();
}
