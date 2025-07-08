#ifndef fatgroov_h
#define fatgroov_h

/********************************************************************
 *  Very-small FAT reader pour ESP32 flash  –  version « verbose »  *
 ********************************************************************/
#include <Arduino.h>           // Serial.printf
#include <esp_partition.h>
#include <cstring>
#include <cctype>

/* ----------- Toggle debug --------------------------------------- */
#define FAT_DEBUG  1           // 0 = muet, 1 = traces
#if FAT_DEBUG
  #define TRACE(...)  Serial.printf(__VA_ARGS__)
#else
  #define TRACE(...)
#endif

/* ---------- Paramètres du volume, à adapter à ton formateur ----- */
struct FatConfig {
    const esp_partition_t* part;   // partition flash contenant la FAT
    uint16_t bytesPerSec;          // 4096 sur FFat
    uint8_t  secsPerClus;          // 1  (4 Kio/cluster)
    uint32_t firstDataSec;         // n° de secteur où commence le cluster 2
    uint32_t rootDirSec;           // n° de secteur racine (cluster 2)
    uint16_t rootDirEnt;           // nb d’entrées dans la racine (512 par défaut)
};

/* ---------- Outil : compare <fname> à une entrée 8.3 ------------ */
static bool match83(const char entry[11], const char* fname)
{
    char sfn[11];  memset(sfn, ' ', 11);

    const char* dot = strchr(fname, '.');
    size_t nBase = dot ? (size_t)(dot - fname) : strlen(fname);
    size_t nExt  = dot ? strlen(dot + 1)       : 0;
	Serial.println("match");
	Serial.println(dot);
	Serial.println(nBase);
	Serial.println(nExt);

    for (size_t i = 0; i < nBase && i < 8; ++i)
        sfn[i] = toupper(fname[i]);
    for (size_t i = 0; i < nExt  && i < 3; ++i)
        sfn[8 + i] = toupper(dot[1 + i]);

    bool ok = memcmp(entry, sfn, 11) == 0;
    TRACE("  ‣ match83 %-11.11s vs %-11.11s  → %s\n",
          sfn, entry, ok ? "OK" : "KO");
    return ok;
}

/* ---------- Recherche dans le répertoire racine ----------------- */
bool fat_find_file(const FatConfig& cfg,
                   const char*     filename,
                   uint32_t&       firstCluster,
                   size_t&       fileSize,
                   uint32_t&       fileOffset /* bytes in flash */)
{
    TRACE("\n[fat_find_file] Recherche \"%s\" ...\n", filename);

    uint8_t  buf[32];
    uint32_t offset = cfg.rootDirSec * cfg.bytesPerSec;

    for (uint32_t i = 0; i < cfg.rootDirEnt; ++i, offset += 32) {

        if (esp_partition_read(cfg.part, offset, buf, 32) != ESP_OK) {
            TRACE("  ! lecture flash échouée à 0x%06X\n", offset);
            return false;
        }

        if (buf[0] == 0x00) {                 // fin de répertoire
            TRACE("  Fin de répertoire atteinte (%u entrées parcourues)\n", i);
            break;
        }
        if (buf[0] == 0xE5) {                 // fichier effacé
            TRACE("  Entrée %u @0x%06X marquée supprimée -> skip\n", i, offset);
            continue;
        }
        if (buf[11] == 0x0F) {                // LFN
            TRACE("  Entrée %u @0x%06X LFN (0x0F) -> skip\n", i, offset);
            continue;
        }

        if (match83(reinterpret_cast<char*>(buf), filename)) {
            uint16_t clLow  = buf[26] | (buf[27] << 8);
            uint16_t clHigh = buf[20] | (buf[21] << 8);
            firstCluster    = (uint32_t)clLow | ((uint32_t)clHigh << 16);
            fileSize        = buf[28] | (buf[29] << 8) |
                              (buf[30] << 16) | (buf[31] << 24);

            uint32_t firstSector = cfg.firstDataSec +
                                   (firstCluster - 2) * cfg.secsPerClus;
            fileOffset = firstSector * cfg.bytesPerSec;

            TRACE("  → trouvé !\n");
            TRACE("    firstCluster : %u\n", firstCluster);
            TRACE("    fileSize     : %u octets\n", fileSize);
            TRACE("    flash offset : 0x%06X\n", fileOffset);
            return true;
        }
    }

    TRACE("  Fichier \"%s\" introuvable dans la racine\n", filename);
    return false;
}

/* ---------- Petit wrapper de lecture séquentielle --------------- */
class FlashFile {
public:
    FlashFile() : _part(nullptr), _base(0), _size(0), _pos(0) {}

    bool open(const esp_partition_t* part,
              uint32_t base, uint32_t size)
    {
        _part = part; _base = base; _size = size; _pos = 0;
        TRACE("\n[FlashFile::open] base=0x%06X  size=%u\n", _base, _size);
        return true;
    }

    size_t read(void* dst, size_t n)
    {
        if (!_part) { TRACE("  ! read sans partition\n"); return 0; }
        if (_pos + n > _size) n = _size - _pos;       // clamp
        if (!n) return 0;

        TRACE("  read %u octets @pos %u (flash 0x%06X)\n",
              n, _pos, _base + _pos);

        if (esp_partition_read(_part, _base + _pos, dst, n) != ESP_OK) {
            TRACE("  ! erreur lecture flash\n");
            return 0;
        }
        _pos += n;
        return n;
    }

    bool seek(uint32_t pos)
    {
        if (pos > _size) { TRACE("  ! seek hors limite (%u > %u)\n", pos, _size); return false; }
        TRACE("  seek %u\n", pos);
        _pos = pos;
        return true;
    }
	
	/* ----- nouveau : test de disponibilité ---------------------- */
    bool available() const            // true tant qu'on n'est pas à la fin
    {
        return _pos < _size;
    }

    /* ----- nouveau : lecture d’un seul octet -------------------- *
     * - lit l’octet courant, l’avance, et le renvoie               *
     * - si on appelle read1() alors que available()==false,        *
     *   la fonction renvoie simplement 0                           */
    uint8_t read1()
    {
        uint8_t b = 0;
        if (_part && _pos < _size) {
            esp_partition_read(_part, _base + _pos, &b, 1);
            _pos++;
        }
        return b;                    // toujours un uint8_t
    }
	
	bool write(uint8_t data)
	{
		if (!_part) return false;

		/* Écriture brute de 1 octet  ---------------------------
		 *  - Sur l’ESP32 un write/erase doit normalement se faire
		 *    par blocs (4 octets écrits / 4 kio effacés).  
		 *  - Ici on fait l’hypothèse que la partition est déjà
		 *    effacée et qu’on programme les octets dans l’ordre ;
		 *    l’IDF accepte une taille non multiple de 4.          */
		if (esp_partition_write(_part, _base + _pos, &data, 1) != ESP_OK)
			return false;

		_pos++;
		if (_pos > _size) _size = _pos;    // allongement séquentiel
		return true;
	}
	
	bool erase(uint32_t size)
    {
        if (!_part) return false;

        

        esp_err_t err = esp_partition_erase_range(_part,  _base + _pos, size);
        if (err != ESP_OK) {
            TRACE("  ! erase error: %s\n", esp_err_to_name(err));
            return false;
        }

        return true;
    }

    size_t size() const { return _size; }
    uint32_t pos()  const { return _pos;  }
    bool     eof()  const { return _pos >= _size; }

private:
    const esp_partition_t* _part;
    uint32_t _base, _pos;
	size_t _size;
};


/* ================================================================ */
/*         Lecture séquentielle du répertoire racine (v2)           */
/* ================================================================ */
class RootDir {
public:
    explicit RootDir(const FatConfig& cfg)
      : _cfg(cfg), _offset(cfg.rootDirSec * cfg.bytesPerSec), _index(0) {}

    /* Revenir au début du répertoire --------------------------------- */
    void rewind()
    {
        _offset = _cfg.rootDirSec * _cfg.bytesPerSec;
        _index  = 0;
        TRACE("[RootDir] rewind → offset 0x%06X\n", _offset);
    }

    /* ---------- Fichier suivant (sauf SYSTEM~1) --------------------- */
    bool nextFile(String& name)
    {
        while (rawNext(name)) {
            if (!name.equalsIgnoreCase("SYSTEM~1"))
                return true;                      // accepté
            TRACE("  RootDir  skip SYSTEM~1 (attribut système)\n");
        }
        return false;
    }

    /* ---------- Fichier suivant par extension ---------------------- *
     *   - ext : chaîne sans le point, en majuscules ou minuscules     *
     *   - ex. nextFileExt("WAV", name)                                */
    bool nextFileExt(const char* ext, String& name)
    {
		Serial.println("nextFileExt");
        String target = ext;           target.toUpperCase();
		Serial.println(target);
        while (rawNext(name)) {
            /* isoler l’extension dans name */
            int dot = name.lastIndexOf('.');
            if (dot >= 0) {
                String e = name.substring(dot + 1); e.toUpperCase();
                if (e == target && !name.equalsIgnoreCase("SYSTEM~1"))
                    return true;
            }
        }
        return false;
    }

private:
    /* -------- lecture brute : renvoie *toutes* les entrées valides -- */
    bool rawNext(String& name)
    {
        uint8_t buf[32];

        while (_index < _cfg.rootDirEnt) {

            if (esp_partition_read(_cfg.part, _offset, buf, 32) != ESP_OK) {
                TRACE("  ! err flash @0x%06X\n", _offset);
                return false;
            }

            _offset += 32;
            _index++;

            if (buf[0] == 0x00)                // fin de table
                return false;
            if (buf[0] == 0xE5 || buf[11] == 0x0F) // effacé ou LFN
                continue;

            /* Décodage 8.3 → String */
            char s[13] = {0};
            memcpy(s, buf, 8);
            for (int i = 7; i >= 0 && s[i] == ' '; --i) s[i] = 0;
            size_t len = strlen(s);
            if (buf[8] != ' ') {
                s[len++] = '.';
                memcpy(s + len, buf + 8, 3);
                for (int i = len + 2; i >= (int)len && s[i] == ' '; --i) s[i] = 0;
            }
            name = String(s);
            TRACE("  RootDir  found %-12s (entry %u)\n", s, _index - 1);
            return true;
        }
        return false;
    }

    const FatConfig& _cfg;
    uint32_t _offset;
    uint32_t _index;
};

#endif