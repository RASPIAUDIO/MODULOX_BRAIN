#include <WiFi.h>
#include <FS.h>
// Remove the file systems that are not needed.
#include <SD.h>
#include <SD_MMC.h>
#include <FFat.h>
#include <SPI.h>
// the thing.
#include <ESPFMfGK.h>

const word filemanagerport = 8080;
// we want a different port than the webserver
ESPFMfGK filemgr(filemanagerport);

// Adds the filé systems
void addFileSystems(void) {
  // This adds the Storage into the Filemanager. You have to at least call one of those.
  // If you don't, begin() will fail. Because a Filemanager without files is useless.

  /**/  //<-- Addd space there like this /** /
  if (FFat.begin(true)) {
    if (!filemgr.AddFS(FFat, "Flash/FFat", false)) {
      Serial.println(F("Adding FFAT failed."));
    }
  } else {
    Serial.println(F("FFat File System not inited."));
  }
  /**/

  /**/
/*  if (SD_MMC.begin("/sdcard", true)) {
    if (!filemgr.AddFS(SD_MMC, "SD-MMC-Card", false)) {
      Serial.println(F("Adding SD_MMC failed."));
    }
  } else {
    Serial.println(F("SD_MMC File System not inited."));
  }*/
  /**/

  /**/
/*  const byte SS = 5;  // D8
  if (SD.begin(SS)) {
    if (!filemgr.AddFS(SD, "SD-Card", false)) {
      Serial.println(F("Adding SD failed."));
    }
  } else {
    Serial.println(F("SD File System not inited."));
  }*/
  /**/
}

uint32_t checkFileFlags(fs::FS &fs, String filename, uint32_t flags) {
  // Checks if target file name is valid for action. This will simply allow everything by returning the queried flag
  if (flags & ESPFMfGK::flagIsValidAction) {
    return flags & (~ESPFMfGK::flagIsValidAction);
  }

  // Checks if target file name is valid for action.
  if (flags & ESPFMfGK::flagIsValidTargetFilename) {
    return flags & (~ESPFMfGK::flagIsValidTargetFilename);
  }

  // Default actions
  uint32_t defaultflags = ESPFMfGK::flagCanDelete | ESPFMfGK::flagCanRename | ESPFMfGK::flagCanGZip |  // ^t
                          ESPFMfGK::flagCanDownload | ESPFMfGK::flagCanUpload | ESPFMfGK::flagCanEdit | // ^t
                          ESPFMfGK::flagAllowPreview;

  return defaultflags;
}

void setupFilemanager(void) {
  // See above.
  filemgr.checkFileFlags = checkFileFlags;

  filemgr.WebPageTitle = "FileManager";
  filemgr.BackgroundColor = "white";
  filemgr.textareaCharset = "accept-charset=\"utf-8\"";

  if ((WiFi.status() == WL_CONNECTED) && (filemgr.begin())) {
    Serial.print(F("Open Filemanager with http://"));
    Serial.print(WiFi.localIP());
    Serial.print(F(":"));
    Serial.print(filemanagerport);
    Serial.print(F("/"));
    Serial.println();
  } else {
    Serial.print(F("Filemanager: did not start"));
  }
}

bool setup_wifi()
{
  bool ret=false;
  int try_number=0;
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED && try_number<100) {
    delay(10);
	try_number++;
  }
  if(try_number<100) ret=true;
  
  if(ret)
  {
    addFileSystems();
    setupFilemanager();
  }
  
  return ret;
  
}


void wifi_listener()
{
	filemgr.handleClient();
}

