// Ambika VST - editor. Mimics the Ambika workflow: a 40x2 character LCD,
// 8 pots below it, 8 page buttons (pressing a page again cycles through its
// sub-pages), voicecard activity LEDs. GPL v3.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class AmbikaLookAndFeel : public juce::LookAndFeel_V4 {
 public:
  AmbikaLookAndFeel();
  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                        float start, float end, juce::Slider&) override;
  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                            bool highlighted, bool down) override;
};

class LcdDisplay : public juce::Component {
 public:
  void setLines(const juce::String& a, const juce::String& b) {
    if (a != line1 || b != line2) { line1 = a; line2 = b; repaint(); }
  }
  // Cursor on one of the 8 parameter cells (-1 = none).
  void setCursor(int cell) { if (cell != cursor) { cursor = cell; repaint(); } }
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  std::function<void(int)> onCellClicked;
 private:
  juce::String line1, line2;
  int cursor = -1;
};

// The Ambika's clickable endless encoder: turn = step the value under the
// cursor, push = move the cursor to the next parameter.
class EncoderKnob : public juce::Component {
 public:
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override;
  void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
  std::function<void(int)> onTurn;
  std::function<void()> onPush;
 private:
  float angle = 0.0f;
  float dragAccumulator = 0.0f;
  int lastY = 0;
  bool dragged = false;
  bool pressed = false;
  float wheelAccumulator = 0.0f;
};

class SequenceEditor : public juce::Component {
 public:
  explicit SequenceEditor(AmbikaProcessor& p) : proc(p) {}
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseUp(const juce::MouseEvent&) override { dragRow = -1; }
 private:
  void edit(const juce::MouseEvent&, bool initial);
  juce::Rectangle<float> rowArea(int row) const;
  AmbikaProcessor& proc;
  int dragRow = -1;
};

class AmbikaEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit AmbikaEditor(AmbikaProcessor&);
  ~AmbikaEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  struct SubPage { juce::String title; std::array<const char*, 8> params; };
  struct Page { juce::String button; std::vector<SubPage> subpages; };

  void timerCallback() override;
  void selectPage(int page, bool cycle);
  void attachKnobs();
  void updateLcd();
  void refreshPresetBox();

  AmbikaProcessor& proc;
  AmbikaLookAndFeel lnf;
  LcdDisplay lcd;
  EncoderKnob encoder;
  juce::Label encoderLabel;
  int cursorCell = 0;
  void encoderTurn(int steps);
  void encoderPush();
  void setCursorCell(int cell);
  juce::Label pageTitle;
  std::array<juce::Slider, 8> knobs;
  std::array<juce::Label, 8> knobLabels;
  std::array<juce::Label, 8> knobValues;
  std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> knobAttachments;
  std::vector<std::unique_ptr<juce::TextButton>> pageButtons;
  std::vector<Page> pages;
  int currentPage = 0;
  std::vector<int> currentSub;

  SequenceEditor seqEditor;

  juce::ComboBox presetBox;
  juce::TextButton prevButton{"<"}, nextButton{">"}, loadButton{"Load"}, saveButton{"Save"},
      randomButton{"Random"};
  juce::ComboBox boardBox;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boardAttachment;
  std::array<juce::Slider, 4> setupKnobs;
  std::array<juce::Label, 4> setupLabels;
  std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> setupAttachments;
  std::unique_ptr<juce::FileChooser> chooser;

  std::array<float, 6> ledLevels{};
  std::array<float, 3> lfoLeds{};
  const ambika_plugin::ParamDef* touchedParam = nullptr;
  int touchedTicks = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbikaEditor)
};
