#include "mb.h"
#include "mbdisplay.h"
#include "granulizer.h"
#include <loader_guard.h>
#include <TFT_eSPI.h>
#include <lfofloat.h>
#include <envfloat.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#define TINY 0.000001f;
#include "rosic_TeeBeeFilter.h"
#include "rosic_OnePoleFilter.h"
#include "rosic_BiquadFilter.h"


#define DELAY_SAMPLES 100000
#include "delaystereo.h"

Granulizer granulizer;
TeeBeeFilter filter;
Lfo lfo;

bool record=false;

bool playglb=true;

#define bufferLen 32
int16_t sBuffer[bufferLen*2];

int sample_index=0;
int play_ind=0;

float lfoamount_prev=0;
float envamount_prev=0;
float volglobal=0.0;
float delay_mix=0.0;

int16_t maxaudio=0;
int lastind=0;

Env env2;

struct SynthProcessBench {
  volatile uint32_t samples;
  volatile uint64_t totalCycles;
  volatile uint32_t minCycles;
  volatile uint32_t maxCycles;
  uint32_t lastReportMs;
};

SynthProcessBench synthProcessBench = {
  0,
  0,
  0xFFFFFFFFu,
  0,
  0
};

static inline void synthBenchRecord(uint32_t cycles) {
  synthProcessBench.samples++;
  synthProcessBench.totalCycles += cycles;
  if (cycles < synthProcessBench.minCycles) synthProcessBench.minCycles = cycles;
  if (cycles > synthProcessBench.maxCycles) synthProcessBench.maxCycles = cycles;
}

void synthBenchReportIfDue() {
  const uint32_t now = millis();
  if (now - synthProcessBench.lastReportMs < 1000) return;
  synthProcessBench.lastReportMs = now;

  const uint32_t samples = synthProcessBench.samples;
  if (samples == 0) return;

  const uint64_t totalCycles = synthProcessBench.totalCycles;
  const uint32_t minCycles = synthProcessBench.minCycles;
  const uint32_t maxCycles = synthProcessBench.maxCycles;

  synthProcessBench.samples = 0;
  synthProcessBench.totalCycles = 0;
  synthProcessBench.minCycles = 0xFFFFFFFFu;
  synthProcessBench.maxCycles = 0;

  const float cpuMHz = (float)ESP.getCpuFreqMHz();
  const float avgCycles = (float)totalCycles / (float)samples;
  const float avgUs = avgCycles / cpuMHz;
  const float minUs = (float)minCycles / cpuMHz;
  const float maxUs = (float)maxCycles / cpuMHz;
  const float sampleBudgetUs = 1000000.0f / (float)SAMPLE_RATE;
  const float budgetPct = (avgUs / sampleBudgetUs) * 100.0f;

  Serial.printf(
    "[bench] Synth_Process samples=%lu avg=%.1f cyc %.3f us min=%.3f us max=%.3f us budget=%.1f%% sr=%u cpu=%.0fMHz\n",
    (unsigned long)samples,
    avgCycles,
    avgUs,
    minUs,
    maxUs,
    budgetPct,
    (unsigned)SAMPLE_RATE,
    cpuMHz
  );
}

void runFlashReadBenchPass(const esp_partition_t *partition, size_t chunkSize, size_t passBytes, int repeats) {
  uint8_t *buffer = (uint8_t *)heap_caps_malloc(chunkSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buffer) {
    Serial.printf("[flashbench] chunk=%lu alloc_failed\n", (unsigned long)chunkSize);
    return;
  }

  uint32_t checksum = 0x811C9DC5u;
  const uint32_t startCycles = ESP.getCycleCount();
  const uint32_t startUs = micros();

  for (int repeat = 0; repeat < repeats; repeat++) {
    size_t offset = 0;
    while (offset < passBytes) {
      const size_t toRead = ((passBytes - offset) < chunkSize) ? (passBytes - offset) : chunkSize;
      const esp_err_t err = esp_partition_read(partition, offset, buffer, toRead);
      if (err != ESP_OK) {
        Serial.printf("[flashbench] chunk=%lu read_error=0x%x offset=%lu\n",
                      (unsigned long)chunkSize,
                      (unsigned)err,
                      (unsigned long)offset);
        heap_caps_free(buffer);
        return;
      }

      checksum ^= buffer[0];
      checksum *= 16777619u;
      checksum ^= buffer[toRead / 2];
      checksum *= 16777619u;
      checksum ^= buffer[toRead - 1];
      checksum *= 16777619u;

      offset += toRead;
    }
  }

  const uint32_t elapsedUs = micros() - startUs;
  const uint32_t elapsedCycles = ESP.getCycleCount() - startCycles;
  const uint32_t totalBytes = (uint32_t)(passBytes * (size_t)repeats);
  const float seconds = (float)elapsedUs / 1000000.0f;
  const float mbps = ((float)totalBytes / 1048576.0f) / seconds;
  const float cyclesPerByte = (float)elapsedCycles / (float)totalBytes;

  Serial.printf(
    "[flashbench] chunk=%lu pass=%lu repeat=%d total=%lu us=%lu MBps=%.2f cycles_per_byte=%.2f checksum=0x%08lx\n",
    (unsigned long)chunkSize,
    (unsigned long)passBytes,
    repeats,
    (unsigned long)totalBytes,
    (unsigned long)elapsedUs,
    mbps,
    cyclesPerByte,
    (unsigned long)checksum
  );

  heap_caps_free(buffer);
}

void runFlashReadBench() {
  const esp_partition_t *partition = esp_ota_get_running_partition();
  if (!partition) {
    Serial.println("[flashbench] no_running_partition");
    return;
  }

  const size_t desiredBytes = 1536 * 1024;
  const size_t passBytes = (partition->size < desiredBytes) ? partition->size : desiredBytes;
  Serial.printf(
    "[flashbench] start label=%s address=0x%lx partition_size=%lu pass=%lu flash_size=%lu psram_size=%lu cpu=%luMHz\n",
    partition->label,
    (unsigned long)partition->address,
    (unsigned long)partition->size,
    (unsigned long)passBytes,
    (unsigned long)ESP.getFlashChipSize(),
    (unsigned long)ESP.getPsramSize(),
    (unsigned long)ESP.getCpuFreqMHz()
  );

  runFlashReadBenchPass(partition, 4096, passBytes, 8);
  runFlashReadBenchPass(partition, 32768, passBytes, 8);
  Serial.println("[flashbench] done");
}

void flashBenchRunAfterBootIfDue() {
  static uint32_t lastRunMs = 0;
  const uint32_t now = millis();
  if (now < 15000 || now - lastRunMs < 30000) return;
  lastRunMs = now;
  runFlashReadBench();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.setDebugOutput(true);
  startLoaderGuard();

  modubrainInit();

  
  
  delay(500);

  Serial.println("setup mbrain OK");

  //granulizer.noteOn(64);
}

// Fonction pour charger un grain depuis un fichier WAV

void test_matrix(int desti, float amoun)
{
  if(desti==1) granulizer.size_change(amoun*127.0);
  if(desti==2) granulizer.density_change(amoun*127.0);
  if(desti==3) granulizer.sample_rate_change(amoun*127.0);
  if(desti==4) filter.SetCutoff(amoun*127.0,true);
}

void change_matrix(int desti)
{
  if(desti==1) {
    granulizer.density_change(param_midi[5]);;
  }
  if(desti==2) 
  {
    granulizer.size_change(param_midi[3]);
    granulizer.sample_rate_change(param_midi[6]);
  }
  if(desti==3) 
  {
    granulizer.density_change(param_midi[5]);
    filter.SetCutoff((float)param_midi[12],true);
  }
  if(desti==4) 
  {
    granulizer.sample_rate_change(param_midi[6]);
  }
}


int cou=0;
// Fonction pour lire les grains et les mixer
//void mixGrains(int16_t *outputBuffer, size_t numSamples) {
void mixGrains(int16_t *outL, int16_t *outR, size_t numSamples) {
  //memset(outputBuffer, 0, numSamples * sizeof(int16_t));  // Initialisation du buffer de sortie à zéro
  //granulizer.load_buffers();
  for (int j = 0; j < numSamples; j++) {
    Synth_Process(&outL[j], &outR[j]);
    cou++;
  }
}

void taskAudio(void *parameter) {
  int16_t audioBuffer[bufferLen*2];
  int numpass=0;
  play_ind=granulizer.sample_start_index[0];
  while (1) {
    if(!record) 
    {
      // Début de la mesure
      uint32_t start = micros();
        
      /*int16_t monoBuffer[bufferLen];
      int16_t testbuf[bufferLen];
      if(playglb) mixGrains(monoBuffer, bufferLen);*/
      int16_t leftBuffer[bufferLen];
      int16_t rightBuffer[bufferLen];
      if(playglb) mixGrains(leftBuffer, rightBuffer, bufferLen);
      else
      {
        for (int i = 0; i < bufferLen; i++) {
          //monoBuffer[i]=0; 
          leftBuffer[i]=0;
          rightBuffer[i]=0;         
        }
      }
      
      for (int i = 0; i < bufferLen; i++) {
        
        /*audioBuffer[2 * i] = monoBuffer[i];     // Canal gauche
        audioBuffer[2 * i + 1] = monoBuffer[i]; // Canal droit*/
        audioBuffer[2 * i] = leftBuffer[i];
        audioBuffer[2 * i + 1] = rightBuffer[i];
      }
      // Fin de la mesure
      uint32_t end = micros();
      // On accumule la durée
      audioCpuTime += (end - start);
      size_t bytes_written = 0;
      i2s_write(i2s_num, audioBuffer, sizeof(audioBuffer), &bytes_written, portMAX_DELAY);
    }
    else {
      size_t bytesIn = 0;
      size_t bytesOut = 0;
      esp_err_t result = i2s_read(i2s_num, &sBuffer, sizeof(sBuffer), &bytesIn, portMAX_DELAY);
      
      //numpass++;
      //Serial.println(bytesIn);
      for (int i = 0; i < bufferLen; i++) {
        sample[sample_index]=sBuffer[i*2];  
        audioBuffer[2 * i + 1] = sBuffer[i*2]; 
        audioBuffer[2 * i] = sBuffer[i*2];   
        sample_index++;
      }
      if(sample_index>200000) but_record();

      if (result == ESP_OK)
      {
        // Send what we just received back to the codec
        esp_err_t result_w = i2s_write(i2s_num, &audioBuffer, bytesIn, &bytesOut, portMAX_DELAY);
        // If there was an I2S write error, let us know on the serial terminal
        if (result_w != ESP_OK)
        {
          Serial.print("I2S write error.");
        }
      }
      else
      {
        Serial.print("I2S read error.");
      }
    }
    // Convertit le mono en stéréo en dupliquant chaque échantillon
    
    
  }
}



void loop() {
  // put your main code here, to run repeatedly:
    flashBenchRunAfterBootIfDue();
    synthBenchReportIfDue();
    Midi_Process();
    USB_Midi_Process();
    enco_turned();
    distance_sensor();
    
    
    //Serial.println(127-(mm-25)/4);
}
