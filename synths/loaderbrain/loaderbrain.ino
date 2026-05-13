#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Rotary.h>
#include <SD.h>
#include <SD_MMC.h>
#include <diskio.h>
#include <sd_diskio.h>
#include <SPI.h>
#include <stdarg.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"

#define SD_CS 13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCK 14
#define TFT_CS_PIN 40

#define BUT1 GPIO_NUM_12
#define BUT2 GPIO_NUM_1
#define BUT3 GPIO_NUM_38
#define BUTENCO GPIO_NUM_39
#define BUTLEFT GPIO_NUM_0

static const char *MANIFEST_PATH = "/MODULOX/manifest.json";
static const char *WIFI_CONFIG_PATH = "/MODULOX/wifi.json";
static const char *CATALOG_URL = "https://apps.raspiaudio.com/modulox/catalog.json";
static const char *TARGET_ID = "ESP32-S3-WROOM-2-N32R16V";
static const size_t IO_BUFFER_SIZE = 32768;
static const int MAX_MACHINES = 8;
static const uint8_t LOADER_TFT_ROTATION = 3;
static const int SD_MMC_TRANSFER_FREQS[] = {20000, 10000, 4000};
static const bool VERIFY_SD_FILE_SHA = false;
static const bool VERIFY_APP_FLASH_AFTER_WRITE = false;
static const bool VERIFY_DATA_FLASH_AFTER_WRITE = false;
static const char *LOADER_PREFS_NAMESPACE = "modulox";
static const char *LOADER_PREFS_LAST_MACHINE = "last_machine";
static const char *LOADER_PREFS_AUTO_BOOT = "auto_boot";
static const char *LOADER_PREFS_STAY_ONCE = "stay_once";
static const uint32_t AUTO_BOOT_WINDOW_MS = 2000;
static const gpio_num_t AUTO_BOOT_BUTTON = BUT3;
static const gpio_num_t AUTO_BOOT_FALLBACK_BUTTON = BUTENCO;

// Sectigo Public Server Authentication CA DV R36, issuer of apps.raspiaudio.com.
static const char APPS_RASPIAUDIO_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIGTDCCBDSgAwIBAgIQOXpmzCdWNi4NqofKbqvjsTANBgkqhkiG9w0BAQwFADBf
MQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQD
Ey1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBSNDYw
HhcNMjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5WjBgMQswCQYDVQQGEwJHQjEY
MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFB1Ymxp
YyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gQ0EgRFYgUjM2MIIBojANBgkqhkiG9w0B
AQEFAAOCAY8AMIIBigKCAYEAljZf2HIz7+SPUPQCQObZYcrxLTHYdf1ZtMRe7Yeq
RPSwygz16qJ9cAWtWNTcuICc++p8Dct7zNGxCpqmEtqifO7NvuB5dEVexXn9RFFH
12Hm+NtPRQgXIFjx6MSJcNWuVO3XGE57L1mHlcQYj+g4hny90aFh2SCZCDEVkAja
EMMfYPKuCjHuuF+bzHFb/9gV8P9+ekcHENF2nR1efGWSKwnfG5RawlkaQDpRtZTm
M64TIsv/r7cyFO4nSjs1jLdXYdz5q3a4L0NoabZfbdxVb+CUEHfB0bpulZQtH1Rv
38e/lIdP7OTTIlZh6OYL6NhxP8So0/sht/4J9mqIGxRFc0/pC8suja+wcIUna0HB
pXKfXTKpzgis+zmXDL06ASJf5E4A2/m+Hp6b84sfPAwQ766rI65mh50S0Di9E3Pn
2WcaJc+PILsBmYpgtmgWTR9eV9otfKRUBfzHUHcVgarub/XluEpRlTtZudU5xbFN
xx/DgMrXLUAPaI60fZ6wA+PTAgMBAAGjggGBMIIBfTAfBgNVHSMEGDAWgBRWc1hk
lfmSGrASKgRieaFAFYghSTAdBgNVHQ4EFgQUaMASFhgOr872h6YyV6NGUV3LBycw
DgYDVR0PAQH/BAQDAgGGMBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYDVR0lBBYwFAYI
KwYBBQUHAwEGCCsGAQUFBwMCMBsGA1UdIAQUMBIwBgYEVR0gADAIBgZngQwBAgEw
VAYDVR0fBE0wSzBJoEegRYZDaHR0cDovL2NybC5zZWN0aWdvLmNvbS9TZWN0aWdv
UHVibGljU2VydmVyQXV0aGVudGljYXRpb25Sb290UjQ2LmNybDCBhAYIKwYBBQUH
AQEEeDB2ME8GCCsGAQUFBzAChkNodHRwOi8vY3J0LnNlY3RpZ28uY29tL1NlY3Rp
Z29QdWJsaWNTZXJ2ZXJBdXRoZW50aWNhdGlvblJvb3RSNDYucDdjMCMGCCsGAQUF
BzABhhdodHRwOi8vb2NzcC5zZWN0aWdvLmNvbTANBgkqhkiG9w0BAQwFAAOCAgEA
YtOC9Fy+TqECFw40IospI92kLGgoSZGPOSQXMBqmsGWZUQ7rux7cj1du6d9rD6C8
ze1B2eQjkrGkIL/OF1s7vSmgYVafsRoZd/IHUrkoQvX8FZwUsmPu7amgBfaY3g+d
q1x0jNGKb6I6Bzdl6LgMD9qxp+3i7GQOnd9J8LFSietY6Z4jUBzVoOoz8iAU84OF
h2HhAuiPw1ai0VnY38RTI+8kepGWVfGxfBWzwH9uIjeooIeaosVFvE8cmYUB4TSH
5dUyD0jHct2+8ceKEtIoFU/FfHq/mDaVnvcDCZXtIgitdMFQdMZaVehmObyhRdDD
4NQCs0gaI9AAgFj4L9QtkARzhQLNyRf87Kln+YU0lgCGr9HLg3rGO8q+Y4ppLsOd
unQZ6ZxPNGIfOApbPVf5hCe58EZwiWdHIMn9lPP6+F404y8NNugbQixBber+x536
WrZhFZLjEkhp7fFXf9r32rNPfb74X/U90Bdy4lzp3+X1ukh1BuMxA/EEhDoTOS3l
7ABvc7BYSQubQ2490OcdkIzUh3ZwDrakMVrbaTxUM2p24N6dB+ns2zptWCva6jzW
r8IWKIMxzxLPv5Kt3ePKcUdvkBU/smqujSczTzzSjIoR5QqQA6lN1ZRSnuHIWCvh
JEltkYnTAH41QJ6SAWO66GrrUESwN/cgZzL4JLEqz1Y=
-----END CERTIFICATE-----
)EOF";

enum ScreenMode {
  SCREEN_MAIN_MENU,
  SCREEN_MACHINE_LIST
};

struct MachineEntry {
  String id;
  String name;
  String version;
  int versionCode = 0;
  String appPath;
  String appSha256;
  size_t appSize = 0;
  String dataPath;
  String dataSha256;
  size_t dataSize = 0;
  String dataPartition = "ffat";
  String dataOffset = "0x910000";
  String dataImageMode = "raw";
};

struct WifiConfig {
  String ssid;
  String password;
};

struct LoaderBootState {
  String lastMachine;
  bool autoBootEnabled = false;
  bool stayLoaderOnce = false;
};

TFT_eSPI tft;
Rotary encoder(6, 7);
uint8_t ioBuffer[IO_BUFFER_SIZE];
MachineEntry machines[MAX_MACHINES];
int machineCount = 0;
int selectedMachine = 0;
int selectedMenu = 0;
bool loading = false;
bool sdReady = false;
bool manifestLoaded = false;
ScreenMode screenMode = SCREEN_MAIN_MENU;
String sdStatusLine = "SD not checked";
bool displayReady = false;
WifiConfig cachedWifi;
bool wifiConfigLoaded = false;
bool displaySuspendedForSd = false;
String lastMessageLine1;
String lastMessageLine2;
String serialCommandLine;
String serial0CommandLine;
fs::FS *sdFs = &SD;
String sdBackendName = "none";

bool edgePressed(gpio_num_t pin);
bool installMachine(const MachineEntry &machine);
bool bootPartitionByLabel(const char *label);
LoaderBootState readLoaderBootState();
void saveAutoBootMachine(const String &machineId);
bool handleLoaderAutoBoot(const LoaderBootState &state, esp_reset_reason_t reason);

void logLine(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

void logPrintf(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
  Serial0.print(buffer);
}

void endSd() {
  SD.end();
  SD_MMC.end();
  SPI.end();
  sdFs = &SD;
  sdBackendName = "none";
}

File sdOpen(const String &path, const char *mode) {
  return sdFs->open(path.c_str(), mode);
}

bool sdExists(const String &path) {
  return sdFs->exists(path.c_str());
}

bool sdRemove(const String &path) {
  return sdFs->remove(path.c_str());
}

bool sdRename(const String &from, const String &to) {
  return sdFs->rename(from.c_str(), to.c_str());
}

bool sdMkdir(const String &path) {
  return sdFs->mkdir(path.c_str());
}

uint32_t readLe32(const uint8_t *data) {
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

void dumpSdSectorProbe(SPIClass &spi) {
  logLine("SD raw probe");
  uint8_t pdrv = sdcard_init(SD_CS, &spi, 400000);
  if (pdrv == 0xFF) {
    logLine("raw init failed");
    return;
  }

  DSTATUS status = disk_initialize(pdrv);
  logPrintf("disk_initialize=0x%02X\n", (unsigned)status);

  if (sd_read_raw(pdrv, ioBuffer, 0)) {
    uint8_t *part = ioBuffer + 446;
    logPrintf("LBA0 sig=%02X%02X p0_type=0x%02X p0_lba=%lu p0_sectors=%lu first=%02X %02X %02X %02X\n",
              ioBuffer[511],
              ioBuffer[510],
              part[4],
              (unsigned long)readLe32(part + 8),
              (unsigned long)readLe32(part + 12),
              ioBuffer[0],
              ioBuffer[1],
              ioBuffer[2],
              ioBuffer[3]);
  } else {
    logLine("read LBA0 failed");
  }

  if (sd_read_raw(pdrv, ioBuffer, 2048)) {
    char oem[9];
    memcpy(oem, ioBuffer + 3, 8);
    oem[8] = 0;
    logPrintf("LBA2048 sig=%02X%02X oem=%s bps=%u spc=%u fats=%u first=%02X %02X %02X\n",
              ioBuffer[511],
              ioBuffer[510],
              oem,
              (unsigned)ioBuffer[11] | ((unsigned)ioBuffer[12] << 8),
              (unsigned)ioBuffer[13],
              (unsigned)ioBuffer[16],
              ioBuffer[0],
              ioBuffer[1],
              ioBuffer[2]);
  } else {
    logLine("read LBA2048 failed");
  }

  sdcard_uninit(pdrv);
}

bool probeSpiPins(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs) {
  SPIClass &probeSpi = SPI;
  SD_MMC.end();
  probeSpi.end();
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  gpio_pullup_en((gpio_num_t)miso);
  gpio_pullup_en((gpio_num_t)mosi);
  gpio_pullup_en((gpio_num_t)cs);
  probeSpi.begin(sck, miso, mosi, cs);

  uint8_t pdrv = sdcard_init(cs, &probeSpi, 400000);
  if (pdrv == 0xFF) {
    logPrintf("SPI probe sck=%u miso=%u mosi=%u cs=%u init=no\n", sck, miso, mosi, cs);
    return false;
  }

  DSTATUS status = disk_initialize(pdrv);
  bool lba0 = status == 0 && sd_read_raw(pdrv, ioBuffer, 0);
  if (!lba0) memset(ioBuffer, 0, 512);
  uint16_t sig = ((uint16_t)ioBuffer[511] << 8) | ioBuffer[510];
  uint8_t ptype = ioBuffer[446 + 4];
  uint32_t plba = readLe32(ioBuffer + 446 + 8);
  bool plausible = lba0 && sig == 0xAA55 && (ptype == 0x0B || ptype == 0x0C || ptype == 0x06 || ptype == 0x0E);

  logPrintf("SPI probe sck=%u miso=%u mosi=%u cs=%u init=0x%02X lba0=%s sig=%04X ptype=%02X plba=%lu\n",
            sck,
            miso,
            mosi,
            cs,
            (unsigned)status,
            lba0 ? "yes" : "no",
            sig,
            ptype,
            (unsigned long)plba);

  sdcard_uninit(pdrv);
  probeSpi.end();
  return plausible;
}

void scanSdSpiPins() {
  logLine("SPI pin scan");
  const uint8_t misoCandidates[] = {SD_MISO, 13, 10, 11, 12, 16, 17, 21, 42};
  const uint8_t csCandidates[] = {SD_CS, 10, 2, 4, 5, 21, 40, 42};

  for (uint8_t miso : misoCandidates) {
    if (probeSpiPins(SD_SCK, miso, SD_MOSI, SD_CS)) return;
  }

  for (uint8_t cs : csCandidates) {
    if (cs == SD_CS) continue;
    if (probeSpiPins(SD_SCK, SD_MISO, SD_MOSI, cs)) return;
  }

  probeSpiPins(12, 13, 11, 10);
}

bool probeSdMmc() {
  logLine("SD_MMC 4-bit probe");
  SD_MMC.end();
  pinMode(4, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(SD_CS, INPUT_PULLUP);
  gpio_pullup_en(GPIO_NUM_4);
  gpio_pullup_en(GPIO_NUM_12);
  gpio_pullup_en(GPIO_NUM_13);
  if (SD_MMC.setPins(SD_SCK, SD_MOSI, SD_MISO, 4, 12, SD_CS)) {
    if (SD_MMC.begin("/sdmmc", false, false, SDMMC_FREQ_DEFAULT, 5)) {
      logPrintf("SD_MMC 4-bit mounted type=%u size=%llu MB\n",
                (unsigned)SD_MMC.cardType(),
                SD_MMC.cardSize() / (1024ULL * 1024ULL));
      bool hasManifest = SD_MMC.exists(MANIFEST_PATH);
      logPrintf("SD_MMC 4-bit manifest %s\n", hasManifest ? "found" : "missing");
      SD_MMC.end();
      return true;
    }
    logLine("SD_MMC 4-bit begin failed");
    SD_MMC.end();
  } else {
    logLine("SD_MMC 4-bit setPins failed");
  }

  logLine("SD_MMC 1-bit probe");
  if (!SD_MMC.setPins(SD_SCK, SD_MOSI, SD_MISO)) {
    logLine("SD_MMC setPins failed");
    return false;
  }

  if (!SD_MMC.begin("/sdmmc", true, false, SDMMC_FREQ_DEFAULT, 5)) {
    logLine("SD_MMC begin failed");
    SD_MMC.end();
    return false;
  }

  logPrintf("SD_MMC mounted type=%u size=%llu MB\n",
            (unsigned)SD_MMC.cardType(),
            SD_MMC.cardSize() / (1024ULL * 1024ULL));
  bool hasManifest = SD_MMC.exists(MANIFEST_PATH);
  logPrintf("SD_MMC manifest %s\n", hasManifest ? "found" : "missing");
  SD_MMC.end();
  return true;
}

const char *MENU_ITEMS[] = {
  "Install from SD",
  "Check WiFi updates",
  "WiFi status"
};
static const int MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

void drawHeader(const String &title) {
  if (!displayReady) {
    logLine(title);
    return;
  }
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("MODULOX Loader", 8, 6, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(title, 8, 30, 2);
  logLine(title);
}

void drawMessage(const String &line1, const String &line2 = "") {
  lastMessageLine1 = line1;
  lastMessageLine2 = line2;
  if (!displayReady) {
    logLine(line1);
    if (line2.length()) logLine(line2);
    return;
  }
  tft.fillRect(0, 55, tft.width(), tft.height() - 55, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(line1, 8, 70, 2);
  if (line2.length()) tft.drawString(line2, 8, 94, 2);
  logLine(line1);
  if (line2.length()) logLine(line2);
}

void drawProgress(const String &title, size_t done, size_t total) {
  if (!displayReady) {
    static uint32_t lastLogMs = 0;
    static String lastTitle;
    const uint32_t now = millis();
    if (title != lastTitle) {
      lastTitle = title;
      lastLogMs = 0;
    }
    if (done == 0 || done == total || now - lastLogMs > 2000) {
      lastLogMs = now;
      logPrintf("%s %lu/%lu\n", title.c_str(), (unsigned long)done, (unsigned long)total);
    }
    return;
  }
  static uint32_t lastDrawMs = 0;
  const uint32_t now = millis();
  if (done != total && now - lastDrawMs < 180) return;
  lastDrawMs = now;

  int pct = total ? (int)((done * 100ULL) / total) : 0;
  if (pct > 100) pct = 100;

  tft.fillRect(0, 55, tft.width(), tft.height() - 55, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(title, 8, 70, 2);
  tft.drawString(String(pct) + "%", 8, 96, 2);
  tft.drawRect(8, 124, tft.width() - 16, 16, TFT_DARKGREY);
  tft.fillRect(10, 126, (tft.width() - 20) * pct / 100, 12, TFT_GREEN);

  logPrintf("%s %d%% (%lu/%lu)\n",
            title.c_str(),
            pct,
            (unsigned long)done,
            (unsigned long)total);
}

void suspendDisplayForSd(const String &line1, const String &line2 = "") {
  if (!displayReady) return;

  drawMessage(line1, line2);
  delay(350);
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);
  TFT_eSPI::getSPIinstance().end();
  displayReady = false;
  displaySuspendedForSd = true;
  logLine("Display SPI released for SD");
}

void resumeDisplayAfterSd() {
  if (!displaySuspendedForSd) return;

  endSd();
  delay(50);
  tft.init();
  displayReady = true;
  displaySuspendedForSd = false;
  tft.setRotation(LOADER_TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  logLine("Display SPI resumed");
}

bool abortSdOperation() {
  endSd();
  resumeDisplayAfterSd();
  if (displayReady && lastMessageLine1.length()) {
    drawMessage(lastMessageLine1, lastMessageLine2);
  }
  return false;
}

bool remountSdForStage(const String &stage) {
  logLine("Remount SD: " + stage);
  endSd();
  delay(100);
  return beginSd();
}

void drawMainMenu() {
  screenMode = SCREEN_MAIN_MENU;
  drawHeader("Main menu");
  if (!displayReady) return;

  for (int i = 0; i < MENU_COUNT; i++) {
    const int y = 62 + i * 28;
    const bool selected = (i == selectedMenu);
    tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_GREEN : TFT_BLACK);
    tft.fillRect(8, y - 2, tft.width() - 16, 24, selected ? TFT_GREEN : TFT_BLACK);
    tft.drawString(String(selected ? "> " : "  ") + MENU_ITEMS[i], 12, y, 2);
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(sdStatusLine, 8, tft.height() - 24, 2);
}

void drawMachineList() {
  screenMode = SCREEN_MACHINE_LIST;
  drawHeader("Select machine");
  if (!displayReady) return;

  if (machineCount == 0) {
    drawMessage("No machine found", MANIFEST_PATH);
    return;
  }

  for (int i = 0; i < machineCount; i++) {
    const int y = 62 + i * 24;
    const bool selected = (i == selectedMachine);
    tft.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_GREEN : TFT_BLACK);
    tft.fillRect(8, y - 2, tft.width() - 16, 22, selected ? TFT_GREEN : TFT_BLACK);
    tft.drawString(String(selected ? "> " : "  ") + machines[i].name, 12, y, 2);
    if (machines[i].version.length()) {
      tft.setTextColor(selected ? TFT_BLACK : TFT_DARKGREY, selected ? TFT_GREEN : TFT_BLACK);
      tft.drawString(machines[i].version, tft.width() - 70, y, 2);
    }
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Press encoder to load", 8, tft.height() - 24, 2);
}

String sha256ToString(const uint8_t digest[32]) {
  static const char hex[] = "0123456789abcdef";
  String out;
  out.reserve(64);
  for (int i = 0; i < 32; i++) {
    out += hex[digest[i] >> 4];
    out += hex[digest[i] & 0x0F];
  }
  return out;
}

bool sha256File(const String &path, size_t expectedSize, String &shaOut, const String &title) {
  File file = sdOpen(path, FILE_READ);
  if (!file || file.isDirectory()) {
    logPrintf("Cannot open %s\n", path.c_str());
    return false;
  }

  const size_t fileSize = file.size();
  if (fileSize < expectedSize) {
    logPrintf("File too small for SHA: %s %lu/%lu\n",
              path.c_str(),
              (unsigned long)fileSize,
              (unsigned long)expectedSize);
    file.close();
    return false;
  }

  mbedtls_sha256_context ctx;
  uint8_t digest[32];
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, false);

  size_t done = 0;
  drawProgress(title, 0, expectedSize);
  while (done < expectedSize) {
    const size_t toRead = min(sizeof(ioBuffer), expectedSize - done);
    int readLen = file.read(ioBuffer, toRead);
    if (readLen <= 0) {
      logPrintf("SHA read failed at %lu/%lu for %s\n",
                (unsigned long)done,
                (unsigned long)expectedSize,
                path.c_str());
      file.close();
      mbedtls_sha256_free(&ctx);
      return false;
    }
    mbedtls_sha256_update(&ctx, ioBuffer, (size_t)readLen);
    done += (size_t)readLen;
    drawProgress(title, done, expectedSize);
    delay(0);
  }

  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  file.close();
  shaOut = sha256ToString(digest);
  return true;
}

bool sha256Partition(const esp_partition_t *partition, size_t size, String &shaOut, const String &title) {
  if (!partition || size > partition->size) return false;

  mbedtls_sha256_context ctx;
  uint8_t digest[32];
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, false);

  size_t offset = 0;
  while (offset < size) {
    const size_t toRead = min(sizeof(ioBuffer), size - offset);
    esp_err_t err = esp_partition_read(partition, offset, ioBuffer, toRead);
    if (err != ESP_OK) {
      logPrintf("partition read failed: 0x%x\n", (unsigned)err);
      mbedtls_sha256_free(&ctx);
      return false;
    }
    mbedtls_sha256_update(&ctx, ioBuffer, toRead);
    offset += toRead;
    drawProgress(title, offset, size);
  }

  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  shaOut = sha256ToString(digest);
  return true;
}

String sdAppPath(const String &id) {
  return "/MODULOX/" + id + "/app.bin";
}

String sdDataPath(const String &id) {
  return "/MODULOX/" + id + "/data.ffat.bin";
}

String dirnameOf(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

bool ensureDirectory(const String &path) {
  if (path == "/" || sdExists(path)) return true;
  int slash = path.lastIndexOf('/');
  if (slash > 0) {
    String parent = path.substring(0, slash);
    if (!ensureDirectory(parent)) return false;
  }
  return sdMkdir(path) || sdExists(path);
}

bool verifyFile(const String &path, size_t expectedSize, const String &expectedSha, const String &label) {
  logPrintf("Verify %s path=%s expected=%lu\n",
            label.c_str(),
            path.c_str(),
            (unsigned long)expectedSize);
  File file = sdOpen(path, FILE_READ);
  if (!file || file.isDirectory()) {
    drawMessage("Missing file", path);
    return false;
  }
  size_t actualSize = file.size();
  logPrintf("Open %s size=%lu\n", label.c_str(), (unsigned long)actualSize);
  file.close();

  if (actualSize != expectedSize) {
    drawMessage("Bad size: " + label, String((unsigned long)actualSize));
    return false;
  }

  if (!VERIFY_SD_FILE_SHA) {
    logLine("SD SHA skipped for " + label);
    return true;
  }

  String actualSha;
  if (!sha256File(path, expectedSize, actualSha, "Verify " + label)) {
    drawMessage("SHA read failed", label);
    return false;
  }

  actualSha.toLowerCase();
  String expected = expectedSha;
  expected.toLowerCase();
  if (actualSha != expected) {
    drawMessage("Bad SHA: " + label, actualSha.substring(0, 16));
    logPrintf("Expected %s\nActual   %s\n", expected.c_str(), actualSha.c_str());
    return false;
  }

  return true;
}

bool readMachine(JsonObject item, MachineEntry &entry, bool remotePaths) {
  JsonObject app = item["app"];
  JsonObject data = item["data"];

  entry.id = item["id"] | "";
  entry.name = item["name"] | entry.id.c_str();
  entry.version = item["version"] | "";
  entry.versionCode = item["version_code"] | 0;
  entry.appPath = remotePaths ? String(app["url"] | "") : String(app["path"] | "");
  entry.appSize = app["size"] | 0;
  entry.appSha256 = app["sha256"] | "";
  entry.dataPath = remotePaths ? String(data["url"] | "") : String(data["path"] | "");
  entry.dataSize = data["size"] | 0;
  entry.dataSha256 = data["sha256"] | "";
  entry.dataPartition = data["partition"] | "ffat";
  entry.dataOffset = data["offset"] | "0x910000";
  entry.dataImageMode = data["image_mode"] | "raw";

  return entry.id.length() &&
         entry.appPath.length() &&
         entry.appSha256.length() == 64 &&
         entry.appSize > 0;
}

int findMachineIndex(const String &id) {
  for (int i = 0; i < machineCount; i++) {
    if (machines[i].id == id) return i;
  }
  return -1;
}

bool loadManifest() {
  File manifest = sdOpen(MANIFEST_PATH, FILE_READ);
  if (!manifest || manifest.isDirectory()) {
    manifestLoaded = false;
    drawMessage("Manifest missing", MANIFEST_PATH);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, manifest);
  manifest.close();
  if (err) {
    manifestLoaded = false;
    drawMessage("Manifest JSON error", err.c_str());
    return false;
  }

  JsonArray array = doc["machines"].as<JsonArray>();
  if (array.isNull()) {
    manifestLoaded = false;
    drawMessage("No machines array", MANIFEST_PATH);
    return false;
  }

  machineCount = 0;
  for (JsonObject item : array) {
    if (machineCount >= MAX_MACHINES) break;
    if (readMachine(item, machines[machineCount], false)) {
      machineCount++;
    }
  }

  if (machineCount == 0) {
    manifestLoaded = false;
    drawMessage("No valid machine", MANIFEST_PATH);
    return false;
  }

  manifestLoaded = true;
  selectedMachine = 0;
  logPrintf("Manifest loaded: %d machine(s)\n", machineCount);
  return true;
}

bool writeAppToOtaSlot(const MachineEntry &machine, const esp_partition_t *appPartition) {
  logPrintf("Open app for OTA: %s\n", machine.appPath.c_str());
  File *appFile = new File(sdOpen(machine.appPath, FILE_READ));
  if (!appFile || !(*appFile)) {
    drawMessage("Open app failed", machine.appPath);
    return false;
  }

  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(appPartition, machine.appSize, &handle);
  if (err != ESP_OK) {
    drawMessage("OTA begin failed", String((unsigned)err, HEX));
    return false;
  }

  size_t written = 0;
  while (written < machine.appSize) {
    int readLen = appFile->read(ioBuffer, sizeof(ioBuffer));
    if (readLen <= 0) {
      esp_ota_abort(handle);
      drawMessage("App read failed", machine.appPath);
      return false;
    }

    err = esp_ota_write(handle, ioBuffer, (size_t)readLen);
    if (err != ESP_OK) {
      esp_ota_abort(handle);
      drawMessage("OTA write failed", String((unsigned)err, HEX));
      return false;
    }

    written += (size_t)readLen;
    drawProgress("Write app", written, machine.appSize);
  }

  logLine("App file read complete");
  err = esp_ota_end(handle);
  if (err != ESP_OK) {
    drawMessage("OTA end failed", String((unsigned)err, HEX));
    return false;
  }

  if (!VERIFY_APP_FLASH_AFTER_WRITE) {
    logLine("App flash SHA skipped after esp_ota_end");
    return true;
  }

  String partitionSha;
  if (!sha256Partition(appPartition, machine.appSize, partitionSha, "Verify app flash")) {
    drawMessage("App verify failed");
    return false;
  }

  String expected = machine.appSha256;
  expected.toLowerCase();
  partitionSha.toLowerCase();
  if (partitionSha != expected) {
    drawMessage("App flash SHA bad", partitionSha.substring(0, 16));
    return false;
  }

  return true;
}

bool writeDataPartition(const MachineEntry &machine) {
  if (machine.dataPath.length() == 0 || machine.dataSize == 0) return true;

  const esp_partition_t *dataPartition = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_DATA_FAT,
    machine.dataPartition.c_str()
  );
  if (!dataPartition) {
    drawMessage("No data partition", machine.dataPartition);
    return false;
  }

  if (machine.dataSize > dataPartition->size) {
    drawMessage("Data too large", String((unsigned long)machine.dataSize));
    return false;
  }

  logPrintf("Open data for flash: %s\n", machine.dataPath.c_str());
  File *dataFile = new File(sdOpen(machine.dataPath, FILE_READ));
  if (!dataFile || !(*dataFile)) {
    drawMessage("Open data failed", machine.dataPath);
    return false;
  }

  drawMessage("Erase data partition", machine.dataPartition);
  esp_err_t err = esp_partition_erase_range(dataPartition, 0, dataPartition->size);
  if (err != ESP_OK) {
    drawMessage("Data erase failed", String((unsigned)err, HEX));
    return false;
  }

  size_t written = 0;
  while (written < machine.dataSize) {
    int readLen = dataFile->read(ioBuffer, sizeof(ioBuffer));
    if (readLen <= 0) {
      drawMessage("Data read failed", machine.dataPath);
      return false;
    }

    err = esp_partition_write(dataPartition, written, ioBuffer, (size_t)readLen);
    if (err != ESP_OK) {
      drawMessage("Data write failed", String((unsigned)err, HEX));
      return false;
    }

    written += (size_t)readLen;
    drawProgress("Write data", written, machine.dataSize);
  }

  logLine("Data file read complete");

  if (!VERIFY_DATA_FLASH_AFTER_WRITE) {
    logLine("Data flash verify skipped after SD SHA check");
    return true;
  }

  String partitionSha;
  if (!sha256Partition(dataPartition, machine.dataSize, partitionSha, "Verify data flash")) {
    drawMessage("Data verify failed");
    return false;
  }

  String expected = machine.dataSha256;
  expected.toLowerCase();
  partitionSha.toLowerCase();
  if (partitionSha != expected) {
    drawMessage("Data flash SHA bad", partitionSha.substring(0, 16));
    return false;
  }

  return true;
}

bool installMachine(const MachineEntry &machine) {
  loading = true;
  drawHeader("Install " + machine.name);

  suspendDisplayForSd("Installing " + machine.name, "Screen paused for SD");
  if (!beginSd()) return abortSdOperation();

  if (!verifyFile(machine.appPath, machine.appSize, machine.appSha256, "app")) return abortSdOperation();
  if (machine.dataPath.length()) {
    if (!verifyFile(machine.dataPath, machine.dataSize, machine.dataSha256, "data")) return abortSdOperation();
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *app1 = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_APP_OTA_1,
    "app1"
  );
  if (!app1) {
    drawMessage("No app1 partition");
    return abortSdOperation();
  }
  if (running && running->address == app1->address) {
    drawMessage("Loader is on app1", "Refusing self-write");
    return abortSdOperation();
  }

  if (!remountSdForStage("write app/data")) return abortSdOperation();
  if (!writeAppToOtaSlot(machine, app1)) return abortSdOperation();
  if (!writeDataPartition(machine)) return abortSdOperation();

  saveAutoBootMachine(machine.id);
  drawMessage("Set boot app1", "Reset now...");
  esp_err_t err = esp_ota_set_boot_partition(app1);
  if (err != ESP_OK) {
    drawMessage("Set boot failed", String((unsigned)err, HEX));
    return abortSdOperation();
  }

  logLine("Install complete, boot app1 selected");
  logLine("Restarting now");
  Serial.flush();
  Serial0.flush();
  delay(300);
  esp_restart();
  return true;
}

void logOtaStatus() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  if (running) {
    logPrintf("OTA running: %s @0x%lx size=0x%lx\n",
              running->label,
              (unsigned long)running->address,
              (unsigned long)running->size);
  } else {
    logLine("OTA running: none");
  }
  if (boot) {
    logPrintf("OTA boot: %s @0x%lx size=0x%lx\n",
              boot->label,
              (unsigned long)boot->address,
              (unsigned long)boot->size);
  } else {
    logLine("OTA boot: none");
  }
}

bool bootPartitionByLabel(const char *label) {
  const esp_partition_t *partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    label
  );
  if (!partition) {
    logPrintf("Boot command failed: partition %s not found\n", label);
    return false;
  }

  esp_err_t err = esp_ota_set_boot_partition(partition);
  if (err != ESP_OK) {
    logPrintf("Boot command failed for %s: 0x%x\n", label, (unsigned)err);
    return false;
  }

  logPrintf("Boot command set %s, restarting\n", label);
  delay(250);
  esp_restart();
  return true;
}

void handleSerialCommand(String command) {
  command.trim();
  if (!command.length()) return;

  String lower = command;
  lower.toLowerCase();
  logPrintf("serial command: %s\n", command.c_str());

  if (lower == "status") {
    logOtaStatus();
  } else if (lower == "boot app0" || lower == "boot loader") {
    bootPartitionByLabel("app0");
  } else if (lower == "boot app1" || lower == "boot synth") {
    bootPartitionByLabel("app1");
  } else if (lower == "autoboot on") {
    setAutoBootEnabled(true);
  } else if (lower == "autoboot off" || lower == "autoboot clear") {
    setAutoBootEnabled(false);
  } else if (lower == "install") {
    if (!sdReady) {
      logLine("install failed: SD not ready");
    } else if (!((manifestLoaded && machineCount > 0) || loadManifest())) {
      logLine("install failed: manifest not loaded");
    } else {
      installMachine(machines[0]);
    }
  } else {
    logLine("commands: status | boot app0 | boot app1 | autoboot on | autoboot off | install");
  }
}

void pollSerialCommands(Stream &stream, String &lineBuffer) {
  while (stream.available()) {
    char c = (char)stream.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleSerialCommand(lineBuffer);
      lineBuffer = "";
    } else if (lineBuffer.length() < 96) {
      lineBuffer += c;
    } else {
      lineBuffer = "";
    }
  }
}

bool readWifiConfig(WifiConfig &config) {
  File file = sdOpen(WIFI_CONFIG_PATH, FILE_READ);
  if (!file || file.isDirectory()) {
    drawMessage("wifi.json missing", WIFI_CONFIG_PATH);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    drawMessage("wifi.json error", err.c_str());
    return false;
  }

  config.ssid = doc["ssid"] | "";
  config.password = doc["password"] | "";
  if (config.ssid.length() == 0) {
    drawMessage("WiFi SSID empty", WIFI_CONFIG_PATH);
    return false;
  }

  return true;
}

bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WifiConfig config = cachedWifi;
  if (!wifiConfigLoaded) {
    if (!readWifiConfig(config)) return false;
    cachedWifi = config;
    wifiConfigLoaded = true;
  }

  drawMessage("Connecting WiFi", config.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  Serial.print(".");
  Serial0.print(".");
  }
  logLine("");

  if (WiFi.status() != WL_CONNECTED) {
    drawMessage("WiFi failed", String((int)WiFi.status()));
    return false;
  }

  drawMessage("WiFi connected", WiFi.localIP().toString());
  delay(700);
  return true;
}

bool fetchCatalog(JsonDocument &catalogDoc) {
  if (!connectWifi()) return false;

  WiFiClientSecure client;
  client.setCACert(APPS_RASPIAUDIO_CA);
  HTTPClient http;
  http.setTimeout(15000);

  drawMessage("Fetch catalog", CATALOG_URL);
  if (!http.begin(client, CATALOG_URL)) {
    drawMessage("HTTPS begin failed");
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    drawMessage("Catalog HTTP error", String(code));
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DeserializationError err = deserializeJson(catalogDoc, payload);
  if (err) {
    drawMessage("Catalog JSON error", err.c_str());
    return false;
  }

  const char *target = catalogDoc["target"] | "";
  if (String(target) != TARGET_ID) {
    drawMessage("Catalog target bad", target);
    return false;
  }

  return true;
}

bool downloadHttpsFile(const String &url, const String &destPath, size_t expectedSize, const String &expectedSha, const String &label) {
  if (!ensureDirectory(dirnameOf(destPath))) {
    drawMessage("Cannot mkdir", dirnameOf(destPath));
    return false;
  }

  String tmpPath = destPath + ".part";
  sdRemove(tmpPath);

  WiFiClientSecure client;
  client.setCACert(APPS_RASPIAUDIO_CA);
  HTTPClient http;
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  drawMessage("Download " + label, url);
  if (!http.begin(client, url)) {
    drawMessage("Download begin failed", label);
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    drawMessage("Download HTTP error", String(code));
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength > 0 && (size_t)contentLength != expectedSize) {
    drawMessage("Download size bad", String(contentLength));
    http.end();
    return false;
  }

  File out = sdOpen(tmpPath, FILE_WRITE);
  if (!out) {
    drawMessage("Open .part failed", tmpPath);
    http.end();
    return false;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, false);

  NetworkClient *stream = http.getStreamPtr();
  size_t written = 0;
  uint32_t lastDataMs = millis();
  while (written < expectedSize) {
    size_t available = stream->available();
    if (available) {
      size_t toRead = min(available, sizeof(ioBuffer));
      toRead = min(toRead, expectedSize - written);
      int readLen = stream->readBytes(ioBuffer, toRead);
      if (readLen <= 0) break;

      size_t fileWritten = out.write(ioBuffer, (size_t)readLen);
      if (fileWritten != (size_t)readLen) {
        out.close();
        http.end();
        mbedtls_sha256_free(&ctx);
        sdRemove(tmpPath);
        drawMessage("SD write failed", label);
        return false;
      }

      mbedtls_sha256_update(&ctx, ioBuffer, (size_t)readLen);
      written += (size_t)readLen;
      lastDataMs = millis();
      drawProgress("Download " + label, written, expectedSize);
    } else {
      if (!http.connected() || millis() - lastDataMs > 20000) break;
      delay(1);
    }
  }

  out.close();
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  if (written != expectedSize) {
    sdRemove(tmpPath);
    drawMessage("Download incomplete", String((unsigned long)written));
    return false;
  }

  String actualSha = sha256ToString(digest);
  String expected = expectedSha;
  actualSha.toLowerCase();
  expected.toLowerCase();
  if (actualSha != expected) {
    sdRemove(tmpPath);
    drawMessage("Download SHA bad", label);
    logPrintf("Expected %s\nActual   %s\n", expected.c_str(), actualSha.c_str());
    return false;
  }

  sdRemove(destPath);
  if (!sdRename(tmpPath, destPath)) {
    sdRemove(tmpPath);
    drawMessage("Rename failed", destPath);
    return false;
  }

  return true;
}

bool writeManifestFromCatalog(JsonDocument &catalogDoc) {
  JsonDocument outDoc;
  JsonObject root = outDoc.to<JsonObject>();
  root["version"] = catalogDoc["version"] | 1;
  root["format"] = "modulox-loader-sd-v1";
  root["target"] = TARGET_ID;
  root["partition_scheme"] = catalogDoc["partition_scheme"] | "app5M_fat24M_32MB";
  root["partition_csv"] = catalogDoc["partition_csv"] | "large_fat_32MB";
  root["flash_size"] = catalogDoc["flash_size"] | "32MB";
  JsonArray outMachines = root["machines"].to<JsonArray>();

  JsonArray remoteMachines = catalogDoc["machines"].as<JsonArray>();
  for (JsonObject remote : remoteMachines) {
    MachineEntry entry;
    if (!readMachine(remote, entry, true)) continue;

    JsonObject machine = outMachines.add<JsonObject>();
    machine["id"] = entry.id;
    machine["name"] = entry.name;
    machine["version"] = entry.version;
    machine["version_code"] = entry.versionCode;

    JsonObject app = machine["app"].to<JsonObject>();
    app["path"] = sdAppPath(entry.id);
    app["size"] = entry.appSize;
    app["sha256"] = entry.appSha256;

    JsonObject data = machine["data"].to<JsonObject>();
    data["path"] = sdDataPath(entry.id);
    data["size"] = entry.dataSize;
    data["sha256"] = entry.dataSha256;
    data["partition"] = entry.dataPartition;
    data["offset"] = entry.dataOffset;
    data["image_mode"] = entry.dataImageMode;
  }

  String tmpPath = String(MANIFEST_PATH) + ".part";
  sdRemove(tmpPath);
  File file = sdOpen(tmpPath, FILE_WRITE);
  if (!file) {
    drawMessage("Manifest write fail", tmpPath);
    return false;
  }
  serializeJsonPretty(outDoc, file);
  file.println();
  file.close();

  sdRemove(MANIFEST_PATH);
  if (!sdRename(tmpPath, MANIFEST_PATH)) {
    drawMessage("Manifest rename fail");
    sdRemove(tmpPath);
    return false;
  }

  return true;
}

bool confirmDownload(int updateCount) {
  drawHeader("WiFi updates");
  drawMessage("Download " + String(updateCount) + " update(s)", "Encoder/BUT3 OK, BUT2 cancel");
  while (true) {
    if (edgePressed(BUTENCO) || edgePressed(BUT3)) return true;
    if (edgePressed(BUT2)) {
      drawMessage("Download cancelled");
      delay(900);
      return false;
    }
    delay(20);
  }
}

bool checkWifiUpdates() {
  loading = true;
  drawHeader("WiFi updates");

  JsonDocument catalogDoc;
  if (!fetchCatalog(catalogDoc)) return false;

  JsonArray remoteMachines = catalogDoc["machines"].as<JsonArray>();
  if (remoteMachines.isNull()) {
    drawMessage("Catalog empty");
    return false;
  }

  if (!manifestLoaded) {
    loadManifest();
  }

  int checked = 0;
  int updateCount = 0;
  for (JsonObject remote : remoteMachines) {
    MachineEntry entry;
    if (!readMachine(remote, entry, true)) continue;
    checked++;

    int localIndex = findMachineIndex(entry.id);
    int localVersion = (localIndex >= 0) ? machines[localIndex].versionCode : 0;
    if (entry.versionCode > localVersion) updateCount++;
    else logPrintf("%s already up to date (%d)\n", entry.id.c_str(), localVersion);
  }

  if (checked == 0) {
    drawMessage("No valid remote machines");
    return false;
  }

  if (updateCount == 0) {
    drawMessage("Already up to date");
    delay(1200);
    return true;
  }

  if (!confirmDownload(updateCount)) return true;

  suspendDisplayForSd("Downloading updates", "Screen paused for SD");
  if (!beginSd()) return abortSdOperation();

  int updated = 0;
  for (JsonObject remote : remoteMachines) {
    MachineEntry entry;
    if (!readMachine(remote, entry, true)) continue;

    int localIndex = findMachineIndex(entry.id);
    int localVersion = (localIndex >= 0) ? machines[localIndex].versionCode : 0;
    if (entry.versionCode <= localVersion) continue;

    String appDest = sdAppPath(entry.id);
    String dataDest = sdDataPath(entry.id);
    if (!downloadHttpsFile(entry.appPath, appDest, entry.appSize, entry.appSha256, entry.id + " app")) return abortSdOperation();
    if (entry.dataPath.length() && entry.dataSize > 0) {
      if (!downloadHttpsFile(entry.dataPath, dataDest, entry.dataSize, entry.dataSha256, entry.id + " data")) return abortSdOperation();
    }
    updated++;
  }

  if (!writeManifestFromCatalog(catalogDoc)) return abortSdOperation();
  if (!loadManifest()) return abortSdOperation();
  resumeDisplayAfterSd();
  drawMessage("Updates downloaded", String(updated) + " machine(s)");
  delay(1500);
  return true;
}

void showWifiStatus() {
  drawHeader("WiFi status");
  if (WiFi.status() == WL_CONNECTED) {
    drawMessage("Connected", WiFi.SSID() + " " + WiFi.localIP().toString());
    return;
  }

  if (wifiConfigLoaded) {
    drawMessage("Configured", cachedWifi.ssid);
    return;
  }

  WifiConfig config;
  if (!readWifiConfig(config)) return;
  cachedWifi = config;
  wifiConfigLoaded = true;
  drawMessage("Configured", cachedWifi.ssid);
}

bool beginSd() {
  endSd();

  pinMode(SD_MOSI, INPUT_PULLUP); // CMD
  pinMode(SD_MISO, INPUT_PULLUP); // D0
  pinMode(SD_CS, INPUT_PULLUP);   // D3, left pulled up for 1-bit mode
  gpio_pullup_en((gpio_num_t)SD_MOSI);
  gpio_pullup_en((gpio_num_t)SD_MISO);
  gpio_pullup_en((gpio_num_t)SD_CS);

  logLine("Mount SD_MMC 1-bit");
  drawMessage("Mount SD", "SD_MMC 1-bit");
  if (!SD_MMC.setPins(SD_SCK, SD_MOSI, SD_MISO)) {
    logLine("SD_MMC setPins failed");
    sdStatusLine = "SD mount failed";
    drawMessage("SD mount failed", "pins");
    return false;
  }

  for (int freqIndex = 0; freqIndex < (int)(sizeof(SD_MMC_TRANSFER_FREQS) / sizeof(SD_MMC_TRANSFER_FREQS[0])); freqIndex++) {
    const int frequency = SD_MMC_TRANSFER_FREQS[freqIndex];
    for (int attempt = 1; attempt <= 2; attempt++) {
      logPrintf("SD_MMC begin %dMHz attempt %d\n", frequency / 1000, attempt);
      if (SD_MMC.begin("/sdmmc", true, false, frequency, 5)) {
        sdFs = &SD_MMC;
        sdBackendName = "SD_MMC";
        uint64_t cardSizeMb = SD_MMC.cardSize() / (1024ULL * 1024ULL);
        sdStatusLine = "SD OK SDMMC " + String(frequency / 1000) + "MHz " + String((unsigned long)cardSizeMb) + "MB";
        logPrintf("SD_MMC size: %llu MB freq=%dMHz\n", cardSizeMb, frequency / 1000);
        return true;
      }

      logPrintf("SD_MMC begin failed %dMHz attempt %d\n", frequency / 1000, attempt);
      SD_MMC.end();
      delay(250);
    }
  }

  sdStatusLine = "SD mount failed";
  drawMessage("SD mount failed", "SD_MMC 1-bit");
  return false;
}

bool edgePressed(gpio_num_t pin) {
  static bool previous[49] = {false};
  static bool idle[49] = {false};
  static bool initialized[49] = {false};
  static uint32_t lastPressMs[49] = {0};
  int index = (int)pin;
  bool level = digitalRead(pin);
  if (index >= 0 && index < 49 && !initialized[index]) {
    idle[index] = level;
    previous[index] = false;
    initialized[index] = true;
  }
  bool now = index >= 0 && index < 49 ? (level != idle[index]) : false;
  bool pressed = false;
  if (index >= 0 && index < 49 && now && !previous[index] && millis() - lastPressMs[index] > 250) {
    pressed = true;
    lastPressMs[index] = millis();
    logPrintf("button gpio=%d pressed level=%d idle=%d\n", index, level ? 1 : 0, idle[index] ? 1 : 0);
  }
  if (index >= 0 && index < 49) previous[index] = now;
  return pressed;
}

void setupButtons() {
  pinMode(BUT1, INPUT_PULLDOWN);
  pinMode(BUT2, INPUT_PULLDOWN);
  pinMode(BUT3, INPUT_PULLDOWN);
  pinMode(BUTENCO, INPUT_PULLDOWN);
  pinMode(BUTLEFT, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  logPrintf("button levels BUT1=%d BUT2=%d BUT3=%d ENCO=%d LEFT=%d\n",
            digitalRead(BUT1),
            digitalRead(BUT2),
            digitalRead(BUT3),
            digitalRead(BUTENCO),
            digitalRead(BUTLEFT));
}

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

bool isFaultReset(esp_reset_reason_t reason) {
  return reason == ESP_RST_PANIC ||
         reason == ESP_RST_INT_WDT ||
         reason == ESP_RST_TASK_WDT ||
         reason == ESP_RST_WDT;
}

LoaderBootState readLoaderBootState() {
  LoaderBootState state;
  Preferences prefs;
  if (!prefs.begin(LOADER_PREFS_NAMESPACE, false)) {
    logLine("NVS open failed for loader state");
    return state;
  }

  state.lastMachine = prefs.getString(LOADER_PREFS_LAST_MACHINE, "");
  state.autoBootEnabled = prefs.getBool(LOADER_PREFS_AUTO_BOOT, false);
  state.stayLoaderOnce = prefs.getBool(LOADER_PREFS_STAY_ONCE, false);
  if (state.stayLoaderOnce) {
    prefs.putBool(LOADER_PREFS_STAY_ONCE, false);
  }
  prefs.end();

  logPrintf("Loader state: last=%s auto=%d stay_once=%d\n",
            state.lastMachine.length() ? state.lastMachine.c_str() : "(none)",
            state.autoBootEnabled ? 1 : 0,
            state.stayLoaderOnce ? 1 : 0);
  return state;
}

void saveAutoBootMachine(const String &machineId) {
  Preferences prefs;
  if (!prefs.begin(LOADER_PREFS_NAMESPACE, false)) {
    logLine("NVS open failed while saving auto-boot");
    return;
  }

  prefs.putString(LOADER_PREFS_LAST_MACHINE, machineId);
  prefs.putBool(LOADER_PREFS_AUTO_BOOT, true);
  prefs.putBool(LOADER_PREFS_STAY_ONCE, false);
  prefs.end();
  logLine("Auto-boot saved for " + machineId);
}

void setAutoBootEnabled(bool enabled) {
  Preferences prefs;
  if (!prefs.begin(LOADER_PREFS_NAMESPACE, false)) {
    logLine("NVS open failed while changing auto-boot");
    return;
  }
  prefs.putBool(LOADER_PREFS_AUTO_BOOT, enabled);
  if (!enabled) prefs.putBool(LOADER_PREFS_STAY_ONCE, false);
  prefs.end();
  logLine(String("Auto-boot ") + (enabled ? "enabled" : "disabled"));
}

bool autoBootInterruptPressed() {
  return digitalRead(AUTO_BOOT_BUTTON) == HIGH || digitalRead(AUTO_BOOT_FALLBACK_BUTTON) == HIGH;
}

void drawAutoBootCountdown(const String &machine, uint32_t remainingMs) {
  if (!displayReady) return;

  tft.fillRect(0, 55, tft.width(), tft.height() - 55, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting " + (machine.length() ? machine : String("app1")), 8, 68, 2);
  tft.drawString("Hold bottom-right", 8, 92, 2);
  tft.drawString("to stay in loader", 8, 116, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(String((remainingMs + 999) / 1000) + "s", 8, 148, 4);
}

bool waitForAutoBootInterrupt(const String &machine) {
  drawHeader("Auto boot");
  logLine("Auto-boot window: hold bottom-right or encoder to stay in loader");

  const uint32_t startMs = millis();
  uint32_t lastDrawMs = 0;
  while (millis() - startMs < AUTO_BOOT_WINDOW_MS) {
    pollSerialCommands(Serial, serialCommandLine);
    pollSerialCommands(Serial0, serial0CommandLine);

    if (autoBootInterruptPressed()) {
      logLine("Auto-boot interrupted by button");
      drawMessage("Loader held", "Button pressed");
      delay(700);
      return true;
    }

    const uint32_t elapsed = millis() - startMs;
    if (lastDrawMs == 0 || millis() - lastDrawMs > 180) {
      lastDrawMs = millis();
      drawAutoBootCountdown(machine, AUTO_BOOT_WINDOW_MS - elapsed);
    }
    delay(10);
  }

  return false;
}

bool handleLoaderAutoBoot(const LoaderBootState &state, esp_reset_reason_t reason) {
  logPrintf("Reset reason: %s (%d)\n", resetReasonName(reason), (int)reason);

  if (state.stayLoaderOnce) {
    drawMessage("Loader requested", "Manual return");
    delay(900);
    return false;
  }

  if (isFaultReset(reason)) {
    drawMessage("Fault reset", resetReasonName(reason));
    delay(1200);
    return false;
  }

  if (!state.autoBootEnabled) {
    return false;
  }

  if (waitForAutoBootInterrupt(state.lastMachine)) {
    return false;
  }

  drawMessage("Boot app1", state.lastMachine);
  Serial.flush();
  Serial0.flush();
  delay(120);
  if (!bootPartitionByLabel("app1")) {
    drawMessage("Boot app1 failed");
    delay(1200);
    return false;
  }
  return true;
}

void handleMainMenuPress() {
  if (!sdReady) {
    drawMessage("SD not ready");
    delay(1200);
    drawMainMenu();
    return;
  }

  if (selectedMenu == 0) {
    if ((manifestLoaded && machineCount > 0) || loadManifest()) drawMachineList();
    else {
      delay(1500);
      drawMainMenu();
    }
  } else if (selectedMenu == 1) {
    if (!checkWifiUpdates()) {
      delay(2500);
    }
    loading = false;
    drawMainMenu();
  } else if (selectedMenu == 2) {
    showWifiStatus();
    delay(2500);
    drawMainMenu();
  }
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 1200) delay(10);
  Serial.setDebugOutput(true);
  Serial0.setDebugOutput(true);
  setupButtons();
  LoaderBootState bootState = readLoaderBootState();
  esp_reset_reason_t resetReason = esp_reset_reason();

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  drawHeader("Boot");
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    logPrintf("Running partition: %s @0x%lx size=0x%lx\n",
              running->label,
              (unsigned long)running->address,
              (unsigned long)running->size);
  }
  logOtaStatus();

  sdReady = beginSd();

  if (sdReady) {
    loadManifest();
    wifiConfigLoaded = readWifiConfig(cachedWifi);
    logPrintf("wifi.json cached: %s\n", wifiConfigLoaded ? cachedWifi.ssid.c_str() : "no");
  }

  SD.end();
  SD_MMC.end();
  SPI.end();

  tft.init();
  displayReady = true;
  tft.setRotation(LOADER_TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  drawHeader("Boot");
  if (!handleLoaderAutoBoot(bootState, resetReason)) {
    drawMainMenu();
  }
}

void loop() {
  pollSerialCommands(Serial, serialCommandLine);
  pollSerialCommands(Serial0, serial0CommandLine);

  if (loading) {
    delay(50);
    return;
  }

  unsigned char result = encoder.process();
  if (screenMode == SCREEN_MAIN_MENU) {
    if (result == DIR_CW && selectedMenu < MENU_COUNT - 1) {
      selectedMenu++;
      drawMainMenu();
    } else if (result == DIR_CCW && selectedMenu > 0) {
      selectedMenu--;
      drawMainMenu();
    }

    bool selectPressed = edgePressed(BUTENCO) || edgePressed(BUT3);
    if (selectPressed) {
      logPrintf("menu select index=%d item=%s\n", selectedMenu, MENU_ITEMS[selectedMenu]);
      handleMainMenuPress();
    }
  } else {
    if (machineCount == 0) {
      delay(50);
      return;
    }

    if (result == DIR_CW && selectedMachine < machineCount - 1) {
      selectedMachine++;
      drawMachineList();
    } else if (result == DIR_CCW && selectedMachine > 0) {
      selectedMachine--;
      drawMachineList();
    }

    if (edgePressed(BUT2)) {
      drawMainMenu();
    } else if (edgePressed(BUTENCO) || edgePressed(BUT3)) {
      if (!installMachine(machines[selectedMachine])) {
        loading = false;
        delay(2500);
        drawMachineList();
      }
    }
  }

  delay(5);
}
