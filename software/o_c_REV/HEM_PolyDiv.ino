// Copyright (c) 2024, Nicholas Michalek
// Copyright (c) 2022, Benjamin Rosenbach
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/* Used ProbDiv applet from benirose as a template */

#include "HSProbLoopLinker.h" // singleton for linking ProbDiv and ProbMelo

class PolyDiv : public HemisphereApplet {
public:

    static constexpr uint8_t MAX_DIV = 32;
    static constexpr uint8_t NUM_STEPS = 5;

    enum PolyDivCursor {
        STEP1A, STEP2A, STEP3A, STEP4A, STEP5A,
        STEP1B, STEP2B, STEP3B, STEP4B, STEP5B,
        LAST_SETTING = STEP5B
    };

    struct DivSequence {
      int step_index;
      int clock_count;
      // duration as number of clock pulses
      uint8_t steps[NUM_STEPS] = { 1, 2, 0, 0, 0 };

      bool Poke() {
            // reset case
            if (step_index < 0) {
                ++step_index;
                return true;
            }

            // quota achieved, advance to next enabled step
            if (++clock_count >= steps[step_index]) {
                clock_count = 0;
                do { // lol this will get stuck if anything goes wrong...
                    ++step_index %= NUM_STEPS;
                } while (steps[step_index] == 0);
                return true;
            }
            return false;
      }
      void Reset() {
        step_index = -1;
        clock_count = 0;
      }

    } div_seq[2];

    const char* applet_name() {
        return "PolyDiv";
    }

    void Start() {
    }

    void Reset() {
        ForEachChannel(ch) {
          div_seq[ch].Reset();
        }
        reset_animation = HEMISPHERE_PULSE_ANIMATION_TIME_LONG;
    }
    void TrigOut(int ch) {
        ClockOut(ch);
        loop_linker->Trigger(ch);
        pulse_animation[ch] = HEMISPHERE_PULSE_ANIMATION_TIME;
    }

    void Controller() {
        loop_linker->RegisterDiv(hemisphere);

        // reset
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) {
          ForEachChannel(ch) {
            if (div_seq[ch].Poke())
              TrigOut(ch);
          }
        }

        ForEachChannel(ch) {
          if (pulse_animation[ch] > 0) {
              pulse_animation[ch]--;
          }
        }
        if (reset_animation > 0) {
            reset_animation--;
        }
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        CursorAction(cursor, LAST_SETTING);
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, LAST_SETTING);
            return;
        }

        const int ch = cursor / NUM_STEPS;
        const int s = cursor % NUM_STEPS;
        const int div = div_seq[ch].steps[s] + direction;
        //if (div < 0) div = MAX_DIV;
        div_seq[ch].steps[s] = constrain(div, (cursor == STEP1A || cursor == STEP1B) ? 1 : 0, MAX_DIV);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: 5 bits only stores 0-31, but I really want to have /32
        // 2 x 5 x 5 == 50 bits == OK
        // 2 x 5 x 6 == 60 bits == ...also OK. hmm
        // so this means either 5-step patterns of 6 bits, or two 6-step patterns are feasible
        const size_t b = 5; // bitsize
        ForEachChannel(ch) {
            for (size_t i = 0; i < NUM_STEPS; i++) {
                Pack(data, PackLocation {ch*NUM_STEPS*b + i*b, b}, div_seq[ch].steps[i]);
            }
        }
        return data;
    }

    void OnDataReceive(uint64_t data) {
        const size_t b = 5; // bitsize
        ForEachChannel(ch) {
            for (size_t i = 0; i < NUM_STEPS; i++) {
                div_seq[ch].steps[i] = Unpack(data, PackLocation {ch*NUM_STEPS*b + i*b, b});
                div_seq[ch].steps[i] = constrain(div_seq[ch].steps[i], 0, MAX_DIV);
            }
            // step 1 cannot be zero
            if (div_seq[ch].steps[0] == 0) ++div_seq[ch].steps[0];
        }
        Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock  2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=       2=";
        help[HEMISPHERE_HELP_OUTS]     = "A=Trig   B=Skips";
        help[HEMISPHERE_HELP_ENCODER]  = "Division per step";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor; // PolyDivCursor 

    int pulse_animation[2] = {0,0};
    int reset_animation = 0;

    ProbLoopLinker *loop_linker = loop_linker->get();

    void DrawInterface() {
      // divisions
      ForEachChannel(ch) {
        for(int i = 0; i < NUM_STEPS; i++) {
          gfxPrint(1 + 31*ch, 15 + (i*10), div_seq[ch].steps[i]);
          DrawSlider(14 + 31*ch, 15 + (i*10), 15, div_seq[ch].steps[i], MAX_DIV, cursor == i+ch*NUM_STEPS);

          if (div_seq[ch].step_index == i)
            gfxIcon(27 + 31*ch, 15 + i*10, LEFT_BTN_ICON);
        }
        // flash division when triggered
        if (pulse_animation[ch] > 0 && div_seq[ch].step_index >= 0) {
          gfxInvert(1 + 31*ch, 15 + (div_seq[ch].step_index*10), 12, 8);
        }
      }
    }

};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to PolyDiv,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
PolyDiv PolyDiv_instance[2];

void PolyDiv_Start(bool hemisphere) {PolyDiv_instance[hemisphere].BaseStart(hemisphere);}
void PolyDiv_Controller(bool hemisphere, bool forwarding) {PolyDiv_instance[hemisphere].BaseController(forwarding);}
void PolyDiv_View(bool hemisphere) {PolyDiv_instance[hemisphere].BaseView();}
void PolyDiv_OnButtonPress(bool hemisphere) {PolyDiv_instance[hemisphere].OnButtonPress();}
void PolyDiv_OnEncoderMove(bool hemisphere, int direction) {PolyDiv_instance[hemisphere].OnEncoderMove(direction);}
void PolyDiv_ToggleHelpScreen(bool hemisphere) {PolyDiv_instance[hemisphere].HelpScreen();}
uint64_t PolyDiv_OnDataRequest(bool hemisphere) {return PolyDiv_instance[hemisphere].OnDataRequest();}
void PolyDiv_OnDataReceive(bool hemisphere, uint64_t data) {PolyDiv_instance[hemisphere].OnDataReceive(data);}
