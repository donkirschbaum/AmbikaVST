// Ambika VST - audio processor. GPL v3.

#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Presets.h"

using namespace ambika_plugin;

AmbikaProcessor::AmbikaProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AMBIKA", createLayout()) {
  for (auto& def : paramDefs()) rawParams.push_back(apvts.getRawParameterValue(def.id));
  for (auto& v : voiceLevel) v.store(0);
  for (auto& v : lfoValue) v.store(0);
  for (auto& v : seqStep) v.store(0);
  ambika::PartData d;
  ambika::InitPartData(&d);
  for (int i = 0; i < 64; ++i) sequence[static_cast<size_t>(i)].store(d.sequence_data[i]);
  engine.Init(44100.0);
  std::memset(&lastPatch, 0, sizeof(lastPatch));
  std::memset(&lastData, 0, sizeof(lastData));
}

AmbikaProcessor::~AmbikaProcessor() { cancelPendingUpdate(); }

bool AmbikaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  auto out = layouts.getMainOutputChannelSet();
  return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AmbikaProcessor::prepareToPlay(double sampleRate, int) {
  engine.SetSampleRate(sampleRate);
  engineReady = true;
  syncParametersToEngine();
}

int AmbikaProcessor::paramValue(int index) const {
  return juce::roundToInt(rawParams[static_cast<size_t>(index)]->load());
}

void AmbikaProcessor::syncParametersToEngine() {
  ambika::Patch* patch = engine.mutable_patch();
  ambika::PartData* data = engine.mutable_data();
  auto* pb = reinterpret_cast<uint8_t*>(patch);
  auto* db = reinterpret_cast<uint8_t*>(data);
  ambika::EngineSettings s;
  const auto& defs = paramDefs();
  for (size_t i = 0; i < defs.size(); ++i) {
    const auto& d = defs[i];
    int v = paramValue(static_cast<int>(i));
    if (d.level == Level::Patch) {
      pb[d.offset] = static_cast<uint8_t>(static_cast<int8_t>(v));
    } else if (d.level == Level::Part) {
      db[d.offset] = static_cast<uint8_t>(static_cast<int8_t>(v));
    } else {
      juce::String id(d.id);
      if (id == pid::board) s.board = v;
      else if (id == pid::voices) s.num_voices = v;
      else if (id == pid::output) s.output_gain = 0.5f * juce::Decibels::decibelsToGain(static_cast<float>(v));
      else if (id == pid::noise) s.noise = v / 100.0f * 3.0f;
      else if (id == pid::spread) s.stereo_spread = v / 100.0f;
      else if (id == pid::drive) s.drive = 0.5f + v / 100.0f;   // 50% = unity
      else if (id == pid::latch) s.clock_release = v;
    }
  }
  for (int i = 0; i < 64; ++i) data->sequence_data[i] = sequence[static_cast<size_t>(i)].load();
  engine.ApplySettings(s);
  if (std::memcmp(patch, &lastPatch, sizeof(lastPatch)) != 0 ||
      std::memcmp(data, &lastData, sizeof(lastData)) != 0) {
    engine.Touch();
    lastPatch = *patch;
    lastData = *data;
  }
  grooveClock.SetGroove(static_cast<uint8_t>(apvts.getRawParameterValue(pid::grooveTemplate)->load()),
                        static_cast<uint8_t>(apvts.getRawParameterValue(pid::grooveAmount)->load()));
}

void AmbikaProcessor::handleMidi(const juce::MidiMessage& m, bool hostClockRunning) {
  int channel = juce::roundToInt(apvts.getRawParameterValue(pid::midiChannel)->load());
  if (channel != 0 && m.getChannel() != channel && m.getChannel() != 0) return;

  if (m.isNoteOn()) {
    bool wasRunning = engine.running();
    engine.NoteOn(static_cast<uint8_t>(m.getNoteNumber()), m.getVelocity(), !hostClockRunning);
    if (!wasRunning && engine.running()) internalPpq = 0.0;
    lastMidiNote.store(m.getNoteNumber());
  } else if (m.isNoteOff()) {
    engine.NoteOff(static_cast<uint8_t>(m.getNoteNumber()));
  } else if (m.isPitchWheel()) {
    engine.PitchBend(static_cast<uint16_t>(m.getPitchWheelValue()));
  } else if (m.isChannelPressure()) {
    engine.ChannelPressure(static_cast<uint8_t>(m.getChannelPressureValue()));
  } else if (m.isAftertouch()) {
    engine.PolyPressure(static_cast<uint8_t>(m.getNoteNumber()), static_cast<uint8_t>(m.getAfterTouchValue()));
  } else if (m.isController()) {
    int cc = m.getControllerNumber(), value = m.getControllerValue();
    if (cc == 1 || cc == 2 || cc == 4 || cc == 64 || cc == 120 || cc == 121 || cc == 123) {
      engine.ControlChange(static_cast<uint8_t>(cc), static_cast<uint8_t>(value));
    } else {
      // Ambika CC mapping: forward to the parameter (on the message thread).
      const auto& defs = paramDefs();
      for (size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].midiCC == cc) {
          int start1, size1, start2, size2;
          ccFifo.prepareToWrite(1, start1, size1, start2, size2);
          if (size1 > 0) {
            ccQueue[static_cast<size_t>(start1)] = {static_cast<int>(i), value};
            ccFifo.finishedWrite(1);
            triggerAsyncUpdate();
          }
          break;
        }
      }
    }
  } else if (m.isAllNotesOff()) {
    engine.AllNotesOff();
  } else if (m.isAllSoundOff()) {
    engine.AllSoundOff();
  }
}

void AmbikaProcessor::handleAsyncUpdate() {
  int start1, size1, start2, size2;
  int ready = ccFifo.getNumReady();
  ccFifo.prepareToRead(ready, start1, size1, start2, size2);
  auto apply = [this](std::pair<int, int> e) {
    const auto& def = paramDefs()[static_cast<size_t>(e.first)];
    if (auto* p = apvts.getParameter(def.id)) {
      // Same scaling as Parameter::Scale in the firmware.
      int range = def.maxValue - def.minValue + 1;
      int v = def.unit == Unit::Raw ? e.second : def.minValue + ((range * (e.second << 1)) >> 8);
      p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
    }
  };
  for (int i = 0; i < size1; ++i) apply(ccQueue[static_cast<size_t>(start1 + i)]);
  for (int i = 0; i < size2; ++i) apply(ccQueue[static_cast<size_t>(start2 + i)]);
  ccFifo.finishedRead(size1 + size2);
}

void AmbikaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  if (!engineReady) { buffer.clear(); return; }
  syncParametersToEngine();

  // Transport.
  bool hostPlaying = false;
  double hostPpq = 0.0, hostBpm = 120.0;
  if (auto* ph = getPlayHead()) {
    if (auto pos = ph->getPosition()) {
      hostPlaying = pos->getIsPlaying();
      if (auto p = pos->getPpqPosition()) hostPpq = *p;
      if (auto b = pos->getBpm()) hostBpm = *b;
    }
  }
  const bool hostSync = juce::roundToInt(apvts.getRawParameterValue(pid::clockSource)->load()) == 1;
  const bool hostClock = hostSync && hostPlaying;
  if (hostSync) {
    if (hostPlaying && !hostWasPlaying) engine.Start();
    if (!hostPlaying && hostWasPlaying) engine.Stop();
  }
  hostWasPlaying = hostPlaying;
  const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
  const double internalBpm = hostSync ? hostBpm : apvts.getRawParameterValue(pid::bpm)->load();
  const double hostPpqPerSample = hostBpm / 60.0 / sr;
  const double internalPpqPerSample = internalBpm / 60.0 / sr;

  float* left = buffer.getWritePointer(0);
  float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
  std::vector<float> tmpRight;
  if (!right) { tmpRight.resize(static_cast<size_t>(n)); right = tmpRight.data(); }

  int rendered = 0;
  auto renderTo = [&](int s) {
    s = juce::jlimit(rendered, n, s);
    if (s > rendered) engine.Render(left + rendered, right + rendered, s - rendered);
    rendered = s;
  };

  auto it = midi.begin();
  int pos = 0;
  while (pos < n) {
    // Handle all MIDI events at the current position.
    while (it != midi.end() && (*it).samplePosition <= pos) {
      handleMidi((*it).getMessage(), hostClock);
      ++it;
    }
    int segEnd = it != midi.end() ? juce::jmin(n, (*it).samplePosition) : n;
    if (segEnd <= pos) segEnd = pos + 1;

    double ticks[64];
    int numTicks = 0;
    double a = 0.0, rate = 1.0;
    bool internal = false;
    if (hostClock) {
      a = hostPpq + pos * hostPpqPerSample;
      rate = hostPpqPerSample;
      numTicks = grooveClock.TicksIn(a, hostPpq + segEnd * hostPpqPerSample, ticks, 64);
    } else if (engine.running()) {
      internal = true;
      a = internalPpq;
      rate = internalPpqPerSample;
      double b = internalPpq + (segEnd - pos) * internalPpqPerSample;
      numTicks = grooveClock.TicksIn(a, b, ticks, 64);
      internalPpq = b;
    }
    for (int k = 0; k < numTicks; ++k) {
      int s = pos + static_cast<int>((ticks[k] - a) / rate);
      renderTo(s);
      engine.ClockTick(internal);
    }
    renderTo(segEnd);
    pos = segEnd;
  }
  while (it != midi.end()) { handleMidi((*it).getMessage(), hostClock); ++it; }
  renderTo(n);
  if (buffer.getNumChannels() == 1) {
    for (int i = 0; i < n; ++i) left[i] = 0.5f * (left[i] + right[i]);
  }
  for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

  for (int i = 0; i < 6; ++i) voiceLevel[static_cast<size_t>(i)].store(engine.voice_level(i));
  for (int i = 0; i < 3; ++i) {
    lfoValue[static_cast<size_t>(i)].store(engine.lfo_value(i));
    seqStep[static_cast<size_t>(i)].store(engine.step(i));
  }
}

// ----------------------------------------------------------------- presets --
void AmbikaProcessor::resetToDefaults() {
  for (auto& def : paramDefs()) {
    if (def.level == Level::Plugin) {
      juce::String id(def.id);
      // Keep the global setup (clock, channel, output...) except the card type.
      if (id != pid::board && id != pid::spread) continue;
    }
    if (auto* p = apvts.getParameter(def.id))
      p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(def.defaultValue)));
  }
}

static int numBankPrograms() {
  int n = 0;
  bankPrograms(&n);
  return n;
}

int AmbikaProcessor::getNumPrograms() {
  return static_cast<int>(factoryPresets().size()) + numBankPrograms();
}

juce::String AmbikaProcessor::getProgramBank(int index) const {
  int own = static_cast<int>(factoryPresets().size());
  if (index < own) return "Ambika VST";
  int n = 0;
  const BankProgram* programs = bankPrograms(&n);
  return index - own < n ? juce::String(programs[index - own].bank) : juce::String();
}

const juce::String AmbikaProcessor::getProgramName(int index) {
  if (index < 0 || index >= getNumPrograms()) return {};
  int own = static_cast<int>(factoryPresets().size());
  if (index < own) return factoryPresets()[static_cast<size_t>(index)].name;
  int n = 0;
  const BankProgram* programs = bankPrograms(&n);
  return juce::String(programs[index - own].bank) + " / " + programs[index - own].name;
}

void AmbikaProcessor::setCurrentProgram(int index) { loadPreset(index); }

void AmbikaProcessor::loadPreset(int index) {
  if (index < 0 || index >= getNumPrograms()) return;
  currentProgram = index;
  int own = static_cast<int>(factoryPresets().size());
  if (index >= own) {
    int n = 0;
    const BankProgram* programs = bankPrograms(&n);
    juce::MemoryBlock mb;
    mb.loadFromHexString(programs[index - own].hex);
    if (mb.getSize() >= sizeof(ambika::Patch) + sizeof(ambika::PartData)) {
      auto* bytes = static_cast<const uint8_t*>(mb.getData());
      loadProgramBytes(bytes, bytes + sizeof(ambika::Patch));
    }
    return;
  }
  resetToDefaults();
  ambika::PartData d;
  ambika::InitPartData(&d);
  for (int i = 0; i < 64; ++i) setSequenceByte(i, d.sequence_data[i]);
  for (auto& kv : factoryPresets()[static_cast<size_t>(index)].values) {
    if (auto* p = apvts.getParameter(kv.first))
      p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(kv.second)));
  }
}

void AmbikaProcessor::randomizePatch() {
  juce::Random rnd;
  for (auto& def : paramDefs()) {
    if (def.level != Level::Patch) continue;
    juce::String id(def.id);
    // Keep the VCA routing (slot 11) so the result is audible, as most
    // Ambika users do after a randomize.
    if (id.startsWith("mod11_")) continue;
    int v = def.minValue + rnd.nextInt(def.maxValue - def.minValue + 1);
    if (auto* p = apvts.getParameter(def.id)) p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
  }
}

// ------------------------------------------------------------- patch files --
// File format: "AMBK" + version byte + 112 bytes patch + 84 bytes part data
// (identical to the Ambika memory layout).
bool AmbikaProcessor::savePatchFile(const juce::File& f) {
  ambika::Patch patch;
  ambika::PartData data;
  auto* pb = reinterpret_cast<uint8_t*>(&patch);
  auto* db = reinterpret_cast<uint8_t*>(&data);
  ambika::InitPatch(&patch);
  ambika::InitPartData(&data);
  const auto& defs = paramDefs();
  for (size_t i = 0; i < defs.size(); ++i) {
    int v = paramValue(static_cast<int>(i));
    if (defs[i].level == Level::Patch) pb[defs[i].offset] = static_cast<uint8_t>(static_cast<int8_t>(v));
    if (defs[i].level == Level::Part) db[defs[i].offset] = static_cast<uint8_t>(static_cast<int8_t>(v));
  }
  for (int i = 0; i < 64; ++i) data.sequence_data[i] = getSequenceByte(i);
  juce::MemoryOutputStream out;
  out.write("AMBK", 4);
  out.writeByte(1);
  out.write(&patch, sizeof(patch));
  out.write(&data, sizeof(data));
  return f.replaceWithData(out.getData(), out.getDataSize());
}

bool AmbikaProcessor::loadPatchFile(const juce::File& f) {
  juce::MemoryBlock mb;
  if (!f.loadFileAsData(mb) || mb.getSize() < 12) return false;
  auto* bytes = static_cast<const uint8_t*>(mb.getData());
  const size_t size = mb.getSize();
  ambika::PartData initPart;
  ambika::InitPartData(&initPart);
  // Original Ambika SD card file (.PRO): RIFF "MBKS" with "obj " chunks.
  if (size >= 12 && std::memcmp(bytes, "RIFF", 4) == 0 && std::memcmp(bytes + 8, "MBKS", 4) == 0) {
    const uint8_t* patch = nullptr;
    const uint8_t* part = reinterpret_cast<const uint8_t*>(&initPart);
    size_t pos = 12;
    while (pos + 8 <= size) {
      uint32_t chunk = static_cast<uint32_t>(bytes[pos + 4]) | (static_cast<uint32_t>(bytes[pos + 5]) << 8) |
                       (static_cast<uint32_t>(bytes[pos + 6]) << 16) | (static_cast<uint32_t>(bytes[pos + 7]) << 24);
      const uint8_t* body = bytes + pos + 8;
      if (pos + 8 + chunk > size) break;
      if (std::memcmp(bytes + pos, "obj ", 4) == 0 && chunk >= 4) {
        // First object of the file wins (a .MUL loads its first part).
        if (body[0] == 1 && chunk >= 4 + sizeof(ambika::Patch) && patch == nullptr) patch = body + 4;
        if (body[0] == 5 && chunk >= 4 + sizeof(ambika::PartData) && part == reinterpret_cast<const uint8_t*>(&initPart)) part = body + 4;
      }
      pos += 8 + chunk + (chunk & 1);
    }
    if (patch == nullptr) return false;
    loadProgramBytes(patch, part);
    return true;
  }
  // Ambika SysEx dump (.syx): F0 00 21 02 00 04 cmd arg <nibbles> checksum F7.
  if (size > 8 && bytes[0] == 0xf0) {
    std::vector<uint8_t> patch, part;
    size_t pos = 0;
    while (pos < size) {
      while (pos < size && bytes[pos] != 0xf0) ++pos;
      size_t end = pos;
      while (end < size && bytes[end] != 0xf7) ++end;
      if (end - pos > 9 && bytes[pos + 1] == 0x00 && bytes[pos + 2] == 0x21 && bytes[pos + 3] == 0x02 &&
          bytes[pos + 4] == 0x00 && bytes[pos + 5] == 0x04) {
        uint8_t command = bytes[pos + 6];
        std::vector<uint8_t> payload;
        for (size_t i = pos + 8; i + 1 < end - 2; i += 2)
          payload.push_back(static_cast<uint8_t>((bytes[i] << 4) | (bytes[i + 1] & 0x0f)));
        if (command == 1 && payload.size() >= sizeof(ambika::Patch)) patch = payload;
        if (command == 5 && payload.size() >= sizeof(ambika::PartData)) part = payload;
      }
      pos = end + 1;
    }
    if (patch.empty()) return false;
    loadProgramBytes(patch.data(), part.empty() ? reinterpret_cast<const uint8_t*>(&initPart) : part.data());
    return true;
  }
  if (size < 5 + sizeof(ambika::Patch) + sizeof(ambika::PartData)) return false;
  if (std::memcmp(bytes, "AMBK", 4) != 0) return false;
  const uint8_t* pb = bytes + 5;
  loadProgramBytes(pb, pb + sizeof(ambika::Patch));
  return true;
}

void AmbikaProcessor::loadProgramBytes(const uint8_t* pb, const uint8_t* db) {
  for (auto& def : paramDefs()) {
    if (def.level == Level::Plugin) continue;
    uint8_t raw = def.level == Level::Patch ? pb[def.offset] : db[def.offset];
    int v = def.minValue < 0 ? static_cast<int>(static_cast<int8_t>(raw)) : static_cast<int>(raw);
    v = juce::jlimit(def.minValue, def.maxValue, v);
    if (auto* p = apvts.getParameter(def.id)) p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));
  }
  for (int i = 0; i < 64; ++i) setSequenceByte(i, db[16 + i]);
}

// ------------------------------------------------------------------- state --
void AmbikaProcessor::getStateInformation(juce::MemoryBlock& destData) {
  auto state = apvts.copyState();
  uint8_t bytes[64];
  for (int i = 0; i < 64; ++i) bytes[i] = getSequenceByte(i);
  state.setProperty("sequence", juce::String::toHexString(bytes, 64, 0), nullptr);
  state.setProperty("program", currentProgram, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void AmbikaProcessor::setStateInformation(const void* data, int sizeInBytes) {
  auto xml = getXmlFromBinary(data, sizeInBytes);
  if (!xml || !xml->hasTagName(apvts.state.getType())) return;
  auto tree = juce::ValueTree::fromXml(*xml);
  juce::String hex = tree.getProperty("sequence").toString();
  if (hex.length() == 128) {
    for (int i = 0; i < 64; ++i)
      setSequenceByte(i, static_cast<uint8_t>(hex.substring(i * 2, i * 2 + 2).getHexValue32()));
  }
  currentProgram = tree.getProperty("program", 0);
  apvts.replaceState(tree);
}

juce::AudioProcessorEditor* AmbikaProcessor::createEditor() { return new AmbikaEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AmbikaProcessor(); }
