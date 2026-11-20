// Ambika VST - editor. GPL v3.

#include "PluginEditor.h"

#include "Presets.h"

using namespace ambika_plugin;

namespace {
const juce::Colour kPanel(0xff1d1f22);
const juce::Colour kPanelLight(0xff2a2d31);
const juce::Colour kInk(0xffd8d4cb);
const juce::Colour kInkDim(0xff8e8a82);
const juce::Colour kAccent(0xffe2463a);   // LED red
const juce::Colour kLcdBack(0xff2747c4);
const juce::Colour kLcdInk(0xffe9f0ff);

const int kWidth = 980;
const int kHeight = 470;
}  // namespace

// ------------------------------------------------------------ look & feel --
AmbikaLookAndFeel::AmbikaLookAndFeel() {
  setColour(juce::ComboBox::backgroundColourId, kPanelLight);
  setColour(juce::ComboBox::textColourId, kInk);
  setColour(juce::ComboBox::outlineColourId, juce::Colours::black);
  setColour(juce::PopupMenu::backgroundColourId, kPanelLight);
  setColour(juce::PopupMenu::textColourId, kInk);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.darker(0.3f));
  setColour(juce::TextButton::textColourOffId, kInk);
  setColour(juce::TextButton::textColourOnId, kInk);
  setColour(juce::Label::textColourId, kInk);
  setColour(juce::Slider::textBoxTextColourId, kInk);
}

void AmbikaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider&) {
  auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(w), static_cast<float>(h)).reduced(4.0f);
  float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
  auto r = juce::Rectangle<float>(size, size).withCentre(bounds.getCentre());
  // Scale ticks.
  g.setColour(kInkDim.withAlpha(0.6f));
  for (int i = 0; i <= 10; ++i) {
    float a = start + (end - start) * i / 10.0f;
    auto p1 = r.getCentre().getPointOnCircumference(size * 0.5f, a);
    auto p2 = r.getCentre().getPointOnCircumference(size * 0.5f - 3.0f, a);
    g.drawLine({p1, p2}, 1.0f);
  }
  auto knob = r.reduced(size * 0.14f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4d52), knob.getX(), knob.getY(),
                                         juce::Colour(0xff0e0f10), knob.getRight(), knob.getBottom(), false));
  g.fillEllipse(knob);
  g.setColour(juce::Colours::black);
  g.drawEllipse(knob, 1.2f);
  auto cap = knob.reduced(knob.getWidth() * 0.18f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34373b), cap.getX(), cap.getY(),
                                         juce::Colour(0xff17181a), cap.getRight(), cap.getBottom(), false));
  g.fillEllipse(cap);
  float angle = start + pos * (end - start);
  auto c = knob.getCentre();
  g.setColour(kInk);
  g.drawLine({c.getPointOnCircumference(knob.getWidth() * 0.12f, angle),
              c.getPointOnCircumference(knob.getWidth() * 0.48f, angle)}, 2.2f);
}

void AmbikaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down) {
  auto r = b.getLocalBounds().toFloat().reduced(1.0f);
  g.setColour(down ? juce::Colour(0xff111213) : (highlighted ? juce::Colour(0xff3a3d42) : juce::Colour(0xff2c2f33)));
  g.fillRoundedRectangle(r, 3.0f);
  g.setColour(juce::Colours::black);
  g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

// -------------------------------------------------------------------- LCD --
void LcdDisplay::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  g.setColour(juce::Colour(0xff0c0d0e));
  g.fillRoundedRectangle(r, 4.0f);
  auto screen = r.reduced(8.0f);
  g.setGradientFill(juce::ColourGradient(kLcdBack.brighter(0.15f), screen.getX(), screen.getY(),
                                         kLcdBack.darker(0.25f), screen.getX(), screen.getBottom(), false));
  g.fillRect(screen);
  // Character cells (5x8 dot matrix look).
  const int cols = 40;
  float cw = screen.getWidth() / cols;
  float ch = screen.getHeight() / 2.0f;
  // Monospace glyphs are ~0.6 em wide: size the font so one fits a cell.
  float fontHeight = juce::jmin(ch * 0.62f, (cw - 2.0f) / 0.62f);
  juce::Font font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), fontHeight, juce::Font::bold));
  g.setFont(font);
  for (int row = 0; row < 2; ++row) {
    const juce::String& line = row == 0 ? line1 : line2;
    for (int c = 0; c < cols; ++c) {
      auto cell = juce::Rectangle<float>(screen.getX() + c * cw, screen.getY() + row * ch, cw, ch).reduced(1.0f, 4.0f);
      g.setColour(kLcdBack.brighter(0.08f));
      g.fillRect(cell);
      if (c < line.length() && line[c] != ' ') {
        g.setColour(kLcdInk);
        float baseline = cell.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.35f;
        g.drawSingleLineText(line.substring(c, c + 1), juce::roundToInt(cell.getCentreX()),
                             juce::roundToInt(baseline), juce::Justification::horizontallyCentred);
      }
    }
  }
}

// ------------------------------------------------------- sequence editor --
juce::Rectangle<float> SequenceEditor::rowArea(int row) const {
  auto r = getLocalBounds().toFloat().reduced(4.0f).withTrimmedLeft(46.0f);
  const float h[4] = {38.0f, 38.0f, 46.0f, 14.0f};
  float y = r.getY();
  for (int i = 0; i < row; ++i) y += h[i] + 6.0f;
  return {r.getX(), y, r.getWidth(), h[row]};
}

void SequenceEditor::paint(juce::Graphics& g) {
  g.setColour(juce::Colour(0xff141516));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
  auto& st = proc.state();
  int lengths[3] = {juce::roundToInt(st.getRawParameterValue("seq1_len")->load()),
                    juce::roundToInt(st.getRawParameterValue("seq2_len")->load()),
                    juce::roundToInt(st.getRawParameterValue("patt_len")->load())};
  const char* names[4] = {"SEQ1", "SEQ2", "NOTE", "G/L"};
  g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
  for (int row = 0; row < 4; ++row) {
    auto area = rowArea(row);
    g.setColour(kInkDim);
    g.drawText(names[row], juce::Rectangle<float>(4.0f, area.getY(), 42.0f, area.getHeight()),
               juce::Justification::centredLeft);
    int seq = row < 2 ? row : 2;
    int playing = (proc.seqStep[static_cast<size_t>(seq)].load() + lengths[seq] - 1) % juce::jmax(1, lengths[seq]);
    float w = area.getWidth() / 16.0f;
    for (int s = 0; s < 16; ++s) {
      auto cell = juce::Rectangle<float>(area.getX() + s * w, area.getY(), w, area.getHeight()).reduced(1.5f, 0.0f);
      bool active = s < lengths[seq];
      g.setColour(active ? juce::Colour(0xff222427) : juce::Colour(0xff17181a));
      g.fillRect(cell);
      if (s == playing && active) {
        g.setColour(kAccent.withAlpha(0.25f));
        g.fillRect(cell);
      }
      juce::Colour bar = active ? kInk : kInkDim.withAlpha(0.4f);
      if (row < 2) {
        float v = proc.getSequenceByte(row * 16 + s) / 255.0f;
        g.setColour(bar);
        g.fillRect(cell.withTop(cell.getBottom() - v * cell.getHeight()));
      } else if (row == 2) {
        int note = proc.getSequenceByte(32 + s * 2) & 0x7f;
        int rel = juce::jlimit(-24, 24, note - 60);
        float y = cell.getCentreY() - rel / 24.0f * (cell.getHeight() * 0.5f - 3.0f);
        g.setColour(kInkDim.withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(cell.getCentreY()), cell.getX(), cell.getRight());
        g.setColour(bar);
        g.fillRect(juce::Rectangle<float>(cell.getX() + 1, y - 2.5f, cell.getWidth() - 2, 5.0f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(juce::MidiMessage::getMidiNoteName(note, true, true, 4), cell.withHeight(11.0f),
                   juce::Justification::centred);
      } else {
        bool gate = proc.getSequenceByte(32 + s * 2) & 0x80;
        bool legato = proc.getSequenceByte(33 + s * 2) & 0x80;
        auto left = cell.withWidth(cell.getWidth() * 0.5f).reduced(2.0f, 2.0f);
        auto right = left.withX(left.getRight() + 4.0f);
        g.setColour(gate ? kAccent : juce::Colour(0xff3a1714));
        g.fillEllipse(left.withSizeKeepingCentre(8.0f, 8.0f));
        g.setColour(legato ? juce::Colour(0xff58c26a) : juce::Colour(0xff17301b));
        g.fillEllipse(right.withSizeKeepingCentre(8.0f, 8.0f));
      }
    }
  }
  g.setColour(kInkDim);
  g.setFont(juce::FontOptions(10.0f));
  g.drawText("drag bars to edit  |  note row: drag pitch  |  G/L: left dot = gate, right dot = legato",
             getLocalBounds().removeFromBottom(14).toFloat(), juce::Justification::centred);
}

void SequenceEditor::mouseDown(const juce::MouseEvent& e) {
  dragRow = -1;
  for (int r = 0; r < 4; ++r)
    if (rowArea(r).contains(e.position)) dragRow = r;
  edit(e, true);
}

void SequenceEditor::mouseDrag(const juce::MouseEvent& e) { edit(e, false); }

void SequenceEditor::edit(const juce::MouseEvent& e, bool initial) {
  if (dragRow < 0) return;
  auto area = rowArea(dragRow);
  int s = juce::jlimit(0, 15, static_cast<int>((e.position.x - area.getX()) / (area.getWidth() / 16.0f)));
  float rel = juce::jlimit(0.0f, 1.0f, (area.getBottom() - e.position.y) / area.getHeight());
  if (dragRow < 2) {
    proc.setSequenceByte(dragRow * 16 + s, static_cast<uint8_t>(juce::roundToInt(rel * 255.0f)));
  } else if (dragRow == 2) {
    int note = 60 + juce::roundToInt((rel - 0.5f) * 48.0f);
    uint8_t old = proc.getSequenceByte(32 + s * 2);
    proc.setSequenceByte(32 + s * 2, static_cast<uint8_t>((old & 0x80) | juce::jlimit(0, 127, note)));
  } else if (initial) {
    float w = area.getWidth() / 16.0f;
    bool leftHalf = (e.position.x - area.getX() - s * w) < w * 0.5f;
    if (leftHalf) {
      uint8_t v = proc.getSequenceByte(32 + s * 2);
      proc.setSequenceByte(32 + s * 2, v ^ 0x80);
    } else {
      uint8_t v = proc.getSequenceByte(33 + s * 2);
      proc.setSequenceByte(33 + s * 2, v ^ 0x80);
    }
  }
  repaint();
}

// ----------------------------------------------------------------- editor --
AmbikaEditor::AmbikaEditor(AmbikaProcessor& p)
    : AudioProcessorEditor(&p), proc(p), seqEditor(p) {
  setLookAndFeel(&lnf);

  // Page layout, following the Ambika's page buttons.
  pages = {
    {"OSC", {{"oscillators", {"osc1_shape", "osc1_param", "osc1_range", "osc1_tune",
                              "osc2_shape", "osc2_param", "osc2_range", "osc2_tune"}}}},
    {"MIXER", {{"mixer", {"mix_balance", "mix_op", "mix_amount", "sub_shape",
                          "sub_level", "noise_level", "fuzz", "crush"}}}},
    {"FILTER", {{"filter", {"cutoff", "resonance", "filter_mode", "filter_env",
                            "filter_lfo", "board", "drive", nullptr}}}},
    {"ENV/LFO", {
      {"envelope / lfo 1", {"env1_attack", "env1_decay", "env1_sustain", "env1_release",
                            "lfo1_shape", "lfo1_rate", "lfo1_sync", nullptr}},
      {"envelope / lfo 2", {"env2_attack", "env2_decay", "env2_sustain", "env2_release",
                            "lfo2_shape", "lfo2_rate", "lfo2_sync", nullptr}},
      {"envelope / lfo 3", {"env3_attack", "env3_decay", "env3_sustain", "env3_release",
                            "lfo3_shape", "lfo3_rate", "lfo3_sync", nullptr}}}},
    {"MATRIX", {}},
    {"MODIF.", {
      {"modifiers 1 & 2", {"modif1_in1", "modif1_in2", "modif1_op", nullptr,
                           "modif2_in1", "modif2_in2", "modif2_op", nullptr}},
      {"modifiers 3 & 4", {"modif3_in1", "modif3_in2", "modif3_op", nullptr,
                           "modif4_in1", "modif4_in2", "modif4_op", nullptr}},
      {"voice lfo (lfo4)", {"vlfo_shape", "vlfo_rate", nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr}}}},
    {"PART", {
      {"part", {"volume", "octave", "tuning", "spread", "raga", "legato", "portamento", "poly_mode"}},
      {"arpeggiator / sequencer", {"arp_mode", "arp_dir", "arp_range", "arp_pattern",
                                   "arp_grid", "seq1_len", "seq2_len", "patt_len"}},
      {"clock / setup", {"clock_src", "bpm", "groove", "groove_amt",
                         "latch", "voices", "midi_ch", nullptr}}}},
    {"SEQ", {{"step sequences & note pattern", {nullptr, nullptr, nullptr, nullptr,
                                                nullptr, nullptr, nullptr, nullptr}}}},
  };
  static const char* const kModIds[14][3] = {
    {"mod1_src", "mod1_dst", "mod1_amt"}, {"mod2_src", "mod2_dst", "mod2_amt"},
    {"mod3_src", "mod3_dst", "mod3_amt"}, {"mod4_src", "mod4_dst", "mod4_amt"},
    {"mod5_src", "mod5_dst", "mod5_amt"}, {"mod6_src", "mod6_dst", "mod6_amt"},
    {"mod7_src", "mod7_dst", "mod7_amt"}, {"mod8_src", "mod8_dst", "mod8_amt"},
    {"mod9_src", "mod9_dst", "mod9_amt"}, {"mod10_src", "mod10_dst", "mod10_amt"},
    {"mod11_src", "mod11_dst", "mod11_amt"}, {"mod12_src", "mod12_dst", "mod12_amt"},
    {"mod13_src", "mod13_dst", "mod13_amt"}, {"mod14_src", "mod14_dst", "mod14_amt"}};
  for (int i = 0; i < 7; ++i) {
    SubPage sp;
    sp.title = "modulation " + juce::String(i * 2 + 1) + " & " + juce::String(i * 2 + 2) +
               (i == 6 ? "  (slot 14 scaled by mod wheel)" : "");
    sp.params = {kModIds[i * 2][0], kModIds[i * 2][1], kModIds[i * 2][2], nullptr,
                 kModIds[i * 2 + 1][0], kModIds[i * 2 + 1][1], kModIds[i * 2 + 1][2], nullptr};
    pages[4].subpages.push_back(sp);
  }
  currentSub.assign(pages.size(), 0);

  addAndMakeVisible(lcd);
  pageTitle.setFont(juce::FontOptions(12.0f, juce::Font::bold));
  pageTitle.setColour(juce::Label::textColourId, kInkDim);
  addAndMakeVisible(pageTitle);

  for (int i = 0; i < 8; ++i) {
    auto& k = knobs[static_cast<size_t>(i)];
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.setMouseDragSensitivity(220);
    addAndMakeVisible(k);
    auto& l = knobLabels[static_cast<size_t>(i)];
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(l);
    auto& vl = knobValues[static_cast<size_t>(i)];
    vl.setJustificationType(juce::Justification::centred);
    vl.setFont(juce::FontOptions(11.0f));
    vl.setColour(juce::Label::textColourId, kInkDim);
    addAndMakeVisible(vl);
  }
  for (size_t i = 0; i < pages.size(); ++i) {
    auto b = std::make_unique<juce::TextButton>(pages[i].button);
    b->onClick = [this, i] { selectPage(static_cast<int>(i), true); };
    addAndMakeVisible(*b);
    pageButtons.push_back(std::move(b));
  }
  addChildComponent(seqEditor);

  // Right panel.
  addAndMakeVisible(presetBox);
  refreshPresetBox();
  presetBox.onChange = [this] {
    int id = presetBox.getSelectedId();
    if (id > 0) proc.loadPreset(id - 1);
  };
  for (auto* b : {&prevButton, &nextButton, &loadButton, &saveButton, &randomButton}) addAndMakeVisible(*b);
  prevButton.onClick = [this] {
    int n = proc.getNumPrograms();
    proc.loadPreset((proc.getCurrentProgram() + n - 1) % n);
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
  };
  nextButton.onClick = [this] {
    int n = proc.getNumPrograms();
    proc.loadPreset((proc.getCurrentProgram() + 1) % n);
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
  };
  randomButton.onClick = [this] { proc.randomizePatch(); };
  saveButton.onClick = [this] {
    chooser = std::make_unique<juce::FileChooser>("Save Ambika patch", juce::File(), "*.ambk");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc) {
                           auto f = fc.getResult();
                           if (f != juce::File()) proc.savePatchFile(f.withFileExtension("ambk"));
                         });
  };
  loadButton.onClick = [this] {
    chooser = std::make_unique<juce::FileChooser>("Load Ambika patch", juce::File(), "*.ambk");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc) {
                           auto f = fc.getResult();
                           if (f.existsAsFile()) proc.loadPatchFile(f);
                         });
  };

  boardBox.addItemList({"SMR4  (LM13700 4-pole LP)", "4P  (SSM2164 4-pole multimode)", "SVF  (SSM2164 2-pole multimode)"}, 1);
  addAndMakeVisible(boardBox);
  boardAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.state(), pid::board, boardBox);

  const char* setupIds[4] = {pid::output, pid::spread, pid::noise, pid::voices};
  const char* setupNames[4] = {"OUTPUT", "STEREO", "HISS", "VOICES"};
  for (size_t i = 0; i < 4; ++i) {
    auto& k = setupKnobs[i];
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 14);
    k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(k);
    setupAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.state(), setupIds[i], k);
    setupLabels[i].setText(setupNames[i], juce::dontSendNotification);
    setupLabels[i].setJustificationType(juce::Justification::centred);
    setupLabels[i].setFont(juce::FontOptions(10.0f, juce::Font::bold));
    setupLabels[i].setColour(juce::Label::textColourId, kInkDim);
    addAndMakeVisible(setupLabels[i]);
  }

  setSize(kWidth, kHeight);
  selectPage(0, false);
  startTimerHz(30);
}

AmbikaEditor::~AmbikaEditor() {
  stopTimer();
  for (auto& a : knobAttachments) a.reset();
  setLookAndFeel(nullptr);
}

void AmbikaEditor::refreshPresetBox() {
  presetBox.clear(juce::dontSendNotification);
  for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem(proc.getProgramName(i), i + 1);
  presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void AmbikaEditor::selectPage(int page, bool cycle) {
  if (cycle && page == currentPage && pages[static_cast<size_t>(page)].subpages.size() > 1) {
    auto& sub = currentSub[static_cast<size_t>(page)];
    sub = (sub + 1) % static_cast<int>(pages[static_cast<size_t>(page)].subpages.size());
  }
  currentPage = page;
  bool seq = pages[static_cast<size_t>(page)].button == "SEQ";
  seqEditor.setVisible(seq);
  for (auto& k : knobs) k.setVisible(!seq);
  for (auto& l : knobLabels) l.setVisible(!seq);
  for (auto& l : knobValues) l.setVisible(!seq);
  attachKnobs();
  updateLcd();
  repaint();
}

void AmbikaEditor::attachKnobs() {
  const auto& sp = pages[static_cast<size_t>(currentPage)].subpages[static_cast<size_t>(currentSub[static_cast<size_t>(currentPage)])];
  pageTitle.setText(sp.title.toUpperCase(), juce::dontSendNotification);
  touchedParam = nullptr;
  for (size_t i = 0; i < 8; ++i) {
    knobAttachments[i].reset();
    const char* id = sp.params[i];
    auto& k = knobs[i];
    k.onDragStart = nullptr;
    k.onValueChange = nullptr;
    if (id != nullptr) {
      knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.state(), id, k);
      const ParamDef* def = findParam(id);
      k.onDragStart = [this, def] { touchedParam = def; touchedTicks = 45; };
      k.onValueChange = [this, def] { touchedParam = def; touchedTicks = 45; };
      k.setEnabled(true);
      k.setAlpha(1.0f);
      const ParamDef* d = findParam(id);
      knobLabels[i].setText(d ? juce::String(d->shortName).toUpperCase() : juce::String(), juce::dontSendNotification);
    } else {
      k.setEnabled(false);
      k.setAlpha(0.25f);
      knobLabels[i].setText({}, juce::dontSendNotification);
    }
  }
}

void AmbikaEditor::updateLcd() {
  const auto& sp = pages[static_cast<size_t>(currentPage)].subpages[static_cast<size_t>(currentSub[static_cast<size_t>(currentPage)])];
  juce::String lines[2];
  if (pages[static_cast<size_t>(currentPage)].button == "SEQ") {
    auto v = [this](const char* id) { return juce::roundToInt(proc.state().getRawParameterValue(id)->load()); };
    const ParamDef* mode = findParam("arp_mode");
    lines[0] = (juce::String("a/sq ") + formatValue(*mode, v("arp_mode"))).paddedRight(' ', 20) +
               (juce::String("grid ") + formatValue(*findParam("arp_grid"), v("arp_grid"))).paddedRight(' ', 20);
    lines[1] = ("len1" + juce::String(v("seq1_len")).paddedLeft(' ', 4)).paddedRight(' ', 10) +
               ("len2" + juce::String(v("seq2_len")).paddedLeft(' ', 4)).paddedRight(' ', 10) +
               ("lenp" + juce::String(v("patt_len")).paddedLeft(' ', 4)).paddedRight(' ', 10) +
               (proc.lastMidiNote.load() >= 0 ? "key " + juce::MidiMessage::getMidiNoteName(proc.lastMidiNote.load(), true, true, 4) : juce::String());
  } else {
    for (int i = 0; i < 8; ++i) {
      const char* id = sp.params[static_cast<size_t>(i)];
      juce::String cell;
      if (id != nullptr) {
        if (const ParamDef* d = findParam(id)) {
          int v = juce::roundToInt(proc.state().getRawParameterValue(id)->load());
          juce::String name = juce::String(d->shortName).substring(0, 4).paddedRight(' ', 4);
          juce::String val = formatValue(*d, v).substring(0, 5);
          cell = name + val.paddedLeft(' ', 5);
        }
      }
      lines[i / 4] += cell.paddedRight(' ', 9) + (i % 4 == 3 ? "" : "|");
    }
  }
  lcd.setLines(lines[0].paddedRight(' ', 40).substring(0, 40), lines[1].paddedRight(' ', 40).substring(0, 40));
}

void AmbikaEditor::timerCallback() {
  updateLcd();
  // Title line: full name and value of the parameter being edited.
  const auto& sp = pages[static_cast<size_t>(currentPage)].subpages[static_cast<size_t>(currentSub[static_cast<size_t>(currentPage)])];
  if (touchedParam != nullptr && touchedTicks > 0) {
    --touchedTicks;
    int v = juce::roundToInt(proc.state().getRawParameterValue(touchedParam->id)->load());
    pageTitle.setText(sp.title.toUpperCase() + "   >   " + juce::String(touchedParam->longName).toUpperCase() +
                      ":  " + formatValue(*touchedParam, v), juce::dontSendNotification);
  } else {
    pageTitle.setText(sp.title.toUpperCase(), juce::dontSendNotification);
  }
  for (size_t i = 0; i < 8; ++i) {
    const char* id = sp.params[i];
    juce::String text;
    if (id != nullptr)
      if (const ParamDef* d = findParam(id))
        text = formatValue(*d, juce::roundToInt(proc.state().getRawParameterValue(id)->load()));
    knobValues[i].setText(text, juce::dontSendNotification);
  }
  bool dirty = false;
  for (size_t i = 0; i < 6; ++i) {
    float target = proc.voiceLevel[i].load() / 255.0f;
    if (std::abs(target - ledLevels[i]) > 0.01f) { ledLevels[i] = target; dirty = true; }
  }
  for (size_t i = 0; i < 3; ++i) {
    float target = proc.lfoValue[i].load() / 255.0f;
    if (std::abs(target - lfoLeds[i]) > 0.01f) { lfoLeds[i] = target; dirty = true; }
  }
  if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
  if (dirty) repaint(getLocalBounds().withTrimmedLeft(660));
  if (seqEditor.isVisible()) seqEditor.repaint();
}

void AmbikaEditor::paint(juce::Graphics& g) {
  auto r = getLocalBounds().toFloat();
  g.setGradientFill(juce::ColourGradient(kPanelLight, 0, 0, kPanel, 0, r.getHeight(), false));
  g.fillAll();
  // Brushed strip on top.
  g.setColour(juce::Colour(0xff121314));
  g.fillRect(0.0f, 0.0f, r.getWidth(), 44.0f);
  g.setColour(kInk);
  g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
  g.drawText("AMBIKA", 24, 6, 200, 32, juce::Justification::centredLeft);
  g.setColour(kInkDim);
  g.setFont(juce::FontOptions(11.0f));
  g.drawText("6-voice hybrid polysynth  -  voicecard firmware port  (unofficial)", 150, 14, 460, 18,
             juce::Justification::centredLeft);

  // Screws.
  for (auto p : {juce::Point<float>(10, 54), juce::Point<float>(r.getWidth() - 10, 54),
                 juce::Point<float>(10, r.getHeight() - 10), juce::Point<float>(r.getWidth() - 10, r.getHeight() - 10)}) {
    g.setColour(juce::Colour(0xff55585c));
    g.fillEllipse(juce::Rectangle<float>(7, 7).withCentre(p));
    g.setColour(juce::Colour(0xff222324));
    g.drawLine(p.x - 2.5f, p.y, p.x + 2.5f, p.y, 1.0f);
  }

  // Page button LEDs.
  for (size_t i = 0; i < pageButtons.size(); ++i) {
    auto b = pageButtons[i]->getBounds().toFloat();
    auto led = juce::Rectangle<float>(8, 8).withCentre({b.getCentreX(), b.getY() - 8});
    bool on = static_cast<int>(i) == currentPage;
    g.setColour(on ? kAccent : juce::Colour(0xff3b1a17));
    g.fillEllipse(led);
    if (on) {
      g.setColour(kAccent.withAlpha(0.3f));
      g.fillEllipse(led.expanded(3));
      // Sub-page dots.
      int n = static_cast<int>(pages[i].subpages.size());
      if (n > 1) {
        for (int s = 0; s < n; ++s) {
          g.setColour(s == currentSub[i] ? kInk : kInkDim.withAlpha(0.4f));
          g.fillEllipse(b.getX() + 6 + s * 7.0f, b.getBottom() + 4, 4, 4);
        }
      }
    }
  }

  // Right panel: voicecards.
  auto panel = juce::Rectangle<float>(660, 56, 300, 400);
  g.setColour(juce::Colour(0xff17181a));
  g.fillRoundedRectangle(panel, 6.0f);
  g.setColour(kInkDim);
  g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  g.drawText("VOICECARDS", juce::Rectangle<float>(676, 150, 120, 14), juce::Justification::centredLeft);
  int numVoices = juce::roundToInt(proc.state().getRawParameterValue(pid::voices)->load());
  for (int i = 0; i < 6; ++i) {
    float x = 680.0f + i * 44.0f;
    auto slot = juce::Rectangle<float>(x - 4, 168, 36, 52);
    g.setColour(i < numVoices ? juce::Colour(0xff0f3d1f) : juce::Colour(0xff1f2022));
    g.fillRoundedRectangle(slot, 3.0f);
    auto led = juce::Rectangle<float>(12, 12).withCentre({slot.getCentreX(), slot.getY() + 16});
    float l = i < numVoices ? ledLevels[static_cast<size_t>(i)] : 0.0f;
    g.setColour(juce::Colour(0xff3b1a17).interpolatedWith(kAccent, l));
    g.fillEllipse(led);
    if (l > 0.05f) {
      g.setColour(kAccent.withAlpha(0.35f * l));
      g.fillEllipse(led.expanded(4));
    }
    g.setColour(kInkDim);
    g.drawText(juce::String(i + 1), slot.withTrimmedTop(30), juce::Justification::centred);
  }
  g.drawText("LFO", juce::Rectangle<float>(676, 230, 40, 14), juce::Justification::centredLeft);
  for (int i = 0; i < 3; ++i) {
    auto led = juce::Rectangle<float>(9, 9).withCentre({720.0f + i * 22.0f, 237.0f});
    g.setColour(juce::Colour(0xff17301b).interpolatedWith(juce::Colour(0xff58e07a), lfoLeds[static_cast<size_t>(i)]));
    g.fillEllipse(led);
  }
}

void AmbikaEditor::resized() {
  lcd.setBounds(24, 74, 620, 96);
  pageTitle.setBounds(28, 52, 600, 18);
  const int knobW = 620 / 8;
  for (int i = 0; i < 8; ++i) {
    knobs[static_cast<size_t>(i)].setBounds(24 + i * knobW + 6, 200, knobW - 12, knobW - 12);
    knobLabels[static_cast<size_t>(i)].setBounds(24 + i * knobW, 182, knobW, 16);
    knobValues[static_cast<size_t>(i)].setBounds(24 + i * knobW, 266, knobW, 16);
  }
  seqEditor.setBounds(24, 178, 620, 186);
  const int btnW = 620 / 8;
  for (size_t i = 0; i < pageButtons.size(); ++i)
    pageButtons[i]->setBounds(24 + static_cast<int>(i) * btnW + 4, 400, btnW - 8, 32);

  presetBox.setBounds(676, 70, 268, 26);
  prevButton.setBounds(676, 104, 30, 26);
  nextButton.setBounds(710, 104, 30, 26);
  loadButton.setBounds(748, 104, 60, 26);
  saveButton.setBounds(812, 104, 60, 26);
  randomButton.setBounds(876, 104, 68, 26);
  boardBox.setBounds(676, 258, 268, 26);
  for (int i = 0; i < 4; ++i) {
    setupLabels[static_cast<size_t>(i)].setBounds(676 + i * 68, 296, 64, 14);
    setupKnobs[static_cast<size_t>(i)].setBounds(676 + i * 68, 310, 64, 78);
  }
}
