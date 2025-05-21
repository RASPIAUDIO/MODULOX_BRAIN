// Arpeggiator.h — version “MIDI params + start/stop”
#pragma once
#include <Arduino.h>
#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>


/* ─── Classe Arpeggiator ─── */
class Arp
{
public:
    enum class Mode : uint8_t
    {
        Up, Down, UpDown, AsPlayed,
        Random,
        Converge,        // centre → extérieur
        Diverge          // extérieur → centre
    };

    explicit Arp(uint16_t bpm = 120,
                         uint8_t  rate = 4,
                         float    gate = 0.80f,
                         Mode     mode = Mode::Up,
                         int8_t   octaveSpan = 0) :
        _bpm{bpm},
        _rate{rate},
        _gate{gate},
        _mode{mode},
        _octaveSpan{octaveSpan}
    {
        _recalcTiming();
        _resetSeq();
    }

    /*──────────────────────────────
      Contrôle global
      ─────────────────────────────*/
    void start()
    {
        if (!_running) {
            _running = true;
            _resetSeq();
        }
    }

    /** Stoppe immédiatement, envoie Note Off si besoin */
    void stop(OscMonoPoly* osc)
    {
        if (_running) {
            if (_noteOn && osc) {
                osc->setNote(_currentMidi, 0);
            }
            _noteOn      = false;
            _currentMidi = -1;
            _running     = false;
			_pendingFirst  = false;    // ← remet à zéro l’indicateur
        }
    }

    /*──────────────────────────────
      Boucle audio (appel rapide)
      ─────────────────────────────*/
    inline void update(OscMonoPoly* osc)
    {
        if (!_running) return;                 // ← inactif

        const uint32_t now = millis();
		/* ── 1. déclencher la toute première note ── */
		if (_pendingFirst && !_notes.empty())
		{
			_currentMidi = _computePitch(_seqIndex);   // seqIndex = 0
			osc->setNote(_currentMidi, 127);

			_noteOn        = true;
			_pendingFirst  = false;
			_stepStartMs   = now;      // point de départ du timing
			return;                    // on sort : pas de NoteOff/Advance ce tour-ci
		}

        /* NOTE OFF ? */
        if (_noteOn && (now - _stepStartMs) >= _noteLengthMs) {
            osc->setNote(_currentMidi, 0);
            _noteOn = false;
        }

        /* PAS SUIVANT ? */
        if ((now - _stepStartMs) >= _stepDurMs) {
            _stepStartMs = now;
            _advanceIndex();

            if (!_notes.empty()) {
                _currentMidi = _computePitch(_seqIndex);
                osc->setNote(_currentMidi, 127);
                _noteOn = true;
            }
        }
    }

    /*──────────────────────────────
      Setters — valeurs MIDI 0-127
      ─────────────────────────────*/
    void update_gate (uint8_t v) { _gate  = _map01(v)*0.90f + 0.05f;           _recalcTiming(); }
    void update_rate (uint8_t v) { _rate  = 1u + static_cast<uint8_t>(v); _recalcTiming(); }
    void update_bpm  (uint8_t v) { _bpm   = 20u + static_cast<uint16_t>(_map01(v)*280u); _recalcTiming(); }
    void update_step (uint8_t v) { _octaveSpan = -2u + static_cast<int8_t>(v); _resetSeq();     }
    void update_mode (uint8_t v)
    {
        _mode = static_cast<Mode>(v);
        _resetSeq();
    }

    /*──────────────────────────────
      Gestion des notes tenues
      ─────────────────────────────*/
    void add_note(int8_t midi)
    {
        if (std::find(_notes.begin(), _notes.end(), midi) == _notes.end()) {
            _notes.push_back(midi);
            if (_needsSort()) std::sort(_notes.begin(), _notes.end());
            _resetSeq();
        }
    }
    void delete_note(int8_t midi, OscMonoPoly* osc)
    {
        auto it = std::remove(_notes.begin(), _notes.end(), midi);
		if (it == _notes.end()) return;          // rien à faire
        /*if (it != _notes.end()) {
            const bool wasPlaying = (midi == _currentMidi);
            _notes.erase(it, _notes.end());
            if (wasPlaying) _currentMidi = -1;
            _resetSeq();
        }*/
		if (osc) {
			const int8_t baseLow = (_octaveSpan >= 0) ? 0 : _octaveSpan;   // -3 … 0
			for (size_t lvl = 0; lvl < _levelCount(); ++lvl) {
				const int8_t pitch = midi + 12 * (baseLow + static_cast<int8_t>(lvl));
				osc->setNote(pitch, 0);           // velocity 0  →  NoteOff
			}
		}
		/* 2) Supprime la note du pool */
		_notes.erase(it, _notes.end());

		/* 3) Si c’était la note actuellement jouée, on marque l’arrêt */
		if (_currentMidi != -1 &&
			( (_currentMidi - midi) % 12 == 0 ) )   // même classe de note
		{
			_noteOn      = false;
			_currentMidi = -1;
		}

		/* 4) Recalcul de la séquence */
		_resetSeq();

    }

private:
    /*──────── Paramètres utilisateur ────────*/
    uint16_t _bpm           {120};   // 20-300
    uint8_t  _rate          {4};     // 1-16
    float    _gate          {0.80f}; // 5-95 %
    Mode     _mode          {Mode::Up};
    int8_t   _octaveSpan    {0};     // −3…+3

    /*──────── État de fonctionnement ────────*/
    bool     _running       {false};

    /*──────── Séquence ────────*/
    std::vector<int8_t> _notes;
    size_t              _seqIndex   {0};
    bool                _revDir     {false};
    size_t              _low{0}, _high{0};
    bool                _takeLow{true};
	bool _pendingFirst {false};

    /*──────── Note courante ────────*/
    uint32_t _stepDurMs    {0};
    uint32_t _noteLengthMs {0};
    uint32_t _stepStartMs  {0};
    bool     _noteOn       {false};
    int8_t   _currentMidi  {-1};

    /*──────── Helpers ────────*/
    static  float  _map01(uint8_t v)         { return static_cast<float>(v)/127.0f; }
    constexpr size_t _levelCount() const     { return std::abs(_octaveSpan)+1; }
    size_t   _seqSize() const                { return _notes.size()*_levelCount(); }
    bool     _needsSort() const
    {
        return _mode==Mode::Up||_mode==Mode::Down||
               _mode==Mode::UpDown||_mode==Mode::Converge||
               _mode==Mode::Diverge;
    }

    void _recalcTiming()
    {
        _stepDurMs    = static_cast<uint32_t>(240000.0f/(_bpm*_rate)); // 4/4
        _noteLengthMs = static_cast<uint32_t>(_stepDurMs*_gate);
    }
    void _resetSeq()
    {
        _seqIndex  = 0;
        _revDir    = false;
        _low       = 0;
        _high      = _seqSize() ? _seqSize()-1 : 0;
        _takeLow   = true;
		_pendingFirst = true;           // ← signale qu’il faut jouer la 1ʳᵉ note
        _stepStartMs = millis();
    }

    int8_t _computePitch(size_t flatIdx) const
    {
        const size_t noteIdx  = flatIdx % _notes.size();
        const size_t levelIdx = flatIdx / _notes.size();

        const int8_t baseLow   = (_octaveSpan>=0) ? 0 : _octaveSpan; // −3…0
        const int8_t octaveOff = baseLow + static_cast<int8_t>(levelIdx);

        return _notes[noteIdx] + 12*octaveOff;
    }

    void _advanceIndex()
    {
        const size_t N = _seqSize();
        if (N==0) return;

        switch (_mode)
        {
            case Mode::Up:         _seqIndex = (_seqIndex+1)%N; break;
            case Mode::Down:       _seqIndex = (_seqIndex==0)?N-1:_seqIndex-1; break;

            case Mode::UpDown:
                if (N==1) break;
                if (!_revDir) {
                    if (++_seqIndex>=N){ _seqIndex=N-2; _revDir=true; }
                } else {
                    if (_seqIndex==0){ _seqIndex=1; _revDir=false; }
                    else             { --_seqIndex; }
                }
                break;

            case Mode::AsPlayed:   _seqIndex = (_seqIndex+1)%N;             break;
            case Mode::Random:     _seqIndex = _rand(_rng)%N;               break;

            case Mode::Converge:
                _seqIndex = _takeLow ? _low++ : _high--;
                _takeLow = !_takeLow;
                if (_low>_high) _resetSeq();
                break;

            case Mode::Diverge:
                _seqIndex = _takeLow ? _high-- : _low++;
                _takeLow = !_takeLow;
                if (_low>_high) _resetSeq();
                break;
        }
    }

    /* PRNG pour Random */
    std::minstd_rand                    _rng{esp_random()};
    std::uniform_int_distribution<int>  _rand{0, INT32_MAX};
};
